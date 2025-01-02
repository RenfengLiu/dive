/*
 * Copyright (C) 2020 Google Inc.
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

#include "capture_client.h"

#include <chrono>
#include <iostream>

#include "message.h"
#include "../log.h"

namespace Dive
{

CaptureClientStatus CaptureClient::handShake()
{
    HandShakeMessage msg;
    msg.major_version = kProtocolMajorVersion;
    msg.minor_version = kProtocolMinorVersion;
    if (!msg.send(m_client.get()))
    {
        LOGD("Send message to server failed.");
        return CaptureClientStatus::kSocketError;
    }

    MessageType msg_type = MessageType::UNKNOWN_MESSAGE_TYPE;
    if (!m_client->recv(&msg_type, sizeof(msg_type)) || msg_type != MessageType::HAND_SHAKE)
    {
        LOGD("Receive message from server failed.");
        return CaptureClientStatus::kSocketError;
    }

    HandShakeMessage resp;
    if (!resp.recv(m_client.get()))
    {
        LOGD("Receive message from server failed.");
        return CaptureClientStatus::kSocketError;
    }
    m_layer_major_version = resp.major_version;
    m_layer_minor_version = resp.minor_version;

    if (resp.major_version > kProtocolMajorVersion)
    {
        LOGD("Version mismatch: host tool is too old.");
        return CaptureClientStatus::kDiveVersionTooOld;
    }
    else if (resp.major_version < kProtocolMajorVersion)
    {
        // Host tool might work with older version of the instance, there's more checking later.
        LOGD("Version mismatch: software on instance is too old.");
    }

    // Version 2.2 added support to get the capabilities of the layer.
    if (resp.major_version >= 2 && resp.minor_version >= 2)
    {
        GetLayerCapabilitiesMessage get_capabilities_msg;
        if (!get_capabilities_msg.send(m_client.get()))
        {
            return CaptureClientStatus::kSocketError;
        }

        LayerCapabilitiesMessage layer_capabilities_msg;
        if (!layer_capabilities_msg.recv(m_client.get()))
        {
            return CaptureClientStatus::kSocketError;
        }
        m_dive_icd_capabilities.u32All = layer_capabilities_msg.m_dive_icd_capabilities.u32All;
        m_dive_icd_spec_version.u64All = layer_capabilities_msg.m_dive_icd_spec_version.u64All;
        m_layer_capabilities.u32All = layer_capabilities_msg.m_layer_capabilities.u32All;

        LOGD("m_dive_icd_spec_version %u.%u.%u",
                   m_dive_icd_spec_version.versions.version.major,
                   m_dive_icd_spec_version.versions.version.minor,
                   m_dive_icd_spec_version.versions.revision.revision);

        // Check capacities and versions
        // For ancient ICD doesn't have any dive functionality, the flag |dive_device_ext_enabled|
        // will not be set.
        if (m_layer_capabilities.support_layer_capabilities &&
            (!m_layer_capabilities.dive_device_ext_enabled ||
             !m_layer_capabilities.support_capture_sqtt_counters))
        {
            return CaptureClientStatus::kInstanceVersionTooOld;
        }

        // Only the latest ICD supports capture sqtt counters, so put it as a ICD version check
        // here. For future checking, we can using the |icd_spec_version|, which will be available
        // after version of |2021.A182_RC08|.
        if (m_layer_capabilities.support_layer_capabilities &&
            !m_layer_capabilities.support_capture_sqtt_counters)
        {
            return CaptureClientStatus::kInstanceVersionTooOld;
        }

        // Check the ICD version if we can get the version number, currently minimum version is
        // 0.4.1
        if (m_layer_capabilities.support_layer_capabilities &&
            m_layer_capabilities.support_icd_capture_version &&
            m_dive_icd_spec_version.versions.version.minor < 4 &&
            m_dive_icd_spec_version.versions.revision.revision < 1)
        {
            return CaptureClientStatus::kInstanceVersionTooOld;
        }
    }

    return CaptureClientStatus::kSuccess;
}

bool CaptureClient::sendCaptureConfig(const CaptureConfig* capture_config)
{
    if (!m_client->send(MessageType::CAPTURE_CONFIG))
        return false;

    if (capture_config == nullptr)
    {
        CaptureConfig config;
        config.m_capture_mode = CaptureMode::kCapturePM4AndSqtt;
#ifndef NDEBUG
        LOGD("Capture mode is %d", int(config.m_capture_mode));
#endif
        CaptureConfigMessage msg(&config);
        return msg.send(m_client.get());
    }

    CaptureConfigMessage msg(capture_config);
    return msg.send(m_client.get());
}

CaptureClientStatus CaptureClient::triggerCapture(std::string&         out_capture_file_path,
                                                  const CaptureConfig* capture_config)
{
    if (!m_initialized && !init(kHostName, kPortNumber))
    {
        return CaptureClientStatus::kSocketError;
    }

#if defined(ENABLE_LEGACY_COUNTER) || defined(ENABLE_SQTT_COUNTER)
    if (m_layer_major_version >= 2 && m_layer_minor_version >= 1)
    {
        if (!sendCaptureConfig(capture_config))
            return CaptureClientStatus::kSocketError;
        if (m_layer_minor_version >= 3)
        {
            // TODO_RW: handle the CaptureConfigMessageDone message only for legacy counter for now
            // (this is a temp solution to make old version of host tool compatible)
            if (capture_config->IsCapturingLegacyCounters())
            {
                CaptureConfigMessageDone msg_capture_config_done(
                Dive::CaptureConfigStatus::CAPTURE_CONFIG_SUCCESS);
                MessageType msg_type = MessageType::UNKNOWN_MESSAGE_TYPE;
                if (!m_client->recv(&msg_type, sizeof(msg_type)))
                {
                    return CaptureClientStatus::kSocketError;
                }
                if (msg_type == MessageType::CAPTURE_CONFIG_DONE &&
                    msg_capture_config_done.recv(m_client.get()))
                {
                    if (msg_capture_config_done.capture_config_status ==
                        CaptureConfigStatus::CAPTURE_CONFIG_FAIL_MULTIPLE_GPA_PASSES)
                    {
                        return CaptureClientStatus::kLegacyCounterNeedMultiplePasses;
                    }
                }
            }
        }
    }
    else  // TODO: Find a better way to return an error message to indicate that capturing perf
          // counter is not supported by older version of layer.
    {
        if (capture_config && capture_config->m_capture_mode != CaptureMode::kCapturePM4AndSqtt)
            return CaptureClientStatus::kUnsupportedCaptureMode;
    }
#endif

    TriggerCaptureMessage msg;

    if (!msg.send(m_client.get()))
    {
        return CaptureClientStatus::kSocketError;
    }

#ifndef NDEBUG
    LOGD("Wait for capture done msg.");
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
#endif
    TriggerCaptureMessageDone msgDone;

    MessageType msg_type = MessageType::UNKNOWN_MESSAGE_TYPE;
    if (!m_client->recv(&msg_type, sizeof(msg_type)))
    {
        return CaptureClientStatus::kSocketError;
    }
#ifndef NDEBUG
    LOGD("Time used to generate capture is %f seconds.",
               (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin)
                .count() /
                1000.0));
#endif
    if (msg_type == MessageType::TRIGGER_CAPTURE_DONE && msgDone.recv(m_client.get()))
    {
        out_capture_file_path = msgDone.path_to_saved_capture;
        size_t pos = out_capture_file_path.find(kDiveCaptureDirOnInstance);

        // If capture starts with "/mnt/developer/ggp/dive/", then it is on the gamelet. Copy it to
        // local for Dive to load.
        if (pos != std::string::npos)
        {
            GetCaptureFileRequest req;
            req.file_path = out_capture_file_path;
#ifndef NDEBUG
            LOGD("Begin to copy capture from instance to local");
            begin = std::chrono::steady_clock::now();
#endif
            if (!req.send(m_client.get()))
            {
                LOGW("Request to copy the capture from instance failed");
                return CaptureClientStatus::kSocketError;
            }

            if (!m_client->recv(&msg_type, sizeof(msg_type)))
            {
                return CaptureClientStatus::kSocketError;
            }

            GetCaptureFileResponse resp;
            if (msg_type == MessageType::GET_CAPTURE_FILE_RSP)
            {
                if (!resp.recv(m_client.get()))
                {
                    LOGW("receive file failed");
                }
            }
            else
            {
                LOGW("Unexpected message of type %u received ",
                             static_cast<uint8_t>(msg_type));
            }

            out_capture_file_path = resp.file_path;
#ifndef NDEBUG
            LOGD("Time used to copy file of size %d from instance is %f seconds.",
                       resp.file_size,
                       (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count() /
                        1000.0));
#endif
        }
        LOGD("Capture is at %s", out_capture_file_path.c_str());
        return CaptureClientStatus::kSuccess;
    }
    else
    {
        LOGW("Wait for capture done failed.");
        return CaptureClientStatus::kCapureFailed;
    }
}

bool CaptureClient::startCapture(std::string pathToSaveCapture)
{
    if (!m_initialized && !init(kHostName, kPortNumber))
    {
        return false;
    }

    StartCaptureMessage msg;
    msg.path_to_save_capture = pathToSaveCapture;

    return msg.send(m_client.get());
}

bool CaptureClient::stopCapture()
{
    if (!m_initialized && !init(kHostName, kPortNumber))
    {
        return false;
    }

    StopCaptureMessage msg;

    if (!msg.send(m_client.get()))
    {
        return false;
    }
    m_client = nullptr;
    m_initialized = false;
    return true;
}

}  // namespace Dive
