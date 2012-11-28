// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/file_path.h"

// Creates a new server, and returns a new named pipe to communicate with it.
HANDLE CreateServer(std::wstring* pipe_number);

// This is the controller process for an upgrade operation.
int CopyCache(const FilePath& output_path, HANDLE pipe, bool copy_to_text);

// This process will only execute commands from the controller.
int RunSlave(const FilePath& input_path, const std::wstring& pipe_number);

// Starts a new process, to generate the files.
int LaunchSlave(CommandLine command_line,
                const std::wstring& pipe_number,
                int version);
