# GFXReconstruct Project Overview

This document provides a high-level overview of the GFXReconstruct project, its architecture, and development conventions.

## Project Purpose

GFXReconstruct is a set of tools for capturing and replaying graphics API calls. It allows developers to record the graphics commands executed by an application and replay them to reconstruct the application's graphics behavior. This is useful for debugging, performance analysis, and regression testing. The project primarily supports Vulkan and D3D12, with experimental support for OpenXR.

## Architecture

The main components of GFXReconstruct are:

*   **Capture Layers/Libraries**:
    *   `VK_LAYER_LUNARG_gfxreconstruct`: A Vulkan layer that intercepts Vulkan API calls.
    *   D3D12 capture libraries for capturing D3D12 application commands.
*   **Command-Line Tools**:
    *   `gfxrecon-replay`: Replays GFXReconstruct capture files.
    *   `gfxrecon-info`: Prints information about GFXReconstruct capture files.
    *   `gfxrecon-compress`: Compresses/decompresses GFXReconstruct capture files.
    *   `gfxrecon-extract`: Extracts SPIR-V binaries from GFXReconstruct capture files.
    *   `gfxrecon-convert`: Converts GFXReconstruct capture files to a JSON Lines format.
    *   `gfxrecon-optimize`: Optimizes capture files for improved replay performance.

## How Vulkan Capture and Replay Works on Android

GFXReconstruct's Vulkan capture and replay on Android involves a sophisticated interplay of a Vulkan layer for capture and a dedicated replay application.

### Vulkan Capture on Android

1.  **Layer Interception:** The core of the capture process is the `VK_LAYER_LUNARG_gfxreconstruct` Vulkan layer. This layer is enabled on the Android device for a specific target application using `adb` commands. When the application makes Vulkan API calls, the layer intercepts them. The primary entry point for this interception is typically `vkGetInstanceProcAddr`, which the layer hooks to redirect calls to its own functions.

2.  **Capture Manager:** A singleton class, `VulkanCaptureManager`, is central to the capture logic. It's initialized when the layer intercepts `vkCreateInstance` and `vkCreateDevice` calls. This manager is responsible for:
    *   **Encoding API Calls:** It encodes the intercepted Vulkan API calls and their parameters into a binary `.gfxr` capture file.
    *   **State Tracking:** The `VulkanStateTracker` component within the capture manager meticulously tracks the state of all Vulkan objects (e.g., buffers, images, command pools, descriptor sets). This ensures that when the capture is replayed, the Vulkan environment can be accurately reconstructed.
    *   **Memory Tracking:** A critical aspect is handling memory modifications. The `PageGuardManager` is employed to detect and record changes to mapped Vulkan memory objects. On Android (and Linux), this often involves using `SIGSEGV` handling or `userfaultfd` to trap memory accesses. When an application modifies mapped memory, these changes are recorded in the capture file to maintain replay fidelity. Different memory tracking modes (`page_guard`, `userfaultfd`, `assisted`, `unassisted`) offer trade-offs in performance and accuracy.

3.  **Capture File Output:** The encoded API calls, state information, and memory modifications are written to a `.gfxr` file, typically stored in a writable location on the Android device (e.g., `/sdcard/Download`).

### Vulkan Replay on Android

1.  **Replay Application:** Replay is performed by a dedicated Android application, `gfxrecon-replay`. This application is installed on the Android device and launched via a Python script (`android/scripts/gfxrecon.py`).

2.  **Entry Point and Loop:** The `android_main` function within `tools/replay/android_main.cpp` serves as the application's entry point. It sets up the replay environment, which includes:
    *   **File Processing:** A `FileProcessor` (or a custom `DiveFileProcessor` in the Dive project) is used to read the `.gfxr` capture file from the device's filesystem.
    *   **Decoding:** A `VulkanDecoder` parses the binary data from the capture file, converting the encoded API calls and parameters back into a usable format.
    *   **Command Consumption:** A `VulkanReplayConsumer` (or a specialized consumer like `DiveVulkanReplayConsumer`) receives the decoded Vulkan commands. This consumer is responsible for executing these commands against the Vulkan API on the Android device, effectively replaying the original application's graphics workload.

3.  **Application Framework:** An `Application` class manages the Android window, surface creation, and user input events. This allows for interactive control during replay, such as pausing, stepping through frames, or generating screenshots.

4.  **Replay Options:** The `gfxrecon.py replay` script provides numerous command-line options to control the replay behavior, including memory translation, screenshot generation, validation layer enablement, and handling of unsupported extensions.

In essence, GFXReconstruct acts as a "man-in-the-middle" during capture, recording every relevant Vulkan interaction, and then faithfully re-executes these interactions during replay within a controlled environment on the Android device.


## Development Conventions

*   **Code Style**: The project uses a `.clang-format` file to enforce a consistent code style.
*   **Contributions**: Contributions are made through pull requests on GitHub. See `CONTRIBUTING.md` for more details.
*   **Build System**: CMake is used for building the project on all platforms.
