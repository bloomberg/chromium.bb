// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_file_reader.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class ScriptPromiseResolver;

// TODO(huangdarwin): Split this class up. There are unrelated responsibilities
// being handled in this class.
class ClipboardPromise final
    : public GarbageCollectedFinalized<ClipboardPromise>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ClipboardPromise);

 public:
  ClipboardPromise(ScriptState*);
  virtual ~ClipboardPromise();

  // Creates promise to execute Clipboard API functions off the main thread.
  static ScriptPromise CreateForRead(ScriptState*);
  static ScriptPromise CreateForReadText(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*, HeapVector<Member<Blob>>);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);

  // Entry points back into ClipboardPromise, from ClipboardFileReader.
  void OnLoadBufferComplete(DOMArrayBuffer*);
  void Reject();

  void Trace(blink::Visitor*) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  bool IsFocusedDocument(ExecutionContext*);

  // Checks for permissions (interacting with PermissionService).
  mojom::blink::PermissionService* GetPermissionService();
  void RequestReadPermission(
      mojom::blink::PermissionService::RequestPermissionCallback);
  void CheckWritePermission(
      mojom::blink::PermissionService::HasPermissionCallback);

  // Checks Read/Write permission (interacting with PermissionService).
  void HandleRead();
  void HandleReadText();
  void HandleWrite(HeapVector<Member<Blob>>*);
  void HandleWriteText(const String&);

  // Reads/Writes after permission check.
  void HandleReadWithPermission(mojom::blink::PermissionStatus);
  void HandleReadTextWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  void WriteNextRepresentation();

  // Decodes for writing.
  void DecodeImageOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner>,
      DOMArrayBuffer*);
  void DecodeTextOnBackgroundThread(scoped_refptr<base::SingleThreadTaskRunner>,
                                    DOMArrayBuffer*);

  // Writes decoded payload to System Clipboard.
  void WriteDecodedImage(sk_sp<SkImage>);
  void WriteDecodedText(const String&);

  // Detects whether an image or text is on the clipboard, and
  // returns all valid clipboard types on the clipboard.
  Vector<String> TypesToRead();

  // Gets Blob containing System Clipboard contents.
  Blob* ReadTextAsBlob();
  Blob* ReadImageAsBlob();

  bool IsValidClipboardType(const String&);

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;

  // Reads a Blob's data so that it can be written to the Clipboard.
  std::unique_ptr<ClipboardFileReader> file_reader_;
  // Checks for Read and Write permission.
  mojom::blink::PermissionServicePtr permission_service_;
  mojom::ClipboardBuffer buffer_;

  String write_data_;
  HeapVector<Member<Blob>> blob_sequence_data_;
  // Index of clipboard representation currently being processed.
  wtf_size_t clipboard_representation_index_;

  // Only used for reading Blobs via the ClipboardFileReader.
  scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;

  // Because v8 is thread-hostile, ensures that all interactions with
  // ScriptState and ScriptPromiseResolver occur on the main thread.
  SEQUENCE_CHECKER(async_clipboard_sequence_checker);

  DISALLOW_COPY_AND_ASSIGN(ClipboardPromise);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
