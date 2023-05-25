#include <time.h>
#include <vector>

#include <trunk-recorder/source.h>
#include <trunk-recorder/plugin_manager/plugin_api.h>
#include <trunk-recorder/gr_blocks/decoder_wrapper.h>
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>

#include <iostream>
#include <cstdlib>
#include <string>
#include <map>
#include <cstring>
#include <mqtt/client.h>

using namespace std;

const int QOS = 1;
const auto TIMEOUT = std::chrono::seconds(10);

class Mqtt_Status : public Plugin_Api, public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
  bool m_open = false;
  bool unit_enabled = false;
  bool message_enabled = false;
  bool tr_calls_set = false;

  int refresh;
  std::vector<Source *> sources;
  std::vector<System *> systems;
  std::vector<Call *> tr_calls;
  Config *config;
  std::string client_id;
  std::string mqtt_broker;
  std::string username;
  std::string password;
  std::string topic;
  std::string unit_topic;
  std::string message_topic;
  std::string instance_id;
  mqtt::async_client *client;

  time_t config_resend_time = time(NULL);
  time_t call_resend_time = time(NULL);

  std::map<std::string, int> system_map;

  std::map<short, std::string> opcodes = {
    {-1, "** Unidentified"},
    {0xff, "** Unidentified"},
    {0x00, "GRP_V_CH_GRANT"},
    {0x02, "GRP_V_CH_GRANT_UPDT"},
    {0x03, "GRP_V_CH_GRANT_UPDT_EXP"},
    {0x04, "UU_V_CH_GRANT"},
    {0x05, "UU_ANS_REQ"},
    {0x06, "UU_V_CH_GRANT_UPDT"},
    {0x08, "Telephone Interconnect Voice Channel Grant"},
    {0x09, "Telephone Interconnect Voice Channel Grant Update"},
    {0x0a, "Telephone Interconnect Answer Request"},
    {0x14, "DATA_GRANT"},
    {0x15, "SNDCP Data Page Request"},
    {0x16, "SNDCP Data Channel Announcement -Explicit"},
    {0x18, "Status Update"},
    {0x1a, "Status Query"},
    {0x1c, "Message Update"},
    {0x1d, "Radio Unit Monitor Command"},
    {0x1f, "Call Alert"},
    {0x20, "Acknowledge Response"},
    {0x21, "Extended Function Command"},
    {0x24, "Extended Function Command"},
    {0x27, "Deny Response"},
    {0x28, "Unit Group Affiliation"},
    {0x29, "Secondary Control Channel Broadcast - Explicit"},
    {0x2a, "Group Affiliation Query"},
    {0x2b, "Location Registration Response"},
    {0x2c, "Unit Registration Response"},
    {0x2d, "AUTHENTICATION COMMAND"},
    {0x2e, "DE-REGISTRATION ACKNOWLEDGE"},
    {0x2f, "Unit DeRegistration Ack"},
    {0x30, "TDMA SYNCHRONIZATION BROADCAST / PATCH"},
    {0x31, "AUTHENTICATION DEMAND"},
    {0x32, "AUTHENTICATION RESPONSE"},
    {0x33, "iden up tdma id"},
    {0x34, "iden vhf/uhf id"},
    {0x35, "Time and Date Announcement"},
    {0x36, "ROAMING ADDRESS COMMAND"},
    {0x37, "ROAMING ADDRESS UPDATE"},
    {0x38, "SYSTEM SERVICE BROADCAST"},
    {0x39, "secondary cc"},
    {0x3a, "rfss status"},
    {0x3b, "network status"},
    {0x3c, "adjacent status"},
    {0x3d, "iden_up"},
    {0x3e, "P_PARM_BCST"},
  };

  std::map<short, std::string> messages_types = {
    {0, "GRANT"},
    {1, "STATUS"},
    {2, "UPDATE"},
    {3, "CONTROL_CHANNEL"},
    {4, "REGISTRATION"},
    {5, "DEREGISTRATION"},
    {6, "AFFILIATION"},
    {7, "SYSID"},
    {8, "ACKNOWLEDGE"},
    {9, "LOCATION"},
    {10, "PATCH_ADD"},
    {11, "PATCH_DELETE"},
    {12, "DATA_GRANT"},
    {13, "UU_ANS_REQ"},
    {14, "UU_V_GRANT"},
    {15, "UU_V_UPDATE"},
    {16, "ADJ_STS_BCST"},
    {99, "UNKNOWN"},
  };


