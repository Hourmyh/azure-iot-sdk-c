// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <stdio.h>
#include <stdlib.h>

#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/string_tokenizer.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/http_proxy_io.h"
#include "azure_c_shared_utility/gballoc.h"

#include "iothubtransportmqtt.h"
#include "iothubtransportmqtt_websockets.h"
#include "iothubtransportamqp.h"
#include "iothubtransportamqp_websockets.h"
#include "iothubtransporthttp.h"

#include "iothub_client_hsm_ll.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_security_factory.h"

#include "azure_prov_client/prov_transport_http_client.h"
#include "azure_prov_client/prov_transport_amqp_client.h"
#include "azure_prov_client/prov_transport_amqp_ws_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_transport_mqtt_ws_client.h"

#include "../../../certs/certs.h"

#include "iothub_client_version.h"

static const char* g_conn_string = "<>";
static const char* g_global_prov_uri = "global.azure-devices-provisioning.net";
static const char* g_id_scope = "[ID Scope]";
static const char* g_trusted_cert = NULL;
TICK_COUNTER_HANDLE g_tick_counter_handle;

static const char* MQTT_PROTOCOL_NAME = "MQTT_PROTOCOL";
static const char* HTTP_PROTOCOL_NAME = "HTTP_PROTOCOL";
static const char* AMQP_PROTOCOL_NAME = "AMQP_PROTOCOL";

#ifdef USE_OPENSSL
    static bool g_using_cert = true;
#else
    static bool g_using_cert = false;
#endif // USE_OPENSSL


static bool g_use_proxy = false;
static const char* PROXY_ADDRESS = "127.0.0.1";
HTTP_PROXY_OPTIONS g_http_proxy;

#define PROXY_PORT                  8888
#define MESSAGES_TO_SEND            2
#define TIME_BETWEEN_MESSAGES       2

typedef enum PROTOCOL_TYPE_TAG
{
    PROTOCOL_UNKNOWN,
    PROTOCOL_MQTT,
    PROTOCOL_MQTT_WS,
    PROTOCOL_HTTP,
    PROTOCOL_AMQP,
    PROTOCOL_AMQP_WS
} PROTOCOL_TYPE;

typedef struct MEM_ANALYSIS_INFO_TAG
{
    const char* iothub_version;
    const char* iothub_protocol;
    size_t msg_sent;


} MEM_ANALYSIS_INFO;

typedef struct IOTHUB_CLIENT_SAMPLE_INFO_TAG
{
    int connected;
    int stop_running;
} IOTHUB_CLIENT_INFO;

static const char* get_os_name()
{
#ifdef WIN32
    return "Windows";
#else
    return "Linux";
#endif
}

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context)
{
    (void)message;
    (void)user_context;
    //IOTHUB_CLIENT_INFO* iothub_info = (IOTHUB_CLIENT_INFO*)user_context;
    //(void)printf("Stop message recieved from IoTHub\r\n");
    //iothub_info->stop_running = 1;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void iothub_connection_status(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    if (user_context == NULL)
    {
        (void)printf("iothub_connection_status user_context is NULL\r\n");
    }
    else
    {
        IOTHUB_CLIENT_INFO* iothub_info = (IOTHUB_CLIENT_INFO*)user_context;
        if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
        {
            iothub_info->connected = 1;
        }
        else
        {
            iothub_info->connected = 0;
            iothub_info->stop_running = 1;
        }
    }
}

static void record_memory_usage(const MEM_ANALYSIS_INFO* iot_mem_info)
{
    (void)printf(" IoTHub SDK Version:\t%s\r\n", iot_mem_info->iothub_version);
    (void)printf("   Operating System:\t%s\r\n", get_os_name());
    (void)printf("  Transport in Used:\t%s\r\n", iot_mem_info->iothub_protocol);
    (void)printf("      Messages Sent:\t%d\r\n", (int)iot_mem_info->msg_sent);
    (void)printf("    Max Memory Used:\t%zu\r\n", gballoc_getMaximumMemoryUsed() );
    (void)printf("Current Memory Used:\t%zu\r\n", gballoc_getCurrentMemoryUsed());
    (void)printf(" Num of Allocations:\t%zu\r\n", gballoc_getAllocationCount());
}

