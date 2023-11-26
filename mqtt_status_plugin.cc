// MQTT Status and Unit Plugin for Trunk-Recorder
// ********************************
// Requires trunk-recorder 4.7 (commit 837a057 14 NOV 2023) or later, and Paho MQTT libraries
// ********************************

#include <time.h>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <string>
#include <map>
#include <cstring>
#include <regex>
#include <mqtt/client.h>
#include <trunk-recorder/source.h>
#include <trunk-recorder/json.hpp>
#include <trunk-recorder/plugin_manager/plugin_api.h>
#include <boost/date_time/posix_time/posix_time.hpp> //time_formatters.hpp>
#include <boost/dll/alias.hpp>                       // for BOOST_DLL_ALIAS
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/crc.hpp>

using namespace std;
namespace logging = boost::log;

class Mqtt_Status : public Plugin_Api, public virtual mqtt::callback
{
  bool m_open = false;
  bool unit_enabled = false;
  bool message_enabled = false;
  bool console_enabled = false;
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
  std::string console_topic;
  std::string instance_id;
  std::string log_prefix;
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

private:
  // Custom backend to send log messages to parent Mqtt_Status plugin
  class MqttSinkBackend : public logging::sinks::text_ostream_backend
  {
  public:
    explicit MqttSinkBackend(Mqtt_Status &parent) : parent_(parent) {}

    void consume(logging::record_view const &rec, std::string const &formatted_message)
    {
      // Extract formatted_message, severity, and time from record
      nlohmann::ordered_json console_json = {
          {"time", boost::posix_time::to_iso_extended_string(rec["TimeStamp"].extract<boost::posix_time::ptime>().get())},
          {"severity", logging::trivial::to_string(rec["Severity"].extract<logging::trivial::severity_level>().get())},
          {"log_msg", parent_.strip_esc_seq(rec["Message"].extract<std::string>().get())}};
      parent_.console_message(console_json);
    }

  private:
    Mqtt_Status &parent_;
  };

public:
  Mqtt_Status(){};

  // ********************************
  // trunk-recorder MQTT messages
  // ********************************

  // console_message()
  //   Send boost::log::trivial messages over MQTT.
  //   MQTT: message_topic/status/trunk_recorder/console
  void console_message(nlohmann::ordered_json console_json)
  {
    send_json(console_json, "console", "console", this->console_topic, false);
  }

  // trunk_message()
  //   Display an overview of received trunk messages.
  //   TRUNK-RECORDER PLUGIN API: Sent on each trunking message
  //   MQTT: message_topic/short_name
  int trunk_message(std::vector<TrunkMessage> messages, System *system) override
  {
    if ((this->message_enabled))
    {
      for (std::vector<TrunkMessage>::iterator it = messages.begin(); it != messages.end(); it++)
      {
        TrunkMessage message = *it;

        nlohmann::ordered_json message_json = {
            {"sys_num", system->get_sys_num()},
            {"sys_name", system->get_short_name()},
            {"trunk_msg", message.message_type},
            {"trunk_msg_type", message_type[message.message_type]},
            {"opcode", int_to_hex(message.opcode, 2)},
            {"opcode_type", opcode_type[message.opcode][0]},
            {"opcode_desc", opcode_type[message.opcode][1]},
            {"meta", strip_esc_seq(message.meta)}};
        return send_json(message_json, "message", "message", this->message_topic + "/" + system->get_short_name().c_str(), false);
      }
    }
    return 0;
  }

