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
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "win8/delegate_execute/command_execute_impl.h"
#include "win8/delegate_execute/crash_server_init.h"
#include "win8/delegate_execute/delegate_execute_operation.h"
#include "win8/delegate_execute/resource.h"

using namespace ATL;

// Usually classes derived from CAtlExeModuleT, or other types of ATL
// COM module classes statically define their CLSID at compile time through
// the use of various macros, and ATL internals takes care of creating the
// class objects and registering them.  However, we need to register the same
// object with different CLSIDs depending on a runtime setting, so we handle
// that logic here, before the main ATL message loop runs.
class DelegateExecuteModule
    : public ATL::CAtlExeModuleT< DelegateExecuteModule > {
 public :
  typedef ATL::CAtlExeModuleT<DelegateExecuteModule> ParentClass;
  typedef CComObject<CommandExecuteImpl> ImplType;

  DelegateExecuteModule()
      : registration_token_(0) {
  }

  HRESULT PreMessageLoop(int nShowCmd) {
    HRESULT hr = S_OK;
    base::string16 clsid_string;
    GUID clsid;
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    if (!dist->GetCommandExecuteImplClsid(&clsid_string))
      return E_FAIL;
    hr = ::CLSIDFromString(clsid_string.c_str(), &clsid);
    if (FAILED(hr))
      return hr;

    // We use the same class creation logic as ATL itself.  See
    // _ATL_OBJMAP_ENTRY::RegisterClassObject() in atlbase.h
    hr = ImplType::_ClassFactoryCreatorClass::CreateInstance(
        ImplType::_CreatorClass::CreateInstance, IID_IUnknown,
        instance_.ReceiveVoid());
    if (FAILED(hr))
      return hr;
    hr = ::CoRegisterClassObject(clsid, instance_, CLSCTX_LOCAL_SERVER,
        REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &registration_token_);
    if (FAILED(hr))
      return hr;

    return ParentClass::PreMessageLoop(nShowCmd);
  }

  HRESULT PostMessageLoop() {
    if (registration_token_ != 0) {
      ::CoRevokeClassObject(registration_token_);
      registration_token_ = 0;
    }

    instance_.Release();

    return ParentClass::PostMessageLoop();
  }

 private:
  base::win::ScopedComPtr<IUnknown> instance_;
  DWORD registration_token_;
};

DelegateExecuteModule _AtlModule;

using delegate_execute::DelegateExecuteOperation;
using base::win::ScopedHandle;

int RelaunchChrome(const DelegateExecuteOperation& operation) {
  AtlTrace("Relaunching [%ls] with flags [%ls]\n",
           operation.mutex().c_str(), operation.relaunch_flags().c_str());
  ScopedHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, operation.mutex().c_str()));
  if (mutex.IsValid()) {
    const int kWaitSeconds = 5;
    DWORD result = ::WaitForSingleObject(mutex.Get(), kWaitSeconds * 1000);
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

  // On Windows 8+ to launch Chrome we rely on Windows to use the
  // IExecuteCommand interface exposed by delegate_execute to launch Chrome
  // into Windows 8 metro mode or desktop.
  // On Windows 7 we don't use delegate_execute and instead use plain vanilla
  // ShellExecute to launch Chrome into ASH or desktop.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    base::win::ScopedCOMInitializer com_initializer;

    base::string16 relaunch_flags(operation.relaunch_flags());
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_FLAG_LOG_USAGE;
    sei.nShow = SW_SHOWNORMAL;
    sei.lpFile = operation.shortcut().value().c_str();
    sei.lpParameters = relaunch_flags.c_str();

    AtlTrace(L"Relaunching Chrome via shortcut [%ls]\n", sei.lpFile);

    if (!::ShellExecuteExW(&sei)) {
      int error = HRESULT_FROM_WIN32(::GetLastError());
      AtlTrace("ShellExecute returned 0x%08X\n", error);
      return error;
    }
  } else {
    base::FilePath chrome_exe_path;
    bool found_exe = CommandExecuteImpl::FindChromeExe(&chrome_exe_path);
    DCHECK(found_exe);
    if (found_exe) {
      bool launch_ash = CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceImmersive);
      if (launch_ash) {
        AtlTrace(L"Relaunching Chrome into Windows ASH on Windows 7\n");
      } else {
        AtlTrace(L"Relaunching Chrome into Desktop From ASH on Windows 7\n");
      }
      SHELLEXECUTEINFO sei = { sizeof(sei) };
      sei.fMask = SEE_MASK_FLAG_LOG_USAGE;
      sei.nShow = SW_SHOWNORMAL;
      // No point in using the shortcut if we are launching into ASH as any
      // additonal command line switches specified in the shortcut will be
      // ignored. This is because we don't have a good way to send the command
      // line switches from the viewer to the browser process.
      sei.lpFile = (launch_ash || operation.shortcut().empty()) ?
            chrome_exe_path.value().c_str() :
                operation.shortcut().value().c_str();
      sei.lpParameters =
          launch_ash ? L"-ServerName:DefaultBrowserServer" : NULL;
      if (!::ShellExecuteExW(&sei)) {
        int error = HRESULT_FROM_WIN32(::GetLastError());
        AtlTrace("ShellExecute returned 0x%08X\n", error);
        return error;
      }
    }
  }
  return S_OK;
}

extern "C" int WINAPI _tWinMain(HINSTANCE , HINSTANCE, LPTSTR, int nShowCmd) {
  scoped_ptr<google_breakpad::ExceptionHandler> breakpad =
      delegate_execute::InitializeCrashReporting();

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
