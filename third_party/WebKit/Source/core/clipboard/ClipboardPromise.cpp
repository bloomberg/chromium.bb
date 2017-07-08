// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/clipboard/ClipboardPromise.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/clipboard/DataTransferItem.h"
#include "core/clipboard/DataTransferItemList.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/clipboard/ClipboardMimeTypes.h"
#include "public/platform/Platform.h"

namespace blink {

ScriptPromise ClipboardPromise::CreateForRead(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&ClipboardPromise::HandleRead,
                                 WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForReadText(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                                 WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWrite(ScriptState* script_state,
                                               DataTransfer* data) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWrite,
                WrapPersistent(clipboard_promise), WrapPersistent(data)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWriteText(ScriptState* script_state,
                                                   const String& data) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWriteText,
                                 WrapPersistent(clipboard_promise), data));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ClipboardPromise::ClipboardPromise(ScriptState* script_state)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      script_promise_resolver_(ScriptPromiseResolver::Create(script_state)),
      buffer_(WebClipboard::kBufferStandard) {}

WebTaskRunner* ClipboardPromise::GetTaskRunner() {
  // TODO(garykac): Replace MiscPlatformAPI with TaskType specific to clipboard.
  return TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI,
                               GetExecutionContext())
      .Get();
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleRead() {
  DCHECK(script_promise_resolver_);
  String plain_text = Platform::Current()->Clipboard()->ReadPlainText(buffer_);

  const DataTransfer::DataTransferType type =
      DataTransfer::DataTransferType::kCopyAndPaste;
  const DataTransferAccessPolicy access =
      DataTransferAccessPolicy::kDataTransferReadable;
  DataObject* data = DataObject::CreateFromString(plain_text);
  DataTransfer* dt = DataTransfer::Create(type, access, data);
  script_promise_resolver_->Resolve(dt);
}

void ClipboardPromise::HandleReadText() {
  DCHECK(script_promise_resolver_);
  String text = Platform::Current()->Clipboard()->ReadPlainText(buffer_);
  script_promise_resolver_->Resolve(text);
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleWrite(DataTransfer* data) {
  DCHECK(script_promise_resolver_);
  size_t num_items = data->items()->length();
  for (unsigned long i = 0; i < num_items; i++) {
    DataTransferItem* item = data->items()->item(i);
    DataObjectItem* objectItem = item->GetDataObjectItem();
    if (objectItem->Kind() == DataObjectItem::kStringKind &&
        objectItem->GetType() == kMimeTypeTextPlain) {
      String text = objectItem->GetAsString();
      Platform::Current()->Clipboard()->WritePlainText(text);
      script_promise_resolver_->Resolve();
      return;
    }
  }
  script_promise_resolver_->Reject();
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK(script_promise_resolver_);
  Platform::Current()->Clipboard()->WritePlainText(data);
  script_promise_resolver_->Resolve();
}

DEFINE_TRACE(ClipboardPromise) {
  visitor->Trace(script_promise_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
