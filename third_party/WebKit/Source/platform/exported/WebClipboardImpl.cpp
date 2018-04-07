// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/exported/web_clipboard_impl.h"

#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/escape.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

struct DragData {
  WTF::String text;
  WTF::String html;
  WTF::HashMap<WTF::String, WTF::String> custom_data;
};

// This plagiarizes the logic in DropDataBuilder::Build, but only extracts the
// data needed for the implementation of WriteDataObject.
// TODO(slangley): Use a mojo struct to send web_drag_data and allow receiving
// side to extract the data required.
DragData BuildDragData(const WebDragData& web_drag_data) {
  DragData result;

  const WebVector<WebDragData::Item>& item_list = web_drag_data.Items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    if (item.storage_type == WebDragData::Item::kStorageTypeString) {
      if (item.string_type == blink::kMimeTypeTextPlain) {
        result.text = item.string_data;
      } else if (item.string_type == blink::kMimeTypeTextHTML) {
        result.html = item.string_data;
      } else if (item.string_type != blink::kMimeTypeTextURIList &&
                 item.string_type != blink::kMimeTypeDownloadURL) {
        result.custom_data.insert(item.string_type, item.string_data);
      }
    }
  }

  return result;
};

String EscapeForHTML(const String& str) {
  std::string output =
      net::EscapeForHTML(StringUTF8Adaptor(str).AsStringPiece());
  return String(output.c_str());
}
// TODO(slangley): crbug.com/775830 Remove the implementation of
// URLToImageMarkup from clipboard_utils.h once we can delete
// MockWebClipboardImpl.
WTF::String URLToImageMarkup(const WebURL& url, const WTF::String& title) {
  WTF::String markup("<img src=\"");
  markup.append(EscapeForHTML(url.GetString()));
  markup.append("\"");
  if (!title.IsEmpty()) {
    markup.append(" alt=\"");
    markup.append(EscapeForHTML(title));
    markup.append("\"");
  }
  markup.append("/>");
  return markup;
}

String EnsureNotNullWTFString(const WebString& string) {
  String result = string;
  if (result.IsNull()) {
    return g_empty_string16_bit;
  }
  return result;
}

}  // namespace

WebClipboardImpl::WebClipboardImpl() {
  Platform::Current()->GetConnector()->BindInterface(
      Platform::Current()->GetBrowserServiceName(), &clipboard_);
}

WebClipboardImpl::~WebClipboardImpl() = default;

uint64_t WebClipboardImpl::SequenceNumber(mojom::ClipboardBuffer buffer) {
  uint64_t result = 0;
  if (!IsValidBufferType(buffer))
    return 0;

  clipboard_->GetSequenceNumber(buffer, &result);
  return result;
}

bool WebClipboardImpl::IsFormatAvailable(mojom::ClipboardFormat format,
                                         mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return false;

  bool result = false;
  clipboard_->IsFormatAvailable(format, buffer, &result);
  return result;
}

WebVector<WebString> WebClipboardImpl::ReadAvailableTypes(
    mojom::ClipboardBuffer buffer,
    bool* contains_filenames) {
  WTF::Vector<WTF::String> types;
  if (IsValidBufferType(buffer)) {
    clipboard_->ReadAvailableTypes(buffer, &types, contains_filenames);
  }
  return types;
}

WebString WebClipboardImpl::ReadPlainText(mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebString();

  WTF::String text;
  clipboard_->ReadText(buffer, &text);
  return text;
}

WebString WebClipboardImpl::ReadHTML(mojom::ClipboardBuffer buffer,
                                     WebURL* source_url,
                                     unsigned* fragment_start,
                                     unsigned* fragment_end) {
  if (!IsValidBufferType(buffer))
    return WebString();

  WTF::String html_stdstr;
  KURL gurl;
  clipboard_->ReadHtml(buffer, &html_stdstr, &gurl,
                       static_cast<uint32_t*>(fragment_start),
                       static_cast<uint32_t*>(fragment_end));
  *source_url = gurl;
  return html_stdstr;
}

WebString WebClipboardImpl::ReadRTF(mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebString();

  WTF::String rtf;
  clipboard_->ReadRtf(buffer, &rtf);
  return rtf;
}

WebBlobInfo WebClipboardImpl::ReadImage(mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebBlobInfo();

  scoped_refptr<BlobDataHandle> blob;
  clipboard_->ReadImage(buffer, &blob);
  if (!blob)
    return WebBlobInfo();
  return blob;
}