static int send_iothub_message(PROTOCOL_TYPE protocol, size_t num_msgs_to_send)
{
    int result;
    IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;
    MEM_ANALYSIS_INFO iot_mem_info;

    gballoc_resetMetrics();
    memset(&iot_mem_info, 0, sizeof(MEM_ANALYSIS_INFO));

    switch (protocol)
    {
        case PROTOCOL_MQTT:
            iothub_transport = MQTT_Protocol;
            iot_mem_info.iothub_protocol = MQTT_PROTOCOL_NAME;
            break;
        case PROTOCOL_MQTT_WS:
            iothub_transport = MQTT_WebSocket_Protocol;
            iot_mem_info.iothub_protocol = MQTT_PROTOCOL_NAME;
            break;
        case PROTOCOL_HTTP:
            iothub_transport = HTTP_Protocol;
            iot_mem_info.iothub_protocol = HTTP_PROTOCOL_NAME;
            break;
        case PROTOCOL_AMQP:
            iothub_transport = AMQP_Protocol;
            iot_mem_info.iothub_protocol = AMQP_PROTOCOL_NAME;
            break;
        case PROTOCOL_AMQP_WS:
            iothub_transport = AMQP_Protocol_over_WebSocketsTls;
            iot_mem_info.iothub_protocol = AMQP_PROTOCOL_NAME;
            break;
        default:
            iothub_transport = NULL;
            result = __LINE__;
            break;
    }

    iot_mem_info.msg_sent = num_msgs_to_send;
    iot_mem_info.iothub_version = IoTHubClient_GetVersionString();

    // Sending the iothub messages
    IOTHUB_CLIENT_LL_HANDLE iothub_client;
    if ((iothub_client = IoTHubClient_LL_CreateFromConnectionString(g_conn_string, iothub_transport) ) == NULL)
    {
        (void)printf("failed create IoTHub client from connection string %s!\r\n", g_conn_string);
        result = __LINE__;
    }
    else
    {
        result = 0;
        IOTHUB_CLIENT_INFO iothub_info;
        tickcounter_ms_t current_tick;
        tickcounter_ms_t last_send_time = TIME_BETWEEN_MESSAGES;
        size_t msg_count = 0;
        iothub_info.stop_running = 0;
        iothub_info.connected = 0;

        (void)IoTHubClient_LL_SetConnectionStatusCallback(iothub_client, iothub_connection_status, &iothub_info);

        //IoTHubClient_LL_SetOption(iothub_client, "logtrace", &g_trace_on);
        if (g_trusted_cert != NULL)
        {
            IoTHubClient_LL_SetOption(iothub_client, OPTION_TRUSTED_CERT, g_trusted_cert);
        }

        if (g_use_proxy)
        {
            IoTHubClient_LL_SetOption(iothub_client, OPTION_HTTP_PROXY, &g_http_proxy);
        }

        if (IoTHubClient_LL_SetMessageCallback(iothub_client, receive_msg_callback, &iothub_info) != IOTHUB_CLIENT_OK)
        {
            (void)printf("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
        }

        do
        {
            if (iothub_info.connected != 0)
            {
                // Send a message every TIME_BETWEEN_MESSAGES seconds
                (void)tickcounter_get_current_ms(g_tick_counter_handle, &current_tick);
                if ((current_tick - last_send_time) / 1000 > TIME_BETWEEN_MESSAGES)
                {
                    static char msgText[1024];
                    sprintf_s(msgText, sizeof(msgText), "{ \"message_index\" : \"%zu\" }", msg_count++);

                    IOTHUB_MESSAGE_HANDLE msg_handle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText));
                    if (msg_handle == NULL)
                    {
                        (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
                    }
                    else
                    {
                        if (IoTHubClient_LL_SendEventAsync(iothub_client, msg_handle, NULL, NULL) != IOTHUB_CLIENT_OK)
                        {
                            (void)printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
                        }
                        else
                        {
                            (void)tickcounter_get_current_ms(g_tick_counter_handle, &last_send_time);
                            (void)printf("IoTHubClient_LL_SendEventAsync accepted message [%zu] for transmission to IoT Hub.\r\n", msg_count);

                        }
                        IoTHubMessage_Destroy(msg_handle);
                    }
                }
            }
            IoTHubClient_LL_DoWork(iothub_client);
            ThreadAPI_Sleep(1);
        } while (iothub_info.stop_running == 0 && msg_count < num_msgs_to_send);

        size_t index = 0;
        for (index = 0; index < 10; index++)
        {
            IoTHubClient_LL_DoWork(iothub_client);
            ThreadAPI_Sleep(1);
        }
        IoTHubClient_LL_Destroy(iothub_client);

        record_memory_usage(&iot_mem_info);
    }
    return result;
}

static int initialize_sdk()
{
    int result;
    if (platform_init() != 0)
    {
        (void)printf("platform_init failed\r\n");
        result = __LINE__;
    }
    else if (gballoc_init() != 0)
    {
        (void)printf("gballoc_init failed\r\n");
        result = __LINE__;
    }
    else if ((g_tick_counter_handle = tickcounter_create()) == NULL)
    {
        (void)printf("tickcounter_create failed\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

int main()
{
    int result;
    if (initialize_sdk() != 0)
    {
        (void)printf("initializing SDK failed\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
        memset(&g_http_proxy, 0, sizeof(HTTP_PROXY_OPTIONS));

        // Setup proper use of helpers if neccessary
        if (g_using_cert)
        {
            g_trusted_cert = certificates;
        }
        else
        {
            g_trusted_cert = NULL;
        }
        if (g_use_proxy)
        {
            g_http_proxy.host_address = PROXY_ADDRESS;
            g_http_proxy.port = PROXY_PORT;
        }

        // Send the mqtt message
        send_iothub_message(PROTOCOL_MQTT, 1);
        send_iothub_message(PROTOCOL_AMQP, 1);

        tickcounter_destroy(g_tick_counter_handle);
        gballoc_deinit();
        platform_deinit();
    }
    return result;
}
