/*
 * Copyright (C) 2021 Google Inc.
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
#include <string>
#include <vector>

namespace Dive
{

enum class CaptureMode : uint8_t
{
    kCapturePM4Only = 0,
    kCapturePM4AndSqtt = 1,
    kCaptureSqttCounter = 2,
    kCaptureLegacyCounterPerDraw = 3,
    kCaptureLegacyCounterPerRenderPass = 4
};

enum class CounterPreset : uint8_t
{
    kAllShaderStages = 0,
    kVertexShaderStage,
    kPixelShaderStage,
    kComputeShaderStage,
};

struct SqttCounterConfig
{
    static const uint32_t kPerfCounterCount = 16;

    uint32_t perf_counters_count = 0;
    uint32_t counter_indices[kPerfCounterCount] = { 0 };
    uint32_t se_indices[kPerfCounterCount] = { 0 };
};

struct CaptureConfig
{
    bool IsCapturingLegacyCounters() const
    {
        return (m_capture_mode == CaptureMode::kCaptureLegacyCounterPerDraw ||
                m_capture_mode == CaptureMode::kCaptureLegacyCounterPerRenderPass);
    }

    CaptureMode m_capture_mode;

    SqttCounterConfig m_sqtt_counter_config;

    std::vector<std::string> m_legacy_counter_config;
};

// Preset for legacy counters
const std::string kPresetCulling[] = {
    "PrimitivesIn",     "PrimitivesOut",    "CulledZeroAreaPrims",
    "CulledMicroPrims", "OutputPrimsRatio", "CulledZeroAreaAndMicroPrimsRatio",
};
const std::string kPresetAllShaderStage[] = {
    "VSBusyCycles",        "PSBusyCycles",         "VALUBusyPercentage",     "SALUBusyPercentage",
    "WaitCntVMPercentage", "WaitCntExpPercentage", "WaitExpAllocPercentage",
};
const std::string kPresetVertexShaderStage[] = {
    "VSBusyCycles",          "VSVALUBusyPercentage",   "VSSALUBusyPercentage",
    "VSWaitCntVMPercentage", "VSWaitCntExpPercentage", "VSWaitExpAllocPercentage",
};
const std::string kPresetPixelShaderStage[] = {
    "PSBusyCycles",          "PSVALUBusyPercentage",   "PSSALUBusyPercentage",
    "PSWaitCntVMPercentage", "PSWaitCntExpPercentage", "PSWaitExpAllocPercentage",
};
const std::string kPresetComputeShaderStage[] = {
    "CSBusyCycles",          "CSVALUBusyPercentage",   "CSSALUBusyPercentage",
    "CSWaitCntVMPercentage", "CSWaitCntExpPercentage", "CSWaitExpAllocPercentage",
};

}  // namespace Dive
