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

class Mqtt_Status : public Plugin_Api, public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
  bool m_open = false;
  bool unit_enabled = false;
  bool message_enabled = false;
  bool tr_calls_set = false;

  int qos;
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

  time_t call_resend_time = time(NULL);

  std::map<short, std::vector<std::string>> opcode_type = {
      {0x00, {"GRP_V_CH_GRANT", "Group Voice Channel Grant"}},
      {0x01, {"RSVD_01", "Reserved 0x01"}},
      {0x02, {"GRP_V_CH_GRANT_UPDT", "Group Voice Channel Grant Update"}},
      {0x03, {"GRP_V_CH_GRANT_UPDT_EXP", "Group Voice Channel Update - Explicit"}},
      {0x04, {"UU_V_CH_GRANT", "Unit To Unit Voice Channel Grant"}},
      {0x05, {"UU_ANS_REQ", "Unit To Unit Answer Request"}},
      {0x06, {"UU_V_CH_GRANT_UPDT", "Unit to Unit Voice Channel Grant Update"}},
      {0x07, {"RSVD_07", "Reserved 0x07"}},
      {0x08, {"TELE_INT_CH_GRANT", "Telephone Interconnect Voice Channel Grant"}},
      {0x09, {"TELE_INT_CH_GRANT_UPDT", "Telephone Interconnect Voice Channel Grant Update"}},
      {0x0a, {"TELE_INT_ANS_REQ", "Telephone Interconnect Answer Request"}},
      {0x0b, {"RSVD_0B", "Reserved 0x0b"}},
      {0x0c, {"RSVD_0C", "Reserved 0x0c"}},
      {0x0d, {"RSVD_0D", "Reserved 0x0d"}},
      {0x0e, {"RSVD_0E", "Reserved 0x0e"}},
      {0x0f, {"RSVD_0F", "Reserved 0x0f"}},
      {0x10, {"OBS_10", "Obsolete 0x10"}},
      {0x11, {"OBS_11", "Obsolete 0x11"}},
      {0x12, {"OBS_12", "Obsolete 0x12"}},
      {0x13, {"OBS_13", "Obsolete 0x13"}},
      {0x14, {"SN-DATA_CHN_GNT", "SNDCP Data Channel Grant"}},
      {0x15, {"SN-DATA_PAGE_REQ", "SNDCP Data Page Request"}},
      {0x16, {"SN-DATA_CHN_ANN_EXP", "SNDCP Data Channel Announcement - Explicit"}},
      {0x17, {"RSVD_17", "Reserved 0x17"}},
      {0x18, {"STS_UPDT", "Status Update"}},
      {0x19, {"RSVD_19", "Reserved 0x19"}},
      {0x1a, {"STS_Q", "Status Query"}},
      {0x1b, {"RSVD_1B", "Reserved 0x1b"}},
      {0x1c, {"MSG_UPDT", "Message Update"}},
      {0x1d, {"RAD_MON_CMD", "Radio Unit Monitor Command"}},
      {0x1e, {"RSVD_1E", "Reserved 0x1e"}},
      {0x1f, {"CALL_ALRT", "Call Alert"}},
      {0x20, {"ACK_RSP_FNE", "Acknowledge Response - FNE"}},
      {0x21, {"QUE_RSP", "Queued Response"}},
      {0x22, {"RSVD_22", "Reserved 0x22"}},
      {0x23, {"RSVD_23", "Reserved 0x23"}},
      {0x24, {"EXT_FNCT_CMD", "Extended Function Command"}},
      {0x25, {"RSVD_25", "Reserved 0x25"}},
      {0x26, {"RSVD_26", "Reserved 0x26"}},
      {0x27, {"DENY_RSP", "Deny Response"}},
      {0x28, {"GRP_AFF_RSP", "Group Affiliation Response"}},
      {0x29, {"SCCB_EXP", "Secondary Control Channel Broadcast - Explicit"}},
      {0x2a, {"GRP_AFF_Q", "Group Affiliation Query"}},
      {0x2b, {"LOC_REG_RSP", "Location Registration Response"}},
      {0x2c, {"U_REG_RSP", "Unit Registration Response"}},
      {0x2d, {"U_REG_CMD", "Unit Registration Command"}},
      {0x2e, {"AUTH_CMD", "Authentication Command"}},
      {0x2f, {"U_DE_REG_ACK", "De-Registration Acknowledge"}},
      {0x30, {"SYNC_BCST", "Sync Broadcast / Patch"}},
      {0x31, {"AUTH_DMD", "Authentication Demand"}},
      {0x32, {"AUTH_FNE_RESP", "Authentication FNE Response"}},
      {0x33, {"IDEN_UP_TDMA", "Identifier Update for TDMA"}},
      {0x34, {"IDEN_UP_VU", "Identifier Update for VHF/UHF Bands"}},
      {0x35, {"TIME_DATE_ANN", "Time and Date Announcement"}},
      {0x36, {"ROAM_ADDR_CMD", "Roaming Address Command"}},
      {0x37, {"ROAM_ADDR_UPDT", "Roaming Address Update"}},
      {0x38, {"SYS_SRV_BCST", "System Service Broadcast"}},
      {0x39, {"SCCB", "Secondary Control Channel Broadcast"}},
      {0x3a, {"RFSS_STS_BCST", "RFSS Status Broadcast"}},
      {0x3b, {"NET_STS_BCST", "Network Status Broadcast"}},
      {0x3c, {"ADJ_STS_BCST", "Adjacent Status Broadcast"}},
      {0x3d, {"IDEN_UP", "Identifier Update"}},
      {0x3e, {"P_PARM_BCST", "Protection Parameter Broadcast"}},
      {0x3f, {"P_PARM_UPDT", "Protection Parameter Update"}},
      {0xff, {"UNK", "Unidentified"}}};

  std::map<short, std::string> message_type = {
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
      {99, "UNKNOWN"}};

  std::map<short, std::string> tr_state = {
      {0, "MONITORING"},
      {1, "RECORDING"},
      {2, "INACTIVE"},
      {3, "ACTIVE"},
      {4, "IDLE"},
      {6, "STOPPED"},
      {7, "AVAILABLE"},
      {8, "IGNORE"}};

  std::map<short, std::string> mon_state = {
      {0, "UNSPECIFIED"},
      {1, "UNKNOWN_TG"},
      {2, "IGNORED_TG"},
      {3, "NO_SOURCE"},
      {4, "NO_RECORDER"},
      {5, "ENCRYPTED"},
      {6, "DUPLICATE"},
      {7, "SUPERSEDED"}};

