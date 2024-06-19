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

uint32_t GetTriggerFrameNum()
{

#if defined(__ANDROID__)
    uint32_t frame_num = 0;
    char     str_value[64];
    int      len = __system_property_get("dive.trigger_frame_num", str_value);
    if (len > 0)
    {
        frame_num = static_cast<uint32_t>(std::stoul(str_value));
    }

    LOGD("trigger frame at %u", frame_num);

    return frame_num;

#endif

    return 0;
}

}  // namespace DiveLayer