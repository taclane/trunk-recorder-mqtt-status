# Example MQTT Messages <!-- omit from toc -->

- [Status Messages](#status-messages)
  - [rates](#rates)
  - [config](#config)
  - [systems](#systems)
  - [system](#system)
  - [calls\_active](#calls_active)
  - [recorders](#recorders)
  - [recorder](#recorder)
  - [call\_start](#call_start)
  - [call\_end](#call_end)
  - [plugin\_status](#plugin_status)
- [Unit Messages](#unit-messages)
  - [call](#call)
  - [end](#end)
  - [on](#on)
  - [off](#off)
  - [ackresp](#ackresp)
  - [join](#join)
  - [data](#data)
  - [ans\_req](#ans_req)
  - [location](#location)
- [Trunking Messages](#trunking-messages)
  - [messages](#messages)
- [Console Messages](#console-messages)
  - [console](#console)

# Status Messages

## rates

Trunk message rate reporting by system

`topic/rates`

```json
{
  "rates": [
    {
      "sys_num": "2",
      "sys_name": "p25trunk",
      "decoderate": "39.67",
      "decoderate_interval": "3",
      "control_channel": "774581250"
    },
    {
      "sys_num": "3",
      "sys_name": "p25trunk2",
      "decoderate": "35.00",
      "decoderate_interval": "3",
      "control_channel": "853750000"
    }
  ],
  "type": "rates",
  "timestamp": "1686699024",
  "instance_id": "east-antenna"
}
```

Changes:

```
rates
  id -> sys_num
      + sys_name
      + decoderate_interval
      + control_channel

Conventional systems are omitted from rate reporting by the plugin.
```

## config

Trunk-recorder source and system config information. The message is retained on the MQTT broker.

`topic/config`

```json
{
    "config": {
        "sources": [
            {
                "source_num": "0",
                "rate": "2400000",
                "center": "772906250",
                "min_hz": "771756250",
                "max_hz": "774056250",
                "error": "0",
                "driver": "osmosdr",
                "device": "rtl=03",
                "antenna": "",
                "gain": "32",
                "analog_recorders": "0",
                "digital_recorders": "5",
                "debug_recorders": "0",
                "sigmf_recorders": "0",
                "silence_frames": "0"
            },
            {
                ...
            }
        ],
        "systems": [
            {
                "sys_num": "0",
                "sys_name": "p25con",
                "system_type": "conventionalP25",
                "talkgroups_file": "",
                "qpsk": "true",
                "squelch_db": "-55",
                "analog_levels": "8",
                "digital_levels": "1",
                "audio_archive": "false",
                "upload_script": "",
                "record_unkown": "true",
                "call_log": "false",
                "channels": ""
            },
            {
                "sys_num": "1",
                "sys_name": "con",
                "system_type": "conventional",
                "talkgroups_file": "",
                "qpsk": "true",
                "squelch_db": "-45",
                "analog_levels": "16",
                "digital_levels": "1",
                "audio_archive": "false",
                "upload_script": "",
                "record_unkown": "true",
                "call_log": "false",
                "channels": ""
            },
            {
                "sys_num": "2",
                "sys_name": "p25trunk",
                "system_type": "p25",
                "talkgroups_file": "p25/trunk.csv",
                "qpsk": "true",
                "squelch_db": "-160",
                "analog_levels": "8",
                "digital_levels": "1",
                "audio_archive": "false",
                "upload_script": "",
                "record_unkown": "true",
                "call_log": "false",
                "control_channel": "774581250",
                "channels": [
                    "774581250",
                    "772431250",
                    "773456250",
                    "774031250"
                ]
            },
            {
                ...
            }
        ],
        "capture_dir": "/dev/shm/trunk-recorder",
        "upload_server": "https://api.openmhz.com",
        "call_timeout": "3",
        "log_file": "false",
        "instance_id": "east-antenna",
        "instance_key": ""
    },
    "type": "config",
    "timestamp": "1686400353",
    "instance_id": "east-antenna"
}
```

Changes:

```
config
  systems
    audioArchive    -> audio_archive
    systemType      -> system_type
    shortName       -> sys_name
    sysNum          -> sys_num
    uploadScript    -> upload_script
    recordUnknown   -> record_unknown
    callLog         -> call_log
    talkgroupsFile  -> talkgroups_file
  captureDir      -> capture_dir
  uploadServer    -> upload_server
  callTimeout     -> call_timeout
  logFile         -> log_file
  instanceId      -> instance_id
  instanceKey     -> instance_key
```

## systems

Configured systems are sent during startup. The message is retained on the MQTT broker.

`topic/systems`

```json
{
  "systems": [
    {
      "sys_num": "0",
      "sys_name": "p25con",
      "type": "conventionalP25",
      "sysid": "0",
      "wacn": "0",
      "nac": "0"
    },
    {
      "sys_num": "1",
      "sys_name": "con",
      "type": "conventional",
      "sysid": "0",
      "wacn": "0",
      "nac": "0"
    },
    {
      "sys_num": "2",
      "sys_name": "p25trunk",
      "type": "p25",
      "sysid": "555",
      "wacn": "BBBBB",
      "nac": "BCC"
    }
  ],
  "type": "systems",
  "timestamp": "1686400355",
  "instance_id": "east-antenna"
}
```

Changes:

```
systems
  id   -> sys_num
  name -> sys_name
```

## system

Sent after a system is configured during trunk-recorder startup.

`topic/system`

```json
{
  "system": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "type": "p25",
    "sysid": "555",
    "wacn": "BBBBB",
    "nac": "BCC"
  },
  "type": "system",
  "timestamp": "1686146741",
  "instance_id": "east-antenna"
}
```

Changes:

```
system
  id   -> sys_num
  name -> sys_name
```

## calls_active

List of active calls, updated every second.

`topic/calls_active`

```json
{
    "calls": [
        {
            "id": "2_4011_1686699318",
            "call_num": "51442",
            "freq": "771356250",
            "sys_num": "2",
            "sys_name": "p25trunk",
            "talkgroup": "4011",
            "talkgroup_alpha_tag": "Bus 1",
            "unit": "899003",
            "unit_alpha_tag": "Trans 9003",
            "elapsed": "12",
            "length": "7.48",
            "call_state": "1",
            "call_state_type": "RECORDING",
            "mon_state": "0",
            "mon_state_type": "UNSPECIFIED",
            "rec_num": "0",
            "src_num": "1",
            "rec_state": "4",
            "rec_state_type": "IDLE",
            "phase2": "true",
            "analog": "false",
            "conventional": "false",
            "encrypted": "false",
            "emergency": "false",
            "stop_time": "1686699318"
        },
        {
            ...
        }
    ],
    "type": "calls_active",
    "timestamp": "1686699330",
    "instance_id": "east-antenna"
}
```

Changes:

```
calls
  callNum      -> call_num
  sysNum       -> sys_num
  shortName    -> sys_name
  talkgrouptag -> talkgroup_alpha_tag
  srcId        -> unit
                + unit_alpha_tag
  state        -> call_state
                + call_state_type
  monState     -> mon_state
                + mon_state_type
  recNum       -> rec_num
  srcNum       -> src_num
  recState     -> rec_state
                + rec_state_type
  stopTime     -> stop_time
```

## recorders

List of all recorders, updated every 3 seconds.

`topic/recorder`

```json
{
    "recorders": [
        {
            "id": "0_19",
            "src_num": "0",
            "rec_num": "19",
            "type": "P25C",
            "duration": "706.50",
            "freq": "457750000",
            "count": "73",
            "rec_state": "4",
            "rec_state_type": "IDLE",
            "squelched": "false"
        },
        {
            ...
        }
    ],
    "type": "recorders",
    "timestamp": "1686699405",
    "instance_id": "east-antenna"
}
```

Changes:

```
recorders
  srcNum -> src_num
  recNum -> rec_num
  state  -> rec_state
          + rec_state_type
          + squelched
```

## recorder

Recorder status updates.

`topic/recorder`

```json
{
  "recorder": {
    "id": "4_16",
    "src_num": "4",
    "rec_num": "16",
    "type": "P25",
    "freq": "852200000",
    "duration": "21182.10",
    "count": "3563",
    "rec_state": "4",
    "rec_state_type": "IDLE",
    "squelched": "false"
  },
  "type": "recorder",
  "timestamp": "1686700173",
  "instance_id": "east-antenna"
}
```

Changes:

```
recorder
  srcNum -> src_num
  recNum -> rec_num
  state  -> rec_state
          + rec_state_type
          + squelched
```

## call_start

Sent when a new trunked call starts, or when a conventional recorder is reset after a call.

`topic/call_start`

```json
{
  "call": {
    "id": "2_3029_1686700194",
    "call_num": "51684",
    "freq": "770331250",
    "sys_num": "2",
    "sys_name": "p25trunk",
    "talkgroup": "3029",
    "talkgroup_alpha_tag": "Metro Police",
    "unit": "849183",
    "unit_alpha_tag": "Metro Dispatch",
    "elapsed": "0",
    "length": "0.00",
    "call_state": "0",
    "call_state_type": "MONITORING",
    "mon_state": "6",
    "mon_state_type": "DUPLICATE",
    "rec_num": "",
    "src_num": "",
    "rec_state": "",
    "rec_state_type": "",
    "phase2": "false",
    "analog": "",
    "conventional": "false",
    "encrypted": "false",
    "emergency": "false",
    "stop_time": "1686700194"
  },
  "type": "call_start",
  "timestamp": "1686700194",
  "instance_id": "east-antenna"
}
```

Changes:

```
call
  callNum      -> call_num
  sysNum       -> sys_num
  shortName    -> sys_name
  talkgrouptag -> talkgroup_alpha_tag
  srcId        -> unit
                + unit_alpha_tag
  state        -> call_state
                + call_state_type
  monState     -> mon_state
                + mon_state_type
  recNum       -> rec_num
  srcNum       -> src_num
  recState     -> rec_state
                + rec_state_type
  stopTime     -> stop_time
```

## call_end

Sent after trunk-recorder completes recording a call.

`topic/call_end`

```json
{
  "call": {
    "call_num": "51697",
    "sys_num": 1,
    "sys_name": "p25trunk",
    "start_time": "1686700238",
    "stop_time": "1686700263",
    "length": "17.60",
    "process_call_time": "1686700268",
    "retry_attempt": "0",
    "error_count": "0",
    "spike_count": "0",
    "freq": "771606250",
    "encrypted": "false",
    "emergency": "false",
    "tdma_slot": "1",
    "phase2_tdma": "true",
    "talkgroup": "4012",
    "talkgroup_tag": "Transportation",
    "talkgroup_alpha_tag": "Bus 2",
    "talkgroup_description": "Bus Dispatch 2",
    "talkgroup_group": "Transit",
    "talkgroup_patches": "",
    "audio_type": "digital tdma"
  },
  "type": "call_end",
  "timestamp": "1686700268",
  "instance_id": "east-antenna"
}
```

Changes:

```
call
  short_name -> sys_name
  callNum    -> call_num
```

## plugin_status

Plugin status message, sent on startup or when the broker loses connection. The message is retained on the MQTT broker.

`topic/trunk_recorder/status`

```json
{
  "status": "connected",
  "instance_id": "east-antenna",
  "client_id": "tr-status"
}
```

# Unit Messages

## call

Channel grants. Sent when a call starts.

`unit_topic/shortname/call`

```json
{
  "call": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "call_num": "51869",
    "start_time": "1686700982",
    "freq": "770581250",
    "unit": "804329",
    "unit_alpha_tag": "State Port. 329",
    "talkgroup": "301",
    "talkgroup_alpha_tag": "State Disp 1",
    "talkgroup_patches": "",
    "encrypted": "false"
  },
  "type": "call",
  "timestamp": "1686700985",
  "instance_id": "east-antenna"
}
```

Changes:

```
call
              + sys_num
  system     -> sys_name
  callNum    -> call_num
  unit_alpha -> unit_alpha_tag
```

## end

Call end information by transmission. Reports information for conventional units.

`unit_topic/shortname/end`

```json
{
  "end": {
    "call_num": "53726",
    "sys_name": "p25trunk",
    "unit": "46999",
    "unit_alpha_tag": "Bus 999",
    "start_time": "1686711476",
    "stop_time": "1686711483",
    "sample_count": "50560",
    "spike_count": "0",
    "error_count": "0",
    "freq": "769656250",
    "length": "6.32",
    "transmission_filename": "/dev/shm/p25trunk/4012-1686711476_769656250.wav",
    "call_filename": "/dev/shm/trunk-recorder/p25trunk/2023/6/13/4012-1686711439_769656250.0-call_53726.wav",
    "position": "33.40",
    "talkgroup": "4012",
    "talkgroup_alpha_tag": "Bus 2",
    "talkgroup_description": "Bus Dispatch 2",
    "talkgroup_group": "Transit",
    "talkgroup_patches": "",
    "encrypted": "false",
    "emergency": "false",
    "signal_system": ""
  },
  "type": "end",
  "timestamp": "1686711488",
  "instance_id": "east-antenna"
}
```

Changes:

```
call
  callNum    -> call_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## on

Unit registration (radio turned on)

`unit_topic/shortname/on`

```json
{
  "on": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "806308",
    "unit_alpha_tag": "State Port. 308"
  },
  "type": "on",
  "timestamp": "1686711527",
  "instance_id": "east-antenna"
}
```

Changes:

```
on
              + sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## off

Unit de-registration (radio turned off)

`unit_topic/shortname/off`

```json
{
  "off": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "804422",
    "unit_alpha_tag": "State Port. 422"
  },
  "type": "off",
  "timestamp": "1686711529",
  "instance_id": "east-antenna"
}
```

Changes:

```
off
              + sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## ackresp

Unit acknowledge response.

`unit_topic/shortname/ackresp`

```json
{
  "ackresp": {
    "sys_num": "2",
    "sys_name": "p25trunk",
    "unit": "52001",
    "unit_alpha_tag": "FD Mobile 001"
  },
  "type": "ackresp",
  "timestamp": "1686711630",
  "instance_id": "east-antenna"
}
```

Changes:

```
ackresp
              + sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## join

Unit group affiliation

`unit_topic/shortname/join`

```json
{
  "join": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "806109",
    "unit_alpha_tag": "State Port. 109",
    "talkgroup": "401",
    "talkgroup_alpha_tag": "State Disp 1",
    "talkgroup_patches": ""
  },
  "type": "join",
  "timestamp": "1686711676",
  "instance_id": "east-antenna"
}
```

Changes:

```
join
  sysNum     -> sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## data

Unit data grant

`unit_topic/shortname/data`

```json
{
  "data": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "849006",
    "unit_alpha_tag": ""
  },
  "type": "data",
  "timestamp": "1686712418",
  "instance_id": "east-antenna"
}
```

Changes:

```
data
              + sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## ans_req

Unit answer request

`unit_topic/shortname/ans_req`

```json
{
  "ans_req": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "2048",
    "unit_alpha_tag": "",
    "talkgroup": "12582912",
    "talkgroup_alpha_tag": ""
  },
  "type": "ans_req",
  "timestamp": "1686712433",
  "instance_id": "east-antenna"
}
```

Changes:

```
ans_req
              + sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

## location

Unit location update

`unit_topic/shortname/location`

```json
{
  "location": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "unit": "54126",
    "unit_alpha_tag": "FD Port. 126",
    "talkgroup": "23507",
    "talkgroup_alpha_tag": "FD",
    "talkgroup_patches": ""
  },
  "type": "location",
  "timestamp": "1686712458",
  "instance_id": "east-antenna"
}
```

Changes:

```
location
  sysNum     -> sys_num
  system     -> sys_name
  unit_alpha -> unit_alpha_tag
```

# Trunking Messages

## messages

Overview of trunking messages that have been decoded.

`message_topic/shortname/messages`

```json
{
  "message": {
    "sys_num": "3",
    "sys_name": "p25trunk",
    "trunk_msg": "3",
    "trunk_msg_type": "CONTROL_CHANNEL",
    "opcode": "39",
    "opcode_type": "SCCB",
    "opcode_desc": "Secondary Control Channel Broadcast",
    "meta": "tsbk39 secondary cc: rfid .... "
  },
  "type": "message",
  "timestamp": "1686712507",
  "instance_id": "east-antenna"
}
```

# Console Messages

## console

Console log messages forwarded over MQTT.

`topic/trunk_recorder/console`

```json
{
    "console": {
        "time": "2023-08-07T12:07:06.327966",
        "severity": "info",
        "log_msg": "[sname]    143C    TG:      12300 (       Eastport FD Disp)    Freq: 771.581250 MHz    Rdio Scanner Upload Success - file size: 18175"
    },
    "type": "console",
    "timestamp": "1691424426",
    "instance_id": "east-antenna"
}
```