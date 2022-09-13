/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#pragma once

#include <atomic>
#include <functional>

#include "AudioPlayer.h"
#include "logger.h"
#include "WebSockets/WSEndpoint.h"
#include "WebSockets/PingPong/PingPongEnabled.h"
#include "WebSockets/Roles/Client.h"
#include "WebSockets/CommunicationInterface/BinaryInterface.h"
#include "WebSockets/ConnectionInitializationResult.h"

#define ENABLE_TIMESTAMP_DETECTION 1
#if ENABLE_TIMESTAMP_DETECTION
#include <endian.h>
#include <inttypes.h>
#endif

#include <sys/prctl.h>
#define PR_SET_FTRACE_MARKER 99

static void prctl_trace_printk(const char *format, ...)
{
#define MAX_DEBUG_LOG_BUFF_SIZE 64
 va_list args;
 va_start(args, format);

 char gDebugPrintBuffer[MAX_DEBUG_LOG_BUFF_SIZE];
 vsnprintf(gDebugPrintBuffer, MAX_DEBUG_LOG_BUFF_SIZE, format, args);
 gDebugPrintBuffer[(MAX_DEBUG_LOG_BUFF_SIZE-1)] = 0;
 va_end(args);
 prctl(PR_SET_FTRACE_MARKER, gDebugPrintBuffer, 0, 0, 0);
}

#define TRACE_PRINTK(fmt, ...) prctl_trace_printk(fmt, ##__VA_ARGS__)

#define MIN_GAP_BETWEEN_PKTS_IN_MS	17
#define DISCONTINUITY_IN_MS	43
static std::chrono::time_point<std::chrono::steady_clock> g_inittime;

static int gpio_set_sysfs_str(const char *path, const char *val)
{
    int fd;
    int bytes;
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        close(fd);
        return 0;
    } else {
        SAPLOG_INFO("unable to open file %s,err: %s", path, strerror(errno));
    }
    return -1;
}

static int beepTestGPIOTrigger(bool enable) {
    static bool gpioConfigured =  false;
    if(gpioConfigured != true) {
        gpio_set_sysfs_str("/sys/class/gpio/export", "459");
        gpio_set_sysfs_str("/sys/class/gpio/gpio459/direction", "out");
        gpio_set_sysfs_str("/sys/class/gpio/gpio459/value", "0");
        gpioConfigured = true;
    }

    if(enable) {
        gpio_set_sysfs_str("/sys/class/gpio/gpio459/value", "1");
        SAPLOG_INFO("%s: DDD ########## GPIO 459 1 \n", __FUNCTION__);
    }
    else {
        gpio_set_sysfs_str("/sys/class/gpio/gpio459/value", "0");
        SAPLOG_INFO("%s: DDD ########## GPIO 459 0 \n", __FUNCTION__);
    }

    return 0;
}

static void cherryaid_write_wav(const char* filename, const char* data, int len) {
   FILE* fp = fopen(filename, "a");
   if (!fp) {
        fprintf(stderr, "Couldn't open the file \"%s\"\n", filename);
        return;
    }
    fwrite(data, len, 1, fp);
    fclose(fp);
}

namespace impl {

template <template <typename, typename> typename Encryption>
class WebSocketClientImpl
{
public:
    explicit WebSocketClientImpl(AudioPlayer* player);
    void connect(const std::string& uri);
    void disconnect();

    WebSockets::WSEndpoint<
        WebSockets::Client,
        WebSockets::BinaryInterface,
        WebSockets::PingPongEnabled,
        Encryption
    > wsClient_;
private:
    WebSocketClientImpl() = delete;
    WebSocketClientImpl(const WebSocketClientImpl&) = delete;
    WebSocketClientImpl& operator=(const WebSocketClientImpl&) = delete;

    void onServiceConnection(WebSockets::ConnectionInitializationResult result);
    void onServiceDisconnected();
    void onMessage(const std::string& msg);
    std::string removeProtocol(const std::string& uri) const;
    std::string ensureAddressHasPortNumber(std::string address) const;

