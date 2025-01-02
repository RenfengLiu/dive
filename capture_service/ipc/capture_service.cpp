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

#include "capture_service.h"

#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "../log.h"
#include "socket_connection.h"

namespace Dive
{

namespace
{
static constexpr char kHostName[] = "127.0.0.1";
static constexpr char kPortNumber[] = "19999";

}  // namespace

CaptureService& GetCaptureService()
{
    static CaptureService captureService;
    return captureService;
}

void CaptureService::StartService()
{
    m_service_thread = std::thread([this]() {
        m_server_socket = SocketConnection::createSocket(kHostName, kPortNumber);
        if (m_server_socket == nullptr)
        {
            LOGW("Create socket connection failed");
            return;
        }

        while (m_service_started.test_and_set())
        {
            LOGD("Capture layer waiting to be connected.");
            auto client = m_server_socket->accept(1000);  // 1s timeout.
            if (client != nullptr)
            {
                LOGD("Connection established.");
                ProcessMessage(std::move(client));
                LOGD("Process message done");
            }
        }
    });
}

void CaptureService::ProcessMessage(std::unique_ptr<Connection> client)
{
    MessageType msg_type = MessageType::UNKNOWN_MESSAGE_TYPE;
    while (client->recv(&msg_type, sizeof(msg_type)))
    {
        switch (msg_type)
        {
        case MessageType::HAND_SHAKE:
        {

            break;
        }
        case MessageType::LAYER_CAPABILITIES:
        {

            break;
        }

        case MessageType::CAPTURE_CONFIG:
        {

            break;
        }
        case MessageType::TRIGGER_CAPTURE:
        {

            break;
        }
        case MessageType::START_CAPTURE:
        {

            break;
        }

        case MessageType::STOP_CAPTURE:
        {

            break;
        }

        case MessageType::GET_CAPTURE_FILE_REQ:
        {

            break;
        }

        default: LOGW("Unknown message received"); break;
        }
    }
}


}  // namespace Dive

