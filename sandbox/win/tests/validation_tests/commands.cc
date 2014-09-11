// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Aclapi.h>
#include <windows.h>
#include <string>

#include "sandbox/win/tests/validation_tests/commands.h"

#include "sandbox/win/tests/common/controller.h"

namespace {

// Returns the HKEY corresponding to name. If there is no HKEY corresponding
// to the name it returns NULL.
HKEY GetHKEYFromString(const base::string16 &name) {
  if (L"HKLM" == name)
    return HKEY_LOCAL_MACHINE;
  else if (L"HKCR" == name)
    return HKEY_CLASSES_ROOT;
  else if (L"HKCC" == name)
    return HKEY_CURRENT_CONFIG;
  else if (L"HKCU" == name)
    return HKEY_CURRENT_USER;
  else if (L"HKU" == name)
    return HKEY_USERS;

  return NULL;
}

// Modifies string to remove the leading and trailing quotes.
void trim_quote(base::string16* string) {
  base::string16::size_type pos1 = string->find_first_not_of(L'"');
  base::string16::size_type pos2 = string->find_last_not_of(L'"');

  if (base::string16::npos == pos1 || base::string16::npos == pos2)
    (*string) = L"";
  else
    (*string) = string->substr(pos1, pos2 + 1);
}

int TestOpenFile(base::string16 path, bool for_write) {
  wchar_t path_expanded[MAX_PATH + 1] = {0};
  DWORD size = ::ExpandEnvironmentStrings(path.c_str(), path_expanded,
                                          MAX_PATH);
  if (!size)
    return sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE file;
  file = ::CreateFile(path_expanded,
                      for_write ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,  // No security attributes.
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS,
                      NULL);  // No template.

  if (INVALID_HANDLE_VALUE != file) {
    ::CloseHandle(file);
    return sandbox::SBOX_TEST_SUCCEEDED;
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return sandbox::SBOX_TEST_DENIED;
    } else {
      return sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  }
}

}  // namespace

namespace sandbox {

SBOX_TESTS_COMMAND int ValidWindow(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HWND window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(_wtoi(argv[0])));

  return TestValidWindow(window);
}

int TestValidWindow(HWND window) {
  if (::IsWindow(window))
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int OpenProcessCmd(int argc, wchar_t **argv) {
  if (2 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  DWORD process_id = _wtol(argv[0]);
  DWORD access_mask = _wtol(argv[1]);
  return TestOpenProcess(process_id, access_mask);
}

int TestOpenProcess(DWORD process_id, DWORD access_mask) {
  HANDLE process = ::OpenProcess(access_mask,
                                 FALSE,  // Do not inherit handle.
                                 process_id);
  if (NULL == process) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  } else {
    ::CloseHandle(process);
    return SBOX_TEST_SUCCEEDED;
  }
}

SBOX_TESTS_COMMAND int OpenThreadCmd(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  DWORD thread_id = _wtoi(argv[0]);
  return TestOpenThread(thread_id);
}

int TestOpenThread(DWORD thread_id) {

  HANDLE thread = ::OpenThread(THREAD_QUERY_INFORMATION,
                               FALSE,  // Do not inherit handles.
                               thread_id);

  if (NULL == thread) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  } else {
    ::CloseHandle(thread);
    return SBOX_TEST_SUCCEEDED;
  }
}

SBOX_TESTS_COMMAND int OpenFileCmd(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::string16 path = argv[0];
  trim_quote(&path);

  return TestOpenReadFile(path);
}

int TestOpenReadFile(const base::string16& path) {
  return TestOpenFile(path, false);
}

int TestOpenWriteFile(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::string16 path = argv[0];
  trim_quote(&path);

  return TestOpenWriteFile(path);
}

int TestOpenWriteFile(const base::string16& path) {
  return TestOpenFile(path, true);
}

