#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for iothub_client_ll_sample_tpm

compileAsC99()

set(memory_analysis_c_files
    memory_analysis.c
    ../../certs/certs.c
)

set(memory_analysis_h_files
)

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

include_directories(.)
include_directories(${IOTHUB_CLIENT_INC_FOLDER})
include_directories(${DEV_AUTH_MODULES_CLIENT_INC_FOLDER})
include_directories(${SHARED_UTIL_INC_FOLDER})
include_directories(${CMAKE_CURRENT_LIST_DIR}/adapters)

add_executable(memory_analysis ${memory_analysis_c_files} ${memory_analysis_h_files})

if(${use_openssl})
    add_definitions(-DUSE_OPENSSL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_OPENSSL")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_OPENSSL")
    if (WIN32)
        target_link_libraries(memory_analysis $ENV{OpenSSLDir}/lib/ssleay32.lib $ENV{OpenSSLDir}/lib/libeay32.lib)
        file(COPY $ENV{OpenSSLDir}/bin/libeay32.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
        file(COPY $ENV{OpenSSLDir}/bin/ssleay32.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
    endif()
endif()

target_link_libraries(memory_analysis
    iothub_client 
    prov_device_ll_client)

if (${use_http})
    target_link_libraries(memory_analysis prov_http_transport)
endif()
if (${use_mqtt})
    target_link_libraries(memory_analysis iothub_client_mqtt_transport iothub_client_mqtt_ws_transport prov_mqtt_transport prov_mqtt_ws_transport)
endif()
if (${use_amqp})
    target_link_libraries(memory_analysis iothub_client_amqp_transport iothub_client_amqp_ws_transport prov_amqp_ws_transport prov_amqp_transport)
endif()

link_security_client(memory_analysis)
linkSharedUtil(memory_analysis)
linkUAMQP(memory_analysis)

