// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardWriter's derived classes.

// Writes a Blob with image/png content to the System Clipboard.
class ClipboardImageWriter final : public ClipboardWriter {
 public:
  ClipboardImageWriter(SystemClipboard* system_clipboard,
                       ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardImageWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* raw_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardImageWriter::DecodeOnBackgroundThread,
                            WrapCrossThreadPersistent(raw_data),
                            WrapCrossThreadPersistent(this), task_runner));
  }
  static void DecodeOnBackgroundThread(
      DOMArrayBuffer* png_data,
      ClipboardImageWriter* writer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(!IsMainThread());
    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
        SegmentReader::CreateFromSkData(
            SkData::MakeWithoutCopy(png_data->Data(), png_data->ByteLength())),
        true, ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
        ColorBehavior::Tag());
    sk_sp<SkImage> image = nullptr;
    // `decoder` is nullptr if `png_data` doesn't begin with the PNG signature.
    if (decoder)
      image = ImageBitmap::GetSkImageFromDecoder(std::move(decoder));

    PostCrossThreadTask(*task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardImageWriter::Write,
                                            WrapCrossThreadPersistent(writer),
                                            std::move(image)));
  }
  void Write(sk_sp<SkImage> image) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!image) {
      promise_->RejectFromReadOrDecodeFailure();
      return;
    }
    if (!promise_->GetLocalFrame())
      return;
    SkBitmap bitmap;
    image->asLegacyBitmap(&bitmap);
    system_clipboard()->WriteImage(std::move(bitmap));
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a Blob with text/plain content to the System Clipboard.
class ClipboardTextWriter final : public ClipboardWriter {
 public:
  ClipboardTextWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardTextWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* raw_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ClipboardTextWriter::DecodeOnBackgroundThread,
                            WrapCrossThreadPersistent(raw_data),
                            WrapCrossThreadPersistent(this), task_runner));
  }
  static void DecodeOnBackgroundThread(
      DOMArrayBuffer* raw_data,
      ClipboardTextWriter* writer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(!IsMainThread());

    String wtf_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(raw_data->Data()),
                         raw_data->ByteLength());
    DCHECK(wtf_string.IsSafeToSendToAnotherThread());
    PostCrossThreadTask(*task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ClipboardTextWriter::Write,
                                            WrapCrossThreadPersistent(writer),
                                            std::move(wtf_string)));
  }
  void Write(const String& text) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!promise_->GetLocalFrame())
      return;
    system_clipboard()->WritePlainText(text);

    promise_->CompleteWriteRepresentation();
  }
};

// Writes a blob with text/html content to the System Clipboard.
class ClipboardHtmlWriter final : public ClipboardWriter {
 public:
  ClipboardHtmlWriter(SystemClipboard* system_clipboard,
                      ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardHtmlWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* html_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto* execution_context = promise_->GetExecutionContext();
    if (!execution_context)
      return;
    execution_context->CountUse(WebFeature::kHtmlClipboardApiWrite);

    String html_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(html_data->Data()),
                         html_data->ByteLength());

    // Sanitizing on the main thread because HTML DOM nodes can only be used
    // on the main thread.
    KURL url;
    unsigned fragment_start = 0;
    unsigned fragment_end = html_string.length();

    LocalFrame* local_frame = promise_->GetLocalFrame();
    if (!local_frame)
      return;
    Document* document = local_frame->GetDocument();
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *document, html_string, fragment_start, fragment_end, url);
    String sanitized_html =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);
    Write(sanitized_html, url);
  }

  void Write(const String& sanitized_html, KURL url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteHTML(sanitized_html, url);
    promise_->CompleteWriteRepresentation();
  }
};

// Writes a blob with image/svg+xml content to the System Clipboard.
class ClipboardSvgWriter final : public ClipboardWriter {
 public:
  ClipboardSvgWriter(SystemClipboard* system_clipboard,
                     ClipboardPromise* promise)
      : ClipboardWriter(system_clipboard, promise) {}
  ~ClipboardSvgWriter() override = default;

