# Trunk Recorder MQTT Status (and Units!) Plugin <!-- omit from toc -->

This is a plugin for Trunk Recorder that publish the current status over MQTT. External programs can use the MQTT messages to collect and display information on monitored systems.

Requires trunk-recorder 4.7 (commit 837a057 14 NOV 2023) or later, and Paho MQTT libraries

- [Install](#install)
- [Configure](#configure)
- [MQTT Messages](#mqtt-messages)
- [Trunk Recorder States](#trunk-recorder-states)
- [MQTT Brokers](#mqtt-brokers)
  - [Mosquitto MQTT Broker](#mosquitto-mqtt-broker)
  - [NanoMQ](#nanomq)
- [Docker](#docker)

## Install

1. **Build and install the current version of Trunk Recorder** following these [instructions](https://github.com/robotastic/trunk-recorder/blob/master/docs/INSTALL-LINUX.md). Make sure you do a `sudo make install` at the end to install the Trunk Recorder binary and libraries system-wide. The plugin will be built against these libraries.

2. **Install the Paho MQTT C & C++ Libraries**.

&emsp; _Install Paho MQTT C_

```bash
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c

cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON  -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
sudo cmake --build build/ --target install
sudo ldconfig
```

&emsp; _Install Paho MQTT C++_

```bash
git clone https://github.com/eclipse/paho.mqtt.cpp
cd paho.mqtt.cpp

cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON
sudo cmake --build build/ --target install
sudo ldconfig
```

&emsp; Alternatively, if your package manager provides recent Paho MQTT libraries:

```bash
sudo apt install libpaho-mqtt-dev libpaho-mqttpp-dev
```

3. **Build and install the plugin:**

&emsp; Plugin repos should be cloned in a location other than the trunk-recorder source dir.

```bash
mkdir build
cd build
cmake ..
sudo make install
```

&emsp; **IMPORTANT NOTE:** To avoid SEGFAULTs or other errors, plugins should be rebuilt after every new release of trunk-recorder.

## Configure

**Plugin options:**

| Key           | Required | Default Value        | Type       | Description                                                                                                                                                                              |
| ------------- | :------: | -------------------- | ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| broker        |    ✓     | tcp://localhost:1883 | string     | The URL for the MQTT Message Broker. It should include the protocol used: **tcp**, **ssl**, **ws**, **wss** and the port, which is generally 1883 for tcp, 8883 for ssl, and 443 for ws. |
| topic         |    ✓     |                      | string     | This is the base MQTT topic. The plugin will create subtopics for the different status messages.                                                                                         |
| unit_topic    |          |                      | string     | Optional topic to report unit stats over MQTT.                                                                                                                                           |
| message_topic |          |                      | string     | Optional topic to report trunking messages over MQTT.                                                                                                                                    |
| console_logs  |          | false                | true/false | Optional setting to report console messages over MQTT.                                                                                                                                   |
| username      |          |                      | string     | If a username is required for the broker, add it here.                                                                                                                                   |
| password      |          |                      | string     | If a password is required for the broker, add it here.                                                                                                                                   |
| client_id     |          | tr-status-xxxxxxxx   | string     | Override the client_id generated for this connection to the MQTT broker.                                                                                                                 |
| qos           |          | 0                    | int        | Set the MQTT message [QOS level](https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/qos.html)                                                                                    |

**Trunk-Recorder options:**

| Key                          | Required | Default Value               | Type   | Description                                                                                |
| ---------------------------- | :------: | --------------------------- | ------ | ------------------------------------------------------------------------------------------ |
| [instance_id](./config.json) |          | <nobr>trunk-recorder</nobr> | string | Append an `instance_id` key to identify the trunk-recorder instance sending MQTT messages. |

**Plugin Usage:**

See the included [config.json](./config.json) for an example how to load this plugin.

```json
    "plugins": [
    {
        "name": "MQTT Status",
        "library": "libmqtt_status_plugin.so",
        "broker": "tcp://io.adafruit.com:1883",
        "topic": "robotastic/feeds",
        "unit_topic": "robotastic/units",
        "username": "robotastic",
        "password": "",
        "console_logs": true,
    }]
```

If the plugin cannot be found, or it is being run from a different location, it may be necessary to supply the full path:

```json
        "library": "/usr/local/lib/trunk-recorder/libmqtt_status_plugin.so",
```

## MQTT Messages

The plugin will provide the following messages to the MQTT broker depending on configured topics.

| Topic                   | Sub-Topic                                          | Retained | Description\*                                                      |
| ----------------------- | -------------------------------------------------- | :------: | ------------------------------------------------------------------ |
| topic                   | [rates](./example_messages.md#rates)               |          | Control channel decode rates                                       |
| topic                   | [config](./example_messages.md#config)             |    ✓     | Trunk-recorder config information                                  |
| topic                   | [systems](./example_messages.md#systems)           |    ✓     | List of configured systems                                         |
| topic                   | [system](./example_messages.md#system)             |          | System configuration/startup                                       |
| topic                   | [calls_active](./example_messages.md#calls_active) |          | List of active calls, updated every second                         |
| topic                   | [recorders](./example_messages.md#recorders)       |          | List of all recorders, updated every 3 seconds                     |
| topic                   | [recorder](./example_messages.md#recorder)         |          | Recorder status changes                                            |
| topic                   | [call_start](./example_messages.md#call_start)     |          | New call                                                           |
| topic                   | [call_end](./example_messages.md#call_end)         |          | Completed call                                                     |
| topic/trunk_recorder    | [status](./example_messages.md#plugin_status)      |    ✓     | Plugin status, sent on startup or when the broker loses connection |
| topic/trunk_recorder    | [console](./example_messages.md#console_logs)      |          | Trunk-Recorder console log messages                                |
| unit_topic/shortname    | [call](./example_messages.md#call)                 |          | Channel grants                                                     |
| unit_topic/shortname    | [end](./example_messages.md#end)                   |          | Call end unit information\*\*                                      |
| unit_topic/shortname    | [on](./example_messages.md#on)                     |          | Unit registration (radio on)                                       |
| unit_topic/shortname    | [off](./example_messages.md#off)                   |          | Unit de-registration (radio off)                                   |
| unit_topic/shortname    | [ackresp](./example_messages.md#ackresp)           |          | Unit acknowledge response                                          |
| unit_topic/shortname    | [join](./example_messages.md#join)                 |          | Unit group affiliation                                             |
| unit_topic/shortname    | [data](./example_messages.md#data)                 |          | Unit data grant                                                    |
| unit_topic/shortname    | [ans_req](./example_messages.md#ans_req)           |          | Unit answer request                                                |
| unit_topic/shortname    | [location](./example_messages.md#location)         |          | Unit location update                                               |
| message_topic/shortname | [messages](./example_messages.md#messages)         |          | Trunking messages                                                  |

\* Some messages have been changed for consistency. Please see links for examples and notes.  
\*\* `end` is not a trunking message, but sent after trunk-recorder ends the call. This can be used to track conventional non-trunked calls.

## Trunk Recorder States

Trunk Recorder uses state definitions to manage call flows, recorder assignment, and demodulator operation. The MQTT plugin will include this information when possible. Below is a summary of these states, but not all may appear in MQTT messages.

**call_state** / **rec_state**
| State | State Type   | Description                                                                                                |
| :---: | ------------ | ---------------------------------------------------------------------------------------------------------- |
|   0   | `MONITORING` | Call: Active - No recorder is assigned - See **mon_state** table                                           |
|   1   | `RECORDING`  | Call: Active - Recorder is assigned<br>Recorder: Assigned to call [Recording] - Demodulating transmissions |
|   2   | `INACTIVE`   | Recorder: Assigned to call [Disconnecting] - Detaching from source and demodulator                         |
|   3   | `ACTIVE`     | Recorder: Assigned to call [Tuned] - Not recording yet                                                     |
|   4   | `IDLE`       | Recorder: Assigned to call [Squelched] - Not recording, has not timed out                                  |
|   6   | `STOPPED`    | Recorder: Not assigned to call - Returning to `AVAILABLE` state                                            |
|   7   | `AVAILABLE`  | Recorder: Not assigned to call - Free for use                                                              |
|   8   | `IGNORE`     | Recorder: Assigned to call [Ignoring] - Ending call after unexpected data on the voice channel             |

**mon_state**
| State | State Type    | Description                                                                                                                      |
| :---: | ------------- | -------------------------------------------------------------------------------------------------------------------------------- |
|   0   | `UNSPECIFIED` | Default state                                                                                                                    |
|   1   | `UNKNOWN_TG`  | Not recording: `recordUnknown` is `false` and talkgroup is not found in the _talkgroup.csv_ (\*not currently implemented)        |
|   2   | `IGNORED_TG`  | Not recording: Talkgroup has the ignore priority (`-1`) set in the _talkgroup.csv_                                               |
|   3   | `NO_SOURCE`   | Not recording: No source exists for the requested voice frequency                                                                |
|   4   | `NO_RECORDER` | Not recording: No recorders are available or talkgroup priority is too low                                                       |
|   5   | `ENCRYPTED`   | Not recording: Encryption indicated by trunking messages or mode field (`E`,`DE`,`TE`) in the _talkgroup.csv_                    |
|   6   | `DUPLICATE`   | Not recording: [multiSite] This call is a duplicate of a prior call                                                              |
|   7   | `SUPERSEDED`  | Not recording: [multiSite] This call is a duplicate of a subsequent call with a site precedence indicated in the _talkgroup.csv_ |

## MQTT Brokers

### Mosquitto MQTT Broker

The [Mosquitto](https://mosquitto.org) MQTT is an easy way to have a local MQTT broker. It can be installed from a lot of package managers.

This broker does not impose a limit on the length of MQTT messages, and will handle packets up to 256 MB in size by default. 

Starting it on a Mac:

```bash
/opt/homebrew/sbin/mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf
```

### NanoMQ

The [NanoMQ](https://nanomq.io) broker is a lightweight alternative, but additional configuration may be required for Trunk Recorder systems with a large number of recorders or heavy call volume. 

This broker **does** impose a limit on the length of MQTT messages, and the `max_packet_size` default of 10 KB may generate `MQTT error [-3]: Disconnected` errors with this plugin. 

```
# #============================================================
# # NanoMQ Broker
# #============================================================

mqtt {
    property_size = 32
    max_packet_size = 10KB
    max_mqueue_len = 2048
    retry_interval = 10s
    keepalive_multiplier = 1.25
...
```
[Editing `/etc/nanomq.conf`](https://nanomq.io/docs/en/latest/config-description/mqtt.html) and increasing the packet size to 100 KB or more should be sufficient for MQTT messages generated by this plugin.

## Docker

The included Dockerfile will allow building a trunk-recorder docker image with this plugin included.

`docker-compose` can be used to automate the build and deployment of this image. In the Docker compose file replace the image line with a build line pointing to the location where this repo has been cloned to.

Docker compose file:

```yaml
version: "3"
services:
  recorder:
    build: ./trunk-recorder-mqtt-status
    container_name: trunk-recorder
    restart: always
    privileged: true
    volumes:
      - /dev/bus/usb:/dev/bus/usb
      - /var/run/dbus:/var/run/dbus
      - /var/run/avahi-daemon/socket:/var/run/avahi-daemon/socket
      - ./:/app
```
