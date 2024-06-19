#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.8)

project(DiveService)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Proto file


# Generated sources


# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}"
"${CMAKE_SOURCE_DIR}/third_party/grpc/third_party/abseil-cpp/"
)


if(WIN32)
  set(COMMAND_UTILS_SRC "command_utils_win32.cc")
  add_definitions(-DUNICODE -D_UNICODE)
else()
  set(COMMAND_UTILS_SRC "command_utils.cc")
endif()


  add_library(device_mgr 
    device_mgr.cc 
    ${COMMAND_UTILS_SRC} 
    android_application.cc               
    trace_mgr.cc
    android_trace_mgr.cc
  )
  set_property(TARGET device_mgr PROPERTY POSITION_INDEPENDENT_CODE 1)

  target_link_libraries(device_mgr 
    absl::flags
    absl::flags_parse
    absl::algorithm
    absl::base
    absl::status 
    absl::statusor
    absl::strings
  )

  if(ANDROID)
    target_link_libraries(device_mgr log)
  endif()
if(NOT ANDROID)
  add_executable(dive_client_cli dive_client_cli.cc )
  target_link_libraries(dive_client_cli
    absl::flags
    absl::flags_parse
    absl::algorithm
    absl::strings
    absl::base
    absl::status 
    absl::statusor
    device_mgr
    )

    install(TARGETS dive_client_cli DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()