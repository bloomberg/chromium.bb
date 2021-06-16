// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_WIN_H_
#define CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_WIN_H_
#pragma once

#include "libcef/browser/file_dialog_runner.h"

class CefFileDialogRunnerWin : public CefFileDialogRunner {
 public:
  CefFileDialogRunnerWin();

  // CefFileDialogRunner methods:
  void Run(AlloyBrowserHostImpl* browser,
           const FileChooserParams& params,
           RunFileChooserCallback callback) override;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_WIN_H_
