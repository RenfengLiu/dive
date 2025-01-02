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

#pragma once
#include <cstdint>

#include "capture_config.h"
#include "connection.h"
namespace Dive
{

namespace
{
inline constexpr char const kDiveCaptureDirOnInstance[] = "/mnt/developer/ggp/dive/";
inline constexpr char const kDiveCaptureDirOnLocal[] = "/tmp/";

// Communication protocol version:  <Major>.<Minor>
// <Major> is incremented for change that breaks compatibility.
// <Minor> is incremented for all small change that do not break compatibility.
inline constexpr uint32_t kProtocolMajorVersion = 2U;
inline constexpr uint32_t kProtocolMinorVersion = 4U;
// 1.0: Initial veraion;
// 2.0: Add message to configure sqtt perf counters;
// 2.1: Update the message to support both sqtt and legacy counters.
// 2.2: Add message to get layer capabilities.
// 2.3: Add message CAPTURE_CONFIG_DONE.
// 2.4: Enable support for perf counter

}  // namespace

enum class MessageType : uint8_t
{
    HAND_SHAKE = 1,
    TRIGGER_CAPTURE,
    TRIGGER_CAPTURE_DONE,
    START_CAPTURE,
    STOP_CAPTURE,
    GET_CAPTURE_FILE_REQ,
    GET_CAPTURE_FILE_RSP,
    CAPTURE_CONFIG,
    LAYER_CAPABILITIES,
    CAPTURE_CONFIG_DONE,
    UNKNOWN_MESSAGE_TYPE = 0XFF,
};

enum class CaptureConfigStatus : uint32_t
{
    CAPTURE_CONFIG_SUCCESS = 0,
    CAPTURE_CONFIG_FAIL_MULTIPLE_GPA_PASSES = 1,
};

// Capabilities of Dive device extension in the ICD
typedef union IcdCapabilities
{
    struct
    {
        uint32_t support_trigger_capture : 1;
        uint32_t support_capture_sqtt_counters : 1;
        uint32_t reserved : 30;
    };
    uint32_t u32All;
} IcdCapabilities;

typedef union IcdVersion
{
    struct
    {
        union
        {
            struct
            {
                uint32_t major : 16;
                uint32_t minor : 16;
            } version;

            uint32_t dword1;
        };

        union
        {
            struct
            {
                uint32_t revision : 16;
                uint32_t reserved : 16;
            } revision;

            uint32_t dword2;
        };
    } versions;
    uint64_t u64All;
} IcdVersion;

// Capabilities of the Dive capture layer
typedef union LayerCapabilities
{
    struct
    {
        uint32_t support_layer_capabilities : 1;
        uint32_t dive_device_ext_enabled : 1;
        uint32_t support_icd_capture_version : 1;
        uint32_t support_trigger_capture : 1;
        uint32_t support_capture_sqtt_counters : 1;
        uint32_t support_capture_legacy_counters : 1;
        uint32_t support_gpa_lib : 1;
        uint32_t reserved : 25;
    };
    uint32_t u32All;
} LayerCapabilities;

struct HandShakeMessage
{
    uint32_t major_version;
    uint32_t minor_version;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct GetLayerCapabilitiesMessage
{
    bool send(Connection* c);
    bool recv(Connection* c);
};

struct LayerCapabilitiesMessage
{
    IcdCapabilities   m_dive_icd_capabilities;
    IcdVersion        m_dive_icd_spec_version;
    LayerCapabilities m_layer_capabilities;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct TriggerCaptureMessage
{
    bool send(Connection* c);
    bool recv(Connection* c);
};

struct TriggerCaptureMessageDone
{
    std::string path_to_saved_capture;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct StartCaptureMessage
{
    std::string path_to_save_capture;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct StopCaptureMessage
{
    bool send(Connection* c);
    bool recv(Connection* c);
};

struct GetCaptureFileRequest
{
    std::string file_path;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct CaptureConfigMessage
{
    CaptureConfigMessage(const CaptureConfig* capture_config) : config(*capture_config) {}
    CaptureConfigMessage() = default;
    CaptureConfig config;

    inline bool IsCapturingLegacyCounters(CaptureMode capture_mode) const
    {
        return (capture_mode == CaptureMode::kCaptureLegacyCounterPerDraw ||
                capture_mode == CaptureMode::kCaptureLegacyCounterPerRenderPass);
    }

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct CaptureConfigMessageDone
{
    CaptureConfigMessageDone(CaptureConfigStatus status) : capture_config_status(status) {}
    CaptureConfigStatus capture_config_status;

    bool send(Connection* c);
    bool recv(Connection* c);
};

struct GetCaptureFileResponse
{
    std::string file_path;
    uint32_t    file_size;

    bool send(Connection* c);
    bool recv(Connection* c);
};

}  // namespace Dive
