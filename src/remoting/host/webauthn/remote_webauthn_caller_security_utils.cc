// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"

#include "base/environment.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "remoting/host/base/process_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/win/trust_util.h"
#endif

#else  // !IS_LINUX && !IS_WIN
#include "base/notreached.h"
#endif

namespace remoting {
namespace {

#if !defined(NDEBUG)

// No static variables needed for debug builds.

#elif BUILDFLAG(IS_LINUX)

constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringPieceType>({
        "/opt/google/chrome/chrome",
        "/opt/google/chrome-beta/chrome",
        "/opt/google/chrome-unstable/chrome",
    });

#elif BUILDFLAG(IS_WIN)

// Names of environment variables that store the path to the Program Files or
// the Program Files (x86) directory. We may find Chrome in any of the env vars
// below, depending on the architecture of the Chrome binary, the NMH binary,
// and the OS.
constexpr auto kProgramFilesEnvVars =
    base::MakeFixedFlatSet<base::StringPiece>({
        "PROGRAMFILES",
        "PROGRAMFILES(X86)",
        "ProgramW6432",
    });

// Relative to the Program Files directory.
constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringPieceType>({
        L"Google\\Chrome\\Application\\chrome.exe",
        L"Google\\Chrome Beta\\Application\\chrome.exe",
        L"Google\\Chrome SxS\\Application\\chrome.exe",
        L"Google\\Chrome Dev\\Application\\chrome.exe",
    });

#endif

}  // namespace

bool IsLaunchedByTrustedProcess() {
#if !defined(NDEBUG)
  // Just return true on debug builds for the convenience of development.
  return true;
#elif BUILDFLAG(IS_LINUX)
  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);
  return kAllowedCallerPrograms.contains(parent_image_path.value());
#elif BUILDFLAG(IS_WIN)
  auto environment = base::Environment::Create();

  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);

  // On Windows, Chrome launches native messaging hosts via cmd for stdio
  // communication. See:
  //   chrome/browser/extensions/api/messaging/native_process_launcher_win.cc
  // Therefore, we check if the parent is cmd and skip to the grandparent if
  // that's the case. It's possible to do stdio communications without cmd, so
  // we don't require the parent to always be cmd.

  // COMSPEC is generally "C:\WINDOWS\system32\cmd.exe". Note that the casing
  // does not match the actual file path's casing.
  std::string comspec_utf8;
  if (environment->GetVar("COMSPEC", &comspec_utf8)) {
    base::FilePath::StringType comspec = base::UTF8ToWide(comspec_utf8);
    if (base::FilePath::CompareEqualIgnoreCase(parent_image_path.value(),
                                               comspec)) {
      // Skip to the grandparent.
      base::win::ScopedHandle parent_handle(
          OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, parent_pid));
      if (parent_handle.is_valid()) {
        parent_pid = base::GetParentProcessId(parent_handle.Get());
        parent_image_path = GetProcessImagePath(parent_pid);
      } else {
        PLOG(ERROR) << "Failed to query parent info.";
      }
    }
  } else {
    LOG(ERROR) << "COMSPEC is not set";
  }

  // Check if the caller's image path is allowlisted.
  for (const base::StringPiece& program_files_env_var : kProgramFilesEnvVars) {
    std::string program_files_path_utf8;
    if (!environment->GetVar(program_files_env_var, &program_files_path_utf8)) {
      continue;
    }
    auto program_files_path =
        base::FilePath::FromUTF8Unsafe(program_files_path_utf8);
    if (!program_files_path.IsParent(parent_image_path)) {
      continue;
    }
    for (const base::FilePath::StringPieceType& allowed_caller_program :
         kAllowedCallerPrograms) {
      base::FilePath allowed_caller_program_full_path =
          program_files_path.Append(allowed_caller_program);
      if (base::FilePath::CompareEqualIgnoreCase(
              parent_image_path.value(),
              allowed_caller_program_full_path.value())) {
        // Caller's path is allowlisted, now also check if it's properly signed.
        return IsBinaryTrusted(parent_image_path);
      }
    }
  }
  // Caller's path is not allowlisted.
  return false;
#else  // !IS_LINUX && !IS_WIN
  NOTIMPLEMENTED();
  return true;
#endif
}

}  // namespace remoting
