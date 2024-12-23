/*
Copyright 2023 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "service.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "command_utils.h"
#include "constants.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/health_check_service_interface.h"
#include "log.h"
#include "trace_mgr.h"

ABSL_FLAG(uint16_t, port, 19999, "Server port for the service");

namespace Dive
{

grpc::Status DiveServiceImpl::StartTrace(grpc::ServerContext *context,
                                         const TraceRequest  *request,
                                         TraceReply          *reply)
{
    GetTraceMgr().TriggerTrace();
    GetTraceMgr().WaitForTraceDone();
    reply->set_trace_file_path(GetTraceMgr().GetTraceFilePath());
    return grpc::Status::OK;
}

grpc::Status DiveServiceImpl::TestConnection(grpc::ServerContext *context,
                                             const TestRequest   *request,
                                             TestReply           *reply)
{
    reply->set_message(request->message() + " received.");
    LOGD("TestConnection request received \n");
    return grpc::Status::OK;
}

grpc::Status DiveServiceImpl::RunCommand(grpc::ServerContext     *context,
                                         const RunCommandRequest *request,
                                         RunCommandReply         *reply)
{
    LOGD("Request command %s", request->command().c_str());
    auto result = ::Dive::RunCommand(request->command());
    if (result.ok())
    {
        reply->set_output(*result);
    }

    return grpc::Status::OK;
}

grpc::Status DiveServiceImpl::GetTraceFileMetaData(grpc::ServerContext       *context,
                                                   const FileMetaDataRequest *request,
                                                   FileMetaDataReply         *response)
{
    std::string target_file = request->name();
    std::cout << "request get metadata for file " << target_file << std::endl;

    response->set_name(target_file);

    if (!std::filesystem::exists(target_file))
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "");
    }

    int64_t file_size = std::filesystem::file_size(target_file);
    response->set_size(file_size);

    return grpc::Status::OK;
}

grpc::Status DiveServiceImpl::DownloadFile(grpc::ServerContext             *context,
                                           const DownLoadRequest           *request,
                                           grpc::ServerWriter<FileContent> *writer)
{
    std::string target_file = request->name();
    std::cout << "request to download file " << target_file << std::endl;

    if (!std::filesystem::exists(target_file))
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "");
    }
    int64_t file_size = std::filesystem::file_size(target_file);

    FileContent file_content;
    int64_t     total_read = 0;
    int64_t     cur_read = 0;

    std::ifstream fin(target_file, std::ios::binary);
    char          buff[kDownLoadFileChunkSize];
    while (!fin.eof())
    {
        file_content.clear_content();
        cur_read = fin.read(buff, kDownLoadFileChunkSize).gcount();
        total_read += cur_read;
        std::cout << "read " << cur_read << std::endl;
        file_content.set_content(std::string(buff, cur_read));
        writer->Write(file_content);
        if (cur_read != kDownLoadFileChunkSize)
            break;
    }
    std::cout << "Read done, file size " << file_size << ", actually send " << total_read
              << std::endl;
    fin.close();

    if (total_read != file_size)
    {
        std::cout << "file size " << file_size << ", actually send " << total_read << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, "");
    }

    return grpc::Status::OK;
}
std::promise<void>            server_ready_promise;

std::unique_ptr<grpc::Server> g_server;

void RunServer(uint16_t port)
{
    std::string     server_address = absl::StrFormat("0.0.0.0:%d", port);
    DiveServiceImpl service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    builder.RegisterService(&service);
    g_server = builder.BuildAndStart();
    LOGI("Server listening on %s", server_address.c_str());
    g_server->Wait();
}

void StopServer()
{
    if (g_server)
    {
        std::future<void> server_ready_future = server_ready_promise.get_future();

        LOGI("g_server wait server finish starting \n");
        // Wait for the server to be ready
        server_ready_future.wait();
        LOGI("g_server  server  started \n");

        LOGI("g_server begin shutdown\n");
        g_server->Shutdown();
        LOGI("g_server  shutdown done, start to wait\n");

        g_server->Wait();
        LOGI("g_server  wait done\n");
    }
    g_server = nullptr;
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

void server_start()
{
    RunServer(absl::GetFlag(FLAGS_port));
}
void server_stop()
{
    StopServer();
}

int server_main()
{
    RunServer(absl::GetFlag(FLAGS_port));
    return 0;
}

ServerRunner::ServerRunner()
{
    Dive::server_start();
}

ServerRunner::~ServerRunner()
{
    LOGI("Wait for server thread to join");
    Dive::server_stop();
}

ServerRunner &GetServerRunner()
{
    static ServerRunner runner;
    return runner;
}

}  // namespace Dive



#include <atomic>

// static std::atomic<bool> initialized(false); // Initialization guard
 int my_global_var = 0;


// extern "C" {
// __attribute__((constructor)) void _layer_keep_alive_func__();
// }

#include <dlfcn.h>
class keep_alive_struct {
 public:
  keep_alive_struct();

  private:
  std::thread worker;
};

keep_alive_struct::keep_alive_struct() {
//   Dl_info info;
//   if (dladdr((void*)&_layer_keep_alive_func__, &info)) 
  {
    // LOGI("info.dli_fname %s \n", info.dli_fname);
    // void* handle = dlopen(info.dli_fname, RTLD_NODELETE);
    void* handle = dlopen("data/local/tmp/libservice.so", RTLD_NODELETE);
    int *global_var_ptr = (int *)dlsym(handle, "my_global_var");
    if(global_var_ptr) {
        LOGI("global_var_ptr is %d\n", *global_var_ptr );
        if(*global_var_ptr == 0){
            LOGI("global_var_ptr is 0\n");
                *global_var_ptr = 1;
                // Dive::GetServerRunner();  
                worker = std::thread(Dive::server_main);
                worker.detach();
        }
        
    }
    else {
        LOGI("global_var_ptr is null\n");
    }

  }


//    if (!initialized.exchange(true)) {
//     DiveLayer::GetServerRunner();
//    }
}

 keep_alive_struct d;
// (void)d;


// extern "C" {
// // _layer_keep_alive_func__ is marked __attribute__((constructor))
// // this means on .so open, it will be called. Once that happens,
// // we create a struct, which on Android and Linux, forces the layer
// // to never be unloaded. There is some global state in perfetto
// // producers that does not like being unloaded.
// void _layer_keep_alive_func__() {
//   keep_alive_struct d;
//   (void)d;
// }
// }
