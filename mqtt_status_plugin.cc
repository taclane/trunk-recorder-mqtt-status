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
  //time_t reconnect_time;
  //bool m_reconnect;
  bool m_open;
  //bool m_done;
  bool m_config_sent;
  
  bool unit_enabled = false;

  int refresh = 60;
  std::vector<Source *> sources;
  std::vector<System *> systems;
  std::vector<Call *> calls;
  Config *config;
  std::string mqtt_broker;
  std::string username;
  std::string password;
  std::string topic;
  std::string unit_topic;
  mqtt::async_client *client;

  time_t config_resend_time = time(NULL);

  std::map<std::string, int> system_map;

protected:
  void on_failure(const mqtt::token &tok) override
  {
    cout << "\tListener failure for token: "
         << tok.get_message_id() << endl;
  }

  void on_success(const mqtt::token &tok) override
  {
    cout << "\tListener success for token: "
         << tok.get_message_id() << endl;
  }

public:
  void connection_lost(const string &cause) override
  {
    cout << "\nConnection lost" << endl;
    if (!cause.empty())
      cout << "\tcause: " << cause << endl;
  }

  void delivery_complete(mqtt::delivery_token_ptr tok) override
  {
    cout << "\tDelivery complete for token: "
         << (tok ? tok->get_message_id() : -1) << endl;
  }


  int system_rates(std::vector<System *> systems, float timeDiff) override
  {
    boost::property_tree::ptree system_node;
    boost::property_tree::ptree systems_node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      system_node = system->get_stats_current(timeDiff);
      system_node.put("shortName", system->get_short_name());
      systems_node.push_back(std::make_pair("", system_node));
    }
    
    /*
     * system_rates() is triggered every ~3 sec. Periodic tasks can more efficiently be 
     * checked here instead of each cycle in poll_one().
    */ 
    resend_configs();
    /*
    */

    return send_object(systems_node, "rates", "rates", this->topic);
  }

