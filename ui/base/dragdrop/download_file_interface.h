// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
#define UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
#pragma once

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

#include "ui/base/ui_export.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

class FilePath;

namespace ui {

// Defines the interface to observe the status of file download.
class UI_EXPORT DownloadFileObserver
    : public base::RefCountedThreadSafe<DownloadFileObserver> {
 public:
  virtual void OnDownloadCompleted(const FilePath& file_path) = 0;
  virtual void OnDownloadAborted() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileObserver>;
  virtual ~DownloadFileObserver() {}
};

// Defines the interface to control how a file is downloaded.
class UI_EXPORT DownloadFileProvider
    : public base::RefCountedThreadSafe<DownloadFileProvider> {
 public:
  virtual bool Start(DownloadFileObserver* observer) = 0;
  virtual void Stop() = 0;
#if defined(OS_WIN)
  virtual IStream* GetStream() = 0;
#endif

 protected:
  friend class base::RefCountedThreadSafe<DownloadFileProvider>;
  virtual ~DownloadFileProvider() {}
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_DOWNLOAD_FILE_INTERFACE_H_
