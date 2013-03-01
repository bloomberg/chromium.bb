// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/chromoting_module.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/win/elevated_controller.h"

namespace remoting {

namespace {

// Holds a reference to the task runner used by the module.
base::LazyInstance<scoped_refptr<AutoThreadTaskRunner> > g_module_task_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromotingModule::ChromotingModule(
    ATL::_ATL_OBJMAP_ENTRY* classes,
    ATL::_ATL_OBJMAP_ENTRY* classes_end)
    : classes_(classes),
      classes_end_(classes_end) {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.succeeded())
    return;

  ATL::_AtlComModule.ExecuteObjectMain(true);
}

ChromotingModule::~ChromotingModule() {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.succeeded())
    return;

  Term();
  ATL::_AtlComModule.ExecuteObjectMain(false);
}

// static
scoped_refptr<AutoThreadTaskRunner> ChromotingModule::task_runner() {
  return g_module_task_runner.Get();
}

bool ChromotingModule::Run() {
  // Don't do anything if COM initialization failed.
  if (!com_initializer_.succeeded())
    return false;

  // Register class objects.
  HRESULT result = RegisterClassObjects(CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to register class objects, result=0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  // Arrange to run |message_loop| until no components depend on it.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  g_module_task_runner.Get() = new AutoThreadTaskRunner(
      message_loop.message_loop_proxy(), run_loop.QuitClosure());

  // Start accepting activations.
  result = CoResumeClassObjects();
  if (FAILED(result)) {
    LOG(ERROR) << "CoResumeClassObjects() failed, result=0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  // Run the loop until the module lock counter reaches zero.
  run_loop.Run();

  // Unregister class objects.
  result = RevokeClassObjects();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to unregister class objects, result=0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  return true;
}

LONG ChromotingModule::Unlock() {
  LONG count = ATL::CAtlModuleT<ChromotingModule>::Unlock();

  if (!count) {
    // Stop accepting activations.
    HRESULT hr = CoSuspendClassObjects();
    CHECK(SUCCEEDED(hr));

    // Release the message loop reference, causing the message loop to exit.
    g_module_task_runner.Get() = NULL;
  }

  return count;
}

HRESULT ChromotingModule::RegisterClassObjects(DWORD class_context,
                                               DWORD flags) {
  for (ATL::_ATL_OBJMAP_ENTRY* i = classes_; i != classes_end_; ++i) {
    HRESULT result = i->RegisterClassObject(class_context, flags);
    if (FAILED(result))
      return result;
  }

  return S_OK;
}

HRESULT ChromotingModule::RevokeClassObjects() {
  for (ATL::_ATL_OBJMAP_ENTRY* i = classes_; i != classes_end_; ++i) {
    HRESULT result = i->RevokeClassObject();
    if (FAILED(result))
      return result;
  }

  return S_OK;
}

// Elevated controller entry point.
int ElevatedControllerMain() {
  ATL::_ATL_OBJMAP_ENTRY elevated_controller_entry[] = {
    OBJECT_ENTRY(__uuidof(ElevatedController), ElevatedController)
  };

  ChromotingModule module(elevated_controller_entry,
                          elevated_controller_entry + 1);
  return module.Run() ? kSuccessExitCode : kInitializationFailed;
}

} // namespace remoting
