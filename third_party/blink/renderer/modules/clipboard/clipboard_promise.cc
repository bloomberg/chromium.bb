// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item_list.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

// And now, a brief note about clipboard permissions.
//
// There are 2 clipboard permissions defined in the spec:
// * clipboard-read
// * clipboard-write
// See https://w3c.github.io/clipboard-apis/#clipboard-permissions
//
// In Chromium we automatically grant clipboard-write access and clipboard-read
// access is gated behind a permission prompt. Both clipboard read and write
// require the tab to be focused (and Chromium must be the foreground app) for
// the operation to be allowed.

namespace blink {

using mojom::blink::PermissionStatus;
using mojom::blink::PermissionService;

ClipboardPromise::~ClipboardPromise() = default;

ScriptPromise ClipboardPromise::CreateForRead(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleRead,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForReadText(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForReadImage(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadImage,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWrite(ScriptState* script_state,
                                               DataTransfer* data) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWrite,
                WrapPersistent(clipboard_promise), WrapPersistent(data)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWriteText(ScriptState* script_state,
                                                   const String& data) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWriteText,
                           WrapPersistent(clipboard_promise), data));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWriteImage(ScriptState* script_state,
                                                    Blob* data) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWriteImage,
                WrapPersistent(clipboard_promise), WrapPersistent(data)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ClipboardPromise::ClipboardPromise(ScriptState* script_state)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      script_state_(script_state),
      script_promise_resolver_(ScriptPromiseResolver::Create(script_state)),
      buffer_(mojom::ClipboardBuffer::kStandard) {}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  // TODO(garykac): Replace MiscPlatformAPI with TaskType specific to clipboard.
  return GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
}

PermissionService* ClipboardPromise::GetPermissionService() {
  if (!permission_service_) {
    ConnectToPermissionService(ExecutionContext::From(script_state_),
                               mojo::MakeRequest(&permission_service_));
  }
  return permission_service_.get();
}

bool ClipboardPromise::IsFocusedDocument(ExecutionContext* context) {
  Document* doc = To<Document>(context);
  return doc && doc->hasFocus();
}

void ClipboardPromise::RequestReadPermission(
    PermissionService::RequestPermissionCallback callback) {
  DCHECK(script_promise_resolver_);

  ExecutionContext* context = ExecutionContext::From(script_state_);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL

  // Document must be focused.
  if (!IsFocusedDocument(context) || !GetPermissionService()) {
    script_promise_resolver_->Reject();
    return;
  }

  // Query for permission if necessary.
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->RequestPermission(
      CreateClipboardPermissionDescriptor(
          mojom::blink::PermissionName::CLIPBOARD_READ, false),
      false, std::move(callback));
}

void ClipboardPromise::CheckWritePermission(
    PermissionService::HasPermissionCallback callback) {
  DCHECK(script_promise_resolver_);

  ExecutionContext* context = ExecutionContext::From(script_state_);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL

  // Document must be focused.
  if (!IsFocusedDocument(context) || !GetPermissionService()) {
    script_promise_resolver_->Reject();
    return;
  }

  // Check current permission (but do not query the user).
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->HasPermission(
      CreateClipboardPermissionDescriptor(
          mojom::blink::PermissionName::CLIPBOARD_WRITE, false),
      std::move(callback));
}

void ClipboardPromise::HandleRead() {
  RequestReadPermission(WTF::Bind(&ClipboardPromise::HandleReadWithPermission,
                                  WrapPersistent(this)));
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  String plain_text = SystemClipboard::GetInstance().ReadPlainText(buffer_);

  const DataTransfer::DataTransferType type =
      DataTransfer::DataTransferType::kCopyAndPaste;
  const DataTransferAccessPolicy access = DataTransferAccessPolicy::kReadable;
  DataObject* data = DataObject::CreateFromString(plain_text);
  DataTransfer* dt = DataTransfer::Create(type, access, data);
  script_promise_resolver_->Resolve(dt);
}

void ClipboardPromise::HandleReadText() {
  RequestReadPermission(WTF::Bind(
      &ClipboardPromise::HandleReadTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleReadTextWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  String text = SystemClipboard::GetInstance().ReadPlainText(buffer_);
  script_promise_resolver_->Resolve(text);
}

void ClipboardPromise::HandleReadImage() {
  RequestReadPermission(WTF::Bind(
      &ClipboardPromise::HandleReadImageWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleReadImageWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  SkBitmap bitmap = SystemClipboard::GetInstance().ReadImage(buffer_);

  SkPixmap pixmap;
  bitmap.peekPixels(&pixmap);

  Vector<uint8_t> png_data;
  SkPngEncoder::Options options;
  if (!ImageEncoder::Encode(&png_data, pixmap, options)) {
    script_promise_resolver_->Reject();
    return;
  }

  std::unique_ptr<BlobData> data = BlobData::Create();
  data->SetContentType(kMimeTypeImagePng);
  data->AppendBytes(png_data.data(), png_data.size());
  const uint64_t length = data->length();
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(data), length);

  Blob* blob = Blob::Create(blob_data_handle);
  script_promise_resolver_->Resolve(blob);
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleWrite(DataTransfer* data) {
  // Scan DataTransfer and extract data types that we support.
  uint32_t num_items = data->items()->length();
  for (uint32_t i = 0; i < num_items; i++) {
    DataTransferItem* item = data->items()->item(i);
    DataObjectItem* objectItem = item->GetDataObjectItem();
    if (objectItem->Kind() == DataObjectItem::kStringKind &&
        objectItem->GetType() == kMimeTypeTextPlain) {
      write_data_ = objectItem->GetAsString();
      break;
    }
  }
  CheckWritePermission(WTF::Bind(&ClipboardPromise::HandleWriteWithPermission,
                                 WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  SystemClipboard::GetInstance().WritePlainText(write_data_);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::HandleWriteText(const String& data) {
  write_data_ = data;
  CheckWritePermission(WTF::Bind(
      &ClipboardPromise::HandleWriteTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteImage(Blob* data) {
  write_image_data_ = data;

  CheckWritePermission(WTF::Bind(
      &ClipboardPromise::HandleWriteImageWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteImageWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  file_reader_ = std::make_unique<ClipboardFileReader>(write_image_data_, this);
}

void ClipboardPromise::HandleWriteTextWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  SystemClipboard::GetInstance().WritePlainText(write_data_);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::OnLoadComplete(DOMArrayBuffer* array_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  file_reader_.reset();

  // Schedule async image decode on another thread.
  background_scheduler::PostOnBackgroundThread(
      FROM_HERE,
      CrossThreadBind(&ClipboardPromise::DecodeImageOnBackgroundThread,
                      WrapCrossThreadPersistent(this), GetTaskRunner(),
                      WrapCrossThreadPersistent(array_buffer)));
}

// Reference: third_party/blink/renderer/core/imagebitmap/
// TODO (crbug.com/916821): Ask ImageBitmapFactory owners if they can help
// refactor and merge this very similar image decoding logic.
void ClipboardPromise::DecodeImageOnBackgroundThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DOMArrayBuffer* png_data) {
  DCHECK(!IsMainThread());

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      SegmentReader::CreateFromSkData(
          SkData::MakeWithoutCopy(png_data->Data(), png_data->ByteLength())),
      true, ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::Tag());

  if (!decoder) {
    PostCrossThreadTask(*task_runner, FROM_HERE,
                        CrossThreadBind(&ClipboardPromise::Reject,
                                        WrapCrossThreadPersistent(this)));
    return;
  }

  sk_sp<SkImage> image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));

  if (!image) {
    PostCrossThreadTask(*task_runner, FROM_HERE,
                        CrossThreadBind(&ClipboardPromise::Reject,
                                        WrapCrossThreadPersistent(this)));
    return;
  }

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBind(&ClipboardPromise::ResolveAndWriteImage,
                      WrapCrossThreadPersistent(this), std::move(image)));
}

void ClipboardPromise::ResolveAndWriteImage(sk_sp<SkImage> image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SkBitmap bitmap;
  image->asLegacyBitmap(&bitmap);

  SystemClipboard::GetInstance().WriteImage(std::move(bitmap));
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::Reject() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  script_promise_resolver_->Reject();
}

void ClipboardPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(write_image_data_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
