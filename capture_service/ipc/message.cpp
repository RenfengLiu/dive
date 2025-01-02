/*
 * Copyright (C) 2021 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "message.h"

#ifdef _WIN32
#    define _WSPIAPI_EMIT_LEGACY
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <netinet/in.h>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

#include <cstring>
#include <filesystem>

// #include "log.h"

namespace Dive
{
bool HandShakeMessage::send(Connection* c)
{
    return c && c->send(MessageType::HAND_SHAKE) && c->send(htonl(major_version)) &&
           c->send(htonl(minor_version));
}

bool HandShakeMessage::recv(Connection* c)
{
    bool ret = c && c->recv(&major_version, sizeof(major_version));
    if (!ret)
    {
        return false;
    }
    major_version = ntohl(major_version);

    ret = c->recv(&minor_version, sizeof(minor_version));
    if (!ret)
    {
        return false;
    }
    minor_version = ntohl(minor_version);
    return true;
}

bool GetLayerCapabilitiesMessage::send(Connection* c)
{
    return c && c->send(MessageType::LAYER_CAPABILITIES);
}

bool GetLayerCapabilitiesMessage::recv(Connection* c)
{
    (void)(c);
    return true;
}

bool LayerCapabilitiesMessage::send(Connection* c)
{
    return c && c->send(htonl(m_dive_icd_capabilities.u32All)) &&
           c->send(htonl(m_dive_icd_spec_version.versions.dword1)) &&
           c->send(htonl(m_dive_icd_spec_version.versions.dword2)) &&
           c->send(htonl(m_layer_capabilities.u32All));
}

bool LayerCapabilitiesMessage::recv(Connection* c)
{
    if (c == nullptr)
        return false;

    if (!c->recv(&m_dive_icd_capabilities.u32All, sizeof(m_dive_icd_capabilities.u32All)))
        return false;
    m_dive_icd_capabilities.u32All = ntohl(m_dive_icd_capabilities.u32All);

    if (!c->recv(&m_dive_icd_spec_version.versions.dword1,
                 sizeof(m_dive_icd_spec_version.versions.dword1)))
        return false;
    m_dive_icd_spec_version.versions.dword1 = ntohl(m_dive_icd_spec_version.versions.dword1);

    if (!c->recv(&m_dive_icd_spec_version.versions.dword2,
                 sizeof(m_dive_icd_spec_version.versions.dword2)))
        return false;
    m_dive_icd_spec_version.versions.dword2 = ntohl(m_dive_icd_spec_version.versions.dword2);

    if (!c->recv(&m_layer_capabilities.u32All, sizeof(m_layer_capabilities.u32All)))
        return false;
    m_layer_capabilities.u32All = ntohl(m_layer_capabilities.u32All);

    return true;
}

bool TriggerCaptureMessage::send(Connection* c)
{
    return c && c->send(MessageType::TRIGGER_CAPTURE);
}

bool TriggerCaptureMessage::recv(Connection* c)
{
    (void)(c);
    return true;
}

bool TriggerCaptureMessageDone::send(Connection* c)
{
    return c && c->send(MessageType::TRIGGER_CAPTURE_DONE) && c->sendString(path_to_saved_capture);
}

bool TriggerCaptureMessageDone::recv(Connection* c)
{
    return c && c->readString(&path_to_saved_capture);
}

bool StartCaptureMessage::send(Connection* c)
{
    return c && c->send(MessageType::START_CAPTURE) && c->sendString(path_to_save_capture);
}

bool StartCaptureMessage::recv(Connection* c)
{
    return c && c->readString(&path_to_save_capture);
}

bool StopCaptureMessage::send(Connection* c)
{
    return c && c->send(MessageType::STOP_CAPTURE);
}

bool StopCaptureMessage::recv(Connection* c)
{
    (void)(c);
    return true;
}
bool GetCaptureFileRequest::send(Connection* c)
{
    return c && c->send(MessageType::GET_CAPTURE_FILE_REQ) && c->sendString(file_path);
}

bool GetCaptureFileRequest::recv(Connection* c)
{
    return c && c->readString(&file_path);
}

bool CaptureConfigMessage::send(Connection* c)
{
    bool ret = false;

    if (c == nullptr)
        return false;
    ret = c && c->send(config.m_capture_mode);
    if (!ret)
    {
        return false;
    }
    if (config.m_capture_mode == CaptureMode::kCaptureSqttCounter)
    {
        auto& sqtt_counters = config.m_sqtt_counter_config;
        if (!c->send(htonl(sqtt_counters.perf_counters_count)))
            return false;
        for (uint32_t i = 0; i < sqtt_counters.kPerfCounterCount; ++i)
        {
            if (!c->send(htonl(sqtt_counters.counter_indices[i])))
                return false;
            if (!c->send(htonl(sqtt_counters.se_indices[i])))
                return false;
        }

        return true;
    }
    else if (IsCapturingLegacyCounters(config.m_capture_mode))
    {
        uint32_t num_counters = static_cast<uint32_t>(config.m_legacy_counter_config.size());
        if (!c->send(htonl(num_counters)))
            return false;
        for (uint32_t i = 0; i < num_counters; i++)
        {
            if (!c->sendString(config.m_legacy_counter_config[i]))
                return false;
        }
    }

    return true;
}

bool CaptureConfigMessage::recv(Connection* c)
{
    if (c == nullptr)
        return false;
    if (!c->recv(&config.m_capture_mode, sizeof(config.m_capture_mode)))
        return false;
    if (config.m_capture_mode == CaptureMode::kCaptureSqttCounter)
    {
        auto& sqtt_counters = config.m_sqtt_counter_config;
        if (!c->recv(&sqtt_counters.perf_counters_count, sizeof(sqtt_counters.perf_counters_count)))
            return false;
        sqtt_counters.perf_counters_count = ntohl(sqtt_counters.perf_counters_count);

        for (uint32_t i = 0; i < sqtt_counters.perf_counters_count; ++i)
        {
            if (!c->recv(&sqtt_counters.counter_indices[i],
                         sizeof(sqtt_counters.counter_indices[i])))
                return false;
            if (!c->recv(&sqtt_counters.se_indices[i], sizeof(sqtt_counters.se_indices[i])))
                return false;
            sqtt_counters.counter_indices[i] = ntohl(sqtt_counters.counter_indices[i]);
            sqtt_counters.se_indices[i] = ntohl(sqtt_counters.se_indices[i]);
        }
    }
    else if (IsCapturingLegacyCounters(config.m_capture_mode))
    {
        uint32_t num_counters = 0;
        if (!c->recv(&num_counters, sizeof(num_counters)))
        {
            return false;
        }
        num_counters = ntohl(num_counters);
        for (uint32_t i = 0; i < num_counters; i++)
        {
            std::string name;
            if (!c->readString(&name))
                return false;
            config.m_legacy_counter_config.emplace_back(std::move(name));
        }
    }

    return true;
}

bool CaptureConfigMessageDone::send(Connection* c)
{
    return c && c->send(MessageType::CAPTURE_CONFIG_DONE) &&
           c->send(htonl((uint32_t)capture_config_status));
}

bool CaptureConfigMessageDone::recv(Connection* c)
{
    bool ret = c && c->recv(&capture_config_status, sizeof(capture_config_status));
    if (!ret)
    {
        return false;
    }
    capture_config_status = (CaptureConfigStatus)ntohl((uint32_t)capture_config_status);

    return ret;
}

bool GetCaptureFileResponse::send(Connection* c)
{
    bool ret = false;

    ret = c && c->send(MessageType::GET_CAPTURE_FILE_RSP);
    if (!ret)
    {
        return false;
    }
    ret = c->sendString(file_path);
    if (!ret)
    {
        return false;
    }
    ret = c->send(htonl(file_size));
    if (!ret)
    {
        return false;
    }

    return c->sendFile(file_path);
}

bool GetCaptureFileResponse::recv(Connection* c)
{
    bool ret = false;
    ret = c && c->readString(&file_path);
    if (!ret)
    {
        return false;
    }
    ret = c->recv(&file_size, sizeof(file_size));
    if (!ret)
    {
        return false;
    }
    file_size = ntohl(file_size);
    std::filesystem::path capture_path(file_path);
    auto                  name = capture_path.filename();
    file_path = (std::filesystem::temp_directory_path() / name).string();
    return c->receiveFile(file_path, file_size);
}

}  // namespace Dive