    std::atomic_bool connected_{false};
    AudioPlayer* player_;
    const std::string defaultAudioPort_{"40001"};
};

template <template <typename, typename> typename Encryption>
WebSocketClientImpl<Encryption>::WebSocketClientImpl(AudioPlayer* player)
    : player_{player}
{
    wsClient_.setOnMessageHandler(std::bind(&WebSocketClientImpl<Encryption>::onMessage, this, std::placeholders::_1));
}

template <template <typename, typename> typename Encryption>
void WebSocketClientImpl<Encryption>::connect(const std::string& uri)
{
    if(connected_)
    {
        SAPLOG_INFO("Websocket already connected.disconnect first.");
        return;
    }

    const auto& address = ensureAddressHasPortNumber(removeProtocol(uri));

    SAPLOG_INFO("Trying to connect to: %s", address.c_str());
    wsClient_.connect(address, std::bind(&WebSocketClientImpl<Encryption>::onServiceConnection, this, std::placeholders::_1),
        std::bind(&WebSocketClientImpl<Encryption>::onServiceDisconnected, this));
}

template <template <typename, typename> typename Encryption>
void WebSocketClientImpl<Encryption>::disconnect()
{
    SAPLOG_INFO("Disconnecting.");
    wsClient_.disconnect();
}

template <template <typename, typename> typename Encryption>
void WebSocketClientImpl<Encryption>::onMessage(const std::string& msg)
{
    const void* in = static_cast<const void*>(msg.c_str());
    size_t len = msg.size();

    const void *buffer = in;
    size_t bytes = len;
    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - g_inittime;

#if ENABLE_TIMESTAMP_DETECTION
    uint64_t *buf = (uint64_t *)buffer;
    uint64_t pktCount = be64toh(buf[0]);
    uint64_t timestamp = be64toh(buf[1]);
    buffer = buffer + 16;
    len -= 16;
    bytes = len;
#endif

    // DD Test: Enable Dump
//    cherryaid_write_wav("/tmp/cherraid_debug.wav", (const char*) buffer, len);

    static bool discontinuityDetected = false;
    /* discontinuity handling section: a discontinuity is defined as a gap between successive packets of DISCONTINUITY_ms. When a discontinuity is detected, the packet is discarded, and all subsequent packets
     * that arrive with 17ms of that last packet are discarded.
     * Normally packets should arrive every 21.333ms, and jitter at cherry side is +/-4ms
     */
    if (discontinuityDetected) {
    	if (((elapsed.count() * 1000) < MIN_GAP_BETWEEN_PKTS_IN_MS) || ((elapsed.count() * 1000) > DISCONTINUITY_IN_MS)) {
#if ENABLE_TIMESTAMP_DETECTION
		TRACE_PRINTK("ERROR: WebSocketClientImpl, discontinuity recovery, gap from last pkt=%.6f ms, discarding pkt=%" PRIu64 " @ time= %" PRIu64 " nsecs", (elapsed.count() * 1000), pktCount, timestamp);
		SAPLOG_INFO("ERROR: WebSocketClientImpl, discontinuity recovery, gap from last pkt=%.6f ms, discarding pkt=%" PRIu64 " @ time= %" PRIu64 " nsecs", (elapsed.count() * 1000), pktCount, timestamp);
#else
    		TRACE_PRINTK("ERROR: WebSocketClientImpl, discontinuity recovery, gap from last pkt=%.6f ms ", (elapsed.count() * 1000));
                SAPLOG_INFO("ERROR: WebSocketClientImpl, discontinuity recovery, gap from last pkt=%.6f ms \n", (elapsed.count() * 1000));
#endif
    	} else {
    		/* discontinuity recovery has completed */
    		discontinuityDetected = false;
    		TRACE_PRINTK("WebSocketClientImpl: discontinuity recovery now complete");
                SAPLOG_INFO("WebSocketClientImpl: discontinuity recovery now complete");
    	}
    } else if ((elapsed.count() * 1000) > DISCONTINUITY_IN_MS) {
#if ENABLE_TIMESTAMP_DETECTION
	TRACE_PRINTK("ERROR: WebSocketClientImpl, discontinuity detected, gap from last pkt=%.6f ms, discarding pkt=%" PRIu64 " @ time= %" PRIu64 " nsecs", (elapsed.count() * 1000), pktCount, timestamp);
	SAPLOG_INFO("ERROR: WebSocketClientImpl, discontinuity detected, gap from last pkt=%.6f ms, discarding pkt=%" PRIu64 " @ time= %" PRIu64 " nsecs", (elapsed.count() * 1000), pktCount, timestamp);
#else
    	TRACE_PRINTK("ERROR: WebSocketClientImpl, discontinuity detected, gap from last pkt=%.6f ms ", (elapsed.count() * 1000));
        SAPLOG_INFO("ERROR: WebSocketClientImpl, discontinuity detected, gap from last pkt=%.6f ms\n ", (elapsed.count() * 1000));
#endif
    	discontinuityDetected = true;
    }
    /* update last time value */
    g_inittime = std::chrono::steady_clock::now();

    TRACE_PRINTK("DDD: WebSocketClientImpl, audio clock value =%s ", audioClock.c_str());
    SAPLOG_INFO("DDD: WebSocketClientImpl, audio clock value =%s \n ", audioClock.c_str());
    /* check if in discontinuity */
    if (discontinuityDetected == false) {
        // SAP input beep detection
        // If there are any samples over BEEP_START_VALUE, then there is a beep
        // After reporting a beep, the next reporting need be at least SAMPLES_BEFORE_NEXT_BEEP later
    #define BEEP_START_VALUE 0x100
    // at least 800ms distance between beeps
    #define FRAMES_BEFORE_NEXT_BEEP (800 * 48)
    #define FRAMES_BEFORE_BEEP_OFF (200 * 48)
        {
            static long int frameElapsed = FRAMES_BEFORE_NEXT_BEEP;
            static int beepCount = 0;
            static bool beepON = false;
            static unsigned long long totalBytesProcessed = 0;
    
            if ( (frameElapsed >= FRAMES_BEFORE_BEEP_OFF) && (beepON == true)) {
                beepTestGPIOTrigger(false);
                beepON = false;
            }
    
            if (frameElapsed >= FRAMES_BEFORE_NEXT_BEEP) {
                short *fp = (short *)buffer;
                int index = 0;
                while (index < bytes/4) {
                    if ((fp[0] > BEEP_START_VALUE) || (fp[0] < -BEEP_START_VALUE)) {
                        beepCount++;
                        SAPLOG_INFO("DDD: TRIGGER ###################### %s SAP Inn ==> Number of beeps = %d \n", __func__, beepCount);
                        prctl_trace_printk("SAP_INN");
                        beepTestGPIOTrigger(true);
                        beepON = true;
                        frameElapsed = 0;
                        break;
                    }
                    fp += 2;
                    index++;
                    frameElapsed++;
                }
            } else {
                frameElapsed += bytes/4;
            }
            totalBytesProcessed += bytes;

#if ENABLE_TIMESTAMP_DETECTION
            TRACE_PRINTK("WebSocketClientImpl: rx'd pkt= %" PRIu64 " @ time= %" PRIu64 " nsecs, samples= %zd,  frameElapsed= %zd ", pktCount, timestamp, bytes/4, frameElapsed);
            SAPLOG_INFO("WebSocketClientImpl: rx'd pkt= %" PRIu64 " @ time= %" PRIu64 " nsecs, samples= %zd,  frameElapsed= %zd ", pktCount, timestamp, bytes/4, frameElapsed);
#endif
            TRACE_PRINTK("SAPIn:Total_I/P_Bytes =%llu ", totalBytesProcessed);
            TRACE_PRINTK("SAPIn:Current_I/P_bytes =%d ", bytes);
            SAPLOG_INFO("DDD: SAP In buffer push : Total_Bytes_Processed = %llu Current_Input_bytes_processed =%d ", totalBytesProcessed, bytes);
        }
#if ENABLE_TIMESTAMP_DETECTION
	player_->push_data(buffer, len);
#else
        player_->push_data(in, len);
#endif
    }
    else {
        SAPLOG_INFO("DDD: Network jitter detected. ###################### Drop buffers !!! \n");
    }
}

template <template <typename, typename> typename Encryption>
void WebSocketClientImpl<Encryption>::onServiceConnection(WebSockets::ConnectionInitializationResult result)
{
    if (!result)
    {
        SAPLOG_INFO("Websocket Connection Error.");
        connected_ = false;
        player_->wsConnectionStatus(WSStatus::NETWORKERROR);
    }
    else
    {
        g_inittime = std::chrono::steady_clock::now();
        SAPLOG_INFO("Websocket Connection Established.");
        connected_ = true;
        player_->wsConnectionStatus(WSStatus::CONNECTED);
    }
}

template <template <typename, typename> typename Encryption>
void WebSocketClientImpl<Encryption>::onServiceDisconnected()
{
    SAPLOG_INFO("Websocket Connection Closed.");
    connected_ = false;
    player_->wsConnectionStatus(WSStatus::DISCONNECTED);
}

template <template <typename, typename> typename Encryption>
std::string WebSocketClientImpl<Encryption>::removeProtocol(const std::string& uri) const
{
    const std::string protocolEndSign{"://"};
    const auto& protocolEnd = uri.find(protocolEndSign);
    if(protocolEnd != std::string::npos)
        return uri.substr(protocolEnd + protocolEndSign.size());
    else
        return uri;
}

template <template <typename, typename> typename Encryption>
std::string WebSocketClientImpl<Encryption>::ensureAddressHasPortNumber(std::string address) const
{
    const std::string portNumberBeginSign{":"};
    const auto& portNumberBegin = address.find_first_of(portNumberBeginSign);

    if(std::string::npos == portNumberBegin)
    {
        const std::string portNumberEndSign{"/"};
        const auto& portNumberEnd = address.find_first_of(portNumberEndSign);
        if (std::string::npos != portNumberEnd)
            address.insert(portNumberEnd, ":" + defaultAudioPort_);
        else
            address += ":" + defaultAudioPort_;
    }
    return address;
}

} // namespace impl
