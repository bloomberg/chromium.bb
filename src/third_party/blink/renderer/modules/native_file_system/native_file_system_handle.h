// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_

#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class NativeFileSystemDirectoryHandle;

class NativeFileSystemHandle : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NativeFileSystemHandle(const String& name);
  static NativeFileSystemHandle* CreateFromMojoEntry(
      mojom::blink::NativeFileSystemEntryPtr);

  virtual bool isFile() const { return false; }
  virtual bool isDirectory() const { return false; }
  const String& name() const { return name_; }

  ScriptPromise moveTo(ScriptState*,
                       NativeFileSystemDirectoryHandle* parent,
                       const String& new_name = String());
  ScriptPromise copyTo(ScriptState*,
                       NativeFileSystemDirectoryHandle* parent,
                       const String& new_name = String());
  ScriptPromise remove(ScriptState*);

  virtual mojom::blink::NativeFileSystemTransferTokenPtr Transfer() = 0;

 private:
  virtual void RemoveImpl(
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr)>) = 0;

  String name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_
