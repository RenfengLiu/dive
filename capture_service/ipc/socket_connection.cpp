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

#include "socket_connection.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

#ifdef _WIN32

#    define _WSPIAPI_EMIT_LEGACY
#    include <winsock2.h>
#    include <ws2tcpip.h>

#else

#    include <errno.h>
#    include <fcntl.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <poll.h>
#    include <stdio.h>
#    include <stdlib.h>
#    include <sys/sendfile.h>
#    include <sys/socket.h>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <sys/un.h>
#    include <unistd.h>

#endif

#include "../log.h"

namespace Dive
{
namespace
{

static const int ACCEPT_ERROR = -1;
static const int ACCEPT_TIMEOUT = -2;
static const int FD_CLOSED = -3;

#ifdef _WIN32
int gWinsockUsageCount = 0;
#endif

template<typename T> size_t clamp_size_t(T val)
{
    return static_cast<size_t>(val > 0 ? val : 0);
}

std::error_code getLastError()
{
#ifdef _WIN32
    return std::error_code{ ::WSAGetLastError(), std::system_category() };
#else
    return std::error_code{ errno, std::system_category() };
#endif
}

const std::string getLastErrorMessage()
{
    return getLastError().message();
}

void close(int fd)
{
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::shutdown(fd, SHUT_RD);
    ::close(fd);
#endif
}

size_t recv(int sockfd, void* buf, size_t len, int flags)
{
#ifdef _WIN32
    return clamp_size_t(::recv(sockfd, static_cast<char*>(buf), static_cast<int>(len), flags));
#else
    return clamp_size_t(::recv(sockfd, buf, len, flags));
#endif
}

size_t send(int sockfd, const void* buf, size_t len, int flags)
{
#ifdef _WIN32
    return clamp_size_t(
    ::send(sockfd, static_cast<const char*>(buf), static_cast<int>(len), flags));
#else
    const size_t result = len;
    while (len > 0u)
    {
        const ssize_t n = ::send(sockfd, buf, len, flags);
        if (n != -1)
        {
            // A signal after some data was transmitted can result in partial send.
            buf = reinterpret_cast<const char*>(buf) + n;
            len -= static_cast<size_t>(n);
        }
        else if (errno == EINTR)
        {
            // A signal occurred before any data was transmitted - retry.
            continue;
        }
        else
        {
            // Other error.
            return 0u;
        }
    }
    return result;
#endif
}

int accept(int sockfd, int timeoutMs)
{
    pollfd socketPollFd;
    socketPollFd.revents = 0;
    socketPollFd.fd = sockfd;
    socketPollFd.events = POLLIN;

    int ret = -1;
#if _WIN32
    ret = WSAPoll(&socketPollFd, 1, timeoutMs);
#else
    ret = poll(&socketPollFd, 1, timeoutMs);
#endif
    if (ret < 0)
    {
        LOGD("Error from poll(): %d", ret);
        return ACCEPT_ERROR;
    }
    else if (ret == 0)
    {
        LOGD("accept timeout.");
        return ACCEPT_TIMEOUT;
    }

    if (socketPollFd.revents != POLLIN || socketPollFd.fd != sockfd)
    {
        if (socketPollFd.revents & POLLHUP)
        {
            LOGD("Error from poll: fd is closed.");
            return FD_CLOSED;
        }
        LOGD("Error from poll: revents %d, fd %d",
                   socketPollFd.revents,
                   (int)socketPollFd.fd);
        return ACCEPT_ERROR;
    }

    return static_cast<int>(::accept(sockfd, nullptr, nullptr));
}

int getaddrinfo(const char*            node,
                const char*            service,
                const struct addrinfo* hints,
                struct addrinfo**      res)
{
    return ::getaddrinfo(node, service, hints, res);
}

void freeaddrinfo(struct addrinfo* res)
{
    ::freeaddrinfo(res);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, size_t optlen)
{
#ifdef _WIN32
    return ::setsockopt(sockfd,
                        level,
                        optname,
                        static_cast<const char*>(optval),
                        static_cast<int>(optlen));
#else
    return ::setsockopt(sockfd, level, optname, optval, optlen);
#endif
}

int socket(int domain, int type, int protocol)
{
    return static_cast<int>(::socket(domain, type, protocol));
}

int listen(int sockfd, int backlog)
{
    return ::listen(sockfd, backlog);
}

int bind(int sockfd, const struct sockaddr* addr, size_t addrlen)
{
#ifdef _WIN32
    return ::bind(sockfd, addr, static_cast<int>(addrlen));
#else
    return ::bind(sockfd, addr, addrlen);
#endif
}

int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
#ifdef _WIN32
    return ::getsockname(sockfd, addr, static_cast<int*>(addrlen));
#else
    return ::getsockname(sockfd, addr, addrlen);
#endif
}

}  // anonymous namespace

SocketConnection::SocketConnection(int socket) : mSocket(socket) {}

