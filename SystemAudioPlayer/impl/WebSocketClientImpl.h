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
#include <chrono>

#include "../utils/cherryaid.h"
#include "../utils/tsm48k.h"

#include "AudioPlayer.h"
#include "logger.h"
#include "WebSockets/WSEndpoint.h"
#include "WebSockets/PingPong/PingPongEnabled.h"
#include "WebSockets/Roles/Client.h"
#include "WebSockets/CommunicationInterface/BinaryInterface.h"
#include "WebSockets/ConnectionInitializationResult.h"

#define MAX_SIZE_PER_CHANNEL 1024
#define MAX_NUM_SAMPLES 768

static std::chrono::time_point<std::chrono::steady_clock> g_inittime;
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
    TSMHandle_t tsmHandle_{NULL};
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
    unsigned int samples_out;
    int16_t leftBuff[MAX_SIZE_PER_CHANNEL] = {0};
    int16_t rightBuff[MAX_SIZE_PER_CHANNEL] = {0};
    int16_t tsmleftBuff[MAX_SIZE_PER_CHANNEL] = {0};
    int16_t tsmrightBuff[MAX_SIZE_PER_CHANNEL] = {0};
    int16_t outBuff[MAX_SIZE_PER_CHANNEL * 2] = {0};
    int16_t tsmOutLen = 0;
    int chbuffiter = 0;
    int outbuffiter = 0;

    std::chrono::duration<double> elpased = std::chrono::steady_clock::now() - g_inittime;
//    const int16_t *in = reinterpret_cast<const int16_t *>(msg.c_str());
    const int16_t *in = static_cast<const int16_t *>(static_cast<const void *>(msg.c_str()));
    size_t len = msg.size();
    size_t samplesPerChannel = len / 4;

    // DD Test
    cherryaid_write_wav("/tmp/cherraid_debug.wav", (const char*) in, len);
    // Split left and right channels for TSM
    for (int mainiter = 0; mainiter < (samplesPerChannel * 2) ; mainiter = mainiter + 2)
    {
        leftBuff[chbuffiter] = in[mainiter];
        rightBuff[chbuffiter] = in[mainiter + 1];
        chbuffiter++;
    }

    cherryaid_new_data_point(samplesPerChannel, elpased.count(), &samples_out);
    float newSamplingRate = 48000 + (samples_out - samplesPerChannel);
    SAPLOG_INFO("SamplesIn %u, samples_out %u, newSamplingRate %f", samplesPerChannel, samples_out, newSamplingRate);

    // TSM block
    const void *out = NULL;
    if (tsmHandle_)
    {
        TSM_COMCAST_setPlayoutSamplingRate(tsmHandle_, newSamplingRate);
        TSM_COMCAST_filter_16(tsmHandle_, leftBuff, samplesPerChannel, tsmleftBuff, &tsmOutLen);
        TSM_COMCAST_filter_16(tsmHandle_, rightBuff, samplesPerChannel, tsmrightBuff, &tsmOutLen);
        for (int iter = 0; iter < /*samplesPerChannel*/tsmOutLen; iter++)
        {
            outBuff[outbuffiter++] = /*leftBuff[iter]*/tsmleftBuff[iter];
            outBuff[outbuffiter++] = /*rightBuff[iter]*/tsmrightBuff[iter];
        }
        out = static_cast<const void *>(outBuff);
    }
    else
    {
        SAPLOG_INFO("CRITICAL Error : TSM not initialized");
        out = static_cast<const void *>(msg.c_str());
    }

    player_->push_data(out, /*samplesPerChannel*/tsmOutLen * 4);
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
	if(!tsmHandle_)
	{
            tsmHandle_ = TSM_COMCAST_create(MAX_NUM_SAMPLES, PCM16);
	}
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
    if(tsmHandle_)
    {
        TSM_COMCAST_delete(tsmHandle_);
	tsmHandle_ = NULL;
    }
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
