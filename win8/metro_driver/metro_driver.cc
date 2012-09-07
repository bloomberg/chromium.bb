// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include <roerrorapi.h>
#include <shobjidl.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/win/scoped_comptr.h"
#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/winrt_utils.h"
#include "sandbox/win/src/sidestep/preamble_patcher.h"

// TODO(siggi): Move this to GYP.
#pragma comment(lib, "runtimeobject.lib")

namespace {

LONG WINAPI ErrorReportingHandler(EXCEPTION_POINTERS* ex_info) {
  // See roerrorapi.h for a description of the
  // exception codes and parameters.
  DWORD code = ex_info->ExceptionRecord->ExceptionCode;
  ULONG_PTR* info = ex_info->ExceptionRecord->ExceptionInformation;
  if (code == EXCEPTION_RO_ORIGINATEERROR) {
    string16 msg(reinterpret_cast<wchar_t*>(info[2]), info[1]);
    LOG(ERROR) << "VEH: Metro error 0x" << std::hex << info[0] << ": " << msg;
  } else if (code == EXCEPTION_RO_TRANSFORMERROR) {
    string16 msg(reinterpret_cast<wchar_t*>(info[3]), info[2]);
    LOG(ERROR) << "VEH: Metro old error 0x" << std::hex << info[0]
               << " new error 0x" << info[1] << ": " << msg;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

// TODO(robertshield): This GUID is hard-coded in a bunch of places that
//     don't allow explicit includes. Find a single place for it to live.
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

}
// Required for base initialization.
// TODO(siggi): This should be handled better, as this way our at exit
//     registrations will run under the loader's lock. However,
//     once metro_driver is merged into Chrome.dll, this will go away anyhow.
base::AtExitManager at_exit;

namespace Hacks {

typedef BOOL (WINAPI* IsImmersiveFunctionPtr)(HANDLE process);
char* g_real_is_immersive_proc_stub = NULL;

HMODULE g_webrtc_quartz_dll_handle = NULL;
bool g_fake_is_immersive_process_ret = false;

BOOL WINAPI MetroChromeIsImmersiveIntercept(HANDLE process) {
  if (g_fake_is_immersive_process_ret && process == ::GetCurrentProcess())
    return FALSE;

  IsImmersiveFunctionPtr real_proc =
      reinterpret_cast<IsImmersiveFunctionPtr>(
          static_cast<char*>(g_real_is_immersive_proc_stub));
  return real_proc(process);
}

void MetroSpecificHacksInitialize() {
  // The quartz dll which is used by the webrtc code in Chrome fails in a metro
  // app. It checks this via the IsImmersiveProcess export in user32. We
  // intercept the same and spoof the value as false. This is ok as technically
  // a metro browser is not a real metro application. The webrtc functionality
  // works fine with this hack.
  // TODO(tommi)
  // BUG:- https://code.google.com/p/chromium/issues/detail?id=140545
  // We should look into using media foundation on windows 8 in metro chrome.
  IsImmersiveFunctionPtr is_immersive_func_address =
      reinterpret_cast<IsImmersiveFunctionPtr>(::GetProcAddress(
          ::GetModuleHandle(L"user32.dll"), "IsImmersiveProcess"));
  DCHECK(is_immersive_func_address);

  // Allow the function to be patched by changing the protections on the page.
  DWORD old_protect = 0;
  ::VirtualProtect(is_immersive_func_address, 5, PAGE_EXECUTE_READWRITE,
                   &old_protect);

  DCHECK(g_real_is_immersive_proc_stub == NULL);
  g_real_is_immersive_proc_stub = reinterpret_cast<char*>(VirtualAllocEx(
      ::GetCurrentProcess(), NULL, sidestep::kMaxPreambleStubSize,
      MEM_COMMIT, PAGE_EXECUTE_READWRITE));
  DCHECK(g_real_is_immersive_proc_stub);

  sidestep::SideStepError patch_result =
      sidestep::PreamblePatcher::Patch(
          is_immersive_func_address, MetroChromeIsImmersiveIntercept,
          g_real_is_immersive_proc_stub, sidestep::kMaxPreambleStubSize);

  DCHECK(patch_result == sidestep::SIDESTEP_SUCCESS);

  // Restore the permissions on the page in user32 containing the
  // IsImmersiveProcess function code.
  DWORD dummy = 0;
  ::VirtualProtect(is_immersive_func_address, 5, old_protect, &dummy);

  // Mimic the original page permissions from the IsImmersiveProcess page
  // on our stub.
  ::VirtualProtect(g_real_is_immersive_proc_stub,
                   sidestep::kMaxPreambleStubSize,
                   old_protect,
                   &old_protect);

  g_fake_is_immersive_process_ret = true;
  g_webrtc_quartz_dll_handle = LoadLibrary(L"quartz.dll");
  g_fake_is_immersive_process_ret = false;

  DCHECK(g_webrtc_quartz_dll_handle);
  if (!g_webrtc_quartz_dll_handle) {
    DVLOG(1) << "Quartz dll load failed with error: " << GetLastError();
  } else {
    // Pin the quartz module to protect against it being inadvarently unloaded.
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN, L"quartz.dll",
                        &g_webrtc_quartz_dll_handle);
  }
}

}  // namespace Hacks

extern "C" __declspec(dllexport)
int InitMetro(LPTHREAD_START_ROUTINE thread_proc, void* context) {
  // Initialize the command line.
  CommandLine::Init(0, NULL);
  logging::InitLogging(
        NULL,
        logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE,
        logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

#if defined(NDEBUG)
  logging::SetMinLogLevel(logging::LOG_ERROR);
  // Bind to the breakpad handling function.
  globals.breakpad_exception_handler =
      reinterpret_cast<BreakpadExceptionHandler>(
          ::GetProcAddress(::GetModuleHandle(NULL),
                           "CrashForException"));
  if (!globals.breakpad_exception_handler) {
    DVLOG(0) << "CrashForException export not found";
  }
#else
  logging::SetMinLogLevel(logging::LOG_VERBOSE);
  // Set the error reporting flags to always raise an exception,
  // which is then processed by our vectored exception handling
  // above to log the error message.
  winfoundtn::Diagnostics::SetErrorReportingFlags(
      winfoundtn::Diagnostics::UseSetErrorInfo |
      winfoundtn::Diagnostics::ForceExceptions);

  HANDLE registration =
      ::AddVectoredExceptionHandler(TRUE, ErrorReportingHandler);
#endif

  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kChromeTraceProviderName);

  DVLOG(1) << "InitMetro";

  mswrw::RoInitializeWrapper ro_initializer(RO_INIT_MULTITHREADED);
  CheckHR(ro_initializer, "RoInitialize failed");

  mswr::ComPtr<winapp::Core::ICoreApplication> core_app;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_ApplicationModel_Core_CoreApplication,
      core_app.GetAddressOf());
  CheckHR(hr, "Failed to create app factory");
  if (FAILED(hr))
    return 1;

  // The metro specific hacks code assumes that there is only one thread active
  // at the moment. This better be the case or we may have race conditions.
  Hacks::MetroSpecificHacksInitialize();

  auto view_factory = mswr::Make<ChromeAppViewFactory>(
      core_app.Get(), thread_proc, context);
  hr = core_app->Run(view_factory.Get());
  DVLOG(1) << "exiting InitMetro, hr=" << hr;

#if !defined(NDEBUG)
  ::RemoveVectoredExceptionHandler(registration);
#endif

  return hr;
}

// Activates the application known by |app_id|.  Returns, among other things,
// E_APPLICATION_NOT_REGISTERED if |app_id| identifies Chrome and Chrome is not
// the default browser.
extern "C" __declspec(dllexport)
HRESULT ActivateApplication(const wchar_t* app_id) {
  base::win::ScopedComPtr<IApplicationActivationManager> activator;
  HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    hr = activator->ActivateApplication(app_id, L"", AO_NONE, &pid);
  }
  return hr;
}
