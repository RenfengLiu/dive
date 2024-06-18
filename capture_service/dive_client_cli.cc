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

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_cat.h"
#include "android_application.h"
#include "device_mgr.h"

enum class Command
{
    kNone,
    kRunAndCapture,
    kCleanup,
};

bool AbslParseFlag(absl::string_view text, Command* command, std::string* error)
{
    if (text == "capture")
    {
        *command = Command::kRunAndCapture;
        return true;
    }
    if (text == "cleanup")
    {
        *command = Command::kCleanup;
        return true;
    }
    if (text.empty())
    {
        *command = Command::kNone;
        return true;
    }
    *error = "unknown value for enumeration";
    return false;
}

std::string AbslUnparseFlag(Command command)
{
    switch (command)
    {
    case Command::kNone: return "";
    case Command::kRunAndCapture: return "capture";
    case Command::kCleanup: return "cleanup";

    default: return absl::StrCat(command);
    }
}

ABSL_FLAG(Command, command, Command::kNone, "list of actions: \n\tcapture \n\t cleanup");
ABSL_FLAG(std::string, device, "", "Device serial");
ABSL_FLAG(std::string, package, "", "Package on the device");
ABSL_FLAG(std::string,
          type,
          "openxr",
          "application type: \n\t`openxr` for OpenXR applications(apk) \n\t.");
ABSL_FLAG(
std::string,
download_path,
".",
"specify the full path to download the capture on the host, default to current directory.");

ABSL_FLAG(uint32_t,
          trigger_frame_num,
          100,
          "specify a frame number that will start to the dive capture");

void print_usage()
{
    std::cout << absl::ProgramUsageMessage() << std::endl;
}

bool run_package(Dive::DeviceManager& mgr, const std::string& app_type, const std::string& package)
{
    std::string serial = absl::GetFlag(FLAGS_device);
    uint32_t    trigger_frame_num = absl::GetFlag(FLAGS_trigger_frame_num);

    if (serial.empty() || package.empty())
    {
        print_usage();
        return false;
    }
    auto dev_ret = mgr.SelectDevice(serial);

    if (!dev_ret.ok())
    {
        std::cout << "Failed to select device " << dev_ret.status().message() << std::endl;
        return false;
    }
    auto dev = *dev_ret;
    auto ret = dev->SetupDevice();
    if (!ret.ok())
    {
        std::cout << "Failed to setup device, error: " << ret.message() << std::endl;
        return false;
    }
    ret = dev->SetTriggerFrameNum(trigger_frame_num);
    if (!ret.ok())
    {
        std::cout << "Failed to setup prop to trigger the capture.";
        return false;
    }
    if (app_type == "openxr")
    {
        ret = dev->SetupApp(package, Dive::ApplicationType::OPENXR_APK);
    }
    else
    {
        print_usage();
        return false;
    }
    if (!ret.ok())
    {
        std::cout << "Failed to setup app, error: " << ret.message() << std::endl;
        return false;
    }

    ret = dev->StartApp();
    if (!ret.ok())
    {
        std::cout << "Failed to start app, error: " << ret.message() << std::endl;
    }

    return ret.ok();
}

bool wait_capture_done(Dive::DeviceManager& mgr, uint32_t trigger_frame_num)
{
    std::string download_path = absl::GetFlag(FLAGS_download_path);
    std::string input;

    char trace_file_path[256];
    sprintf(trace_file_path, "/sdcard/Download/trace-frame-%04u.rd", trigger_frame_num);
    auto dev = mgr.GetDevice();
    if (dev == nullptr)
    {
        std::cout << "dev is null";
        return false;
    }
    auto app = dev->GetCurrentApplication();
    if (app == nullptr)
    {
        std::cout << "app is null";
        return false;
    }
    const int max_loop = 100;
    int       loop = 0;
    while (app->IsRunning())
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (dev->IsFileExistOnDevice(trace_file_path))
        {
            std::cout << std::endl <<"Capture file is ready" << std::endl;
            std::filesystem::path p(trace_file_path);
            std::filesystem::path target_download_path(download_path);
            if (!std::filesystem::exists(target_download_path))
            {
                std::error_code ec;
                if (!std::filesystem::create_directories(target_download_path, ec))
                {
                    std::cout << "error create directory: " << ec << std::endl;
                }
            }
            target_download_path /= p.filename();
            auto ret = mgr.GetDevice()->RetrieveTraceFile(trace_file_path,
                                                          target_download_path.generic_string());
            if (ret.ok())
                std::cout << "Capture saved at " << target_download_path << std::endl;
            else
                std::cout << "Failed to retrieve capture file" << std::endl;

            return ret.ok();
        }
        if (loop++ > max_loop)
        {
            std::cout << "Capture failed, timed out." << std::endl;
            return -1;
        }
    }
    std::cout << "Capture failed, application process exited before taking capture. " << std::endl;

    return -1;
}

bool run_and_capture(Dive::DeviceManager& mgr,
                     const std::string&   app_type,
                     const std::string&   package)
{

    uint32_t trigger_frame_num = absl::GetFlag(FLAGS_trigger_frame_num);

    run_package(mgr, app_type, package);
    wait_capture_done(mgr, trigger_frame_num);

    std::cout << "Press Enter to exit" << std::endl;
    std::string input;
    if (std::getline(std::cin, input))
    {
        std::cout << "Exiting..." << std::endl;
    }

    return true;
}

bool clean_up_app_and_device(Dive::DeviceManager& mgr, const std::string& package)
{
    std::string serial = absl::GetFlag(FLAGS_device);

    if (serial.empty())
    {
        std::cout << "Please run with `--device [serial]` and `--package [package]` options."
                  << std::endl;
        print_usage();
        return false;
    }

    if (package.empty())
    {
        std::cout << "Package not provided. You run run with `--package [package]` options to "
                     "clean up package specific settings.";
    }

    return mgr.Cleanup(serial, package).ok();
}

int main(int argc, char** argv)
{
    absl::SetProgramUsageMessage("Run app with --help for more details");
    absl::ParseCommandLine(argc, argv);
    Command     cmd = absl::GetFlag(FLAGS_command);
    std::string serial = absl::GetFlag(FLAGS_device);
    std::string package = absl::GetFlag(FLAGS_package);
    std::string app_type = absl::GetFlag(FLAGS_type);

    Dive::DeviceManager mgr;
    auto                list = mgr.ListDevice();
    if (list.empty())
    {
        std::cout << "No device connected" << std::endl;
        return 0;
    }

    switch (cmd)
    {

    case Command::kRunAndCapture:
    {
        run_and_capture(mgr, app_type, package);
        break;
    }
    case Command::kCleanup:
    {
        clean_up_app_and_device(mgr, package);
        break;
    }
    case Command::kNone:
    {
        print_usage();
        break;
    }
    }
}
