// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_HANDLE_H_

#include "third_party/blink/renderer/modules/filesystem/file_system_base_handle.h"
#include "third_party/blink/renderer/modules/filesystem/get_system_directory_options.h"

namespace blink {

class FileSystemDirectoryHandle : public FileSystemBaseHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FileSystemDirectoryHandle(DOMFileSystemBase*, const String& full_path);

  bool isDirectory() const override { return true; }
  ScriptPromise getFile(ScriptState*, const String& name);
  ScriptPromise getDirectory(ScriptState*, const String& name);

  static ScriptPromise getSystemDirectory(
      ScriptState*,
      const GetSystemDirectoryOptions& options);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_HANDLE_H_