SBOX_TESTS_COMMAND int OpenKey(int argc, wchar_t **argv) {
  if (0 == argc || argc > 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  // Get the hive.
  HKEY base_key = GetHKEYFromString(argv[0]);

  // Get the subkey.
  base::string16 subkey;
  if (2 == argc) {
    subkey = argv[1];
    trim_quote(&subkey);
  }

  return TestOpenKey(base_key, subkey);
}

int TestOpenKey(HKEY base_key, base::string16 subkey) {
  HKEY key;
  LONG err_code = ::RegOpenKeyEx(base_key,
                                 subkey.c_str(),
                                 0,  // Reserved, must be 0.
                                 MAXIMUM_ALLOWED,
                                 &key);
  if (ERROR_SUCCESS == err_code) {
    ::RegCloseKey(key);
    return SBOX_TEST_SUCCEEDED;
  } else if (ERROR_INVALID_HANDLE == err_code ||
             ERROR_ACCESS_DENIED  == err_code) {
    return SBOX_TEST_DENIED;
  } else {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
}

// Returns true if the current's thread desktop is the interactive desktop.
// In Vista there is a more direct test but for XP and w2k we need to check
// the object name.
bool IsInteractiveDesktop(bool* is_interactive) {
  HDESK current_desk = ::GetThreadDesktop(::GetCurrentThreadId());
  if (NULL == current_desk) {
    return false;
  }
  wchar_t current_desk_name[256] = {0};
  if (!::GetUserObjectInformationW(current_desk, UOI_NAME, current_desk_name,
                                  sizeof(current_desk_name), NULL)) {
    return false;
  }
  *is_interactive = (0 == _wcsicmp(L"default", current_desk_name));
  return true;
}

SBOX_TESTS_COMMAND int OpenInteractiveDesktop(int, wchar_t **) {
  return TestOpenInputDesktop();
}

int TestOpenInputDesktop() {
  bool is_interactive = false;
  if (IsInteractiveDesktop(&is_interactive) && is_interactive) {
    return SBOX_TEST_SUCCEEDED;
  }
  HDESK desk = ::OpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW);
  if (desk) {
    ::CloseDesktop(desk);
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int SwitchToSboxDesktop(int, wchar_t **) {
  return TestSwitchDesktop();
}

int TestSwitchDesktop() {
  HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (NULL == desktop) {
    return SBOX_TEST_FAILED;
  }
  if (::SwitchDesktop(desktop)) {
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int OpenAlternateDesktop(int, wchar_t **argv) {
  return TestOpenAlternateDesktop(argv[0]);
}

int TestOpenAlternateDesktop(wchar_t *desktop_name) {
  // Test for WRITE_DAC permission on the handle.
  HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (desktop) {
    HANDLE test_handle;
    if (::DuplicateHandle(::GetCurrentProcess(), desktop,
                          ::GetCurrentProcess(), &test_handle,
                          WRITE_DAC, FALSE, 0)) {
      DWORD result = ::SetSecurityInfo(test_handle, SE_WINDOW_OBJECT,
                                       DACL_SECURITY_INFORMATION, NULL, NULL,
                                       NULL, NULL);
      ::CloseHandle(test_handle);
      if (result == ERROR_SUCCESS) {
        return SBOX_TEST_SUCCEEDED;
      }
    } else if (::GetLastError() != ERROR_ACCESS_DENIED) {
      return SBOX_TEST_FAILED;
    }
  }

  // Open by name with WRITE_DAC.
  desktop = ::OpenDesktop(desktop_name, 0, FALSE, WRITE_DAC);
  if (desktop || ::GetLastError() != ERROR_ACCESS_DENIED) {
    ::CloseDesktop(desktop);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_DENIED;
}

BOOL CALLBACK DesktopTestEnumProc(LPTSTR desktop_name, LPARAM result) {
  return TRUE;
}

SBOX_TESTS_COMMAND int EnumAlternateWinsta(int, wchar_t **) {
  return TestEnumAlternateWinsta();
}

int TestEnumAlternateWinsta() {
  int result = SBOX_TEST_DENIED;
  // Try to enumerate the destops on the alternate windowstation.
  if (::EnumDesktopsW(NULL, DesktopTestEnumProc, 0)) {
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int SleepCmd(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  ::Sleep(_wtoi(argv[0]));
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int AllocateCmd(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  size_t mem_size = static_cast<size_t>(_wtoll(argv[0]));
  void* memory = ::VirtualAlloc(NULL, mem_size, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
  if (!memory) {
    // We need to give the broker a chance to kill our process on failure.
    ::Sleep(5000);
    return SBOX_TEST_DENIED;
  }

  if (!::VirtualFree(memory, 0, MEM_RELEASE))
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}


}  // namespace sandbox
