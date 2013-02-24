// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/chrome_util.h"

#include <windows.h>
#include <atlbase.h>
#include <shlobj.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/md5.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "google_update/google_update_idl.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kAppUserModelId[] = L"Chrome";
#else  // GOOGLE_CHROME_BUILD
const wchar_t kAppUserModelId[] = L"Chromium";
#endif  // GOOGLE_CHROME_BUILD

#if defined(GOOGLE_CHROME_BUILD)

// TODO(grt): These constants live in installer_util.  Consider moving them
// into common_constants to allow for reuse.
const base::FilePath::CharType kNewChromeExe[] =
    FILE_PATH_LITERAL("new_chrome.exe");
const wchar_t kRenameCommandValue[] = L"cmd";
const wchar_t kChromeAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kRegPathChromeClient[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const int kExitCodeRenameSuccessful = 23;

// Returns the name of the global event used to detect if |chrome_exe| is in
// use by a browser process.
// TODO(grt): Move this somewhere central so it can be used by both this
// IsBrowserRunning (below) and IsBrowserAlreadyRunning (browser_util_win.cc).
string16 GetEventName(const base::FilePath& chrome_exe) {
  static wchar_t const kEventPrefix[] = L"Global\\";
  const size_t prefix_len = arraysize(kEventPrefix) - 1;
  string16 name;
  name.reserve(prefix_len + chrome_exe.value().size());
  name.assign(kEventPrefix, prefix_len);
  name.append(chrome_exe.value());
  std::replace(name.begin() + prefix_len, name.end(), '\\', '!');
  std::transform(name.begin() + prefix_len, name.end(),
                 name.begin() + prefix_len, tolower);
  return name;
}

// Returns true if |chrome_exe| is in use by a browser process. In this case,
// "in use" means past ChromeBrowserMainParts::PreMainMessageLoopRunImpl.
bool IsBrowserRunning(const base::FilePath& chrome_exe) {
  base::win::ScopedHandle handle(::OpenEvent(
      SYNCHRONIZE, FALSE, GetEventName(chrome_exe).c_str()));
  if (handle.IsValid())
    return true;
  DWORD last_error = ::GetLastError();
  if (last_error != ERROR_FILE_NOT_FOUND) {
    AtlTrace("%hs. Failed to open browser event; error %u.\n", __FUNCTION__,
             last_error);
  }
  return false;
}

// Returns true if the file new_chrome.exe exists in the same directory as
// |chrome_exe|.
bool NewChromeExeExists(const base::FilePath& chrome_exe) {
  base::FilePath new_chrome_exe(chrome_exe.DirName().Append(kNewChromeExe));
  return file_util::PathExists(new_chrome_exe);
}

bool GetUpdateCommand(bool is_per_user, string16* update_command) {
  const HKEY root = is_per_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  base::win::RegKey key(root, kRegPathChromeClient, KEY_QUERY_VALUE);

  return key.ReadValue(kRenameCommandValue, update_command) == ERROR_SUCCESS;
}

#endif  // GOOGLE_CHROME_BUILD

// TODO(grt): This code also lives in installer_util.  Refactor for reuse.
bool IsPerUserInstall(const base::FilePath& chrome_exe) {
  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    return !StartsWith(chrome_exe.value().c_str(), program_files_path, false);
  } else {
    NOTREACHED();
  }
  return true;
}

// TODO(gab): This code also lives in shell_util. Refactor for reuse.
string16 ByteArrayToBase32(const uint8* bytes, size_t size) {
  static const char kEncoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

  // Eliminate special cases first.
  if (size == 0) {
    return string16();
  } else if (size == 1) {
    string16 ret;
    ret.push_back(kEncoding[(bytes[0] & 0xf8) >> 3]);
    ret.push_back(kEncoding[(bytes[0] & 0x07) << 2]);
    return ret;
  } else if (size >= std::numeric_limits<size_t>::max() / 8) {
    // If |size| is too big, the calculation of |encoded_length| below will
    // overflow.
    AtlTrace("%hs. Byte array is too long.\n", __FUNCTION__);
    return string16();
  }

  // Overestimate the number of bits in the string by 4 so that dividing by 5
  // is the equivalent of rounding up the actual number of bits divided by 5.
  const size_t encoded_length = (size * 8 + 4) / 5;

  string16 ret;
  ret.reserve(encoded_length);

  // A bit stream which will be read from the left and appended to from the
  // right as it's emptied.
  uint16 bit_stream = (bytes[0] << 8) + bytes[1];
  size_t next_byte_index = 2;
  int free_bits = 0;
  while (free_bits < 16) {
    // Extract the 5 leftmost bits in the stream
    ret.push_back(kEncoding[(bit_stream & 0xf800) >> 11]);
    bit_stream <<= 5;
    free_bits += 5;

    // If there is enough room in the bit stream, inject another byte (if there
    // are any left...).
    if (free_bits >= 8 && next_byte_index < size) {
      free_bits -= 8;
      bit_stream += bytes[next_byte_index++] << free_bits;
    }
  }

  if (ret.length() != encoded_length) {
    AtlTrace("%hs. Encoding doesn't match expected length.\n", __FUNCTION__);
    return string16();
  }
  return ret;
}