SocketConnection::~SocketConnection()
{
    Dive::close(mSocket);
}

size_t SocketConnection::send(const void* data, size_t size)
{
    return Dive::send(mSocket, data, size, 0);
}

size_t SocketConnection::recv(void* data, size_t size)
{
    return Dive::recv(mSocket, data, size, MSG_WAITALL);
}

const std::string SocketConnection::error()
{
    return getLastErrorMessage();
}

void SocketConnection::close()
{
    Dive::close(mSocket);
    mSocket = -1;
}

std::unique_ptr<Connection> SocketConnection::accept(int timeoutMs /* = NO_TIMEOUT */)
{
    int clientSocket = Dive::accept(mSocket, timeoutMs);
    switch (clientSocket)
    {
    case ACCEPT_ERROR:
        LOGW("Failed to accept incoming connection: %s", getLastErrorMessage().c_str());
        return nullptr;
    case ACCEPT_TIMEOUT: LOGD("Timeout accepting incoming connection"); return nullptr;
    case FD_CLOSED: return nullptr;
    default: return std::unique_ptr<Connection>(new SocketConnection(clientSocket));
    }
}

std::unique_ptr<Connection> SocketConnection::createSocket(const char* hostname, const char* port)
{
    // Network initializer to ensure that the network driver is initialized during
    // the lifetime of the create function. If the connection created successfully
    // then the new connection will hold a reference to a networkInitializer
    // struct to ensure that the network is initialized
    NetworkInitializer networkInitializer;

    struct addrinfo* addr;
    struct addrinfo  hints
    {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const int getaddrinfoRes = Dive::getaddrinfo(hostname, port, &hints, &addr);
    if (0 != getaddrinfoRes)
    {
        LOGW("getaddrinfo() failed: %d - %s.",
                     getaddrinfoRes,
                     getLastErrorMessage().c_str());
        return nullptr;
    }
    auto addrDeleter = [](struct addrinfo* ptr) { Dive::freeaddrinfo(ptr); };  // deferred.
    std::unique_ptr<struct addrinfo, decltype(addrDeleter)> addrScopeGuard(addr, addrDeleter);

    const int sock = Dive::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (-1 == sock)
    {
        LOGW("socket() failed: %s.", getLastErrorMessage().c_str());
        return nullptr;
    }
    auto socketCloser = [](const int* ptr) { Dive::close(*ptr); };  // deferred.
    std::unique_ptr<const int, decltype(socketCloser)> sockScopeGuard(&sock, socketCloser);

    const int one = 1;
    if (-1 == Dive::setsockopt(sock,
                               SOL_SOCKET,
                               SO_REUSEADDR
#ifdef _WIN32
#else
                               | SO_REUSEPORT
#endif
                               ,
                               (const char*)&one,
                               sizeof(int)))
    {
        LOGW("setsockopt() failed: %s", getLastErrorMessage().c_str());
        return nullptr;
    }

    if (-1 == Dive::bind(sock, addr->ai_addr, addr->ai_addrlen))
    {
        LOGW("bind() failed: %s.", getLastErrorMessage().c_str());
        return nullptr;
    }
    struct sockaddr_in sin;
    socklen_t          len = sizeof(sin);
    if (-1 == Dive::getsockname(sock, (struct sockaddr*)&sin, &len))
    {
        LOGW("getsockname() failed: %s.", getLastErrorMessage().c_str());
        return nullptr;
    }

    if (-1 == Dive::listen(sock, 10))
    {
        LOGW("listen() failed: %s.", getLastErrorMessage().c_str());
        return nullptr;
    }

    LOGW("Bound on port '%d'", ntohs(sin.sin_port));
    fflush(stdout);  // Force the message for piped readers

    sockScopeGuard.release();
    return std::unique_ptr<Connection>(new SocketConnection(sock));
}

std::unique_ptr<Connection> SocketConnection::connectToSocket(const char* hostname,
                                                              const char* port)
{
    NetworkInitializer networkInitializer;
    struct addrinfo*   addr;
    struct addrinfo    hints
    {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const int getaddrinfoRes = Dive::getaddrinfo(hostname, port, &hints, &addr);
    if (0 != getaddrinfoRes)
    {
        std::string err_msg = getLastErrorMessage();
        LOGW("getaddrinfo() failed: %d - %s.",
                     getaddrinfoRes,
                     getLastErrorMessage().c_str());
        return nullptr;
    }

    auto addrDeleter = [](struct addrinfo* ptr) { Dive::freeaddrinfo(ptr); };  // deferred.
    std::unique_ptr<struct addrinfo, decltype(addrDeleter)> addrScopeGuard(addr, addrDeleter);

    const int sock = Dive::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (-1 == sock)
    {
        LOGW("socket() failed: %s.", getLastErrorMessage().c_str());
        return nullptr;
    }
    auto socketCloser = [](const int* ptr) { Dive::close(*ptr); };  // deferred.
    std::unique_ptr<const int, decltype(socketCloser)> sockScopeGuard(&sock, socketCloser);

    if (connect(sock, (struct sockaddr*)addr->ai_addr, sizeof(struct sockaddr_in)) < 0)
    {
        LOGW("Connection to server failed ");
        return nullptr;
    }

    sockScopeGuard.release();
    return std::unique_ptr<Connection>(new SocketConnection(sock));
}

uint32_t SocketConnection::getFreePort(const char* hostname)
{
    // Network initializer to ensure that the network driver is initialized during
    // the lifetime of the create function. If the connection created successfully
    // then the new connection will hold a reference to a networkInitializer
    // struct to ensure that the network is initialized
    NetworkInitializer networkInitializer;

    struct addrinfo* addr;
    struct addrinfo  hints
    {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const int getaddrinfoRes = Dive::getaddrinfo(hostname, "0", &hints, &addr);
    if (0 != getaddrinfoRes)
    {
        LOGW("getaddrinfo() failed: %d - %s.",
                     getaddrinfoRes,
                     getLastErrorMessage().c_str());
        return 0;
    }
    auto addrDeleter = [](struct addrinfo* ptr) { Dive::freeaddrinfo(ptr); };  // deferred.
    std::unique_ptr<struct addrinfo, decltype(addrDeleter)> addrScopeGuard(addr, addrDeleter);

    const int sock = Dive::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (-1 == sock)
    {
        LOGW("socket() failed: %s.", getLastErrorMessage().c_str());
        return 0;
    }
    auto socketCloser = [](const int* ptr) { Dive::close(*ptr); };  // deferred.
    std::unique_ptr<const int, decltype(socketCloser)> sockScopeGuard(&sock, socketCloser);

    const int one = 1;
    if (-1 == Dive::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(int)))
    {
        LOGW("setsockopt() failed: %s", getLastErrorMessage().c_str());
        return 0;
    }

    if (-1 == Dive::bind(sock, addr->ai_addr, addr->ai_addrlen))
    {
        LOGW("bind() failed: %s.", getLastErrorMessage().c_str());
        return 0;
    }
    struct sockaddr_in sin;
    socklen_t          len = sizeof(sin);
    if (-1 == Dive::getsockname(sock, (struct sockaddr*)&sin, &len))
    {
        LOGW("getsockname() failed: %s.", getLastErrorMessage().c_str());
        return 0;
    }

    if (-1 == Dive::listen(sock, 10))
    {
        LOGW("listen() failed: %s.", getLastErrorMessage().c_str());
        return 0;
    }
    return ntohs(sin.sin_port);
}

bool SocketConnection::sendFile(const std::string& file_name)
{
#ifdef _WIN32
    return false;
#else
    struct stat file_stat
    {};
    if (stat(file_name.c_str(), &file_stat) != 0)
    {
        LOGW("Can not access file %s", file_name.c_str());
        return false;
    }
    ssize_t file_size = file_stat.st_size;
    if (file_size == 0)
    {
        return false;
    }

    int out_fd = open(file_name.c_str(), O_RDONLY);
    if (out_fd == -1)
    {
        LOGW("Failed to open file %s to send to client", file_name.c_str());
        return false;
    }
    off_t offset = 0;

    ssize_t sent = sendfile(mSocket, out_fd, &offset, static_cast<size_t>(file_size));
    ::close(out_fd);
    if (sent <= 0 || sent != file_size)
    {
        LOGW("Send file %s failed", file_name.c_str());
        return false;
    }
    return true;
#endif
}

bool SocketConnection::receiveFile(std::string file_name, size_t file_size)
{
    std::ofstream in_file(file_name, std::ofstream::binary);
    if (!in_file.is_open())
    {
        LOGW("Open file %s failed", file_name.c_str());
        return false;
    }
    const int buf_size = 4096;
    char      buf[buf_size];
    size_t    data_left = file_size;
    size_t    received = 0;

    while (data_left != 0)
    {
        if (data_left <= buf_size)
        {
            received = recv(buf, data_left);
            if (received != data_left)
            {
                return false;
            }
        }
        else
        {
            received = recv(buf, buf_size);
            if (received != buf_size)
            {
                return false;
            }
        }
        in_file.write(buf, static_cast<int>(received));
        data_left -= received;
    }

    return true;
}

SocketConnection::NetworkInitializer::NetworkInitializer()
{
#ifdef _WIN32
    if (gWinsockUsageCount++ == 0)
    {
        WSADATA wsaData;
        int     wsaInitRes = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsaInitRes != 0)
        {
            LOGW("WSAStartup failed with error code: %d", wsaInitRes);
        }
    }
#endif
}

SocketConnection::NetworkInitializer::~NetworkInitializer()
{
#ifdef _WIN32
    if (--gWinsockUsageCount == 0)
    {
        ::WSACleanup();
    }
#endif
}

}  // namespace Dive
