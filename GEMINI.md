gfxr_replay is the app at third_party/gfxreconstruct/tools/replay/

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
