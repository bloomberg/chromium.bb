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
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_file_reader.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class DataTransfer;
class ScriptPromiseResolver;

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
  // TODO (crbug.com/916823): Move ReadImage and WriteImage into Read/Write
  // functions, so that the API surface doesn't change.
  static ScriptPromise CreateForReadImage(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*, DataTransfer*);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);
  static ScriptPromise CreateForWriteImage(ScriptState*, Blob*);

  void OnLoadComplete(DOMArrayBuffer*);
  void Reject();

  void Trace(blink::Visitor*) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();
  mojom::blink::PermissionService* GetPermissionService();

  void DecodeImageOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner>,
      DOMArrayBuffer*);
  void ResolveAndWriteImage(sk_sp<SkImage>);

  bool IsFocusedDocument(ExecutionContext*);

  void RequestReadPermission(
      mojom::blink::PermissionService::RequestPermissionCallback);
  void CheckWritePermission(
      mojom::blink::PermissionService::HasPermissionCallback);

  void HandleRead();
  void HandleReadWithPermission(mojom::blink::PermissionStatus);

  void HandleReadText();
  void HandleReadTextWithPermission(mojom::blink::PermissionStatus);

  void HandleReadImage();
  void HandleReadImageWithPermission(mojom::blink::PermissionStatus);

  void HandleWrite(DataTransfer*);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);

  void HandleWriteText(const String&);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  void HandleWriteImage(Blob*);
  void HandleWriteImageWithPermission(mojom::blink::PermissionStatus);

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;
  std::unique_ptr<ClipboardFileReader> file_reader_;
  mojom::blink::PermissionServicePtr permission_service_;
  mojom::ClipboardBuffer buffer_;

  String write_data_;
  Member<Blob> write_image_data_;

  SEQUENCE_CHECKER(async_clipboard_sequence_checker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
