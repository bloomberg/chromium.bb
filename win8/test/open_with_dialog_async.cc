// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation for the asynchronous interface to the Windows shell
// SHOpenWithDialog function.  The call is made on a dedicated UI thread in a
// single-threaded apartment.

#include "win8/test/open_with_dialog_async.h"

#include <shlobj.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/win/windows_version.h"

namespace win8 {

namespace {

struct OpenWithContext {
  OpenWithContext(
      HWND parent_window_in,
      const string16& file_name_in,
      const string16& file_type_class_in,
      int open_as_info_flags_in,
      const scoped_refptr<base::SingleThreadTaskRunner>& client_runner_in,
      const OpenWithDialogCallback& callback_in);
  ~OpenWithContext();

  base::Thread thread;
  HWND parent_window;
  string16 file_name;
  string16 file_type_class;
  int open_as_info_flags;
  scoped_refptr<base::SingleThreadTaskRunner> client_runner;
  OpenWithDialogCallback callback;
};

OpenWithContext::OpenWithContext(
    HWND parent_window_in,
    const string16& file_name_in,
    const string16& file_type_class_in,
    int open_as_info_flags_in,
    const scoped_refptr<base::SingleThreadTaskRunner>& client_runner_in,
    const OpenWithDialogCallback& callback_in)
    : thread("OpenWithDialog"),
      parent_window(parent_window_in),
      file_name(file_name_in),
      file_type_class(file_type_class_in),
      open_as_info_flags(open_as_info_flags_in),
      client_runner(client_runner_in),
      callback(callback_in) {
  thread.init_com_with_mta(false);
  thread.Start();
}

OpenWithContext::~OpenWithContext() {}

// Runs the caller-provided |callback| with the result of the call to
// SHOpenWithDialog on the caller's initial thread.
void OnOpenWithDialogDone(OpenWithContext* context, HRESULT result) {
  DCHECK(context->client_runner->BelongsToCurrentThread());
  OpenWithDialogCallback callback(context->callback);

  // Join with the thread.
  delete context;

  // Run the client's callback.
  callback.Run(result);
}

// Calls SHOpenWithDialog (blocking), and returns the result back to the client
// thread.
void OpenWithDialogTask(OpenWithContext* context) {
  DCHECK_EQ(context->thread.thread_id(), base::PlatformThread::CurrentId());
  OPENASINFO open_as_info = {
    context->file_name.c_str(),
    context->file_type_class.c_str(),
    context->open_as_info_flags
  };

  HRESULT result = ::SHOpenWithDialog(context->parent_window, &open_as_info);

  // Bounce back to the calling thread to release resources and deliver the
  // callback.
  if (!context->client_runner->PostTask(
          FROM_HERE,
          base::Bind(&OnOpenWithDialogDone, context, result))) {
    // The calling thread has gone away. There's nothing to be done but leak.
    // In practice this is only likely to happen at shutdown, so there isn't
    // much of a concern that it'll happen in the real world.
    DLOG(ERROR) << "leaking OpenWith thread; result = " << std::hex << result;
  }
}

}  // namespace

void OpenWithDialogAsync(
    HWND parent_window,
    const string16& file_name,
    const string16& file_type_class,
    int open_as_info_flags,
    const OpenWithDialogCallback& callback) {
  DCHECK_GE(base::win::GetVersion(), base::win::VERSION_VISTA);
  OpenWithContext* context =
      new OpenWithContext(parent_window, file_name, file_type_class,
                          open_as_info_flags,
                          base::ThreadTaskRunnerHandle::Get(), callback);
  context->thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&OpenWithDialogTask, context));
}

}  // namespace win8
