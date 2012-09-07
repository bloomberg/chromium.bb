// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <initguid.h>
#include <shellapi.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_switches.h"
#include "command_execute_impl.h"
#include "win8/delegate_execute/delegate_execute_operation.h"
#include "win8/delegate_execute/resource.h"

using namespace ATL;

class DelegateExecuteModule
    : public ATL::CAtlExeModuleT< DelegateExecuteModule > {
 public :
  typedef ATL::CAtlExeModuleT<DelegateExecuteModule> ParentClass;

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_DELEGATEEXECUTE,
                                    "{B1935DA1-112F-479A-975B-AB8588ABA636}")

  virtual HRESULT AddCommonRGSReplacements(IRegistrarBase* registrar) throw() {
    AtlTrace(L"In %hs\n", __FUNCTION__);
    HRESULT hr = ParentClass::AddCommonRGSReplacements(registrar);
    if (FAILED(hr))
      return hr;

    wchar_t delegate_execute_clsid[MAX_PATH] = {0};
    if (!StringFromGUID2(__uuidof(CommandExecuteImpl), delegate_execute_clsid,
                         ARRAYSIZE(delegate_execute_clsid))) {
      ATLASSERT(false);
      return E_FAIL;
    }

    hr = registrar->AddReplacement(L"DELEGATE_EXECUTE_CLSID",
                                   delegate_execute_clsid);
    ATLASSERT(SUCCEEDED(hr));
    return hr;
  }
};

DelegateExecuteModule _AtlModule;

// Relaunch metro Chrome by ShellExecute on |shortcut| with --force-immersive.
// |handle_value| is an optional handle on which this function will wait before
// performing the relaunch.
int RelaunchChrome(const FilePath& shortcut, const string16& handle_value) {
  base::win::ScopedHandle handle;

  if (!handle_value.empty()) {
    uint32 the_handle = 0;
    if (!base::StringToUint(handle_value, &the_handle)) {
      // Failed to parse the handle value.  Skip the wait but proceed with the
      // relaunch.
      AtlTrace(L"Failed to parse handle value %ls\n", handle_value.c_str());
    } else {
      handle.Set(reinterpret_cast<HANDLE>(the_handle));
    }
  }

  if (handle.IsValid()) {
    AtlTrace(L"Waiting for chrome.exe to exit.\n");
    DWORD result = ::WaitForSingleObject(handle, 10 * 1000);
    AtlTrace(L"And we're back.\n");
    if (result != WAIT_OBJECT_0) {
      AtlTrace(L"Failed to wait for parent to exit; result=%u.\n", result);
      // This could mean that Chrome has hung.  Conservatively proceed with
      // the relaunch anyway.
    }
    handle.Close();
  }

  base::win::ScopedCOMInitializer com_initializer;

  AtlTrace(L"Launching Chrome via %ls.\n", shortcut.value().c_str());
  int ser = reinterpret_cast<int>(
      ::ShellExecute(NULL, NULL, shortcut.value().c_str(),
                     ASCIIToWide(switches::kForceImmersive).c_str(), NULL,
                     SW_SHOWNORMAL));
  AtlTrace(L"ShellExecute returned %d.\n", ser);
  return ser <= 32;
}

//
extern "C" int WINAPI _tWinMain(HINSTANCE , HINSTANCE,
                                LPTSTR, int nShowCmd) {
  using delegate_execute::DelegateExecuteOperation;
  base::AtExitManager exit_manager;

  CommandLine::Init(0, NULL);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  DelegateExecuteOperation operation;

  operation.Initialize(cmd_line);
  switch (operation.operation_type()) {
    case DelegateExecuteOperation::EXE_MODULE:
      return _AtlModule.WinMain(nShowCmd);

    case DelegateExecuteOperation::RELAUNCH_CHROME:
      return RelaunchChrome(operation.relaunch_shortcut(),
          cmd_line->GetSwitchValueNative(switches::kWaitForHandle));

    default:
      NOTREACHED();
  }

  return 1;
}
