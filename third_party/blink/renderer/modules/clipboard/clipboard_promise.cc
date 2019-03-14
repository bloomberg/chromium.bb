// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
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

// There are 2 clipboard permissions defined in the spec:
// * clipboard-read
// * clipboard-write
// See https://w3c.github.io/clipboard-apis/#clipboard-permissions
//
// In Chromium we automatically grant clipboard-write access and clipboard-read
// access is gated behind a permission prompt. Both clipboard read and write
// require the tab to be focused (and Chromium must be the foreground app) for
// the operation to be allowed.
//
// Writing a Blob's data to the system clipboard is accomplished by:
// (1) Reading the blob's contents using a ClipboardFileReader.
// (2) Decoding the blob's contents to avoid RCE in native applications that may
//     vulnerabilities in their decoders.
// (3) Writing the blob's decoded contents to the system clipboard.
//
// Reading a type from the system clipboard to a Blob is accomplished by:
// (1) Reading the item from the system clipboard
// (2) Encoding the blob's contents.
// (3) Writing the contents to a blob.
//
// TODO (huangdarwin): Strongly consider making a class break to have
// decoders and encoders in a separate class, with this class mostly checking
// for permissions, and the next class doing decoding/encoding for individual
// types.
//
// TODO (huangdarwin): Use a SequencedTaskRunner to write one clipboard type
// before the next one? This could allow for removing awkward logic regarding
// clipboard_representation_index_.

namespace blink {

using mojom::blink::PermissionStatus;
using mojom::blink::PermissionService;

ClipboardPromise::~ClipboardPromise() = default;

// static
ScriptPromise ClipboardPromise::CreateForRead(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleRead,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForReadText(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForWrite(ScriptState* script_state,
                                               HeapVector<Member<Blob>> data) {
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(script_state);
  HeapVector<Member<Blob>>* blob_sequence =
      MakeGarbageCollected<HeapVector<Member<Blob>>>(data);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWrite,
                           WrapPersistent(clipboard_promise),
                           WrapPersistent(blob_sequence)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
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
      clipboard_representation_index_(0),
      file_reading_task_runner_(
          GetExecutionContext()->GetTaskRunner(TaskType::kFileReading)) {}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  // TODO(https://crbug.com/941835): Replace MiscPlatformAPI with TaskType
  // specific to clipboard.
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

void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  Vector<String> types_to_read = TypesToRead();
  HeapVector<Member<Blob>> blobs;
  blobs.ReserveInitialCapacity(types_to_read.size());
  for (const String& type_to_read : types_to_read) {
    if (type_to_read == kMimeTypeImagePng) {
      blobs.push_back(ReadImageAsBlob());
    } else if (type_to_read == kMimeTypeTextPlain) {
      blobs.push_back(ReadTextAsBlob());
    } else {
      NOTREACHED() << "Type " << type_to_read << " was not implemented";
    }
  }

  if (!blobs.size()) {
    script_promise_resolver_->Reject(DOMException::Create(
        DOMExceptionCode::kDataError, "No valid data on clipboard."));
    return;
  }

  script_promise_resolver_->Resolve(std::move(blobs));
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

void ClipboardPromise::HandleWrite(HeapVector<Member<Blob>>* data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  CHECK(data);
  blob_sequence_data_ = std::move(*data);

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

  // Check that all blobs have valid and unique MIME types.
  HashSet<String> unique_types;
  unique_types.ReserveCapacityForSize(blob_sequence_data_.size());
  for (const Member<Blob>& blob : blob_sequence_data_) {
    String type = blob->type();
    if (!IsValidClipboardType(type)) {
      script_promise_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kNotAllowedError,
                               "Write type " + type + " not supported."));
      return;
    }
    if (unique_types.Contains(type)) {
      script_promise_resolver_->Reject(
          DOMException::Create(DOMExceptionCode::kNotAllowedError,
                               "Attempting to write duplicate type " + type +
                                   +". All types must be unique"));
      return;
    }
    unique_types.insert(type);
  }

  DCHECK(!clipboard_representation_index_);
  WriteNextRepresentation();
}

// Called to begin writing a type, or after writing each type.
void ClipboardPromise::WriteNextRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  if (clipboard_representation_index_ == blob_sequence_data_.size()) {
    SystemClipboard::GetInstance().CommitWrite();
    script_promise_resolver_->Resolve();
    return;
  }

  const Member<Blob>& blob =
      blob_sequence_data_[clipboard_representation_index_];
  clipboard_representation_index_++;
  DCHECK(IsValidClipboardType(blob->type()));

  // Creates a FileReader to read the Blob.
  DCHECK(!file_reader_);
  file_reader_ = std::make_unique<ClipboardFileReader>(
      blob, this, file_reading_task_runner_);
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

  SystemClipboard::GetInstance().WritePlainText(write_data_);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::OnLoadBufferComplete(DOMArrayBuffer* array_buffer) {
  DCHECK(array_buffer);
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  file_reader_.reset();

  String blob_type =
      blob_sequence_data_[clipboard_representation_index_ - 1]->type();
  DCHECK(IsValidClipboardType(blob_type));

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
  } else {
    NOTREACHED();
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
      CrossThreadBind(&ClipboardPromise::WriteDecodedImage,
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
      CrossThreadBind(&ClipboardPromise::WriteDecodedText,
                      WrapCrossThreadPersistent(this), std::move(wtf_string)));
}

void ClipboardPromise::WriteDecodedImage(sk_sp<SkImage> image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SkBitmap bitmap;
  image->asLegacyBitmap(&bitmap);

  SystemClipboard::GetInstance().WriteImageNoCommit(std::move(bitmap));
  WriteNextRepresentation();
}

void ClipboardPromise::WriteDecodedText(const String& text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  SystemClipboard::GetInstance().WritePlainTextNoCommit(text);
  WriteNextRepresentation();
}

void ClipboardPromise::Reject() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);

  script_promise_resolver_->Reject(DOMException::Create(
      DOMExceptionCode::kDataError,
      "Failed to read Blob for clipboard item type " +
          blob_sequence_data_[clipboard_representation_index_]->type() + "."));
}

// TODO(huangdarwin): This is beginning to share responsibility
// with data_object.cc, so how can we share some code?
Vector<String> ClipboardPromise::TypesToRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_clipboard_sequence_checker);
  Vector<String> available_types =
      SystemClipboard::GetInstance().ReadAvailableTypes();
  Vector<String> types_to_read;
  types_to_read.ReserveInitialCapacity(available_types.size());
  for (const String& available_type : available_types) {
    if (IsValidClipboardType(available_type))
      types_to_read.push_back(available_type);
  }
  return types_to_read;
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

bool ClipboardPromise::IsValidClipboardType(const String& type) {
  // TODO (https://crbug.com/931839): Add support for text/html and other types.
  return type == kMimeTypeImagePng || type == kMimeTypeTextPlain;
}

void ClipboardPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(blob_sequence_data_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
