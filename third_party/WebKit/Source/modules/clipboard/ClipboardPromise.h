// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipboardPromise_h
#define ClipboardPromise_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/WebKit/common/clipboard/clipboard.mojom-blink.h"

namespace blink {

class DataTransfer;
class ScriptPromiseResolver;

class ClipboardPromise final
    : public GarbageCollectedFinalized<ClipboardPromise>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ClipboardPromise);
  WTF_MAKE_NONCOPYABLE(ClipboardPromise);

 public:
  virtual ~ClipboardPromise(){};

  static ScriptPromise CreateForRead(ScriptState*);
  static ScriptPromise CreateForReadText(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*, DataTransfer*);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);

  virtual void Trace(blink::Visitor*);

 private:
  ClipboardPromise(ScriptState*);

  scoped_refptr<WebTaskRunner> GetTaskRunner();
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

  void HandleWrite(DataTransfer*);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);

  void HandleWriteText(const String&);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  ScriptState* script_state_;

  Member<ScriptPromiseResolver> script_promise_resolver_;

  mojom::blink::PermissionServicePtr permission_service_;

  mojom::ClipboardBuffer buffer_;

  WebString write_data_;
};

}  // namespace blink

#endif  // ClipboardPromise_h