public:
  Mqtt_Status() {};

  // ********************************
  // trunk-recorder MQTT messages
  // ********************************

  int trunk_message(std::vector<TrunkMessage> messages, System *system) override 
  {
    // MQTT: message_topic/short_name
    // Lay the groundwork for trunk_message!
    
    if ((this->message_enabled)) {  
      for (std::vector<TrunkMessage>::iterator it = messages.begin(); it != messages.end(); it++) {
        TrunkMessage message = *it;
        boost::property_tree::ptree message_node;

        message_node.put("short_name", system->get_short_name());
        message_node.put("sys_num", system->get_sys_num());
        message_node.put("trunk_msg", message.message_type);
        message_node.put("trunk_msg_alpha", messages_types[message.message_type]);
        message_node.put("opcode", message.opcode);
        message_node.put("opcode_alpha", opcodes[message.opcode]);

        send_object(message_node, "message", system->get_short_name().c_str(), this->message_topic);
      }
    }
  return 0;
  }

  int system_rates(std::vector<System *> systems, float timeDiff) override
  {
    // MQTT: topic/rates
    // Send control channel messages per second updates

    boost::property_tree::ptree system_node;
    boost::property_tree::ptree systems_node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      std::string sys_type = system->get_system_type();

      // Filter out conventional systems.  They do not have a call rate and 
      // get_current_control_channel() will cause a sefgault on non-trunked systems.
      if (sys_type.find("conventional") == std::string::npos) {
        system_node = system->get_stats_current(timeDiff);
        system_node.put("short_name", system->get_short_name());
        system_node.put("control_channel", system->get_current_control_channel());
        systems_node.push_back(std::make_pair("", system_node));
      }
    }

    return send_object(systems_node, "rates", "rates", this->topic);
  }

  void send_config(std::vector<Source *> sources, std::vector<System *> systems)
  {
    // MQTT: topic/config
    // Periodically send elements of the trunk recorder config.json

    if (m_open == false)
      return;

    boost::property_tree::ptree root;
    boost::property_tree::ptree systems_node;
    boost::property_tree::ptree sources_node;

    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); ++it)
    {
      Source *source = *it;
      std::vector<Gain_Stage_t> gain_stages;
      boost::property_tree::ptree source_node;
      source_node.put("source_num", source->get_num());
      source_node.put("antenna", source->get_antenna());

      source_node.put("silence_frames", source->get_silence_frames());

      source_node.put("min_hz", source->get_min_hz());
      source_node.put("max_hz", source->get_max_hz());
      source_node.put("center", source->get_center());
      source_node.put("rate", source->get_rate());
      source_node.put("driver", source->get_driver());
      source_node.put("device", source->get_device());
      source_node.put("error", source->get_error());
      source_node.put("gain", source->get_gain());
      gain_stages = source->get_gain_stages();
      for (std::vector<Gain_Stage_t>::iterator gain_it = gain_stages.begin(); gain_it != gain_stages.end(); ++gain_it)
      {
        source_node.put(gain_it->stage_name + "_gain", gain_it->value);
      }
      source_node.put("antenna", source->get_antenna());
      source_node.put("analog_recorders", source->analog_recorder_count());
      source_node.put("digital_recorders", source->digital_recorder_count());
      source_node.put("debug_recorders", source->debug_recorder_count());
      source_node.put("sigmf_recorders", source->sigmf_recorder_count());
      sources_node.push_back(std::make_pair("", source_node));
    }

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *sys = (System *)*it;

      boost::property_tree::ptree sys_node;
      boost::property_tree::ptree channels_node;
      sys_node.put("audioArchive", sys->get_audio_archive());
      sys_node.put("systemType", sys->get_system_type());
      sys_node.put("shortName", sys->get_short_name());
      sys_node.put("sysNum", sys->get_sys_num());
      sys_node.put("uploadScript", sys->get_upload_script());
      sys_node.put("recordUnkown", sys->get_record_unknown());
      sys_node.put("callLog", sys->get_call_log());
      sys_node.put("talkgroupsFile", sys->get_talkgroups_file());
      sys_node.put("analog_levels", sys->get_analog_levels());
      sys_node.put("digital_levels", sys->get_digital_levels());
      sys_node.put("qpsk", sys->get_qpsk_mod());
      sys_node.put("squelch_db", sys->get_squelch_db());
      std::vector<double> channels;

      if ((sys->get_system_type() == "conventional") || (sys->get_system_type() == "conventionalP25"))
      {
        channels = sys->get_channels();
      }
      else
      {
        channels = sys->get_control_channels();
        sys_node.put("controlChannel", sys->get_current_control_channel());
      }

      for (std::vector<double>::iterator chan_it = channels.begin(); chan_it != channels.end(); ++chan_it)
      {
        double channel = *chan_it;
        boost::property_tree::ptree channel_node;
        channel_node.put("", channel);
        channels_node.push_back(std::make_pair("", channel_node));
      }
      sys_node.add_child("channels", channels_node);

      if (sys->get_system_type() == "smartnet")
      {
        sys_node.put("bandplan", sys->get_bandplan());
        sys_node.put("bandfreq", sys->get_bandfreq());
        sys_node.put("bandplan_base", sys->get_bandplan_base());
        sys_node.put("bandplan_high", sys->get_bandplan_high());
        sys_node.put("bandplan_spacing", sys->get_bandplan_spacing());
        sys_node.put("bandplan_offset", sys->get_bandplan_offset());
      }
      systems_node.push_back(std::make_pair("", sys_node));
    }
    root.add_child("sources", sources_node);
    root.add_child("systems", systems_node);
    root.put("captureDir", this->config->capture_dir);
    root.put("uploadServer", this->config->upload_server);

    // root.put("defaultMode", default_mode);
    root.put("callTimeout", this->config->call_timeout);
    root.put("logFile", this->config->log_file);
    root.put("instanceId", this->config->instance_id);
    root.put("instanceKey", this->config->instance_key);
    root.put("logFile", this->config->log_file);
    root.put("type", "config");

    if (this->config->broadcast_signals == true)
    {
      root.put("broadcast_signals", this->config->broadcast_signals);
    }

    send_object(root, "config", "config", this->topic);
  }

  int send_systems(std::vector<System *> systems)
  {
    // MQTT: topic/systems
    // Send the configuration information for all systems

    boost::property_tree::ptree system_node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      system_node.push_back(std::make_pair("", system->get_stats()));
    }
    return send_object(system_node, "systems", "systems", this->topic);
  }

  int send_system(System *system)
  {
    // MQTT: topic/system
    // Send the configuration information for a single system

    return send_object(system->get_stats(), "system", "system", this->topic);
  }

  int calls_active(std::vector<Call *> calls) override
  {
    // MQTT: topic/calls_active
    // Send the list of all active calls
    
    // Reset a pointer to the active call list if needed later
    this->tr_calls = calls;
    this->tr_calls_set = true;
    
    boost::property_tree::ptree call_node;

    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); ++it)
    {
      Call *call = *it;
      if ( (call->get_current_length() > 0) || (!call->is_conventional()) ) {
        call_node.push_back(std::make_pair("", round_stats(call->get_stats())));
      }
    }

    return send_object(call_node, "calls", "calls_active", this->topic);
  }

  int send_recorders(std::vector<Recorder *> recorders)
  {
    // MQTT: topic/recorders
    // Send the status of all recorders

    boost::property_tree::ptree rec_node;

    for (std::vector<Recorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
    {
      Recorder *recorder = *it;
      // Some stats are rounded to prevent long/repeating floats
      rec_node.push_back(std::make_pair("", round_stats(recorder->get_stats())));
    }

    return send_object(rec_node, "recorders", "recorders", this->topic);
  }

  int send_recorder(Recorder *recorder)
  {
    // MQTT: topic/recorders
    // Send the status of a single recorder

    // Some stats are rounded to prevent long/repeating floats
    return send_object(round_stats(recorder->get_stats()), "recorder", "recorder", this->topic);
  }

  int call_start(Call *call) override
  {
    // MQTT: topic/call_start
    // MQTT: unit_topic/shortname/call
    // Send information about a new call and report information on the unit initiating it.

    long talkgroup_num = call->get_talkgroup();
    long source_id = call->get_current_source_id();
    std::string short_name = call->get_short_name();
    
    if ((this->unit_enabled))
    {
      boost::property_tree::ptree call_node;
      std::vector<unsigned long> talkgroup_patches = call->get_system()->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;
      BOOST_FOREACH (auto& TGID, talkgroup_patches)
      {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      call_node.put("callNum", call->get_call_num());
      call_node.put("system", short_name );
      call_node.put("unit", source_id );
      call_node.put("unit_alpha", call->get_system()->find_unit_tag(source_id));
      call_node.put("start_time", call->get_start_time());
      call_node.put("freq", call->get_freq());
      call_node.put("talkgroup", talkgroup_num);
      call_node.put("talkgroup_patches", patch_string);
      call_node.put("talkgroup_alpha_tag", call->get_talkgroup_tag());
      call_node.put("talkgroup_patches", patch_string);
      call_node.put("encrypted", call->get_encrypted());
      send_object(call_node, "call", "call", this->unit_topic+"/"+short_name);
    }

    // Some stats are rounded to prevent long/repeating floats
    return send_object(round_stats(call->get_stats()), "call", "call_start", this->topic);
  }

  int call_end(Call_Data_t call_info) override
  {
    // MQTT: topic/call_end
    // MQTT: unit_topic/shortname/end
    // Send information about a completed call and report participataing units.
    // This enables the collection unit information in conventional systems without a control channel.

    // Generate list of talkgroup patches
    std::vector<unsigned long> talkgroup_patches = call_info.patched_talkgroups;
    std::string patch_string;
    bool first = true;
    BOOST_FOREACH (auto& TGID, talkgroup_patches)
    {
      if (!first) { patch_string += ","; }
      first = false;
      patch_string += std::to_string(TGID);
    }

    if (this->unit_enabled)
    {
      boost::property_tree::ptree end_node;
      // source_list[] can be used to suppliment transmission_list[] info
      std::vector<Call_Source> source_list = call_info.transmission_source_list;
      int transmision_num = 0;
      
      BOOST_FOREACH (auto& transmission, call_info.transmission_list)
      {
        end_node.put("callNum", call_info.call_num);
        end_node.put("system", call_info.short_name);
        end_node.put("unit", transmission.source);
        end_node.put("unit_alpha", source_list[transmision_num].tag);
        end_node.put("start_time", transmission.start_time);
        end_node.put("stop_time", transmission.stop_time);
        end_node.put("sample_count", transmission.sample_count);
        end_node.put("spike_count", transmission.spike_count);
        end_node.put("error_count", transmission.error_count);
        end_node.put("freq", call_info.freq);
        end_node.put("length", transmission.length);
        end_node.put("transmission_filename", transmission.filename);
        end_node.put("transmission_base_filename", transmission.base_filename);
        end_node.put("call_filename", call_info.filename);
        end_node.put("position", source_list[transmision_num].position);
        end_node.put("talkgroup", call_info.talkgroup);
        end_node.put("talkgroup_alpha_tag",call_info.talkgroup_alpha_tag);
        end_node.put("talkgroup_description",call_info.talkgroup_description);
        end_node.put("talkgroup_group",call_info.talkgroup_group);
        end_node.put("talkgroup_patches", patch_string);
        end_node.put("encrypted", call_info.encrypted);
        end_node.put("emergency", source_list[transmision_num].emergency);
        end_node.put("signal_system", source_list[transmision_num].signal_system);
        
        // Some stats are rounded to prevent long/repeating floats
        send_object(round_stats(end_node), "end", "end", this->unit_topic+"/"+call_info.short_name.c_str());
        transmision_num++;
      }
    }
    boost::property_tree::ptree call_node;
    //call_node.put("status",call_info.status);
    call_node.put("callNum",call_info.call_num);
    call_node.put("process_call_time",call_info.process_call_time);
    call_node.put("retry_attempt",call_info.retry_attempt);
    call_node.put("error_count",call_info.error_count);
    call_node.put("spike_count",call_info.spike_count);
    call_node.put("freq",call_info.freq);
    call_node.put("encrypted",call_info.encrypted);
    call_node.put("emergency",call_info.emergency);
    call_node.put("tdma_slot",call_info.tdma_slot);
    call_node.put("phase2_tdma",call_info.phase2_tdma);
    //call_node.put("transmission_list",call_info.transmission_list);
    call_node.put("short_name",call_info.short_name);
    //call_node.put("upload_script",call_info.upload_script = sys->get_upload_script();
    //call_node.put("audio_archive",call_info.audio_archive = sys->get_audio_archive();
    //call_node.put("transmission_archive",call_info.transmission_archive = sys->get_transmission_archive();
    //call_node.put("call_log",call_info.call_log = sys->get_call_log();
    call_node.put("call_num",call_info.call_num);
    //call_node.put("compress_wav",call_info.compress_wav);
    call_node.put("talkgroup",call_info.talkgroup);
    //call_node.put("talkgroup_display",call_info.talkgroup_display = call->get_talkgroup_display();
    //call_info.patched_talkgroups = sys->get_talkgroup_patch(call_info.talkgroup);
    //call_info.min_transmissions_removed = 0;
    call_node.put("talkgroup_tag",call_info.talkgroup_tag);
    call_node.put("talkgroup_alpha_tag",call_info.talkgroup_alpha_tag);
    call_node.put("talkgroup_description",call_info.talkgroup_description);
    call_node.put("talkgroup_group",call_info.talkgroup_group);
    call_node.put("talkgroup_patches", patch_string);
    call_node.put("audio_type",call_info.audio_type);

    // call_node.put("transmission_source_list",call_info.transmission_source_list);
    // call_node.put("transmission_error_list",call_info.transmission_error_list);
    call_node.put("start_time",call_info.start_time);
    call_node.put("stop_time",call_info.stop_time);
    call_node.put("length",call_info.length);

    // Some stats are rounded to prevent long/repeating floats
    return send_object(round_stats(call_node), "call", "call_end", this->topic);
  }

  int unit_registration(System *sys, long source_id) override
  {
    // MQTT: unit_topic/shortname/on
    // Unit registration on a system (on)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));

      return send_object(unit_node, "on", "on", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_deregistration(System *sys, long source_id) override
  {
    // MQTT: unit_topic/shortname/off
    // Unit de-registration on a system (off)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));

      return send_object(unit_node, "off", "off", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_acknowledge_response(System *sys, long source_id) override
  {
    // MQTT: unit_topic/shortname/ackresp
    // Unit acknowledge response (ackresp)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));

      return send_object(unit_node, "ackresp", "ackresp", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_group_affiliation(System *sys, long source_id, long talkgroup_num) override
  {
    // MQTT: unit_topic/shortname/join
    // Unit talkgroup affiliation (join)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;
      std::vector<unsigned long> talkgroup_patches = sys->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;

      BOOST_FOREACH (auto& TGID, talkgroup_patches)
      {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup_num);
      unit_node.put("talkgroup_patches", patch_string);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }

      return send_object(unit_node, "join", "join", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_data_grant(System *sys, long source_id) override
  {
    // MQTT: unit_topic/shortname/data
    // Unit data grant (data)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));
      
      return send_object(unit_node, "data", "data", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_answer_request(System *sys, long source_id, long talkgroup) override
  {
    // MQTT: unit_topic/shortname/ans_req
    // Unit answer_request (ans_req)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup);
      Talkgroup *tg = sys->find_talkgroup(talkgroup);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }

      return send_object(unit_node, "ans_req", "ans_req", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_location(System *sys, long source_id, long talkgroup_num) override
  {
    // MQTT: unit_topic/shortname/location
    // Unit location/roaming update (location)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;
      std::vector<unsigned long> talkgroup_patches = sys->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;

      BOOST_FOREACH (auto& TGID, talkgroup_patches)
      {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      unit_node.put("system", sys->get_short_name());
      unit_node.put("unit", source_id );
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup_num);
      unit_node.put("talkgroup_patches", patch_string);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }

      return send_object(unit_node, "location", "location", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }


  // ********************************
  // trunk-recorder plugin API & startup
  // ********************************

  int parse_config(boost::property_tree::ptree &cfg) override
  {
    // Called before init(); parses the config information for this plugin.
    
    this->mqtt_broker = cfg.get<std::string>("broker", "tcp://localhost:1883");
    this->client_id = cfg.get<std::string>("client_id", "tr-status");
    this->username = cfg.get<std::string>("username", "");
    this->password = cfg.get<std::string>("password", "");
    this->topic = cfg.get<std::string>("topic", "");
    this->refresh = cfg.get<int>("refresh", 60);

    this->unit_topic = cfg.get<std::string>("unit_topic", "");
    if (this->unit_topic != "")
      this->unit_enabled = true;        
    else
      this->unit_topic = "[disabled]";  

    this->message_topic = cfg.get<std::string>("message_topic", "");
    if (this->message_topic != "")
      this->message_enabled = true;
    else  
      this->message_topic = "[disabled]";

    // Print plugin startup info
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker: " << this->mqtt_broker;
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Client Name: " << this->client_id;
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker Username: " << this->username;
    if (this->password != "" )
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker Password: ********";
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Topic: " << this->topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT Unit Status Plugin Topic: " << this->unit_topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT Trunk Message Plugin Topic: " << this->message_topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT System/Config Refresh Interval: " << this->refresh;

    return 0;
  }

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    // Plugin initialization; called after parse_config().

    frequency_format = config->frequency_format;
    this->instance_id = config->instance_id;
    
    // Establish pointers to systems, sources, and configs if needed later.
    this->sources = sources;
    this->systems = systems;
    this->config = config;

    // Build a system_map for get_system_by_shortname() if needed later.
    int sys_number = 0;
    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *sys = (System *)*it;
      std::string short_name = sys->get_short_name();
      this->system_map[short_name] = sys_number;
      sys_number += 1;
    }

    return 0;
  }

  int start() override
  {
    // Called after trunk-recorder finishes setup and the plugin is initialized

    open_connection();
    return 0;
  }

  int setup_config(std::vector<Source *> sources, std::vector<System *> systems) override
  {
    // Called at the same periodicity of system_rates(), this can be use to accomplish
    // occasional plugin tasks more efficiently than checking each cycle of poll_one().
    
    resend_configs();

    return 0;
  }

  int poll_one() override
  {
    // Called during each pass through the main loop of trunk-recorder.

    resend_calls();
    return 0;
  }

  int setup_recorder(Recorder *recorder) override
  {
    // Called when a new recorder has been created.

    this->send_recorder(recorder);
    return 0;
  }

  int setup_system(System *system) override
  {
    // Called when a new system has been created.

    this->send_system(system);
    return 0;
  }

  int setup_systems(std::vector<System *> systems) override
  {
    // Called during startup when the initial systems have been created.

    this->send_systems(systems);
    return 0;
  }


  // ********************************
  // Helper functions
  // ********************************

  int resend_configs()
  {
    // Triggered by setup_config() every 3 seconds.
    // The full list of system configs and systems normally sent only once on startup, and may be
    // missed by MQTT clients.  The "refresh" option controls the interval they are resent.

    time_t now_time = time(NULL);

    if (((now_time - this->config_resend_time ) > refresh ) && (this->config_resend_time > 0))
    {
      send_config(this->sources, this->systems);
      send_systems(this->systems);
      this->config_resend_time = now_time;
    }

    // Recorders are updated every 3 seconds.
    std::vector<Recorder *> recorders;
    for (std::vector<Source *>::iterator it = this->sources.begin(); it != this->sources.end(); ++it)
    {
      Source *source = *it;
      std::vector<Recorder *> sourceRecorders = source->get_recorders();
      recorders.insert(recorders.end(), sourceRecorders.begin(), sourceRecorders.end());
    }
    send_recorders(recorders);

    return 0;
  }

  int resend_calls()
  {
    // Triggered by poll_one().
    // Use a pointer from calls_active() to update the active call list once a second.

    time_t now_time = time(NULL);

    if (((now_time - this->call_resend_time ) > 1 ))
    {
      calls_active(this->tr_calls);
      this->call_resend_time = now_time;
    }
    
    return 0;
  }

  System *get_system_by_shortname(std::string short_name)
  {
    // Workaround to find a system by its shortname to aid in metadata lookups.

    // Example:
    //   System *sys = get_system_by_shortname(call_info.short_name);
    //   BOOST_LOG_TRIVIAL(error) << sys->get_short_name();

    int sys_num = system_map[short_name];
    return this->systems[sys_num];
  }

  std::string round_to_str(double num)
  {
    // Round a float to two decimal places and return it as as string.

    char rounded[20];
    snprintf(rounded, sizeof(rounded), "%.2f", num);
    return std::string(rounded);
  }

  boost::property_tree::ptree round_stats(boost::property_tree::ptree stats)
  {
    // Parse a proprty tree for gnarly floats and round them to two decimal places

    std::vector<std::string> keys = { "position", "length", "duration" };
    BOOST_FOREACH (const std::string& key, keys) 
    {
      if (stats.find(key) != stats.not_found()) {
        stats.put(key, round_to_str(stats.get<double>(key)));
      }
    }
    return stats;
  }


  // ********************************
  // Paho MQTT
  // ********************************

  void open_connection()
  {
    // Open the connection to the destination MQTT server using paho libraries.
    // Set `m_open = true` when the connection is established.

    const char *LWT_PAYLOAD = "Last will and testament.";
    // set up access channels to only log interesting things
    client = new mqtt::async_client(this->mqtt_broker, this->client_id, config->capture_dir + "/store");

    mqtt::connect_options connOpts;

    if ((this->username != "") && (this->password != ""))
    {
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tSetting MQTT Broker username and password..." << endl;
      connOpts = mqtt::connect_options_builder().clean_session().user_name(this->username).password(this->password).will(mqtt::message("final", LWT_PAYLOAD, QOS)).finalize();
    }
    else
    {
      connOpts = mqtt::connect_options_builder().clean_session().will(mqtt::message("final", LWT_PAYLOAD, QOS)).finalize();
    }

    mqtt::ssl_options sslopts;
    sslopts.set_verify(false);
    sslopts.set_enable_server_cert_auth(false);
    connOpts.set_ssl(sslopts);
    connOpts.set_automatic_reconnect(10, 40); // this seems to be a blocking reconnect
    try
    {
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tConnecting...";
      mqtt::token_ptr conntok = client->connect(connOpts);
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tWaiting for the connection...";
      conntok->wait();
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \t ...OK";
      m_open = true;
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << exc.what() << endl;
    }
  }

  int send_object(boost::property_tree::ptree data, std::string name, std::string type, std::string object_topic)
  {
    // Send a MQTT message using the configured connection and paho libraries

    if (m_open == false)
      return 0;

    time_t now_time = time(NULL);

    boost::property_tree::ptree root;

    if (object_topic.back() == '/')
    {
      object_topic.erase(object_topic.size() - 1);
    }
    object_topic = object_topic + "/" + type;
    
    // Build the MQTT payload, add addtional keys [timestamp, instanceId]
    root.add_child(name, data);
    root.put("type", type);
    root.put("timestamp", now_time);
    if (instance_id != "") {
       root.put("instanceId", instance_id);
    }
    std::stringstream stats_str;
    boost::property_tree::write_json(stats_str, root);

    try
    {
      mqtt::message_ptr pubmsg = mqtt::make_message(object_topic, stats_str.str());
      pubmsg->set_qos(QOS);
      client->publish(pubmsg); //->wait_for(TIMEOUT);
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << "MQTT Status Plugin - " <<exc.what() << endl;
    }

    return 0;
  }

  // Paho mqtt::iaction_listeners.  Required, but not used.
  void on_failure(const mqtt::token &tok) override {};
  void on_success(const mqtt::token &tok) override {};
  
  // Paho mqtt::callbacks.
  void connection_lost(const string &cause) override
  {
    // Paho MQTT: This method is called when the connection to the server is lost.
    BOOST_LOG_TRIVIAL(error) << "MQTT Status Plugin - Connection lost to: " << this->mqtt_broker << "/tcause: " << cause;
  }


  // ********************************
  // Create the plugin
  // ********************************

  // Factory method
  static boost::shared_ptr<Mqtt_Status> create()
  {
    return boost::shared_ptr<Mqtt_Status>(
        new Mqtt_Status());
  }
};

BOOST_DLL_ALIAS(
    Mqtt_Status::create, // <-- this function is exported with...
    create_plugin        // <-- ...this alias name
)