WebString WebClipboardImpl::ReadCustomData(mojom::ClipboardBuffer buffer,
                                           const WebString& type) {
  if (!IsValidBufferType(buffer))
    return WebString();

  WTF::String data;
  clipboard_->ReadCustomData(buffer, EnsureNotNullWTFString(type), &data);
  return data;
}

void WebClipboardImpl::WritePlainText(const WebString& plain_text) {
  clipboard_->WriteText(mojom::ClipboardBuffer::kStandard, plain_text);
  clipboard_->CommitWrite(mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteHTML(const WebString& html_text,
                                 const WebURL& source_url,
                                 const WebString& plain_text,
                                 bool write_smart_paste) {
  clipboard_->WriteHtml(mojom::ClipboardBuffer::kStandard, html_text,
                        source_url);
  clipboard_->WriteText(mojom::ClipboardBuffer::kStandard, plain_text);

  if (write_smart_paste)
    clipboard_->WriteSmartPasteMarker(mojom::ClipboardBuffer::kStandard);
  clipboard_->CommitWrite(mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteImage(const WebImage& image,
                                  const WebURL& url,
                                  const WebString& title) {
  DCHECK(!image.IsNull());
  if (!WriteImageToClipboard(mojom::ClipboardBuffer::kStandard,
                             image.GetSkBitmap()))
    return;

  if (url.IsValid() && !url.IsEmpty()) {
    clipboard_->WriteBookmark(mojom::ClipboardBuffer::kStandard,
                              url.GetString(), EnsureNotNullWTFString(title));

    // When writing the image, we also write the image markup so that pasting
    // into rich text editors, such as Gmail, reveals the image. We also don't
    // want to call writeText(), since some applications (WordPad) don't pick
    // the image if there is also a text format on the clipboard.
    clipboard_->WriteHtml(mojom::ClipboardBuffer::kStandard,
                          URLToImageMarkup(url, title), KURL());
  }
  clipboard_->CommitWrite(mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteDataObject(const WebDragData& data) {
  const DragData& drag_data = BuildDragData(data);
  // TODO(dcheng): Properly support text/uri-list here.
  // Avoid calling the WriteFoo functions if there is no data associated with a
  // type. This prevents stomping on clipboard contents that might have been
  // written by extension functions such as chrome.bookmarkManagerPrivate.copy.
  if (!drag_data.text.IsNull()) {
    clipboard_->WriteText(mojom::ClipboardBuffer::kStandard, drag_data.text);
  }
  if (!drag_data.html.IsNull()) {
    clipboard_->WriteHtml(mojom::ClipboardBuffer::kStandard, drag_data.html,
                          KURL());
  }
  if (!drag_data.custom_data.IsEmpty()) {
    clipboard_->WriteCustomData(mojom::ClipboardBuffer::kStandard,
                                std::move(drag_data.custom_data));
  }
  clipboard_->CommitWrite(mojom::ClipboardBuffer::kStandard);
}

bool WebClipboardImpl::IsValidBufferType(mojom::ClipboardBuffer buffer) {
  switch (buffer) {
    case mojom::ClipboardBuffer::kStandard:
      return true;
    case mojom::ClipboardBuffer::kSelection:
#if defined(USE_X11)
      return true;
#else
      // Chrome OS and non-X11 unix builds do not support
      // the X selection clipboad.
      // TODO: remove the need for this case, see http://crbug.com/361753
      return false;
#endif
  }
  return true;
}

bool WebClipboardImpl::WriteImageToClipboard(mojom::ClipboardBuffer buffer,
                                             const SkBitmap& bitmap) {
  // Only 32-bit bitmaps are supported.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  const WebSize size(bitmap.width(), bitmap.height());

  void* pixels = bitmap.getPixels();
  // TODO(piman): this should not be NULL, but it is. crbug.com/369621
  if (!pixels)
    return false;

  base::CheckedNumeric<uint32_t> checked_buf_size = 4;
  checked_buf_size *= size.width;
  checked_buf_size *= size.height;
  if (!checked_buf_size.IsValid())
    return false;

  // Allocate a shared memory buffer to hold the bitmap bits.
  uint32_t buf_size = checked_buf_size.ValueOrDie();
  auto shared_buffer = mojo::SharedBufferHandle::Create(buf_size);
  auto mapping = shared_buffer->Map(buf_size);
  memcpy(mapping.get(), pixels, buf_size);

  clipboard_->WriteImage(buffer, size, std::move(shared_buffer));
  return true;
}

}  // namespace blink
