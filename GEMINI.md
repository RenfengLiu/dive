# Dive Project Overview

This document provides a high-level overview of the Dive project, its architecture, and development conventions.

## Project Purpose

Dive is a graphics profiler for Android applications that use Vulkan and OpenXR. It provides tools to capture, replay, and analyze graphics command streams, helping developers to identify and debug performance issues.

## Architecture

The Dive project is transitioning to a new architecture based on the `gfxreconstruct` project for capturing and replaying Vulkan applications. The previous layer-based capture system is being deprecated.

The main components of the new architecture are:

*   **`gfxreconstruct`**: Dive uses a fork of the `gfxreconstruct` project, which is included as a subtree in the `third_party/gfxreconstruct` directory. `gfxreconstruct` provides the core functionality for capturing and replaying Vulkan API calls.
*   **`dive_client_cli`**: This command-line tool is the main entry point for interacting with Dive. It provides commands for:
    *   `gfxr_capture`: Capturing a Vulkan application on an Android device.
    *   `gfxr_replay`: Replaying a GFXR capture on an Android device and running various analyses on the captured data.
*   **`host_cli`**: A command-line tool for modifying GFXR files.
*   **Dive UI**: A Qt-based graphical user interface (GUI) for visualizing and analyzing capture data.
*   **`gfxr_ext`**: This directory contains extensions to the `gfxreconstruct` decoding and replay process. These extensions allow Dive to perform custom analysis on the GFXR stream, such as extracting PM4 command buffer data and integrating with RenderDoc.
*   **`gfxr_dump_resources`**: A tool that processes a GFXR file and generates a JSON file that can be used with `gfxreconstruct`'s `--dump-resources` feature. This is useful for extracting resource data from a capture, especially when the capture does not replay correctly.

## Building and Running

### Prerequisites

*   CMake
*   Ninja
*   Qt Framework (version 5.15.2)
*   Android NDK (version 25.2.9519653)
*   Python
*   Mako Templates for Python

### Building the Host Tools (Linux)

```bash
# Assumes QTDIR is set to the Qt installation directory
export CMAKE_PREFIX_PATH=$QTDIR
export PATH=$QTDIR:$PATH
cd <dive_path>
git submodule update --init --recursive
mkdir build
cd build
cmake -GNinja ..
ninja
```

### Building the Android Libraries

```bash
./scripts/build_android.sh Debug
```

### Running the CLI

The main command-line tool is `dive_client_cli`. It can be used to capture, replay, and manage graphics data on Android devices.

```bash
# Example: List connected devices
./build/bin/dive_client_cli --command list_device

# Example: GFXR capture
./build/bin/dive_client_cli --device <device_serial> --command gfxr_capture --package <package_name> --type vulkan

# Example: GFXR replay with GPU timing analysis
./build/bin/dive_client_cli --device <device_serial> --command gfxr_replay --gfxr_replay_file_path <path_to_gfxr_file> --gfxr_replay_run_type gpu_timing
```

## Development Conventions

### Code Style

*   **Formatting**: The project uses `clang-format` version 18.1.8 for code formatting. A script is provided at `scripts/clangformat.sh` to format committed files.
*   **Naming**:
    *   `CamelCase` for class and function names.
    *   `snake_case` for variable names.
    *   `m_` prefix for class member variables.

### Contribution Process

*   Contributions are made through GitHub pull requests.
*   All submissions require review and approval from two Google reviewers.
*   The preferred merge strategy is "Squash and merge" to maintain a linear git history.

## GFXR Replay and Analysis

Dive significantly extends the replay capabilities of `gfxreconstruct` to enable detailed performance analysis. This is primarily achieved through a custom replay consumer and a sophisticated state management system for looping.

### Extending `gfxreconstruct` Replay

Dive injects its own logic into the replay process using a custom `DiveVulkanReplayConsumer` that inherits from `gfxreconstruct`'s `VulkanReplayConsumer`. This allows Dive to:

*   **Intercept Vulkan Calls**: By overriding the `Process_vk*` methods, Dive can execute custom code before and after each Vulkan call during replay.
*   **GPU Performance Analysis**: The `GPUTime` class is a key component that gets called from the `DiveVulkanReplayConsumer`. It injects timestamp queries into the replayed command stream to measure the GPU execution time of command buffers, render passes, and draw calls. This data is then used to generate detailed performance statistics.
*   **Specialized Analyses**: The `--gfxr_replay_run_type` option in `dive_client_cli` enables different analysis modes. This is made possible by the flexible architecture of the custom consumer, which can be adapted to different tasks like dumping PM4 command buffer data or triggering a RenderDoc capture.

### Replay Loop Implementation

The ability to loop a single frame is crucial for performance analysis. Dive's implementation of the replay loop relies on careful state management:

