# Trunk Recorder MQTT Status (and Units!) Plugin

This is a plugin for Trunk Recorder that publish the current status over MQTT. External programs can use the MQTT messages to display what is going on.

## Install

1. **Build and install the current version of Trunk Recorder** following these [instructions](https://github.com/robotastic/trunk-recorder/blob/master/docs/INSTALL-LINUX.md). Make sure you do a `sudo make install` at the end to install the Trunk Recorder binary and libaries systemwide. The plugin will be built against these libraries.

2. **Install the Paho MQTT C & C++ Libraries**.

- *Install Paho MQTT C*
```bash
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c

cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON  -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
sudo cmake --build build/ --target install
sudo ldconfig
```

- *Install Paho MQTT C++*
```bash
git clone https://github.com/eclipse/paho.mqtt.cpp
cd paho.mqtt.cpp

cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON  -DPAHO_BUILD_DOCUMENTATION=TRUE -DPAHO_BUILD_SAMPLES=TRUE
sudo cmake --build build/ --target install
sudo ldconfig
```

- Alternatively, if your package manager provides recent Paho MQTT libraries:

```bash
sudo apt install libpaho-mqtt-dev libpaho-mqttpp-dev
```


3. Build and install the plugin:

```bash
mkdir build
cd build
cmake ..
sudo make install
```

## Configure
__Plugin options:__
| Key        | Required | Default Value | Type   | Description                                                  |
|------------| :------: | ------------- | ------ | ------------------------------------------------------------ |
| client_id  |          | tr-status     | string | This is the optional client id to send to MQTT. |
| broker     |    ✓     |   tcp://localhost:1883            | string | The URL for the MQTT Message Broker. It should include the protocol used: **tcp**, **ssl**, **ws**, **wss** and the port, which is generally 1883 for tcp, 8883 for ssl, and 443 for ws. |
| topic      |    ✓     |               | string | This is the base topic to use. The plugin will create subtopics for the different types of status messages. |
| unit_topic |          |               | string | Optional field to enable reporting of unit stats over MQTT. |
| message_topic |          |               | string | Optional field to enable reporting of trunking messages over MQTT. |
| username   |          |               | string | If a username is required for the broker, add it here. |
| password   |          |               | string | If a password is required for the broker, add it here. |
| refresh    |          |        60     | int    | Recorders and configs are normally only sent at startup, this sets the interval this information is refreshed. A value of -1 will disable. |
| qos    |          |        0     | int    | Set the MQTT message [QOS level](https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/qos.html) |

__Trunk-Recorder options:__
| Location | Key        | Required | Default Value | Type   | Description                                                  |
|------------|------------| :------: | ------------- | ------ | ------------------------------------------------------------ |
| [top-level](./config.json) | instance_id |          |   trunk-recorder   | string | If multiple `trunk-recorder`s are reporting to a central location, an `instance_id` can be appended to each MQTT message to identify data origin. |

### Plugin Usage Example
See the included [config.json](./config.json) as an example of how to load this plugin.

```yaml
    "plugins": [
    {
        "name": "MQTT Status",
        "client_id": "tr_status",
        "library": "libmqtt_status_plugin.so",
        "broker": "tcp://io.adafruit.com:1883",
        "topic": "robotastic/feeds",
        "unit_topic": "robotastic/units",
        "username": "robotastic",
        "password": "",
        "refresh": 30,
        "qos": 0
    }]
```
If the plugin cannot be found, or it is being run from a different location, it may be necesarry to supply the full path:
```yaml
        "library": "/usr/local/lib/trunk-recorder/libmqtt_status_plugin.so",
```

### MQTT Messages
| Topic | Sub-Topic | Description |
| ----- | ------- | ----------- |
| topic | rates | Control channel decode rates |
| topic | config | Trunk-recorder config information, sent at `refresh` interval  |
| topic | systems | Configured systems, sent at `refresh` interval |
| topic | calls_active | List of active calls, updated every 1 second|
| topic | recorders | List of system recorders, updated every 3 seconds |
| topic | recoders | Recorder status changes |
| topic | call_start | New calls |
| topic | call_end | Completed calls |
| topic/trunk_recorder | `client_id` | Plugin status message, sent on startup or when the broker loses connection |
| unit_topic/shortname | call | Channel grants |
| unit_topic/shortname | end | Call end unit information\* |
| unit_topic/shortname | on | Unit registration (radio on) |
| unit_topic/shortname | off | Unit degregistration (radio off) |
| unit_topic/shortname | ackresp | Unit acknowledge response |
| unit_topic/shortname | join | Unit group affiliation |
| unit_topic/shortname | data | Unit data grant |
| unit_topic/shortname | ans_req | Unit answer request |
| unit_topic/shortname | location | Unit location update |
| message_topic/shortname | messages | Trunking messages |

\*`end` is not a trunking message, but sent after trunk-recorder ends the call.  This can be used to track conventional non-trunked calls.

### Mosquitto MQTT Broker
The Mosquitto MQTT is an easy way to have a local MQTT broker. It can be installed from a lot of package managers. 


Starting it on a Mac:
```bash
/opt/homebrew/sbin/mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf
```

## Docker

The included Dockerfile will allow buliding a trunk-recorder docker image with this plugin included.

`docker-compose` can be used to automate the build and deployment of this image. In the Docker compose file replace the image line with a build line pointing to the location where this repo has been cloned to.   

Docker compose file:

```yaml
version: '3'
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