 private:
  void StartWrite(
      DOMArrayBuffer* svg_data,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    String svg_string =
        String::FromUTF8(reinterpret_cast<const LChar*>(svg_data->Data()),
                         svg_data->ByteLength());

    // Sanitizing on the main thread because SVG/XML DOM nodes can only be used
    // on the main thread.
    KURL url;
    unsigned fragment_start = 0;
    unsigned fragment_end = svg_string.length();

    LocalFrame* local_frame = promise_->GetLocalFrame();
    if (!local_frame)
      return;
    Document* document = local_frame->GetDocument();
    DocumentFragment* fragment = CreateSanitizedFragmentFromMarkupWithContext(
        *document, svg_string, fragment_start, fragment_end, url);
    String sanitized_svg =
        CreateMarkup(fragment, kIncludeNode, kResolveAllURLs);
    Write(sanitized_svg);
  }

  void Write(const String& svg_html) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    system_clipboard()->WriteSvg(svg_html);
    promise_->CompleteWriteRepresentation();
  }
};

}  // anonymous namespace

// ClipboardWriter functions.

// static
ClipboardWriter* ClipboardWriter::Create(SystemClipboard* system_clipboard,
                                         const String& mime_type,
                                         ClipboardPromise* promise) {
  DCHECK(ClipboardWriter::IsValidType(mime_type, /*is_raw=*/false));
  if (mime_type == kMimeTypeImagePng) {
    return MakeGarbageCollected<ClipboardImageWriter>(system_clipboard,
                                                      promise);
  }
  if (mime_type == kMimeTypeTextPlain)
    return MakeGarbageCollected<ClipboardTextWriter>(system_clipboard, promise);

  if (mime_type == kMimeTypeTextHTML)
    return MakeGarbageCollected<ClipboardHtmlWriter>(system_clipboard, promise);

  if (mime_type == kMimeTypeImageSvg &&
      RuntimeEnabledFeatures::ClipboardSvgEnabled())
    return MakeGarbageCollected<ClipboardSvgWriter>(system_clipboard, promise);

  NOTREACHED()
      << "IsValidType() and Create() have inconsistent implementations.";
  return nullptr;
}

ClipboardWriter::ClipboardWriter(SystemClipboard* system_clipboard,
                                 ClipboardPromise* promise)
    : promise_(promise),
      clipboard_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction)),
      file_reading_task_runner_(promise->GetExecutionContext()->GetTaskRunner(
          TaskType::kFileReading)),
      system_clipboard_(system_clipboard),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {}

ClipboardWriter::~ClipboardWriter() {
  DCHECK(!file_reader_);
}

// static
bool ClipboardWriter::IsValidType(const String& type,
                                  bool is_custom_format_type) {
  if (is_custom_format_type)
    return type.length() < mojom::blink::RawClipboardHost::kMaxFormatSize;

  if (type == kMimeTypeImageSvg)
    return RuntimeEnabledFeatures::ClipboardSvgEnabled();

  // TODO(https://crbug.com/1029857): Add support for other types.
  return type == kMimeTypeImagePng || type == kMimeTypeTextPlain ||
         type == kMimeTypeTextHTML;
}

void ClipboardWriter::WriteToSystem(Blob* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!file_reader_);
  file_reader_ = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsArrayBuffer, this,
      std::move(file_reading_task_runner_));
  file_reader_->Start(blob->GetBlobDataHandle());
}

// FileReaderLoaderClient implementation.

void ClipboardWriter::DidStartLoading() {}
void ClipboardWriter::DidReceiveData() {}

void ClipboardWriter::DidFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DOMArrayBuffer* array_buffer = file_reader_->ArrayBufferResult();
  DCHECK(array_buffer);

  file_reader_.reset();
  self_keep_alive_.Clear();

  StartWrite(array_buffer, clipboard_task_runner_);
}

void ClipboardWriter::DidFail(FileErrorCode error_code) {
  file_reader_.reset();
  self_keep_alive_.Clear();
  promise_->RejectFromReadOrDecodeFailure();
}

void ClipboardWriter::Trace(Visitor* visitor) const {
  visitor->Trace(promise_);
  visitor->Trace(system_clipboard_);
}

}  // namespace blink
