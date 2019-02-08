// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_

#include "base/sequence_checker.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
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
  WTF_MAKE_NONCOPYABLE(ClipboardPromise);

 public:
  ClipboardPromise(ScriptState*);
  virtual ~ClipboardPromise();

  static ScriptPromise CreateForRead(ScriptState*);
  static ScriptPromise CreateForReadText(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*, Blob*);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);

  void OnLoadBufferComplete(DOMArrayBuffer*);
  void Reject();

  void Trace(blink::Visitor*) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();
  mojom::blink::PermissionService* GetPermissionService();

  bool IsFocusedDocument(ExecutionContext*);

  void RequestReadPermission(
      mojom::blink::PermissionService::RequestPermissionCallback);
  void CheckWritePermission(
      mojom::blink::PermissionService::HasPermissionCallback);

  void HandleRead();
  void HandleReadWithPermission(mojom::blink::PermissionStatus);

  void HandleReadText();
  void HandleReadTextWithPermission(mojom::blink::PermissionStatus);

  void HandleWrite(Blob*);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);

  void HandleWriteText(const String&);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  void DecodeImageOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner>,
      DOMArrayBuffer*);
  void DecodeTextOnBackgroundThread(scoped_refptr<base::SingleThreadTaskRunner>,
                                    DOMArrayBuffer*);

  void ResolveAndWriteImage(sk_sp<SkImage>);
  void ResolveAndWriteText(const String&);

  // Detect whether an image or text is on the clipboard.
  // Prioritizes image/png over text/plain over none.
  String TypeToRead();

  // Get Blob containing System Clipboard contents.
  Blob* ReadTextAsBlob();
  Blob* ReadImageAsBlob();

  // Because v8 is thread-hostile, ensure that all interactions with ScriptState
  // and ScriptPromiseResolver occur on the main thread.
  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;

  std::unique_ptr<ClipboardFileReader> file_reader_;
  mojom::blink::PermissionServicePtr permission_service_;
  mojom::ClipboardBuffer buffer_;

  String write_data_;
  Member<Blob> blob_data_;

  scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;

  SEQUENCE_CHECKER(async_clipboard_sequence_checker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