// TODO(gab): This code also lives in shell_util. Refactor for reuse.
bool GetUserSpecificRegistrySuffix(string16* suffix) {
  string16 user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    AtlTrace("%hs. GetUserSidString failed.\n", __FUNCTION__);
    return false;
  }
  COMPILE_ASSERT(sizeof(base::MD5Digest) == 16, size_of_MD5_not_as_expected_);
  base::MD5Digest md5_digest;
  std::string user_sid_ascii(UTF16ToASCII(user_sid));
  base::MD5Sum(user_sid_ascii.c_str(), user_sid_ascii.length(), &md5_digest);
  const string16 base32_md5(
      ByteArrayToBase32(md5_digest.a, arraysize(md5_digest.a)));
  // The value returned by the base32 algorithm above must never change and must
  // always be 26 characters long (i.e. if someone ever moves this to base and
  // implements the full base32 algorithm (i.e. with appended '=' signs in the
  // output), they must provide a flag to allow this method to still request
  // the output with no appended '=' signs).
  if (base32_md5.length() != 26U) {
    AtlTrace("%hs. Base32 encoding of md5 hash is incorrect.\n", __FUNCTION__);
    return false;
  }
  suffix->reserve(base32_md5.length() + 1);
  suffix->assign(1, L'.');
  suffix->append(base32_md5);
  return true;
}

}  // namespace

namespace delegate_execute {

void UpdateChromeIfNeeded(const base::FilePath& chrome_exe) {
#if defined(GOOGLE_CHROME_BUILD)
  // Nothing to do if a browser is already running or if there's no
  // new_chrome.exe.
  if (IsBrowserRunning(chrome_exe) || !NewChromeExeExists(chrome_exe))
    return;

  base::ProcessHandle process_handle = base::kNullProcessHandle;

  if (IsPerUserInstall(chrome_exe)) {
    // Read the update command from the registry.
    string16 update_command;
    if (!GetUpdateCommand(true, &update_command)) {
      AtlTrace("%hs. Failed to read update command from registry.\n",
               __FUNCTION__);
    } else {
      // Run the update command.
      base::LaunchOptions launch_options;
      launch_options.start_hidden = true;
      if (!base::LaunchProcess(update_command, launch_options,
                               &process_handle)) {
        AtlTrace("%hs. Failed to launch command to finalize update; "
                 "error %u.\n", __FUNCTION__, ::GetLastError());
        process_handle = base::kNullProcessHandle;
      }
    }
  } else {
    // Run the update command via Google Update.
    HRESULT hr = S_OK;
    base::win::ScopedComPtr<IProcessLauncher> process_launcher;
    hr = process_launcher.CreateInstance(__uuidof(ProcessLauncherClass));
    if (FAILED(hr)) {
      AtlTrace("%hs. Failed to Create ProcessLauncher; hr=0x%X.\n",
               __FUNCTION__, hr);
    } else {
      ULONG_PTR handle = 0;
      hr = process_launcher->LaunchCmdElevated(
          kChromeAppGuid, kRenameCommandValue, GetCurrentProcessId(), &handle);
      if (FAILED(hr)) {
        AtlTrace("%hs. Failed to launch command to finalize update; "
                 "hr=0x%X.\n", __FUNCTION__, hr);
      } else {
        process_handle = reinterpret_cast<base::ProcessHandle>(handle);
      }
    }
  }

  // Wait for the update to complete and report the results.
  if (process_handle != base::kNullProcessHandle) {
    int exit_code = 0;
    // WaitForExitCode will close the handle in all cases.
    if (!base::WaitForExitCode(process_handle, &exit_code)) {
      AtlTrace("%hs. Failed to get result when finalizing update.\n",
               __FUNCTION__);
    } else if (exit_code != kExitCodeRenameSuccessful) {
      AtlTrace("%hs. Failed to finalize update with exit code %d.\n",
               __FUNCTION__, exit_code);
    } else {
      AtlTrace("%hs. Finalized pending update.\n", __FUNCTION__);
    }
  }
#endif
}

// TODO(gab): This code also lives in shell_util. Refactor for reuse.
string16 GetAppId(const base::FilePath& chrome_exe) {
  string16 app_id(kAppUserModelId);
  string16 suffix;
  if (IsPerUserInstall(chrome_exe) &&
      !GetUserSpecificRegistrySuffix(&suffix)) {
    AtlTrace("%hs. GetUserSpecificRegistrySuffix failed.\n",
              __FUNCTION__);
  }
  return app_id.append(suffix);
}

}  // delegate_execute
