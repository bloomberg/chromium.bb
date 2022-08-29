// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/utils/io/command.h"

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <sstream>
#include <string>

namespace tint::utils {

namespace {

/// Handle is a simple wrapper around the Win32 HANDLE
class Handle {
  public:
    /// Constructor
    Handle() : handle_(nullptr) {}

    /// Constructor
    explicit Handle(HANDLE handle) : handle_(handle) {}

    /// Destructor
    ~Handle() { Close(); }

    /// Move assignment operator
    Handle& operator=(Handle&& rhs) {
        Close();
        handle_ = rhs.handle_;
        rhs.handle_ = nullptr;
        return *this;
    }

    /// Closes the handle (if it wasn't already closed)
    void Close() {
        if (handle_) {
            CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

    /// @returns the handle
    operator HANDLE() { return handle_; }

    /// @returns true if the handle is not invalid
    operator bool() { return handle_ != nullptr; }

  private:
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    HANDLE handle_ = nullptr;
};

/// Pipe is a simple wrapper around a Win32 CreatePipe() function
class Pipe {
  public:
    /// Constructs the pipe
    explicit Pipe(bool for_read) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hread;
        HANDLE hwrite;
        if (CreatePipe(&hread, &hwrite, &sa, 0)) {
            read = Handle(hread);
            write = Handle(hwrite);
            // Ensure the read handle to the pipe is not inherited
            if (!SetHandleInformation(for_read ? read : write, HANDLE_FLAG_INHERIT, 0)) {
                read.Close();
                write.Close();
            }
        }
    }

    /// @returns true if the pipe has an open read or write file
    operator bool() { return read || write; }

    /// The reader end of the pipe
    Handle read;

    /// The writer end of the pipe
    Handle write;
};

bool ExecutableExists(const std::string& path) {
    DWORD type = 0;
    return GetBinaryTypeA(path.c_str(), &type);
}

std::string FindExecutable(const std::string& name) {
    if (ExecutableExists(name)) {
        return name;
    }
    if (ExecutableExists(name + ".exe")) {
        return name + ".exe";
    }
    if (name.find("/") == std::string::npos && name.find("\\") == std::string::npos) {
        char* path_env = nullptr;
        size_t path_env_len = 0;
        if (_dupenv_s(&path_env, &path_env_len, "PATH")) {
            return "";
        }
        std::istringstream path{path_env};
        free(path_env);
        std::string dir;
        while (getline(path, dir, ';')) {
            auto test = dir + "\\" + name;
            if (ExecutableExists(test)) {
                return test;
            }
            if (ExecutableExists(test + ".exe")) {
                return test + ".exe";
            }
        }
    }
    return "";
}

}  // namespace

Command::Command(const std::string& path) : path_(path) {}

Command Command::LookPath(const std::string& executable) {
    return Command(FindExecutable(executable));
}

bool Command::Found() const {
    return ExecutableExists(path_);
}

Command::Output Command::Exec(std::initializer_list<std::string> arguments) const {
    Pipe stdout_pipe(true);
    Pipe stderr_pipe(true);
    Pipe stdin_pipe(false);
    if (!stdin_pipe || !stdout_pipe || !stderr_pipe) {
        Output output;
        output.err = "Command::Exec(): Failed to create pipes";
        return output;
    }

    if (!input_.empty()) {
        if (!WriteFile(stdin_pipe.write, input_.data(), input_.size(), nullptr, nullptr)) {
            Output output;
            output.err = "Command::Exec() Failed to write stdin";
            return output;
        }
    }
    stdin_pipe.write.Close();

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_pipe.write;
    si.hStdError = stderr_pipe.write;
    si.hStdInput = stdin_pipe.read;

    std::stringstream args;
    args << path_;
    for (auto& arg : arguments) {
        args << " " << arg;
    }

    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr,                                // No module name (use command line)
                        const_cast<LPSTR>(args.str().c_str()),  // Command line
                        nullptr,                                // Process handle not inheritable
                        nullptr,                                // Thread handle not inheritable
                        TRUE,                                   // Handles are inherited
                        0,                                      // No creation flags
                        nullptr,                                // Use parent's environment block
                        nullptr,                                // Use parent's starting directory
                        &si,                                    // Pointer to STARTUPINFO structure
                        &pi)) {  // Pointer to PROCESS_INFORMATION structure
        Output out;
        out.err = "Command::Exec() CreateProcess() failed";
        return out;
    }

    stdin_pipe.read.Close();
    stdout_pipe.write.Close();
    stderr_pipe.write.Close();

    struct StreamReadThreadArgs {
        HANDLE stream;
        std::string output;
    };

    auto stream_read_thread = [](LPVOID user) -> DWORD {
        auto* thread_args = reinterpret_cast<StreamReadThreadArgs*>(user);
        DWORD n = 0;
        char buf[256];
        while (ReadFile(thread_args->stream, buf, sizeof(buf), &n, NULL)) {
            auto s = std::string(buf, buf + n);
            thread_args->output += std::string(buf, buf + n);
        }
        return 0;
    };

    StreamReadThreadArgs stdout_read_args{stdout_pipe.read, {}};
    auto* stdout_read_thread =
        ::CreateThread(nullptr, 0, stream_read_thread, &stdout_read_args, 0, nullptr);

    StreamReadThreadArgs stderr_read_args{stderr_pipe.read, {}};
    auto* stderr_read_thread =
        ::CreateThread(nullptr, 0, stream_read_thread, &stderr_read_args, 0, nullptr);

    HANDLE handles[] = {pi.hProcess, stdout_read_thread, stderr_read_thread};
    constexpr DWORD num_handles = sizeof(handles) / sizeof(handles[0]);

    Output output;

    auto res = WaitForMultipleObjects(num_handles, handles, /* wait_all = */ TRUE, INFINITE);
    if (res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + num_handles) {
        output.out = stdout_read_args.output;
        output.err = stderr_read_args.output;
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        output.error_code = static_cast<int>(exit_code);
    } else {
        output.err = "Command::Exec() WaitForMultipleObjects() returned " + std::to_string(res);
    }

    return output;
}

}  // namespace tint::utils
