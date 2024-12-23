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

#include "layer_common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "capture_service/log.h"
#include "capture_service/server.h"

namespace DiveLayer
{

bool IsLibwrapLoaded()
{
    bool loaded = false;
#if defined(__ANDROID__)
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps)
    {
        return loaded;
    }

    char  *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, maps) > 0)
    {
        if (strstr(line, "libwrap.so"))
        {
            loaded = true;
            break;
        }
    }
    free(line);
    fclose(maps);
#endif
    return loaded;
}

}  // namespace DiveLayer


// #include <atomic>

// // static std::atomic<bool> initialized(false); // Initialization guard
//  int my_global_var = 0;


// extern "C" {
// __attribute__((constructor)) void _layer_keep_alive_func__();
// }

// #include <dlfcn.h>
// class keep_alive_struct {
//  public:
//   keep_alive_struct();
// };

// keep_alive_struct::keep_alive_struct() {
//   Dl_info info;
//   if (dladdr((void*)&_layer_keep_alive_func__, &info)) {
//     LOGI("info.dli_fname %s \n", info.dli_fname);
//     void* handle = dlopen(info.dli_fname, RTLD_NODELETE);
//     int *global_var_ptr = (int *)dlsym(handle, "my_global_var");
//     if(global_var_ptr) {
//         if(*global_var_ptr == 0){
//             LOGI("global_var_ptr is 0");
//                 *global_var_ptr = 1;
//                 // DiveLayer::GetServerRunner();  
//         }
        
//     }
//     else {
//         LOGI("global_var_ptr is null");
//     }

//   }


// //    if (!initialized.exchange(true)) {
// //     DiveLayer::GetServerRunner();
// //    }
// }


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

extern "C" {
__attribute__((constructor)) void _load_service_func__();
}

#include <dlfcn.h>
class load_service_struct {
 public:
  load_service_struct();
};

void (*OnNewFrame)() = NULL;
void (*TriggerTrace)() = NULL;
void (*WaitForTraceDone)() = NULL;
const char* (*GetTraceFilePath)() = NULL;


load_service_struct::load_service_struct() {
    void* handle = dlopen("/data/local/tmp/libservice.so", RTLD_NODELETE);
    if(handle == NULL) {
        LOGI("LOAD libservice failed");
    }
    else {
        LOGI("LOAD libservice success");
       OnNewFrame = reinterpret_cast<void (*)()>( dlsym(handle, "OnNewFrame"));
       TriggerTrace =  reinterpret_cast<void (*)()>(dlsym(handle, "TriggerTrace"));
       WaitForTraceDone =  reinterpret_cast<void (*)()>(dlsym(handle, "WaitForTraceDone"));
       GetTraceFilePath = reinterpret_cast<const char* (*)()>( dlsym(handle, "GetTraceFilePath"));
    }
}

extern "C" {
// _layer_keep_alive_func__ is marked __attribute__((constructor))
// this means on .so open, it will be called. Once that happens,
// we create a struct, which on Android and Linux, forces the layer
// to never be unloaded. There is some global state in perfetto
// producers that does not like being unloaded.
void _load_service_func__() {
  load_service_struct d;
  (void)d;
}
}


