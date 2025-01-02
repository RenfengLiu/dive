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

#include <sstream>
#include <string>

// #include "capture_config.h"
#include "connection.h"
#include "message.h"
#include "socket_connection.h"

namespace Dive
{

namespace
{
inline constexpr char kHostName[] = "127.0.0.1";
inline constexpr char kPortNumber[] = "19999";
}  // namespace

enum class CaptureClientStatus
{
    kSuccess = 0,
    kSocketError = -1,
    kDiveVersionTooOld = -2,
    kInstanceVersionTooOld = -3,
    kLegacyCounterNeedMultiplePasses = -4,
    kUnsupportedCaptureMode = -5,
    kCapureFailed = -6,
};

class CaptureClient
{
public:
    CaptureClient(std::string host = kHostName, std::string port = kPortNumber) :
        m_host(host),
        m_port(port)
    {
        m_dive_icd_capabilities.u32All = 0;
        m_layer_capabilities.u32All = 0;
        m_client = SocketConnection::connectToSocket(host.c_str(), port.c_str());
        if (m_client != nullptr)
        {
            m_initialized = true;
        }
    }

    bool init(std::string host, std::string port)
    {
        m_client = SocketConnection::connectToSocket(host.c_str(), port.c_str());
        if (m_client != nullptr)
        {
            m_initialized = true;
        }
        return m_client != nullptr;
    }

    CaptureClientStatus      handShake();
    CaptureClientStatus      triggerCapture(std::string& capture_file_path,
                                            const CaptureConfig* = nullptr);
    bool                     startCapture(std::string pathToSaveCapture);
    bool                     stopCapture();
    const IcdCapabilities&   GetIcdCapabilities() const { return m_dive_icd_capabilities; }
    const LayerCapabilities& GetLayerCapabilities() const { return m_layer_capabilities; }
    const IcdVersion         GetDiveIcdSpecVersion() const { return m_dive_icd_spec_version; }
    bool                     IsPerfCounterEnabled() const { return m_layer_minor_version >= 4; }
    inline std::string       GetLayerVersionString()
    {
        std::stringstream os;
        os << m_layer_major_version << "." << m_layer_minor_version;
        return os.str();
    }

    inline std::string GetIcdVersionString()
    {
        std::stringstream os;

        os << m_dive_icd_spec_version.versions.version.major << "."
           << m_dive_icd_spec_version.versions.version.minor << "."
           << m_dive_icd_spec_version.versions.revision.revision;
        return os.str();
    }

private:
    bool sendCaptureConfig(const CaptureConfig* capture_config);

    std::string                 m_host;
    std::string                 m_port;
    std::unique_ptr<Connection> m_client;
    bool                        m_initialized;
    uint32_t                    m_layer_major_version = 0;
    uint32_t                    m_layer_minor_version = 0;
    IcdCapabilities             m_dive_icd_capabilities;
    LayerCapabilities           m_layer_capabilities;
    IcdVersion                  m_dive_icd_spec_version{};
};

}  // namespace Dive