*   **State Markers**: The GFXR capture contains special markers to delineate the end of the setup phase and the end of each frame. The `DiveVulkanReplayConsumer` uses these markers to manage the replay state.
*   **State Snapshot and Restore**: At the end of the setup phase, the consumer takes a "snapshot" of the initial state of resources like fences. At the end of each frame loop, it restores this state, allowing the frame to be replayed again from a known-good starting point.
*   **Deferred Resource Destruction**: To prevent resources created during the setup phase from being destroyed during the loop, they are added to a `deferred_release_list_`.
*   **Resettable Command Buffers**: Dive ensures that all command pools are created with the `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag, which allows command buffers to be re-recorded in each iteration of the loop.
*   **Loop Trigger**: The loop itself is triggered by passing the `--loop-single-frame-count` option to `gfxreconstruct` via the `--gfxr_replay_flags` parameter in `dive_client_cli`.

### Potential Issues with the Frame Loop

While powerful, the frame loop implementation has some limitations that are important to be aware of:

*   **Incomplete State Reset**: The current implementation primarily focuses on resetting fences. Other state (e.g., memory contents, descriptor sets) may not be fully reset, which can lead to rendering errors or crashes on subsequent loop iterations.
*   **Performance Overhead**: To ensure stability, the replay loop uses `vkDeviceWaitIdle` to synchronize the CPU and GPU. This is a heavy-handed approach that can alter the performance characteristics of the application and potentially mask the very performance issues that are being investigated.
*   **Brittleness**: The state-reset logic is complex and may not be robust enough to handle all possible capture scenarios. It has been tested on a limited set of captures and may fail with captures from other applications or drivers.

## Network Communication

The `network` directory contains the components responsible for communication between the Dive host tools (e.g., `dive_client_cli`) and the capture service running on the Android device. It implements a client-server architecture with a custom messaging protocol.

### Core Components

*   **`ISerializable`**: An interface for objects that can be converted to and from a byte buffer. This forms the basis for all network messages.
*   **Messages**: A set of classes that represent the different types of messages that can be sent between the client and server (e.g., `HandshakeRequest`, `Pm4CaptureRequest`). They handle their own serialization and deserialization.
*   **`SocketConnection`**: A platform-independent wrapper for socket operations, supporting both TCP and Unix domain sockets. It abstracts away the low-level details of sending and receiving data.
*   **`IMessageHandler`**: An interface for processing received messages. It defines callbacks for client connections, disconnections, and message handling.
*   **`UnixDomainServer`**: A single-client server that listens on a Unix domain socket. It uses an `IMessageHandler` to process client requests. This is typically used on the Android device.
*   **`TcpClient`**: A TCP client that connects to the server, performs a handshake, and sends requests. It includes a keep-alive mechanism to maintain the connection. This is used by the host tools.

### Communication Protocol

The communication protocol uses a Type-Length-Value (TLV) framing system:

1.  **Type**: A 32-bit integer identifying the message type.
2.  **Length**: A 32-bit integer specifying the size of the payload.
3.  **Value**: The serialized message payload.

This ensures that messages can be reliably sent and received over the stream-based socket connection.

### Workflow

1.  The `UnixDomainServer` starts on the Android device, listening on a Unix domain socket.
2.  The `dive_client_cli` (using `TcpClient`) connects to the server via `adb forward`.
3.  A handshake is performed to ensure version compatibility.
4.  The client sends requests (e.g., to start a capture or download a file).
5.  The server processes the requests using its `IMessageHandler` and sends back responses.
6.  A keep-alive mechanism periodically sends ping/pong messages to ensure the connection is active.

## Gemini Instructions

Always read the content of the file before doing any modification, as the content may have been changed. 
Always not assume the functionality based on the folder name, file name, class name, etc,. always read the code and do  your own analysis.
Always build the code for both host and Android after your modification to make sure there's no build errors.

### PR Review Guidelines

When asked to review a pull request, follow these guidelines:

1.  **Understand the Change**: Read the PR description and comments to understand the purpose of the change.
2.  **Code Style**:
    *   Ensure the code is formatted with `clang-format` version 18.1.8.
    *   Verify that the naming conventions (`CamelCase` for functions/classes, `snake_case` for variables, `m_` prefix for members) are followed.
3.  **Build and Test**:
    *   Verify that the code builds successfully for both the host (Linux/Windows) and Android.
    *   If new features are added, ensure that corresponding tests are also included.
    *   If bug fixes are made, check if tests are added to prevent regressions.
4.  **Code Quality**:
    *   Review the code for clarity, correctness, and efficiency.
    *   Check for potential issues like memory leaks, race conditions, or other bugs.
    *   Ensure that the changes are consistent with the existing architecture and design of the project.
5.  **Provide Feedback**:
    *   Provide clear and constructive feedback.
    *   If you suggest changes, explain why they are needed.
    *   Be respectful and professional in your comments.

## Build
To build the Android gfxr_replay apk, use command `./scripts/build_android.sh`
To build the host tool, run `ninja -C build`

## Read files using `cat` instead of `read_file`

Always read the content of the file before doing any modification, as the content may have been changed. 
Always not assume the functionality based on the folder name, file name, class name, etc,. always read the code and do  your own analysis.
Always build the code for both host and Android after your modification.
Follow the code style defined in ./CONTRIBUTING.md, if not specified, follow the Google C++ code style at https://engdoc.corp.google.com/eng/doc/devguide/cpp/styleguide.md?cl=head and follow the readability at https://g3doc.corp.google.com/company/users/suzhang/readability/snippets/c++.md


Do not use the `read_file` tool! Use `cat` in the command line instead.

## Never modify the whole file in one go

Under no circumstances should you write the whole file while modifying it.
Always apply minimal changes.
If required changes are located in different parts of the file,
or are large or excessive, apply a series of small, targeted changes.
Do not modify large parts of the file at once.


## Do your research before you start and practice example-driven development

Reason about which files could contain useful examples that could help guide you
through the task.
Look at neighboring directories or other potentially related or similar files.
If there are many potential examples, choose a subset arbitrarily.
Draw inspiration from the code structure and idioms that you find there.

Make sure that you have read the contents of several files before you start any
task.

## C++:

*   **Readability Snippets:** Before reviewing, read the C++ readability
    snippets at
    https://g3doc.corp.google.com/company/users/suzhang/readability/snippets/c++.md
    to get the most up-to-date guidelines, as they may change over time.
*   **Scoping:** Use if-scoped variable declarations (e.g., `if
    (absl::optional<ClientInfo> client_info = GetClientInfo(req)) {}`).
*   **`const`:** 'const' on value parameters is not needed in function
    declarations, per go/totw/109.