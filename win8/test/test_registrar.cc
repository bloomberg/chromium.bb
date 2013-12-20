// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Win8 default browser registration utility.
//
// This tool can register and unregister a given exe as a potential default
// metro browser on Win8. It does not make the exe become THE default browser,
// for a mechnism to do this please see open_with_dialog_controller.h.
//
// TODO(robertshield): By default, this creates a run-time dependency on
// chrome.exe since it's the only thing we have right now that works as a
// default viewer process. Investigate extracting the metro init code and
// building them into a standalone viewer process.

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <shellapi.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "win8/test/test_registrar_constants.h"
#include "win8/test/test_registrar_resource.h"

namespace {

const wchar_t kDelegateExecuteCLSID[] =
    L"{FC0064A6-D1DE-4A83-92D2-5BB4EEBB70B5}";

void InitializeCommandLineDefaultValues() {
  CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(win8::test::kTestAppUserModelId))
    command_line.AppendSwitchNative(win8::test::kTestAppUserModelId,
                                    win8::test::kDefaultTestAppUserModelId);

  if (!command_line.HasSwitch(win8::test::kTestExeName))
    command_line.AppendSwitchNative(win8::test::kTestExeName,
                                    win8::test::kDefaultTestExeName);

  if (!command_line.HasSwitch(win8::test::kTestExePath)) {
    base::FilePath exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    exe_path = exe_path.Append(win8::test::kDefaultTestExePath);

    command_line.AppendSwitchNative(win8::test::kTestExePath,
                                    exe_path.value());
  }

  if (!command_line.HasSwitch(win8::test::kTestProgId))
    command_line.AppendSwitchNative(win8::test::kTestProgId,
                                    win8::test::kDefaultTestProgId);
}

}  // namespace

// Implementation of an ATL module that provides the necessary replacement
// values for the default browser .rgs script.
class TestDelegateExecuteModule
    : public ATL::CAtlExeModuleT< TestDelegateExecuteModule > {
 public :
  typedef ATL::CAtlExeModuleT<TestDelegateExecuteModule> ParentClass;

  DECLARE_REGISTRY_RESOURCEID(IDR_TESTDELEGATEEXECUTE);

  HRESULT RegisterServer(BOOL reg_type_lib) {
    return ParentClass::RegisterServer(FALSE);
  }

  virtual HRESULT AddCommonRGSReplacements(IRegistrarBase* registrar) throw() {
    AtlTrace(L"In %hs\n", __FUNCTION__);
    HRESULT hr = ParentClass::AddCommonRGSReplacements(registrar);
    if (FAILED(hr))
      return hr;

    registrar->AddReplacement(L"DELEGATE_EXECUTE_CLSID", kDelegateExecuteCLSID);

    CommandLine& command_line = *CommandLine::ForCurrentProcess();

    registrar->AddReplacement(L"APP_USER_MODEL_ID",
                              command_line.GetSwitchValueNative(
                                  win8::test::kTestAppUserModelId).c_str());
    registrar->AddReplacement(L"EXE_NAME",
                              command_line.GetSwitchValueNative(
                                  win8::test::kTestExeName).c_str());
    registrar->AddReplacement(L"PROG_ID",
                              command_line.GetSwitchValueNative(
                                  win8::test::kTestProgId).c_str());

    base::string16 exe_path(
        command_line.GetSwitchValueNative(win8::test::kTestExePath));

    base::string16 exe_open_command(
        base::StringPrintf(L"\"%ls\" -- %%*", exe_path.c_str()));
    registrar->AddReplacement(L"EXE_OPEN_COMMAND", exe_open_command.c_str());

    base::string16 exe_icon(base::StringPrintf(L"%ls,0", exe_path.c_str()));
    registrar->AddReplacement(L"EXE_ICON", exe_icon.c_str());

    base::string16 prog_id_open_command(
        base::StringPrintf(L"\"%ls\" -- \"%%1\"", exe_path.c_str()));
    registrar->AddReplacement(L"PROG_ID_OPEN_COMMAND",
                              prog_id_open_command.c_str());

    ATLASSERT(SUCCEEDED(hr));
    return hr;
  }
};

TestDelegateExecuteModule _AtlModule;

EXTERN_C const GUID CLSID_TestDefaultBrowserRegistrar;
class ATL_NO_VTABLE DECLSPEC_UUID("FC0064A6-D1DE-4A83-92D2-5BB4EEBB70B5")
    TestDefaultBrowserRegistrar
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<TestDefaultBrowserRegistrar,
                         &CLSID_TestDefaultBrowserRegistrar> {
 public:
  DECLARE_REGISTRY_RESOURCEID(IDR_TESTDELEGATEEXECUTE);

  BEGIN_COM_MAP(TestDefaultBrowserRegistrar)
  END_COM_MAP()
};

extern "C" int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nShowCmd) {
  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);
  InitializeCommandLineDefaultValues();

  HRESULT ret_code = _AtlModule.WinMain(nShowCmd);

  return ret_code;
}

OBJECT_ENTRY_AUTO(__uuidof(TestDefaultBrowserRegistrar),
                  TestDefaultBrowserRegistrar);
