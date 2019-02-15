// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
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

ScriptPromise ClipboardPromise::CreateForWrite(ScriptState* script_state,
                                               Blob* data) {
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

ClipboardPromise::ClipboardPromise(ScriptState* script_state)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      script_state_(script_state),
      script_promise_resolver_(ScriptPromiseResolver::Create(script_state)),
      buffer_(mojom::ClipboardBuffer::kStandard),
      file_reading_task_runner_(
          GetExecutionContext()->GetTaskRunner(TaskType::kFileReading)) {}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  // TODO(garykac): Replace MiscPlatformAPI with TaskType specific to clipboard.
  return GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
}

PermissionService* ClipboardPromise::GetPermissionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (!permission_service_) {
    ConnectToPermissionService(ExecutionContext::From(script_state_),
                               mojo::MakeRequest(&permission_service_));
  }
  return permission_service_.get();
}

bool ClipboardPromise::IsFocusedDocument(ExecutionContext* context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL
  Document* doc = To<Document>(context);
  return doc && doc->hasFocus();
}

void ClipboardPromise::RequestReadPermission(
    PermissionService::RequestPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  DCHECK(script_promise_resolver_);

  if (!IsFocusedDocument(ExecutionContext::From(script_state_))) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Document is not focused."));
    return;
  }
  if (!GetPermissionService()) {
    script_promise_resolver_->Reject(
        DOMException::Create(DOMExceptionCode::kNotAllowedError,
                             "Permission Service could not connect."));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  DCHECK(script_promise_resolver_);

  if (!IsFocusedDocument(ExecutionContext::From(script_state_))) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Document is not focused."));
    return;
  }
  if (!GetPermissionService()) {
    script_promise_resolver_->Reject(
        DOMException::Create(DOMExceptionCode::kNotAllowedError,
                             "Permission Service could not connect."));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  RequestReadPermission(WTF::Bind(&ClipboardPromise::HandleReadWithPermission,
                                  WrapPersistent(this)));
}

// TODO(garykac): This currently only handles images and plain text.
void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  String type_to_read = TypeToRead();
  Blob* blob = nullptr;

  if (type_to_read == kMimeTypeImagePng) {
    blob = ReadImageAsBlob();
  } else if (type_to_read == kMimeTypeTextPlain) {
    blob = ReadTextAsBlob();
  } else {
    // Reject when there's no data on clipboard.
  }

  if (!blob) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kDataError, "No valid data on clipboard."));
    return;
  }

  script_promise_resolver_->Resolve(std::move(blob));
}

void ClipboardPromise::HandleReadText() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  RequestReadPermission(WTF::Bind(
      &ClipboardPromise::HandleReadTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleReadTextWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  String text = SystemClipboard::GetInstance().ReadPlainText(buffer_);
  script_promise_resolver_->Resolve(text);
}

// TODO(huangdarwin): This currently only handles plain text and images.
// TODO(huangdarwin): Use an array or structure to hold multiple
// Blobs, like how DataTransfer holds multiple DataTransferItems.
void ClipboardPromise::HandleWrite(Blob* data) {
  blob_data_ = data;

  CheckWritePermission(WTF::Bind(&ClipboardPromise::HandleWriteWithPermission,
                                 WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  String type_to_read = blob_data_->type();
  if (type_to_read == kMimeTypeImagePng || type_to_read == kMimeTypeTextPlain) {
    DCHECK(!file_reader_);
    file_reader_ = std::make_unique<ClipboardFileReader>(
        blob_data_, this, file_reading_task_runner_);
  } else {
    script_promise_resolver_->Reject(
        DOMException::Create(DOMExceptionCode::kNotAllowedError,
                             "Write type " + type_to_read + " not supported."));
    return;
  }
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  write_data_ = data;
  CheckWritePermission(WTF::Bind(
      &ClipboardPromise::HandleWriteTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteTextWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  ResolveAndWriteText(write_data_);
}

void ClipboardPromise::OnLoadBufferComplete(DOMArrayBuffer* array_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  String blob_type = blob_data_->type();
  DCHECK(blob_type == kMimeTypeImagePng || blob_type == kMimeTypeTextPlain);

  file_reader_.reset();

  if (blob_type == kMimeTypeImagePng) {
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBind(&ClipboardPromise::DecodeImageOnBackgroundThread,
                        WrapCrossThreadPersistent(this), GetTaskRunner(),
                        WrapCrossThreadPersistent(array_buffer)));
  } else if (blob_type == kMimeTypeTextPlain) {
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBind(&ClipboardPromise::DecodeTextOnBackgroundThread,
                        WrapCrossThreadPersistent(this), GetTaskRunner(),
                        WrapCrossThreadPersistent(array_buffer)));
  }
}

// Reference: third_party/blink/renderer/core/imagebitmap/.
// Logic modified from CropImageAndApplyColorSpaceConversion, but not using
// directly due to extra complexity in imagebitmap that isn't necessary for
// async clipboard image decoding.
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

void ClipboardPromise::DecodeTextOnBackgroundThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DOMArrayBuffer* raw_data) {
  DCHECK(!IsMainThread());

  String wtf_string = String::FromUTF8(
      reinterpret_cast<const LChar*>(raw_data->Data()), raw_data->ByteLength());

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBind(&ClipboardPromise::ResolveAndWriteText,
                      WrapCrossThreadPersistent(this), std::move(wtf_string)));
}

void ClipboardPromise::ResolveAndWriteImage(sk_sp<SkImage> image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SkBitmap bitmap;
  image->asLegacyBitmap(&bitmap);

  SystemClipboard::GetInstance().WriteImage(std::move(bitmap));
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::ResolveAndWriteText(const String& text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SystemClipboard::GetInstance().WritePlainText(text);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::Reject() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  script_promise_resolver_->Reject();
}

// TODO(huangdarwin): This is beginning to share responsibility
// with data_object.cc, so how can we share some code?
String ClipboardPromise::TypeToRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  String type_to_read;
  for (const String& available_type :
       SystemClipboard::GetInstance().ReadAvailableTypes()) {
    if (available_type == kMimeTypeImagePng)
      return available_type;
    if (available_type == kMimeTypeTextPlain)
      type_to_read = available_type;
  }
  return type_to_read;
}

Blob* ClipboardPromise::ReadTextAsBlob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  String plain_text = SystemClipboard::GetInstance().ReadPlainText(buffer_);

  // Encode text to UTF-8, the standard text format for blobs.
  CString utf_text = plain_text.Utf8();

  return Blob::Create(reinterpret_cast<const unsigned char*>(utf_text.data()),
                      utf_text.length(), kMimeTypeTextPlain);
}

Blob* ClipboardPromise::ReadImageAsBlob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SkBitmap bitmap = SystemClipboard::GetInstance().ReadImage(buffer_);

  // Encode bitmap to Vector<uint8_t> on main thread.
  SkPixmap pixmap;
  bitmap.peekPixels(&pixmap);

  Vector<uint8_t> png_data;
  SkPngEncoder::Options options;
  if (!ImageEncoder::Encode(&png_data, pixmap, options)) {
    return nullptr;
  }

  return Blob::Create(png_data.data(), png_data.size(), kMimeTypeImagePng);
}

void ClipboardPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(blob_data_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
