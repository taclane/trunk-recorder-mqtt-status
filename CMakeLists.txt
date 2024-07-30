find_package(eclipse-paho-mqtt-c REQUIRED)
find_package(PahoMqttCpp REQUIRED)
set(PahoMqttC_LIBRARIES eclipse-paho-mqtt-c::paho-mqtt3as)
set(PahoMqttCpp_LIBRARIES PahoMqttCpp::paho-mqttpp3)

add_library(mqtt_status_plugin
MODULE
  mqtt_status_plugin.cc
)

include_directories(
  ${CMAKE_BINARY_DIR}/../
)

 target_link_libraries(mqtt_status_plugin ${PahoMqttC_LIBRARIES} ${PahoMqttCpp_LIBRARIES} trunk_recorder_library ssl crypto ${Boost_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_FILTER_LIBRARIES} ${GNURADIO_DIGITAL_LIBRARIES} ${GNURADIO_ANALOG_LIBRARIES} ${GNURADIO_AUDIO_LIBRARIES} ${GNURADIO_UHD_LIBRARIES} ${UHD_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GNURADIO_OSMOSDR_LIBRARIES}  ${LIBOP25_REPEATER_LIBRARIES} gnuradio-op25_repeater) # gRPC::grpc++_reflection protobuf::libprotobuf)

 if(NOT Gnuradio_VERSION VERSION_LESS "3.8")

    target_link_libraries(mqtt_status_plugin
    gnuradio::gnuradio-analog
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-digital
    gnuradio::gnuradio-filter
    gnuradio::gnuradio-pmt
    ) 

endif()

install(TARGETS mqtt_status_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/trunk-recorder)

