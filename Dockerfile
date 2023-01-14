FROM robotastic/trunk-recorder:latest

# Build MQTT Stats
RUN apt update && export DEBIAN_FRONTEND=noninteractive && \ 
    apt install -y libpaho-mqtt-dev libpaho-mqtt1.3  libpaho-mqttpp-dev libpaho-mqttpp3-1  && rm -rf /var/lib/apt/lists/*
    
WORKDIR /src/trunk-recorder-mqtt-status

COPY . .

WORKDIR /src/trunk-recorder-mqtt-status/build

RUN cmake .. && make install

WORKDIR /app
