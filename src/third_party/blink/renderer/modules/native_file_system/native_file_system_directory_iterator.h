// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_

#include "base/files/file.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class NativeFileSystemDirectoryHandle;
class NativeFileSystemHandle;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class NativeFileSystemDirectoryIterator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NativeFileSystemDirectoryIterator(
      NativeFileSystemDirectoryHandle* directory);

  ScriptPromise next(ScriptState*);
  // TODO(mek): This return method should cancel the backend directory iteration
  // operation, to avoid doing useless work.
  void IteratorReturn() {}

  void Trace(Visitor*) override;

 private:
  void OnGotEntries(mojom::blink::NativeFileSystemErrorPtr result,
                    Vector<mojom::blink::NativeFileSystemEntryPtr> entries);

  base::File::Error error_ = base::File::FILE_OK;
  bool waiting_for_more_entries_ = true;
  HeapDeque<Member<NativeFileSystemHandle>> entries_;
  Member<ScriptPromiseResolver> pending_next_;
  Member<NativeFileSystemDirectoryHandle> directory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