  // system_rates()
  //   Send control channel messages per second updates; rounded to two decimal places
  //   TRUNK-RECORDER PLUGIN API: Called every three seconds (timeDiff)
  //   MQTT: topic/rates
  int system_rates(std::vector<System *> systems, float timeDiff) override
  {
    nlohmann::ordered_json system_json;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      std::string sys_type = system->get_system_type();

      // Filter out conventional systems.  They do not have a call rate and
      // get_current_control_channel() will cause a sefgault on non-trunked systems.
      if (sys_type.find("conventional") == std::string::npos)
      {
        boost::property_tree::ptree stat_node = system->get_stats_current(timeDiff);
        system_json += {
            {"sys_num", stat_node.get<int>("id")},
            {"sys_name", system->get_short_name()},
            {"decoderate", round_float(stat_node.get<double>("decoderate"))},
            {"decoderate_interval", timeDiff},
            {"control_channel", system->get_current_control_channel()}};
      }
    }
    return send_json(system_json, "rates", "rates", this->topic, false);
  }

  // send_config()
  //   Send elements of the trunk recorder config.json on startup.
  //   MQTT: topic/config
  //     retained = true; Message will be kept at the MQTT broker to avoid the need to resend.
  int send_config(std::vector<Source *> sources, std::vector<System *> systems)
  {
    nlohmann::ordered_json config_json;

    for (std::vector<Source *>::iterator it = sources.begin(); it != sources.end(); ++it)
    {
      Source *source = *it;
      json gain_stages_json;
      std::vector<Gain_Stage_t> gain_stages = source->get_gain_stages();

      for (std::vector<Gain_Stage_t>::iterator gain_it = gain_stages.begin(); gain_it != gain_stages.end(); ++gain_it)
      {
        gain_stages_json += {gain_it->stage_name + "_gain", gain_it->value};
      }

      config_json["sources"] += {
          {"source_num", source->get_num()},
          {"rate", source->get_rate()},
          {"center", source->get_center()},
          {"min_hz", source->get_min_hz()},
          {"max_hz", source->get_max_hz()},
          {"error", source->get_error()},
          {"driver", source->get_driver()},
          {"device", source->get_device()},
          {"antenna", source->get_antenna()},
          {"gain", source->get_gain()},
          {"gain_stages", gain_stages_json},
          {"analog_recorders", source->analog_recorder_count()},
          {"digital_recorders", source->digital_recorder_count()},
          {"debug_recorders", source->debug_recorder_count()},
          {"sigmf_recorders", source->sigmf_recorder_count()},
          {"silence_frames", source->get_silence_frames()},
      };
    }

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *sys = (System *)*it;

      nlohmann::ordered_json system_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"system_type", sys->get_system_type()},
          {"talkgroups_file", sys->get_talkgroups_file()},
          {"qpsk", sys->get_qpsk_mod()},
          {"squelch_db", sys->get_squelch_db()},
          {"analog_levels", sys->get_analog_levels()},
          {"digital_levels", sys->get_digital_levels()},
          {"audio_archive", sys->get_audio_archive()},
          {"upload_script", sys->get_upload_script()},
          {"record_unkown", sys->get_record_unknown()},
          {"call_log", sys->get_call_log()}};

      if ((sys->get_system_type() == "conventional") || (sys->get_system_type() == "conventionalP25"))
      {
        system_json["channels"] = sys->get_channels();
      }
      else
      {
        system_json["control_channel"] = sys->get_current_control_channel();
        system_json["channels"] = sys->get_control_channels();
      }

      if (sys->get_system_type() == "smartnet")
      {
        system_json["bandplan"] = sys->get_bandplan();
        system_json["bandfreq"] = sys->get_bandfreq();
        system_json["bandplan_base"] = sys->get_bandplan_base();
        system_json["bandplan_high"] = sys->get_bandplan_high();
        system_json["bandplan_spacing"] = sys->get_bandplan_spacing();
        system_json["bandplan_offset"] = sys->get_bandplan_offset();
      };

      config_json["systems"] += system_json;
    }

    config_json["capture_dir"] = this->config->capture_dir;
    config_json["upload_server"] = this->config->upload_server;
    config_json["call_timeout"] = this->config->call_timeout;
    config_json["log_file"] = this->config->log_file;
    config_json["instance_id"] = this->config->instance_id;
    config_json["instance_key"] = this->config->instance_key;
    if (this->config->broadcast_signals == true)
    {
      config_json["broadcast_signals"] = this->config->broadcast_signals;
    }

    return send_json(config_json, "config", "config", this->topic, true);
  }

  // setup_systems()
  //   Send the configuration information for all systems on startup.
  //   TRUNK-RECORDER PLUGIN API: Called during startup when the systems have been created.
  //   MQTT: topic/systems
  //     retained = true; Message will be kept at the MQTT broker to avoid the need to resend.
  int setup_systems(std::vector<System *> systems) override
  {
    nlohmann::ordered_json systems_json;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      boost::property_tree::ptree stat_node = system->get_stats();
      systems_json += {
          {"sys_num", stat_node.get<int>("id")},
          {"sys_name", stat_node.get<std::string>("name")},
          {"type", stat_node.get<std::string>("type")},
          {"sysid", int_to_hex(stat_node.get<int>("sysid"), 0)},
          {"wacn", int_to_hex(stat_node.get<int>("wacn"), 0)},
          {"nac", int_to_hex(stat_node.get<int>("nac"), 0)},
          {"rfss", system->get_sys_rfss()},
          {"site_id", system->get_sys_site_id()}};
    }
    return send_json(systems_json, "systems", "systems", this->topic, true);
  }

  // setup_system()
  //   Send the configuration information for a single system on startup.
  //   TRUNK-RECORDER PLUGIN API:  Called after a system has been created.
  //   MQTT: topic/system
  int setup_system(System *system) override
  {
    boost::property_tree::ptree stat_node = system->get_stats();
    nlohmann::ordered_json system_json = {
        {"sys_num", stat_node.get<int>("id")},
        {"sys_name", stat_node.get<std::string>("name")},
        {"type", stat_node.get<std::string>("type")},
        {"sysid", int_to_hex(stat_node.get<int>("sysid"), 0)},
        {"wacn", int_to_hex(stat_node.get<int>("wacn"), 0)},
        {"nac", int_to_hex(stat_node.get<int>("nac"), 0)},
        {"rfss", system->get_sys_rfss()},
        {"site_id", system->get_sys_site_id()}};
    // Resend the full system list with each update
    setup_systems(this->systems);
    return send_json(system_json, "system", "system", this->topic, false);
  }

  // calls_active()
  //   Send the list of all active calls every second
  //   TRUNK-RECORDER PLUGIN API: Called when a call starts or ends
  //   MQTT: topic/calls_active
  //     Not all calls have recorder info
  int calls_active(std::vector<Call *> calls) override
  {
    // Reset a pointer to the active call list if needed later
    this->tr_calls = calls;
    this->tr_calls_set = true;

    nlohmann::ordered_json calls_json;

    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); ++it)
    {
      Call *call = *it;
      if ((call->get_current_length() > 0) || (!call->is_conventional()))
      {
        boost::property_tree::ptree stat_node = call->get_stats();
        calls_json += {
            {"id", stat_node.get<std::string>("id")},
            {"call_num", stat_node.get<long>("callNum")},
            {"freq", stat_node.get<double>("freq")},
            {"sys_num", stat_node.get<int>("sysNum")},
            {"sys_name", stat_node.get<std::string>("shortName")},
            {"talkgroup", stat_node.get<long>("talkgroup")},
            {"talkgroup_alpha_tag", stat_node.get<std::string>("talkgrouptag")},
            {"unit", stat_node.get<long>("srcId")},
            {"unit_alpha_tag", call->get_system()->find_unit_tag(stat_node.get<long>("srcId"))},
            {"elapsed", stat_node.get<long>("elapsed")},
            {"length", round_float(stat_node.get<double>("length"))},
            {"call_state", stat_node.get<int>("state")},
            {"call_state_type", tr_state[stat_node.get<int>("state")]},
            {"mon_state", stat_node.get<int>("monState")},
            {"mon_state_type", mon_state[stat_node.get<int>("monState")]},
            {"rec_num", stat_node.get<int>("recNum", -1)},
            {"src_num", stat_node.get<int>("srcNum", -1)},
            {"rec_state", stat_node.get<int>("recState", -1)},
            {"rec_state_type", tr_state[stat_node.get<int>("recState", -1)]},
            {"phase2", stat_node.get<bool>("phase2")},
            {"analog", stat_node.get<bool>("analog", false)},
            {"conventional", stat_node.get<bool>("conventional")},
            {"encrypted", stat_node.get<bool>("encrypted")},
            {"emergency", stat_node.get<bool>("emergency")},
            {"stop_time", stat_node.get<long>("stopTime")}};
      }
    }
    return send_json(calls_json, "calls", "calls_active", this->topic, false);
  }

  // send_recorders()
  //   Send the status of all recorders.
  //   MQTT: topic/recorders
  int send_recorders(std::vector<Recorder *> recorders)
  {
    nlohmann::ordered_json recorders_json;

    for (std::vector<Recorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
    {
      Recorder *recorder = *it;
      boost::property_tree::ptree stat_node = recorder->get_stats();
      recorders_json += {
          {"id", stat_node.get<std::string>("id")},
          {"src_num", stat_node.get<int>("srcNum")},
          {"rec_num", stat_node.get<int>("recNum")},
          {"type", stat_node.get<std::string>("type")},
          {"duration", round_float(stat_node.get<double>("duration"))},
          {"freq", recorder->get_freq()},
          {"count", stat_node.get<int>("count")},
          {"rec_state", stat_node.get<int>("state")},
          {"rec_state_type", tr_state[stat_node.get<int>("state")]},
          {"squelched", recorder->is_squelched()}};
    }
    return send_json(recorders_json, "recorders", "recorders", this->topic, false);
  }

  // setup_recorder()
  //   Send updates on individual recorders
  //   TRUNK-RECORDER PLUGIN API: Called when a recorder has been created or changes status
  //   MQTT: topic/recorder
  int setup_recorder(Recorder *recorder) override
  {
    boost::property_tree::ptree stat_node = recorder->get_stats();

    nlohmann::ordered_json recorder_json = {
        {"id", stat_node.get<std::string>("id")},
        {"src_num", stat_node.get<int>("srcNum")},
        {"rec_num", stat_node.get<int>("recNum")},
        {"type", stat_node.get<std::string>("type")},
        {"duration", round_float(stat_node.get<double>("duration"))},
        {"freq", recorder->get_freq()},
        {"count", stat_node.get<int>("count")},
        {"rec_state", stat_node.get<int>("state")},
        {"rec_state_type", tr_state[stat_node.get<int>("state")]},
        {"squelched", recorder->is_squelched()}};
    return send_json(recorder_json, "recorder", "recorder", this->topic, false);
  }

  // call_start()
  //   Send information about a new call or the unit initiating it.
  //   TRUNK-RECORDER PLUGIN API: Called when a call starts
  //   MQTT: topic/call_start
  //      Not all calls have recorder info.
  //   MQTT: unit_topic/shortname/call
  int call_start(Call *call) override
  {
    long talkgroup_num = call->get_talkgroup();
    long source_id = call->get_current_source_id();
    std::string short_name = call->get_short_name();
    boost::property_tree::ptree stat_node = call->get_stats();

    if ((this->unit_enabled))
    {
      std::string patch_string = patches_to_str(call->get_system()->get_talkgroup_patch(talkgroup_num));
      nlohmann::ordered_json unit_json = {
          {"sys_num", call->get_system()->get_sys_num()},
          {"sys_name", short_name},
          {"call_num", call->get_call_num()},
          {"start_time", call->get_start_time()},
          {"freq", call->get_freq()},
          {"unit", source_id},
          {"unit_alpha_tag", call->get_system()->find_unit_tag(source_id)},
          {"talkgroup", talkgroup_num},
          {"talkgroup_alpha_tag", call->get_talkgroup_tag()},
          {"talkgroup_patches", patch_string},
          {"encrypted", call->get_encrypted()}};
      send_json(unit_json, "call", "call", this->unit_topic + "/" + short_name, false);
    };

    nlohmann::ordered_json call_json = {
        {"id", stat_node.get<std::string>("id")},
        {"call_num", stat_node.get<long>("callNum")},
        {"freq", stat_node.get<double>("freq")},
        {"sys_num", stat_node.get<int>("sysNum")},
        {"sys_name", stat_node.get<std::string>("shortName")},
        {"talkgroup", stat_node.get<int>("talkgroup")},
        {"talkgroup_alpha_tag", stat_node.get<std::string>("talkgrouptag")},
        {"unit", stat_node.get<long>("srcId")},
        {"unit_alpha_tag", call->get_system()->find_unit_tag(source_id)},
        {"elapsed", stat_node.get<long>("elapsed")},
        {"length", round_float(stat_node.get<double>("length"))},
        {"call_state", stat_node.get<int>("state")},
        {"call_state_type", tr_state[stat_node.get<int>("state")]},
        {"mon_state", stat_node.get<int>("monState")},
        {"mon_state_type", mon_state[stat_node.get<int>("monState")]},
        {"phase2", stat_node.get<bool>("phase2")},
        {"analog", stat_node.get<bool>("analog", false)},
        {"rec_num", stat_node.get<int>("recNum", -1)},
        {"src_num", stat_node.get<int>("srcNum", -1)},
        {"rec_state", stat_node.get<int>("recState", -1)},
        {"rec_state_type", tr_state[stat_node.get<int>("recState", -1)]},
        {"conventional", stat_node.get<bool>("conventional")},
        {"encrypted", stat_node.get<bool>("encrypted")},
        {"emergency", stat_node.get<bool>("emergency")},
        {"stop_time", stat_node.get<long>("stopTime")}};
    return send_json(call_json, "call", "call_start", this->topic, false);
  }

  // call_end()
  //   Send information about a completed call and participating (trunked/conventional) units.
  //   TRUNK-RECORDER PLUGIN API: Called after a call ends
  //   MQTT: topic/call_end
  //   MQTT: unit_topic/shortname/end
  int call_end(Call_Data_t call_info) override
  {
    std::string patch_string = patches_to_str(call_info.patched_talkgroups);

    if (this->unit_enabled)
    {
      // source_list[] can be used to supplement transmission_list[] info
      std::vector<Call_Source> source_list = call_info.transmission_source_list;
      int transmission_num = 0;

      BOOST_FOREACH (auto &transmission, call_info.transmission_list)
      {
        nlohmann::ordered_json unit_json = {
            {"call_num", call_info.call_num},
            {"sys_num", call_info.sys_num},
            {"sys_name", call_info.short_name},
            {"unit", transmission.source},
            {"unit_alpha_tag", source_list[transmission_num].tag},
            {"start_time", transmission.start_time},
            {"stop_time", transmission.stop_time},
            {"sample_count", transmission.sample_count},
            {"spike_count", transmission.spike_count},
            {"error_count", transmission.error_count},
            {"freq", call_info.freq},
            {"length", round_float(transmission.length)},
            {"transmission_filename", transmission.filename},
            {"call_filename", call_info.filename},
            {"position", round_float(source_list[transmission_num].position)},
            {"talkgroup", call_info.talkgroup},
            {"talkgroup_alpha_tag", call_info.talkgroup_alpha_tag},
            {"talkgroup_description", call_info.talkgroup_description},
            {"talkgroup_group", call_info.talkgroup_group},
            {"talkgroup_patches", patch_string},
            {"encrypted", call_info.encrypted},
            {"emergency", source_list[transmission_num].emergency},
            {"signal_system", source_list[transmission_num].signal_system}};
        send_json(unit_json, "end", "end", this->unit_topic + "/" + call_info.short_name.c_str(), false);
        transmission_num++;
      }
    }

    nlohmann::ordered_json call_json = {
        {"call_num", call_info.call_num},
        {"sys_num", call_info.sys_num},
        {"sys_name", call_info.short_name},
        {"start_time", call_info.start_time},
        {"stop_time", call_info.stop_time},
        {"length", round_float(call_info.length)},
        {"process_call_time", call_info.process_call_time},
        {"retry_attempt", call_info.retry_attempt},
        {"error_count", call_info.error_count},
        {"spike_count", call_info.spike_count},
        {"freq", call_info.freq},
        {"encrypted", call_info.encrypted},
        {"emergency", call_info.emergency},
        {"tdma_slot", call_info.tdma_slot},
        {"phase2_tdma", call_info.phase2_tdma},
        {"talkgroup", call_info.talkgroup},
        {"talkgroup_tag", call_info.talkgroup_tag},
        {"talkgroup_alpha_tag", call_info.talkgroup_alpha_tag},
        {"talkgroup_description", call_info.talkgroup_description},
        {"talkgroup_group", call_info.talkgroup_group},
        {"talkgroup_patches", patch_string},
        {"audio_type", call_info.audio_type},
    };
    return send_json(call_json, "call", "call_end", this->topic, false);
  }

  // unit_registration()
  //   Unit registration on a system (on)
  //   TRUNK-RECORDER PLUGIN API: Called each REGISTRATION message
  //   MQTT: unit_topic/shortname/on
  int unit_registration(System *sys, long source_id) override
  {
    if ((this->unit_enabled))
    {
      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)},
      };
      return send_json(unit_json, "on", "on", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_deregistration()
  //   Unit de-registration on a system (off)
  //   TRUNK-RECORDER PLUGIN API: Called each DEREGISTRATION message
  //   MQTT: unit_topic/shortname/off
  int unit_deregistration(System *sys, long source_id) override
  {
    if ((this->unit_enabled))
    {
      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)}};
      return send_json(unit_json, "off", "off", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_acknowledge_response()
  //   Unit acknowledge response (ackresp)
  //   TRUNK-RECORDER PLUGIN API: Called each ACKNOWLEDGE message
  //   MQTT: unit_topic/shortname/ackresp
  int unit_acknowledge_response(System *sys, long source_id) override
  {
    if ((this->unit_enabled))
    {
      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)}};
      return send_json(unit_json, "ackresp", "ackresp", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_group_affiliation()
  //   Unit talkgroup affiliation (join)
  //   TRUNK-RECORDER PLUGIN API: Called each AFFILIATION message
  //   MQTT: unit_topic/shortname/join
  int unit_group_affiliation(System *sys, long source_id, long talkgroup_num) override
  {
    if ((this->unit_enabled))
    {
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      std::string patch_string = patches_to_str(sys->get_talkgroup_patch(talkgroup_num));

      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)},
          {"talkgroup", talkgroup_num},
          {"talkgroup_alpha_tag", ""},
          {"talkgroup_patches", patch_string}};
      if (tg != NULL)
      {
        unit_json["talkgroup_alpha_tag"] = tg->alpha_tag;
      }
      return send_json(unit_json, "join", "join", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_data_grant()
  //   Unit data grant (data)
  //   TRUNK-RECORDER PLUGIN API: Called each DATA_GRANT message
  //   MQTT: unit_topic/shortname/data
  int unit_data_grant(System *sys, long source_id) override
  {
    if ((this->unit_enabled))
    {
      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)},
      };
      return send_json(unit_json, "data", "data", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_answer_request()
  //   TRUNK-RECORDER PLUGIN API: Called each UU_ANS_REQ message
  //   MQTT: unit_topic/shortname/ans_req
  int unit_answer_request(System *sys, long source_id, long talkgroup_num) override
  {
    if ((this->unit_enabled))
    {
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      std::string patch_string = patches_to_str(sys->get_talkgroup_patch(talkgroup_num));

      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)},
          {"talkgroup", talkgroup_num},
          {"talkgroup_alpha_tag", ""},
          {"talkgroup_patches", patch_string}};
      if (tg != NULL)
      {
        unit_json["talkgroup_alpha_tag"] = tg->alpha_tag;
      }
      return send_json(unit_json, "ans_req", "ans_req", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // unit_location()
  //   Unit location/roaming update (location)
  //   TRUNK-RECORDER PLUGIN API: Called each LOCATION message
  //   MQTT: unit_topic/shortname/location
  int unit_location(System *sys, long source_id, long talkgroup_num) override
  {
    if ((this->unit_enabled))
    {
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      std::string patch_string = patches_to_str(sys->get_talkgroup_patch(talkgroup_num));

      nlohmann::ordered_json unit_json = {
          {"sys_num", sys->get_sys_num()},
          {"sys_name", sys->get_short_name()},
          {"unit", source_id},
          {"unit_alpha_tag", sys->find_unit_tag(source_id)},
          {"talkgroup", talkgroup_num},
          {"talkgroup_alpha_tag", ""},
          {"talkgroup_patches", patch_string}};
      if (tg != NULL)
      {
        unit_json["talkgroup_alpha_tag"] = tg->alpha_tag;
      }
      return send_json(unit_json, "location", "location", this->unit_topic + "/" + sys->get_short_name().c_str(), false);
    }
    return 0;
  }

  // ********************************
  // trunk-recorder plugin API & startup
  // ********************************

  // parse_config()
  //   TRUNK-RECORDER PLUGIN API: Called before init(); parses the config information for this plugin.
  // int parse_config(boost::property_tree::ptree &cfg) override
  int parse_config(json config_data) override
  {
    this->log_prefix = "\t[MQTT Status]\t";
    this->mqtt_broker = config_data.value("broker", "tcp://localhost:1883");
    this->username = config_data.value("username", "");
    this->password = config_data.value("password", "");
    this->topic = config_data.value("topic", "");
    this->unit_topic = config_data.value("unit_topic", "");
    this->message_topic = config_data.value("message_topic", "");
    this->console_enabled = config_data.value("console_logs", false);
    this->qos = config_data.value("qos", 0);
    this->client_id = config_data.value("client_id", generate_client_id());

    if (this->unit_topic != "")
      this->unit_enabled = true;

    if (this->message_topic != "")
      this->message_enabled = true;

    if (this->console_enabled == true)
      this->console_topic = this->topic + "/trunk_recorder";

    // Remove any trailing slashes from topics
    if (this->topic.back() == '/')
      this->topic.erase(this->topic.size() - 1);

    if (this->unit_topic.back() == '/')
      this->unit_topic.erase(this->unit_topic.size() - 1);

    if (this->message_topic.back() == '/')
      this->message_topic.erase(this->message_topic.size() - 1);

    // Print plugin startup info
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Broker:                 " << this->mqtt_broker;
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Username:               " << this->username;
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Password:               " << ((this->password == "") ? "[none]" : "********");
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Client ID:              " << this->client_id;
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Status Topic:           " << this->topic;
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Unit Topic:             " << ((this->unit_topic == "") ? "[disabled]" : this->unit_topic + "/shortname");
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Trunk Message Topic:    " << ((this->message_topic == "") ? "[disabled]" : this->message_topic + "/shortname");
    BOOST_LOG_TRIVIAL(info) << log_prefix << "Console Message Topic:  " << ((this->console_enabled == false) ? "[disabled]" : this->console_topic + "/console");
    BOOST_LOG_TRIVIAL(info) << log_prefix << "MQTT QOS:               " << this->qos;
    return 0;
  }

  // init()
  //   TRUNK-RECORDER PLUGIN API: Plugin initialization; called after parse_config().
  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    // frequency_format = config->frequency_format;
    this->instance_id = config->instance_id;
    if (this->instance_id == "")
      this->instance_id = "trunk-recorder";

    // Establish pointers to systems, sources, and configs if needed later.
    this->sources = sources;
    this->systems = systems;
    this->config = config;
    return 0;
  }

  // start()
  //   TRUNK-RECORDER PLUGIN API: Called after trunk-recorder finishes setup and the plugin is initialized
  int start() override
  {
    this->log_prefix = "[MQTT Status]\t";
    // Start the MQTT connection
    open_connection();
    // Send config and system MQTT messages
    send_config(this->sources, this->systems);
    setup_systems(this->systems);

    // Setup custom logging sink for MQTT messages
    if (this->console_enabled == true)
    {
      typedef logging::sinks::synchronous_sink<MqttSinkBackend> mqtt_sink_t;

      boost::shared_ptr<mqtt_sink_t> mqtt_sink = boost::make_shared<mqtt_sink_t>(boost::make_shared<MqttSinkBackend>(*this));
      logging::core::get()->add_sink(mqtt_sink);
    }

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

  // poll_one()
  // TRUNK-RECORDER PLUGIN API: Called during each pass through the main loop of trunk-recorder.
  int poll_one() override
  {
    // Refresh calls every 1 second
    resend_calls();
    return 0;
  }

  // ********************************
  // Helper functions
  // ********************************

  // resend_recorders()
  //   Triggered by setup_config() every 3 seconds to update the status of all recorders.
  void resend_recorders()
  {
    std::vector<Recorder *> recorders;
    for (std::vector<Source *>::iterator it = this->sources.begin(); it != this->sources.end(); ++it)
    {
      Source *source = *it;
      std::vector<Recorder *> sourceRecorders = source->get_recorders();
      recorders.insert(recorders.end(), sourceRecorders.begin(), sourceRecorders.end());
    }
    send_recorders(recorders);
  }

  // resend_calls()
  //   Update the active call list once per second when called by poll_one().
  void resend_calls()
  {
    time_t now_time = time(NULL);

    if (((now_time - this->call_resend_time) >= 1))
    {
      calls_active(this->tr_calls);
      this->call_resend_time = now_time;
    }
  }

  // int_to_hex()
  //   Return a hexidecimal value for a given integer, zero-padded for "places"
  std::string int_to_hex(int num, int places)
  {
    std::stringstream stream;
    stream << std::setfill('0') << std::uppercase << std::setw(places) << std::hex << num;
    std::string result(stream.str());
    return result;
  }

  // generate_client_id()
  //   Return a unique-enough client_id based on a simple crc of broker address and topic
  std::string generate_client_id()
  {
    std::string prefix = "tr-status-";
    std::string info = this->mqtt_broker + "/" + this->topic;
    boost::crc_32_type info_crc;
    info_crc.process_bytes(info.data(), info.length());

    std::stringstream stream;
    stream << prefix << std::hex << info_crc.checksum();
    std::string result(stream.str());
    return result;
  }

  // strip_esc_seq()
  //   Strip the console escape sequences from a string, convert /t to spaces
  std::string strip_esc_seq(const std::string &input)
  {
    std::regex escape_seq_regex("\u001B\\[[0-9;]+m");
    std::regex tab_regex("\t");
    return std::regex_replace(std::regex_replace(input, escape_seq_regex, ""), tab_regex, "    ");
  }

  // round_float()
  //   Round a float to two decimal places and return it as as double.
  //   "position", "length", and "duration" are the usual offenders.
  double round_float(double num)
  {
    char rounded[20];
    snprintf(rounded, sizeof(rounded), "%.2f", num);
    double result = atof(rounded);
    return result;
  }

  // patches_to_str()
  //   Combine a vector of talkgroup patches into a string.
  std::string patches_to_str(std::vector<unsigned long> talkgroup_patches)
  {
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

  // open_connection()
  //   Open the connection to the destination MQTT server using paho libraries.
  //   Send a status message on connect/disconnect.
  //   MQTT: message_topic/status/trunk_recorder/status
  void open_connection()
  {
    // Set a connect/disconnect message between client and broker
    std::string status_topic = this->topic + "/trunk_recorder/status";

    json status_msg = {
        {"status", "connected"},
        {"instance_id", this->instance_id},
        {"client_id", this->client_id},
    };
    mqtt::message_ptr conn_msg = mqtt::message_ptr_builder()
                                     .topic(status_topic)
                                     .payload(status_msg.dump())
                                     .qos(this->qos)
                                     .retained(true)
                                     .finalize();

    status_msg["status"] = "disconnected";
    std::string lwt_json = status_msg.dump();
    auto will_msg = mqtt::message(status_topic, lwt_json.c_str(), strlen(lwt_json.c_str()), this->qos, true);

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
      BOOST_LOG_TRIVIAL(info) << log_prefix << "Setting MQTT Broker username and password..." << endl;
      connOpts.set_user_name(this->username);
      connOpts.set_password(this->password);
    }

    // Open a connection to the broker, set m_open true if successful, publish a connect message
    client = new mqtt::async_client(this->mqtt_broker, this->client_id, config->capture_dir + "/store");
    try
    {
      BOOST_LOG_TRIVIAL(info) << log_prefix << "Connecting...";
      mqtt::token_ptr conntok = client->connect(connOpts);
      BOOST_LOG_TRIVIAL(info) << log_prefix << "Waiting for the connection...";
      conntok->wait();
      BOOST_LOG_TRIVIAL(info) << log_prefix << "OK";
      m_open = true;
      client->publish(conn_msg);
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << log_prefix << exc.what() << endl;
    }
  }

  // send_json()
  //   Send a MQTT message using the configured connection and paho libraries.
  //   send_json(
  //      json data                         <- json payload,
  //      std::string name                  <- json payload name,
  //      std::string type                  <- subtopic / message type
  //      std::string object_topic          <- topic base,
  //      bool retained                     <- retain message at the broker (config, system, etc.)
  //      )
  int send_json(nlohmann::ordered_json data, std::string name, std::string type, std::string object_topic, bool retained)
  {
    // Ignore requests to send MQTT messages before the connection is opened
    if (m_open == false)
      return 0;

    nlohmann::ordered_json payload = {
        {"type", type},
        {name, data},
        {"timestamp", time(NULL)},
        {"instance_id", instance_id}};

    // Assemble the MQTT message
    mqtt::message_ptr pubmsg = mqtt::message_ptr_builder()
                                   .topic(object_topic + "/" + type)
                                   .payload(payload.dump())
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
      BOOST_LOG_TRIVIAL(error) << log_prefix << exc.what() << endl;
    }
    return 0;
  }

  // Paho mqtt::callbacks.
  // connection_lost()
  //   Paho MQTT: This method is called if the connection to the broker is lost.
  void connection_lost(const string &cause) override
  {
    BOOST_LOG_TRIVIAL(error) << log_prefix << "Connection lost to: " << this->mqtt_broker << "/tcause: " << cause;
  }

  // ********************************
  // Create the plugin
  // ********************************

  // Factory method
  static boost::shared_ptr<Mqtt_Status> create()
  {
    return boost::shared_ptr<Mqtt_Status>(new Mqtt_Status());
  }
};

BOOST_DLL_ALIAS(
    Mqtt_Status::create, // <-- this function is exported with...
    create_plugin        // <-- ...this alias name
)
