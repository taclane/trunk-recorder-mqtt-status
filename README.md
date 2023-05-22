# Trunk Recorder MQTT Status (and Units!) Plugin

This is a plugin for Trunk Recorder that publish the current status over MQTT. External programs can use the MQTT messages to display what is going on.

## Install

1. **Build and install the current version of Trunk Recorder** following these [instructions](https://github.com/robotastic/trunk-recorder/blob/master/docs/INSTALL-LINUX.md). Make sure you do a `sudo make install` at the end to install the Trunk Recorder binary and libaries systemwide. The plugin will be built against these libraries.

2. Now, **install the Paho MQTT C & C++ Libraries**. The full documentation for that is [here](https://github.com/eclipse/paho.mqtt.cpp#unix-and-linux)... but the basic commands are as follows:

*Install Paho MQTT C*
```bash
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c

cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON  -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
sudo cmake --build build/ --target install
sudo ldconfig
```

*Install Paho MQTT C++*
```bash
git clone https://github.com/eclipse/paho.mqtt.cpp
cd paho.mqtt.cpp

cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON  -DPAHO_BUILD_DOCUMENTATION=TRUE -DPAHO_BUILD_SAMPLES=TRUE
sudo cmake --build build/ --target install
sudo ldconfig
```

3. Build and install the plugin:

```bash
mkdir build
cd build
cmake ..
sudo make install
```

## Configure

| Key       | Required | Default Value | Type   | Description                                                  |
| --------- | :------: | ------------- | ------ | ------------------------------------------------------------ |
| broker    |    ✓     |   tcp://localhost:1883            | string | The URL for the MQTT Message Broker. It should include the protocol used: **tcp**, **ssl**, **ws**, **wss** and the port, which is generally 1883 for tcp, 8883 for ssl, and 443 for ws. |
| topic     |    ✓     |               | string | This is the base topic to use. The plugin will create subtopics for the different types of status messages. |
| unit_topic|          |               | string | Optional field to enable reporting of unit stats over MQTT. |
| username  |          |               | string | If a username is required for the broker, add it here. |
| password  |          |               | string | If a password is required for the broker, add it here. |
| refresh   |          |        60     | int    | Recorders and configs are normally only sent at startup, this sets the interval this information is refreshed. A value of -1 will disable. |


### Plugin Object Example
See the included [config.json](./config.json) as an example of how to load this plugin.

```yaml
    "plugins": [
    {
        "name": "mqtt status",
        "library": "libmqtt_status_plugin.so",
        "broker": "tcp://io.adafruit.com:1883",
        "topic": "robotastic/feeds",
        "unit_topic": "robotastic/units",
        "username": "robotastic",
        "password": "",
        "refresh": 30
    }]
```

### MQTT Messages
| Topic | Message | Description |
| ----- | ------- | ----------- |
| topic | rates | control channel decode rates |
| topic | config | trunk-recorder config information, sent at `refresh` interval  |
| topic | systems | configured systems, sent at `refresh` interval |
| topic | calls_active | list of active calls|
| topic | recorders | list of system recorders, sent at `refresh` interval |
| topic | recoders | recorder updates |
| topic | call_start | new calls |
|topic  | call_end | completed calls |
| unit_topic/shortname | call | channel grants |
| unit_topic/shortname | end | call end unit information * |
| unit_topic/shortname | on | unit registration |
| unit_topic/shortname | off | unit regregistration |
| unit_topic/shortname | ackresp | unit acknowledge response |
| unit_topic/shortname | join | unit group affiliation |
| unit_topic/shortname | data | unit data grant |
| unit_topic/shortname | ans_req | uit answer request |
| unit_topic/shortname | location | unit location update |

*`end` is not a trunking message, but sent after trunk-recorder ends the call.  This can be used to track conventional non-trunked calls.

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
