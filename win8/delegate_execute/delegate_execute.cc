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
#include "base/process_util.h"
#include "base/string16.h"
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

using delegate_execute::DelegateExecuteOperation;
using base::win::ScopedHandle;

int RelaunchChrome(const DelegateExecuteOperation& operation) {
  AtlTrace("Relaunching [%ls] with flags [%s]\n",
           operation.mutex().c_str(), operation.relaunch_flags());
  ScopedHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, operation.mutex().c_str()));
  if (mutex.IsValid()) {
    const int kWaitSeconds = 5;
    DWORD result = ::WaitForSingleObject(mutex, kWaitSeconds * 1000);
    if (result == WAIT_ABANDONED) {
      // This is the normal case. Chrome exits and windows marks the mutex as
      // abandoned.
    } else if (result == WAIT_OBJECT_0) {
      // This is unexpected. Check if somebody is not closing the mutex on
      // RelaunchChromehelper, the mutex should not be closed.
      AtlTrace("Unexpected release of the relaunch mutex!!\n");
    } else if (result == WAIT_TIMEOUT) {
      // This could mean that Chrome is hung. Proceed to exterminate.
      DWORD pid = operation.GetParentPid();
      AtlTrace("%ds timeout. Killing Chrome %d\n", kWaitSeconds, pid);
      base::KillProcessById(pid, 0, false);
    } else {
      AtlTrace("Failed to wait for relaunch mutex, result is 0x%x\n", result);
    }
  } else {
    // It is possible that chrome exits so fast that the mutex is not there.
    AtlTrace("No relaunch mutex found\n");
  }

  base::win::ScopedCOMInitializer com_initializer;

  string16 flags(ASCIIToWide(operation.relaunch_flags()));
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_LOG_USAGE;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpFile = operation.shortcut().value().c_str();
  sei.lpParameters = flags.c_str();

  AtlTrace(L"Relaunching Chrome via shortcut [%ls]\n", sei.lpFile);

  if (!::ShellExecuteExW(&sei)) {
    int error = HRESULT_FROM_WIN32(::GetLastError());
    AtlTrace("ShellExecute returned 0x%08X\n", error);
    return error;
  }
  return S_OK;
}

extern "C" int WINAPI _tWinMain(HINSTANCE , HINSTANCE, LPTSTR, int nShowCmd) {
  base::AtExitManager exit_manager;
  AtlTrace("delegate_execute enter\n");

  CommandLine::Init(0, NULL);
  HRESULT ret_code = E_UNEXPECTED;
  DelegateExecuteOperation operation;
  if (operation.Init(CommandLine::ForCurrentProcess())) {
    switch (operation.operation_type()) {
      case DelegateExecuteOperation::DELEGATE_EXECUTE:
        ret_code = _AtlModule.WinMain(nShowCmd);
        break;
      case DelegateExecuteOperation::RELAUNCH_CHROME:
        ret_code = RelaunchChrome(operation);
        break;
      default:
        NOTREACHED();
    }
  }
  AtlTrace("delegate_execute exit, code = %d\n", ret_code);
  return ret_code;
}
