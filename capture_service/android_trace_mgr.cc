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

#include "trace_mgr.h"

#include <string>
#include <thread>

#include "log.h"

extern "C"
{
// void SetCaptureState(int state);
// void SetCaptureName(const char* name, const char* frame_num);
#include <dlfcn.h>
    const static std::string kLibWrapPath = "/data/local/tmp/libwrap.so";
    typedef void             (*set_capture_state)(int state);
    typedef void             (*set_capture_name)(const char* name, const char* frame_num);

    void* get_func(const char* name)
    {
        static void* layer = NULL;
        layer = dlopen(kLibWrapPath.c_str(), RTLD_LAZY);
        if (!layer)
        {
            LOGI("failed to laod layer code");
            return NULL;
        }
        void* func = NULL;
        func = dlsym(layer, name);
        if (!func)
        {
            LOGI("failed to load func address");
        }
        LOGI("get_func %s succeed", name);
        return func;
    }
}

namespace
{
const static std::string kTraceFilePath{ "/data/local/tmp/" };

// void (*SetCaptureState)(int) =
//         (set_capture_state)get_func("SetCaptureState");
// void (*SetCaptureName)(const char* name, const char* frame_num) =
//         (set_capture_name)get_func("SetCaptureName");
}  // namespace

namespace Dive
{

void AndroidTraceManager::TraceByFrame()
{
    void (*SetCaptureState)(int) = (set_capture_state)get_func("SetCaptureState");
    void (*SetCaptureName)(const char* name,
                           const char* frame_num) = (set_capture_name)get_func("SetCaptureName");

    std::string path = kTraceFilePath + "trace-frame";
    std::string num = std::to_string(m_frame_num);
    char        full_path[256];
    sprintf(full_path, "%s-%04u.rd", path.c_str(), m_frame_num);
    SetCaptureName(path.c_str(), num.c_str());
    {
        absl::MutexLock lock(&m_state_lock);
        m_state = TraceState::Triggered;
    }
    SetTraceFilePath(std::string(full_path));
    LOGD("Set capture file path as %s", GetTraceFilePath().c_str());
}

void AndroidTraceManager::TraceByDuration()
{
    void (*SetCaptureState)(int) = (set_capture_state)get_func("SetCaptureState");
    void (*SetCaptureName)(const char* name,
                           const char* frame_num) = (set_capture_name)get_func("SetCaptureName");
    m_trace_num++;
    std::string path = kTraceFilePath + "trace";
    std::string num = std::to_string(m_trace_num);
    char        full_path[256];
    sprintf(full_path, "%s-%04u.rd", path.c_str(), m_trace_num);
    SetCaptureName(path.c_str(), num.c_str());
    {
        absl::MutexLock lock(&m_state_lock);
        m_state = TraceState::Triggered;
    }
    SetTraceFilePath(std::string(full_path));

    {
        absl::MutexLock lock(&m_state_lock);
        SetCaptureState(1);
        m_state = TraceState::Tracing;
    }
    LOGD("Set capture file path as %s", GetTraceFilePath().c_str());

    // TODO: pass in this duration in stead of hard code a number.
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    {
        absl::MutexLock lock(&m_state_lock);
        SetCaptureState(0);
        m_state = TraceState::Finished;
    }
}

void AndroidTraceManager::TriggerTrace()
{
    if (m_frame_num > 0)
    {
        TraceByFrame();
    }
    else
    {
        TraceByDuration();
    }
}

void AndroidTraceManager::OnNewFrame()
{
    m_frame_num++;
    absl::MutexLock lock(&m_state_lock);
    if (ShouldStartTrace())
    {
        OnTraceStart();
    }
    else if (ShouldStopTrace())
    {
        OnTraceStop();
    }
}

void AndroidTraceManager::WaitForTraceDone()
{
    m_state_lock.Lock();
    auto capture_done = [this] { return m_state == TraceState::Finished; };
    m_state_lock.Await(absl::Condition(&capture_done));
    m_state_lock.Unlock();
}

bool AndroidTraceManager::ShouldStartTrace() const
{
#ifndef NDEBUG
    m_state_lock.AssertHeld();
#endif
    return (m_state == TraceState::Triggered);
}

bool AndroidTraceManager::ShouldStopTrace() const
{
#ifndef NDEBUG
    m_state_lock.AssertHeld();
#endif
    return (m_state == TraceState::Tracing &&
            m_frame_num - m_trace_start_frame > GetNumFrameToTrace());
}

void AndroidTraceManager::OnTraceStart()
{
    void (*SetCaptureState)(int) = (set_capture_state)get_func("SetCaptureState");
    void (*SetCaptureName)(const char* name,
                           const char* frame_num) = (set_capture_name)get_func("SetCaptureName");
#ifndef NDEBUG
    m_state_lock.AssertHeld();
#endif
    SetCaptureState(1);
    m_state = TraceState::Tracing;
    LOGI("Triggered at frame %d", m_frame_num);
    m_trace_start_frame = m_frame_num;
}

void AndroidTraceManager::OnTraceStop()
{
    void (*SetCaptureState)(int) = (set_capture_state)get_func("SetCaptureState");
    void (*SetCaptureName)(const char* name,
                           const char* frame_num) = (set_capture_name)get_func("SetCaptureName");
#ifndef NDEBUG
    m_state_lock.AssertHeld();
#endif
    SetCaptureState(0);
    m_state = TraceState::Finished;
    LOGI("Finished at frame %d", m_frame_num);
}

}  // namespace Dive