public:
  Mqtt_Status(){};

  // ********************************
  // trunk-recorder MQTT messages
  // ********************************

  int trunk_message(std::vector<TrunkMessage> messages, System *system) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Sent on each trunking message

    // MQTT: message_topic/short_name
    //   Lay the groundwork for trunk_message!

    if ((this->message_enabled))
    {
      for (std::vector<TrunkMessage>::iterator it = messages.begin(); it != messages.end(); it++)
      {
        TrunkMessage message = *it;
        boost::property_tree::ptree message_node;

        message_node.put("sys_num", system->get_sys_num());
        message_node.put("sys_name", system->get_short_name());
        message_node.put("trunk_msg", message.message_type);
        message_node.put("trunk_msg_type", message_type[message.message_type]);
        message_node.put("opcode", int_to_hex(message.opcode, 2));
        message_node.put("opcode_type", opcode_type[message.opcode][0]);
        message_node.put("opcode_desc", opcode_type[message.opcode][1]);

        return send_object(message_node, "message", system->get_short_name().c_str(), this->message_topic, false);
      }
    }
    return 0;
  }

  int system_rates(std::vector<System *> systems, float timeDiff) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called every three seconds (timeDiff)

    // MQTT: topic/rates
    //   Send control channel messages per second updates; rounded to two decimal places

    boost::property_tree::ptree systems_node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      std::string sys_type = system->get_system_type();

      // Filter out conventional systems.  They do not have a call rate and
      // get_current_control_channel() will cause a sefgault on non-trunked systems.
      if (sys_type.find("conventional") == std::string::npos)
      {
        boost::property_tree::ptree stat_node = system->get_stats_current(timeDiff);
        boost::property_tree::ptree system_node;

        system_node.put("sys_num", stat_node.get<std::string>("id"));
        system_node.put("sys_name", system->get_short_name());
        system_node.put("decoderate", round_to_str(stat_node.get<double>("decoderate")));
        system_node.put("decoderate_interval", timeDiff);
        system_node.put("control_channel", system->get_current_control_channel());
        systems_node.push_back(std::make_pair("", system_node));
      }
    }

    return send_object(systems_node, "rates", "rates", this->topic, false);
  }

  int send_config(std::vector<Source *> sources, std::vector<System *> systems)
  {
    // MQTT: topic/config
    //   Send elements of the trunk recorder config.json.
    //   retained = true; Message will be kept at the MQTT broker to avoid the need to resend.

    boost::property_tree::ptree root;
    boost::property_tree::ptree systems_node;
    boost::property_tree::ptree sources_node;

    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); ++it)
    {
      Source *source = *it;
      std::vector<Gain_Stage_t> gain_stages;
      boost::property_tree::ptree source_node;
      source_node.put("source_num", source->get_num());
      source_node.put("rate", source->get_rate());
      source_node.put("center", source->get_center());
      source_node.put("min_hz", source->get_min_hz());
      source_node.put("max_hz", source->get_max_hz());
      source_node.put("error", source->get_error());
      source_node.put("driver", source->get_driver());
      source_node.put("device", source->get_device());
      source_node.put("antenna", source->get_antenna());
      source_node.put("gain", source->get_gain());
      gain_stages = source->get_gain_stages();
      for (std::vector<Gain_Stage_t>::iterator gain_it = gain_stages.begin(); gain_it != gain_stages.end(); ++gain_it)
      {
        source_node.put(gain_it->stage_name + "_gain", gain_it->value);
      }
      source_node.put("analog_recorders", source->analog_recorder_count());
      source_node.put("digital_recorders", source->digital_recorder_count());
      source_node.put("debug_recorders", source->debug_recorder_count());
      source_node.put("sigmf_recorders", source->sigmf_recorder_count());
      source_node.put("silence_frames", source->get_silence_frames());
      sources_node.push_back(std::make_pair("", source_node));
    }

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *sys = (System *)*it;

      boost::property_tree::ptree sys_node;
      boost::property_tree::ptree channels_node;
      sys_node.put("sys_num", sys->get_sys_num());
      sys_node.put("sys_name", sys->get_short_name());
      sys_node.put("system_type", sys->get_system_type());
      sys_node.put("talkgroups_file", sys->get_talkgroups_file());
      sys_node.put("qpsk", sys->get_qpsk_mod());
      sys_node.put("squelch_db", sys->get_squelch_db());
      sys_node.put("analog_levels", sys->get_analog_levels());
      sys_node.put("digital_levels", sys->get_digital_levels());
      sys_node.put("audio_archive", sys->get_audio_archive());
      sys_node.put("upload_script", sys->get_upload_script());
      sys_node.put("record_unkown", sys->get_record_unknown());
      sys_node.put("call_log", sys->get_call_log());

      std::vector<double> channels;
      if ((sys->get_system_type() == "conventional") || (sys->get_system_type() == "conventionalP25"))
      {
        channels = sys->get_channels();
      }
      else
      {
        channels = sys->get_control_channels();
        sys_node.put("control_channel", sys->get_current_control_channel());
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
    root.put("capture_dir", this->config->capture_dir);
    root.put("upload_server", this->config->upload_server);

    // root.put("defaultMode", default_mode);
    root.put("call_timeout", this->config->call_timeout);
    root.put("log_file", this->config->log_file);
    root.put("instance_id", this->config->instance_id);
    root.put("instance_key", this->config->instance_key);
    root.put("log_file", this->config->log_file);

    if (this->config->broadcast_signals == true)
    {
      root.put("broadcast_signals", this->config->broadcast_signals);
    }

    return send_object(root, "config", "config", this->topic, true);
  }

  int setup_systems(std::vector<System *> systems) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called during startup when the systems have been created.
    //   May not have NAC or other details until all systems finish setting up.

    // MQTT: topic/systems
    //   Send the configuration information for all systems
    //   retained = true; Message will be kept at the MQTT broker to avoid the need to resend.

    boost::property_tree::ptree systems_node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      boost::property_tree::ptree system_node;
      boost::property_tree::ptree stat_node = system->get_stats();

      system_node.put("sys_num", stat_node.get<std::string>("id"));
      system_node.put("sys_name", stat_node.get<std::string>("name"));
      system_node.put("type", stat_node.get<std::string>("type"));
      system_node.put("sysid", int_to_hex(stat_node.get<int>("sysid"), 0));
      system_node.put("wacn", int_to_hex(stat_node.get<int>("wacn"), 0));
      system_node.put("nac", int_to_hex(stat_node.get<int>("nac"), 0));

      systems_node.push_back(std::make_pair("", system_node));
    }
    return send_object(systems_node, "systems", "systems", this->topic, true);
  }

  int setup_system(System *system) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called after a system has been created.

    // MQTT: topic/system
    //   Send the configuration information for a single system.

    boost::property_tree::ptree system_node;
    boost::property_tree::ptree stat_node = system->get_stats();

    system_node.put("sys_num", stat_node.get<std::string>("id"));
    system_node.put("sys_name", stat_node.get<std::string>("name"));
    system_node.put("type", stat_node.get<std::string>("type"));
    system_node.put("sysid", int_to_hex(stat_node.get<int>("sysid"), 0));
    system_node.put("wacn", int_to_hex(stat_node.get<int>("wacn"), 0));
    system_node.put("nac", int_to_hex(stat_node.get<int>("nac"), 0));

    // Resend the full system list with each update
    setup_systems(this->systems);

    return send_object(system_node, "system", "system", this->topic, false);
  }

  int calls_active(std::vector<Call *> calls) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called when a call starts or ends

    // MQTT: topic/calls_active
    //   Send the list of all active calls; use a pointer to allow
    //   updates between the normal API calls

    // Reset a pointer to the active call list if needed later
    this->tr_calls = calls;
    this->tr_calls_set = true;

    boost::property_tree::ptree calls_node;

    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); ++it)
    {
      Call *call = *it;
      if ((call->get_current_length() > 0) || (!call->is_conventional()))
      {
        boost::property_tree::ptree stat_node = call->get_stats();
        boost::property_tree::ptree call_node;

        call_node.put("id", stat_node.get<std::string>("id"));
        call_node.put("call_num", stat_node.get<std::string>("callNum"));
        call_node.put("freq", stat_node.get<std::string>("freq"));
        call_node.put("sys_num", stat_node.get<std::string>("sysNum"));
        call_node.put("sys_name", stat_node.get<std::string>("shortName"));
        call_node.put("talkgroup", stat_node.get<std::string>("talkgroup"));
        call_node.put("talkgroup_alpha_tag", stat_node.get<std::string>("talkgrouptag"));
        call_node.put("unit", stat_node.get<std::string>("srcId"));
        call_node.put("unit_alpha_tag", call->get_system()->find_unit_tag(stat_node.get<long>("srcId")));
        call_node.put("elapsed", stat_node.get<std::string>("elapsed"));
        call_node.put("length", round_to_str(stat_node.get<double>("length")));
        call_node.put("call_state", stat_node.get<std::string>("state"));
        call_node.put("call_state_type", tr_state[stat_node.get<int>("state")]);
        call_node.put("mon_state", stat_node.get<std::string>("monState"));
        call_node.put("mon_state_type", mon_state[stat_node.get<int>("monState")]);

        call_node.put("phase2", stat_node.get<std::string>("phase2"));
        call_node.put("conventional", stat_node.get<std::string>("conventional"));
        call_node.put("encrypted", stat_node.get<std::string>("encrypted"));
        call_node.put("emergency", stat_node.get<std::string>("emergency"));
        call_node.put("stop_time", stat_node.get<std::string>("stopTime"));

        // Not all calls have recorder info
        if (stat_node.count("recNum"))
        {
          call_node.put("rec_num", stat_node.get<std::string>("recNum"));
          call_node.put("src_num", stat_node.get<std::string>("srcNum"));
          call_node.put("rec_state", stat_node.get<std::string>("recState"));
          call_node.put("rec_state_type", tr_state[stat_node.get<int>("recState")]);
          call_node.put("analog", stat_node.get<std::string>("analog"));
        }

        calls_node.push_back(std::make_pair("", call_node));
      }
    }

    return send_object(calls_node, "calls", "calls_active", this->topic, false);
  }

  int send_recorders(std::vector<Recorder *> recorders)
  {
    // MQTT: topic/recorders
    //   Send the status of all recorders

    boost::property_tree::ptree recs_node;

    for (std::vector<Recorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
    {
      Recorder *recorder = *it;
      boost::property_tree::ptree stat_node = recorder->get_stats();
      boost::property_tree::ptree rec_node;

      rec_node.put("id", stat_node.get<std::string>("id"));
      rec_node.put("src_num", stat_node.get<std::string>("srcNum"));
      rec_node.put("rec_num", stat_node.get<std::string>("recNum"));
      rec_node.put("type", stat_node.get<std::string>("type"));
      rec_node.put("duration", round_to_str(stat_node.get<double>("duration")));
      rec_node.put("freq", recorder->get_freq());
      rec_node.put("count", stat_node.get<std::string>("count"));
      rec_node.put("rec_state", stat_node.get<std::string>("state"));
      rec_node.put("rec_state_type", tr_state[stat_node.get<int>("state")]);

      recs_node.push_back(std::make_pair("", rec_node));
    }

    return send_object(recs_node, "recorders", "recorders", this->topic, false);
  }

  int setup_recorder(Recorder *recorder) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called when a recorder has been created or changes status

    // MQTT: topic/recorder
    //   Send updates on individual recorders

    boost::property_tree::ptree stat_node = recorder->get_stats();
    boost::property_tree::ptree rec_node;

    rec_node.put("id", stat_node.get<std::string>("id"));
    rec_node.put("src_num", stat_node.get<std::string>("srcNum"));
    rec_node.put("rec_num", stat_node.get<std::string>("recNum"));
    rec_node.put("type", stat_node.get<std::string>("type"));
    rec_node.put("freq", recorder->get_freq());
    rec_node.put("duration", round_to_str(stat_node.get<double>("duration")));
    rec_node.put("count", stat_node.get<std::string>("count"));
    rec_node.put("rec_state", stat_node.get<std::string>("state"));
    rec_node.put("rec_state_type", tr_state[stat_node.get<int>("state")]);

    return send_object(rec_node, "recorder", "recorder", this->topic, false);
  }

  int call_start(Call *call) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called when a call starts

    // MQTT: topic/call_start
    //   Send information about a new call.
    // MQTT: unit_topic/shortname/call
    //   Send information on the unit initiating a new call.

    long talkgroup_num = call->get_talkgroup();
    long source_id = call->get_current_source_id();
    std::string short_name = call->get_short_name();

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree call_node;
      std::string patch_string = patches_to_str(call->get_system()->get_talkgroup_patch(talkgroup_num));

      call_node.put("sys_num", call->get_system()->get_sys_num());
      call_node.put("sys_name", short_name);
      call_node.put("call_num", call->get_call_num());
      call_node.put("start_time", call->get_start_time());
      call_node.put("freq", call->get_freq());
      call_node.put("unit", source_id);
      call_node.put("unit_alpha_tag", call->get_system()->find_unit_tag(source_id));
      call_node.put("talkgroup", talkgroup_num);
      call_node.put("talkgroup_alpha_tag", call->get_talkgroup_tag());
      call_node.put("talkgroup_patches", patch_string);
      call_node.put("encrypted", call->get_encrypted());
      send_object(call_node, "call", "call", this->unit_topic + "/" + short_name, false);
    }

    boost::property_tree::ptree stat_node = call->get_stats();
    boost::property_tree::ptree call_node;

    call_node.put("id", stat_node.get<std::string>("id"));
    call_node.put("call_num", stat_node.get<std::string>("callNum"));
    call_node.put("freq", stat_node.get<std::string>("freq"));
    call_node.put("sys_num", stat_node.get<std::string>("sysNum"));
    call_node.put("sys_name", stat_node.get<std::string>("shortName"));
    call_node.put("talkgroup", stat_node.get<std::string>("talkgroup"));
    call_node.put("talkgroup_alpha_tag", stat_node.get<std::string>("talkgrouptag"));
    call_node.put("unit", stat_node.get<std::string>("srcId"));
    call_node.put("unit_alpha_tag", call->get_system()->find_unit_tag(source_id));
    call_node.put("elapsed", stat_node.get<std::string>("elapsed"));
    call_node.put("length", round_to_str(stat_node.get<double>("length")));
    call_node.put("call_state", stat_node.get<std::string>("state"));
    call_node.put("call_state_type", tr_state[stat_node.get<int>("state")]);
    call_node.put("mon_state", stat_node.get<std::string>("monState"));
    call_node.put("mon_state_type", mon_state[stat_node.get<int>("monState")]);

    call_node.put("phase2", stat_node.get<std::string>("phase2"));
    call_node.put("conventional", stat_node.get<std::string>("conventional"));
    call_node.put("encrypted", stat_node.get<std::string>("encrypted"));
    call_node.put("emergency", stat_node.get<std::string>("emergency"));
    call_node.put("stop_time", stat_node.get<std::string>("stopTime"));

    // Not all calls have recorder info
    if (stat_node.count("recNum"))
    {
      call_node.put("rec_num", stat_node.get<std::string>("recNum"));
      call_node.put("src_num", stat_node.get<std::string>("srcNum"));
      call_node.put("rec_state", stat_node.get<std::string>("recState"));
      call_node.put("rec_state_type", tr_state[stat_node.get<int>("recState")]);
      call_node.put("analog", stat_node.get<std::string>("analog"));
    }

    return send_object(call_node, "call", "call_start", this->topic, false);
  }

  int call_end(Call_Data_t call_info) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called after a call ends

    // MQTT: topic/call_end
    //   Send information about a completed call.
    // MQTT: unit_topic/shortname/end
    //   Send information on participataing units.
    //   This enables the collection unit information in conventional systems
    //   without a control channel.

    // Generate list of talkgroup patches
    std::string patch_string = patches_to_str(call_info.patched_talkgroups);

    if (this->unit_enabled)
    {
      boost::property_tree::ptree end_node;
      // source_list[] can be used to suppliment transmission_list[] info
      std::vector<Call_Source> source_list = call_info.transmission_source_list;
      int transmision_num = 0;

      BOOST_FOREACH (auto &transmission, call_info.transmission_list)
      {
        end_node.put("call_num", call_info.call_num);
        end_node.put("sys_name", call_info.short_name);
        end_node.put("unit", transmission.source);
        end_node.put("unit_alpha_tag", source_list[transmision_num].tag);
        end_node.put("start_time", transmission.start_time);
        end_node.put("stop_time", transmission.stop_time);
        end_node.put("sample_count", transmission.sample_count);
        end_node.put("spike_count", transmission.spike_count);
        end_node.put("error_count", transmission.error_count);
        end_node.put("freq", call_info.freq);
        end_node.put("length", round_to_str(transmission.length));
        end_node.put("transmission_filename", transmission.filename);
        end_node.put("call_filename", call_info.filename);
        end_node.put("position", round_to_str(source_list[transmision_num].position));
        end_node.put("talkgroup", call_info.talkgroup);
        end_node.put("talkgroup_alpha_tag", call_info.talkgroup_alpha_tag);
        end_node.put("talkgroup_description", call_info.talkgroup_description);
        end_node.put("talkgroup_group", call_info.talkgroup_group);
        end_node.put("talkgroup_patches", patch_string);
        end_node.put("encrypted", call_info.encrypted);
        end_node.put("emergency", source_list[transmision_num].emergency);
        end_node.put("signal_system", source_list[transmision_num].signal_system);

        send_object(end_node, "end", "end", this->unit_topic + "/" + call_info.short_name.c_str(), false);
        transmision_num++;
      }
    }
    boost::property_tree::ptree call_node;
    call_node.put("call_num", call_info.call_num);
    call_node.put("sys_name", call_info.short_name);
    call_node.put("start_time", call_info.start_time);
    call_node.put("stop_time", call_info.stop_time);
    call_node.put("length", round_to_str(call_info.length));
    call_node.put("process_call_time", call_info.process_call_time);
    call_node.put("retry_attempt", call_info.retry_attempt);
    call_node.put("error_count", call_info.error_count);
    call_node.put("spike_count", call_info.spike_count);
    call_node.put("freq", call_info.freq);
    call_node.put("encrypted", call_info.encrypted);
    call_node.put("emergency", call_info.emergency);
    call_node.put("tdma_slot", call_info.tdma_slot);
    call_node.put("phase2_tdma", call_info.phase2_tdma);
    call_node.put("talkgroup", call_info.talkgroup);
    call_node.put("talkgroup_tag", call_info.talkgroup_tag);
    call_node.put("talkgroup_alpha_tag", call_info.talkgroup_alpha_tag);
    call_node.put("talkgroup_description", call_info.talkgroup_description);
    call_node.put("talkgroup_group", call_info.talkgroup_group);
    call_node.put("talkgroup_patches", patch_string);
    call_node.put("audio_type", call_info.audio_type);

    // call_node.put("status",call_info.status);
    // call_node.put("transmission_list",call_info.transmission_list);
    // call_node.put("upload_script",call_info.upload_script = sys->get_upload_script();
    // call_node.put("audio_archive",call_info.audio_archive = sys->get_audio_archive();
    // call_node.put("transmission_archive",call_info.transmission_archive = sys->get_transmission_archive();
    // call_node.put("call_log",call_info.call_log = sys->get_call_log();
    // call_node.put("compress_wav",call_info.compress_wav);
    // call_node.put("talkgroup_display",call_info.talkgroup_display = call->get_talkgroup_display();
    // call_info.min_transmissions_removed = 0;
    // call_node.put("transmission_source_list",call_info.transmission_source_list);
    // call_node.put("transmission_error_list",call_info.transmission_error_list);

    // Some stats are rounded to prevent long/repeating floats
    return send_object(call_node, "call", "call_end", this->topic, false);
  }

  int unit_registration(System *sys, long source_id) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each REGISTRATION message

    // MQTT: unit_topic/shortname/on
    //   Unit registration on a system (on)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));

      return send_object(unit_node, "on", "on", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_deregistration(System *sys, long source_id) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each DEREGISTRATION message

    // MQTT: unit_topic/shortname/off
    //   Unit de-registration on a system (off)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));

      return send_object(unit_node, "off", "off", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_acknowledge_response(System *sys, long source_id) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each ACKNOWLEDGE message

    // MQTT: unit_topic/shortname/ackresp
    //   Unit acknowledge response (ackresp)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));

      return send_object(unit_node, "ackresp", "ackresp", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_group_affiliation(System *sys, long source_id, long talkgroup_num) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each AFFILIATION message

    // MQTT: unit_topic/shortname/join
    //   Unit talkgroup affiliation (join)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;
      std::string patch_string = patches_to_str(sys->get_talkgroup_patch(talkgroup_num));

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup_num);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }
      unit_node.put("talkgroup_patches", patch_string);

      return send_object(unit_node, "join", "join", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_data_grant(System *sys, long source_id) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each DATA_GRANT message

    // MQTT: unit_topic/shortname/data
    //   Unit data grant (data)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));

      return send_object(unit_node, "data", "data", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_answer_request(System *sys, long source_id, long talkgroup) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each UU_ANS_REQ message

    // MQTT: unit_topic/shortname/ans_req
    //   Unit answer_request (ans_req)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha_tag", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup);
      Talkgroup *tg = sys->find_talkgroup(talkgroup);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }

      return send_object(unit_node, "ans_req", "ans_req", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  int unit_location(System *sys, long source_id, long talkgroup_num) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called each LOCATION message

    // MQTT: unit_topic/shortname/location
    //   Unit location/roaming update (location)

    if ((this->unit_enabled))
    {
      boost::property_tree::ptree unit_node;
      std::string patch_string = patches_to_str(sys->get_talkgroup_patch(talkgroup_num));

      unit_node.put("sys_num", sys->get_sys_num());
      unit_node.put("sys_name", sys->get_short_name());
      unit_node.put("unit", source_id);
      unit_node.put("unit_alpha", sys->find_unit_tag(source_id));
      unit_node.put("talkgroup", talkgroup_num);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL)
      {
        unit_node.put("talkgroup_alpha_tag", tg->alpha_tag);
      }
      unit_node.put("talkgroup_patches", patch_string);
      return send_object(unit_node, "location", "location", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // ********************************
  // trunk-recorder plugin API & startup
  // ********************************

  int parse_config(boost::property_tree::ptree &cfg) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called before init(); parses the config information for this plugin.

    this->mqtt_broker = cfg.get<std::string>("broker", "tcp://localhost:1883");
    this->client_id = cfg.get<std::string>("client_id", "tr-status");
    this->username = cfg.get<std::string>("username", "");
    this->password = cfg.get<std::string>("password", "");
    this->topic = cfg.get<std::string>("topic", "");
    this->qos = cfg.get<int>("qos", 0);

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

    // Remove any trailing slashes from topics
    if (this->topic.back() == '/')
      this->topic.erase(this->topic.size() - 1);

    if (this->unit_topic.back() == '/')
      this->unit_topic.erase(this->unit_topic.size() - 1);

    if (this->message_topic.back() == '/')
      this->message_topic.erase(this->message_topic.size() - 1);

    // Print plugin startup info
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker: " << this->mqtt_broker;
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Client Name: " << this->client_id;
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker Username: " << this->username;
    if (this->password != "")
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker Password: ********";
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Topic: " << this->topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT Unit Status Plugin Topic: " << this->unit_topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT Trunk Message Plugin Topic: " << this->message_topic;
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin message QOS: " << this->qos;

    return 0;
  }

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Plugin initialization; called after parse_config().

    frequency_format = config->frequency_format;
    this->instance_id = config->instance_id;
    if (this->instance_id == "")
      this->instance_id = "trunk-recorder";

    // Establish pointers to systems, sources, and configs if needed later.
    this->sources = sources;
    this->systems = systems;
    this->config = config;

    return 0;
  }

  int start() override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called after trunk-recorder finishes setup and the plugin is initialized

    // Start the MQTT connection
    open_connection();

    // Send config and system MQTT messages
    send_config(this->sources, this->systems);
    setup_systems(this->systems);

    return 0;
  }

  int setup_config(std::vector<Source *> sources, std::vector<System *> systems) override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called at the same periodicity of system_rates(), this can be use to accomplish
    //   occasional plugin tasks more efficiently than checking each cycle of poll_one().

    // Refresh recorders every 3 seconds
    resend_recorders();

    return 0;
  }

  int poll_one() override
  {
    // TRUNK-RECORDER PLUGIN API
    //   Called during each pass through the main loop of trunk-recorder.

    // Refresh calls every 1 second
    resend_calls();

    return 0;
  }

  // ********************************
  // Helper functions
  // ********************************

  void resend_recorders()
  {
    // Periodically update the status of all recorders.
    // Triggered by setup_config() every 3 seconds.

    std::vector<Recorder *> recorders;
    for (std::vector<Source *>::iterator it = this->sources.begin(); it != this->sources.end(); ++it)
    {
      Source *source = *it;
      std::vector<Recorder *> sourceRecorders = source->get_recorders();
      recorders.insert(recorders.end(), sourceRecorders.begin(), sourceRecorders.end());
    }
    send_recorders(recorders);
  }

  void resend_calls()
  {
    // Use a pointer from calls_active() to update the active call list once a second.
    // Triggered by poll_one().

    time_t now_time = time(NULL);

    if (((now_time - this->call_resend_time) >= 1))
    {
      calls_active(this->tr_calls);
      this->call_resend_time = now_time;
    }
  }

  std::string int_to_hex(int num, int places)
  {
    // Return a hexidecimal value for a given integer, zero-padded for "places"

    std::stringstream stream;
    stream << std::setfill('0') << std::uppercase << std::setw(places) << std::hex << num;
    std::string result(stream.str());
    return result;
  }

  std::string round_to_str(double num)
  {
    // Round a float to two decimal places and return it as as string.
    // ["position", "length", "duration"] are the usual offenders.

    char rounded[20];
    snprintf(rounded, sizeof(rounded), "%.2f", num);
    return std::string(rounded);
  }

  std::string patches_to_str(std::vector<unsigned long> talkgroup_patches)
  {
    // Combine a vector of talkgroup patches into a string.

    std::string patch_string;

    BOOST_FOREACH (auto &TGID, talkgroup_patches)
    {
      if (!patch_string.empty())
        patch_string += ",";
      patch_string += std::to_string(TGID);
    }
    return patch_string;
  }

  // ********************************
  // Paho MQTT
  // ********************************

  void open_connection()
  {
    // Open the connection to the destination MQTT server using paho libraries.

    // Set a connect/disconnect message between client and broker
    std::stringstream connect_json;
    std::stringstream lwt_json;
    std::string status_topic = this->topic + "/trunk_recorder/" + this->client_id;

    boost::property_tree::ptree status;
    status.put("status", "connected");
    status.put("instance_id", this->instance_id);
    status.put("client_id", this->client_id);
    boost::property_tree::write_json(connect_json, status);
    status.put("status", "disconnected");
    boost::property_tree::write_json(lwt_json, status);

    mqtt::message_ptr conn_msg = mqtt::message_ptr_builder()
                                     .topic(status_topic)
                                     .payload(connect_json.str())
                                     .qos(this->qos)
                                     .retained(true)
                                     .finalize();

    std::string lwt_string = lwt_json.str();
    auto will_msg = mqtt::message(status_topic, lwt_string.c_str(), strlen(lwt_string.c_str()), this->qos, true);

    // Set SSL options
    mqtt::ssl_options sslopts = mqtt::ssl_options_builder()
                                    .verify(false)
                                    .enable_server_cert_auth(false)
                                    .finalize();

    // Set connection options
    mqtt::connect_options connOpts = mqtt::connect_options_builder()
                                         .clean_session()
                                         .ssl(sslopts)
                                         .automatic_reconnect(std::chrono::seconds(10), std::chrono::seconds(40))
                                         .will(will_msg)
                                         .finalize();

    // Set user/pass if indicated
    if ((this->username != "") && (this->password != ""))
    {
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tSetting MQTT Broker username and password..." << endl;
      connOpts.set_user_name(this->username);
      connOpts.set_password(this->password);
    }

    // Open a connection to the broker, set m_open true if successful, publish a connect message
    client = new mqtt::async_client(this->mqtt_broker, this->client_id, config->capture_dir + "/store");
    try
    {
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tConnecting...";
      mqtt::token_ptr conntok = client->connect(connOpts);
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tWaiting for the connection...";
      conntok->wait();
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \t ...OK";
      m_open = true;
      client->publish(conn_msg);
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << exc.what() << endl;
    }
  }

  int send_object(boost::property_tree::ptree data, std::string name, std::string type, std::string object_topic, bool retained)
  {
    // Send a MQTT message using the configured connection and paho libraries.
    // send_object(
    //    boost::property_tree::ptree data  <- payload,
    //    std::string name                  <- subtopic,
    //    std::string type                  <- message type,
    //    std::string object_topic          <- topic,
    //    bool retained                     <- retain message at the broker (config, system, etc.)
    //    )

    // Ignore requests to send MQTT messages before the connection is opened
    if (m_open == false)
      return 0;

    // Build the MQTT payload, add addtional keys [timestamp, instance_id]
    boost::property_tree::ptree payload;
    payload.add_child(name, data);
    payload.put("type", type);
    payload.put("timestamp", time(NULL));
    payload.put("instance_id", instance_id);

    std::stringstream payload_json;
    boost::property_tree::write_json(payload_json, payload);

    // Assemble the MQTT message
    mqtt::message_ptr pubmsg = mqtt::message_ptr_builder()
                                   .topic(object_topic + "/" + type)
                                   .payload(payload_json.str())
                                   .qos(this->qos)
                                   .retained(retained)
                                   .finalize();

    // Publish the MQTT message
    try
    {
      client->publish(pubmsg);
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << "MQTT Status Plugin - " << exc.what() << endl;
    }

    return 0;
  }

  // Paho mqtt::iaction_listeners.  Required, but not used.
  void on_failure(const mqtt::token &tok) override{};
  void on_success(const mqtt::token &tok) override{};

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
