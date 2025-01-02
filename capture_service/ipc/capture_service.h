
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

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>

#include "message.h"
#include "../log.h"
#include "connection.h"


namespace Dive
{

class CaptureService
{
public:
    ~CaptureService()
    {
        m_service_started.clear();
        if (m_service_thread.joinable())
        {
            m_service_thread.join();
        }
        m_server_socket = nullptr;
    }




    void StartService();
    void ProcessMessage(std::unique_ptr<Connection> client);
private:

    std::atomic_flag            m_service_started = ATOMIC_FLAG_INIT;
    std::thread                 m_service_thread;
    std::unique_ptr<Connection> m_server_socket;

};

CaptureService& GetCaptureService();

}  // namespace Dive
