// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_H_

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace extensions {

// Implements custom bindings for the sync file system API.
class SyncFileSystemCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit SyncFileSystemCustomBindings(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // FileSystemObject GetSyncFileSystemObject(string name, string root_url):
  // construct a file system object from the given name and root_url.
  void GetSyncFileSystemObject(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_H_
