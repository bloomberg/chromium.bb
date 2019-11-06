// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_

#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/mojo/revocable_interface_ptr.h"

namespace blink {

class NativeFileSystemFileHandle final : public NativeFileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeFileSystemFileHandle(
      const String& name,
      RevocableInterfacePtr<mojom::blink::NativeFileSystemFileHandle>);

  bool isFile() const override { return true; }

  ScriptPromise createWriter(ScriptState*);
  ScriptPromise getFile(ScriptState*);

  mojom::blink::NativeFileSystemTransferTokenPtr Transfer() override;

  mojom::blink::NativeFileSystemFileHandle* MojoHandle() {
    return mojo_ptr_.get();
  }

 private:
  void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) override;
  void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) override;

  RevocableInterfacePtr<mojom::blink::NativeFileSystemFileHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_