//m_done(false),
  Mqtt_Status() : m_open(false), m_config_sent(false)
  {
  }

  void send_config(std::vector<Source *> sources, std::vector<System *> systems)
  {
    if (m_open == false)
      return;

    if (m_config_sent)
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
      }

      // std::cout << "starts: " << std::endl;

      for (std::vector<double>::iterator chan_it = channels.begin(); chan_it != channels.end(); ++chan_it)
      {
        double channel = *chan_it;
        boost::property_tree::ptree channel_node;
        // std::cout << "Hello: " << channel << std::endl;
        channel_node.put("", channel);

        // Add this node to the list.
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
    m_config_sent = true;
  }

  int send_systems(std::vector<System *> systems)
  {
    boost::property_tree::ptree node;

    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it)
    {
      System *system = *it;
      node.push_back(std::make_pair("", system->get_stats()));
    }
    return send_object(node, "systems", "systems", this->topic);
  }

  int send_system(System *system)
  {
    return send_object(system->get_stats(), "system", "system", this->topic);
  }

  int calls_active(std::vector<Call *> calls) override
  {
    boost::property_tree::ptree node;

    for (std::vector<Call *>::iterator it = calls.begin(); it != calls.end(); ++it)
    {
      Call *call = *it;
      // if (call->get_state() == RECORDING) {
      node.push_back(std::make_pair("", call->get_stats()));
      //}
    }

    return send_object(node, "calls", "calls_active", this->topic);
  }

  int send_recorders(std::vector<Recorder *> recorders)
  {
    boost::property_tree::ptree node;

    for (std::vector<Recorder *>::iterator it = recorders.begin(); it != recorders.end(); ++it)
    {
      Recorder *recorder = *it;
      node.push_back(std::make_pair("", recorder->get_stats()));
    }

    return send_object(node, "recorders", "recorders", this->topic);
  }


  int send_recorder(Recorder *recorder)
  {
    return send_object(recorder->get_stats(), "recorder", "recorder", this->topic);
  }

  //************UNIT REPORTING********
  // Re-task call_start to also report unit ID information
  int call_start(Call *call) override {
    long talkgroup_num = call->get_talkgroup();
    long source_id = call->get_current_source_id();
    std::string short_name = call->get_short_name();
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      std::vector<unsigned long> talkgroup_patches = call->get_system()->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;
      BOOST_FOREACH (auto& TGID, talkgroup_patches) {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      node.put("callNum", call->get_call_num());
      node.put("system", short_name );
      node.put("unit", source_id );
      node.put("unit_alpha", call->get_system()->find_unit_tag(source_id));
      node.put("start_time", call->get_start_time());
      node.put("freq", call->get_freq());
      node.put("talkgroup", talkgroup_num);
      node.put("talkgroup_patches", patch_string);
      node.put("talkgroup_alpha", call->get_talkgroup_tag());
      node.put("talkgroup_patches", patch_string);
      node.put("encrypted", call->get_encrypted());
      send_object(node, "call", "call", this->unit_topic+"/"+short_name);
    }
    return send_object(call->get_stats(), "call", "call_start", this->topic);
  }

  // Use the call_end to catch conventional p25 UID/TG information since there are no control channel messages
  int call_end(Call_Data_t call_info) override
  {
    int sys_num = system_map[call_info.short_name];
    System *sys = this->systems[sys_num];

    //BOOST_LOG_TRIVIAL(error) << sys->get_short_name();

    // Generate list of talkgroup patches
    std::vector<unsigned long> talkgroup_patches = call_info.patched_talkgroups;
    bool first = true;
    std::string patch_string;
    BOOST_FOREACH (auto& TGID, talkgroup_patches) {
      if (!first) { patch_string += ","; }
      first = false;
      patch_string += std::to_string(TGID);
    }

    if (this->unit_enabled) {
      boost::property_tree::ptree node;

      // Transmission (in transmission_list) doesn't store position, so duplicate the logic to calculate it.
      double total_length = 0;

      BOOST_FOREACH (auto& transmission, call_info.transmission_list) {
        node.put("callNum", call_info.call_num);
        node.put("system", call_info.short_name);
        node.put("unit", transmission.source);
        node.put("unit_alpha", sys->find_unit_tag(transmission.source));
        node.put("start_time", transmission.start_time);
        node.put("stop_time", transmission.stop_time);
        node.put("sample_count", transmission.sample_count);
        node.put("spike_count", transmission.spike_count);
        node.put("error_count", transmission.error_count);
        node.put("freq", call_info.freq);
        node.put("length", transmission.length);
        node.put("transmission_filename", transmission.filename);
        node.put("transmission_base_filename", transmission.base_filename);
        node.put("call_filename", call_info.filename);
        node.put("position", total_length);
        node.put("talkgroup", call_info.talkgroup);
        node.put("talkgroup_alpha_tag",call_info.talkgroup_alpha_tag);
        node.put("talkgroup_description",call_info.talkgroup_description);
        node.put("talkgroup_group",call_info.talkgroup_group);
        node.put("talkgroup_patches", patch_string);
        node.put("encrypted", call_info.encrypted);
        send_object(node, "end", "end", this->unit_topic+"/"+call_info.short_name.c_str());

        total_length = total_length + transmission.length;
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
    call_node.put("audio_type",call_info.audio_type);
    
    // call_node.put("transmission_source_list",call_info.transmission_source_list);
    // call_node.put("transmission_error_list",call_info.transmission_error_list);
    call_node.put("start_time",call_info.start_time);
    call_node.put("stop_time",call_info.stop_time);
    call_node.put("length",call_info.length);

    return send_object(call_node, "call", "call_end", this->topic);

  }

  int unit_registration(System *sys, long source_id) override {
    //unit_affiliations[source_id] = 0;
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      return send_object(node, "on", "on", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_deregistration(System *sys, long source_id) override { 
    //unit_affiliations[source_id] = -1;
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      return send_object(node, "off", "off", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }  

  int unit_acknowledge_response(System *sys, long source_id) override { 
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      return send_object(node, "ackresp", "ackresp", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_group_affiliation(System *sys, long source_id, long talkgroup_num) override {
    //unit_affiliations[source_id] = talkgroup_num;
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      std::vector<unsigned long> talkgroup_patches = sys->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;
      BOOST_FOREACH (auto& TGID, talkgroup_patches) {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      node.put("talkgroup", talkgroup_num);
      node.put("talkgroup_patches", patch_string);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL) {
        node.put("talkgroup_alpha", tg->alpha_tag);
      }
      return send_object(node, "join", "join", this->unit_topic+"/"+sys->get_short_name().c_str());  
    }
    return 1;
  }

  int unit_data_grant(System *sys, long source_id) override {
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      return send_object(node, "data", "data", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_answer_request(System *sys, long source_id, long talkgroup) override {
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      node.put("talkgroup", talkgroup);
      Talkgroup *tg = sys->find_talkgroup(talkgroup);
      if (tg != NULL) {
        node.put("talkgroup_alpha", tg->alpha_tag);
      }
      return send_object(node, "ans_req", "ans_req", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }

  int unit_location(System *sys, long source_id, long talkgroup_num) override {
    //unit_affiliations[source_id] = talkgroup_num;
    if ((this->unit_enabled)) {
      boost::property_tree::ptree node;
      std::vector<unsigned long> talkgroup_patches = sys->get_talkgroup_patch(talkgroup_num);
      std::string patch_string;
      bool first = true;
      BOOST_FOREACH (auto& TGID, talkgroup_patches) {
        if (!first) { patch_string += ","; }
        first = false;
        patch_string += std::to_string(TGID);
      }
      node.put("system", sys->get_short_name());
      node.put("unit", source_id );
      node.put("unit_alpha", sys->find_unit_tag(source_id));
      node.put("talkgroup", talkgroup_num);
      node.put("talkgroup_patches", patch_string);
      Talkgroup *tg = sys->find_talkgroup(talkgroup_num);
      if (tg != NULL) {
        node.put("talkgroup_alpha", tg->alpha_tag);
      }
      return send_object(node, "location", "location", this->unit_topic+"/"+sys->get_short_name().c_str());
    }
    return 1;
  }
  //************UNIT REPORTING END****

  int send_object(boost::property_tree::ptree data, std::string name, std::string type, std::string object_topic)
  {
    if (m_open == false)
      return 0;

    time_t now_time = time(NULL);

    boost::property_tree::ptree root;
    //std::string object_topic = topicname;

    if (object_topic.back() == '/') {
      object_topic.erase(object_topic.size() - 1);
    }
    object_topic = object_topic + "/" + type;
    root.add_child(name, data);
    root.put("type", type);
    root.put("timestamp", now_time);

    std::stringstream stats_str;
    boost::property_tree::write_json(stats_str, root);

    try
    {
      mqtt::message_ptr pubmsg = mqtt::make_message(object_topic, stats_str.str());
      pubmsg->set_qos(QOS);
      pubmsg->set_retained(retain);
      client->publish(pubmsg); //->wait_for(TIMEOUT);
    }
    catch (const mqtt::exception &exc)
    {
      BOOST_LOG_TRIVIAL(error) << "MQTT Status Plugin - " <<exc.what() << endl;
    }

    return 0; 
  }

  int poll_one() override
  {
    return 0;
  }

  int resend_configs()
  {
    time_t now_time = time(NULL);
      
    if (((now_time - this->config_resend_time ) > refresh ) && (this->config_resend_time > 0)){
      this->m_config_sent = false;

      std::vector<Recorder *> recorders;
      for (std::vector<Source *>::iterator it = this->sources.begin(); it != this->sources.end(); ++it) {
        Source *source = *it;
        std::vector<Recorder *> sourceRecorders = source->get_recorders();
        recorders.insert(recorders.end(), sourceRecorders.begin(), sourceRecorders.end());
      }

      send_config(this->sources, this->systems);
      send_systems(this->systems);
      send_recorders(recorders);
      this->config_resend_time = now_time;
    } 

    return 0;
  }

  void open_connection()
  {
    const char *LWT_PAYLOAD = "Last will and testament.";
    // set up access channels to only log interesting things
    client = new mqtt::async_client(this->mqtt_broker, this->clientid, config->capture_dir + "/store");

    mqtt::connect_options connOpts;

    if ((this->username != "") && (this->password != ""))
    {
      BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin - \tSetting MQTT Broker username and password..." << endl;
      connOpts = mqtt::connect_options_builder().clean_session().user_name(this->username).password(this->password).will(mqtt::message("final", LWT_PAYLOAD, QOS)).finalize();
      ;
    }
    else
    {
      connOpts = mqtt::connect_options_builder().clean_session().will(mqtt::message("final", LWT_PAYLOAD, QOS)).finalize();
      ;
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

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override
  {
    frequency_format = config->frequency_format; 
    this->sources = sources;
    this->systems = systems;
    this->config = config;

    // build a system_map in case you need to lookup a system by shortname later.
    int sys_number = 0;
    for (std::vector<System *>::iterator it = systems.begin(); it != systems.end(); ++it) {
      System *sys = (System *)*it;
      
      std::string short_name = sys->get_short_name();
      this->system_map[short_name] = sys_number;
      
      sys_number += 1;
    }

    return 0;
  }

  int start() override
  {
    open_connection();
    return 0;
  }

  int setup_recorder(Recorder *recorder) override
  {
    this->send_recorder(recorder);
    return 0;
  }

  int setup_system(System *system) override
  {
    this->send_system(system);
    return 0;
  }

  int setup_systems(std::vector<System *> systems) override
  {
    this->send_systems(systems);
    return 0;
  }

  int setup_config(std::vector<Source *> sources, std::vector<System *> systems) override
  {
    send_config(sources, systems);
    return 0;
  }

  // Factory method
  static boost::shared_ptr<Mqtt_Status> create()
  {
    return boost::shared_ptr<Mqtt_Status>(
        new Mqtt_Status());
  }

  int parse_config(boost::property_tree::ptree &cfg) override
  {
    this->mqtt_broker = cfg.get<std::string>("broker", "tcp://localhost:1883");
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker: " << this->mqtt_broker;
    this->username = cfg.get<std::string>("username", "");
    this->password = cfg.get<std::string>("password", "");
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Broker Username: " << this->username;
    this->topic = cfg.get<std::string>("topic", "");
    BOOST_LOG_TRIVIAL(info) << " MQTT Status Plugin Topic: " << this->topic;
    this->unit_topic = cfg.get<std::string>("unit_topic", "");
    if (this->unit_topic == "") {
      BOOST_LOG_TRIVIAL(info) << " MQTT Unit Status Plugin: Disabled";
    } else {
      BOOST_LOG_TRIVIAL(info) << " MQTT Unit Status Plugin Topic: " << this->unit_topic;
      this->unit_enabled = true;
    }
    this->refresh = cfg.get<int>("refresh", 60);
    BOOST_LOG_TRIVIAL(info) << " MQTT Recorder/Source Refresh Interval: " << this->refresh;
    
    return 0;
  }

};

BOOST_DLL_ALIAS(
    Mqtt_Status::create, // <-- this function is exported with...
    create_plugin        // <-- ...this alias name
)
