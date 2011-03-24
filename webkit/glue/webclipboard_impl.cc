// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webclipboard_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "ui/base/clipboard/clipboard.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webkit_glue.h"

#if WEBKIT_USING_CG
#include "skia/ext/skia_utils_mac.h"
#endif

using WebKit::WebClipboard;
using WebKit::WebData;
using WebKit::WebImage;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

namespace webkit_glue {

// Static
std::string WebClipboardImpl::URLToMarkup(const WebURL& url,
    const WebString& title) {
  std::string markup("<a href=\"");
  markup.append(url.spec());
  markup.append("\">");
  // TODO(darin): HTML escape this
  markup.append(EscapeForHTML(UTF16ToUTF8(title)));
  markup.append("</a>");
  return markup;
}

// Static
std::string WebClipboardImpl::URLToImageMarkup(const WebURL& url,
    const WebString& title) {
  std::string markup("<img src=\"");
  markup.append(url.spec());
  markup.append("\"");
  if (!title.isEmpty()) {
    markup.append(" alt=\"");
    markup.append(EscapeForHTML(UTF16ToUTF8(title)));
    markup.append("\"");
  }
  markup.append("/>");
  return markup;
}

WebClipboardImpl::~WebClipboardImpl() {
}

bool WebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  ui::Clipboard::FormatType format_type;
  ui::Clipboard::Buffer buffer_type;

  if (!ConvertBufferType(buffer, &buffer_type))
    return false;

  switch (format) {
    case FormatPlainText:
      return ClipboardIsFormatAvailable(ui::Clipboard::GetPlainTextFormatType(),
                                        buffer_type) ||
          ClipboardIsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                     buffer_type);
    case FormatHTML:
      format_type = ui::Clipboard::GetHtmlFormatType();
      break;
    case FormatSmartPaste:
      format_type = ui::Clipboard::GetWebKitSmartPasteFormatType();
      break;
    case FormatBookmark:
#if defined(OS_WIN) || defined(OS_MACOSX)
      format_type = ui::Clipboard::GetUrlWFormatType();
      break;
#endif
    default:
      NOTREACHED();
      return false;
  }

  return ClipboardIsFormatAvailable(format_type, buffer_type);
}

WebString WebClipboardImpl::readPlainText(Buffer buffer) {
  ui::Clipboard::Buffer buffer_type;
  if (!ConvertBufferType(buffer, &buffer_type))
    return WebString();

  if (ClipboardIsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                 buffer_type)) {
    string16 text;
    ClipboardReadText(buffer_type, &text);
    if (!text.empty())
      return text;
  }

  if (ClipboardIsFormatAvailable(ui::Clipboard::GetPlainTextFormatType(),
                                 buffer_type)) {
    std::string text;
    ClipboardReadAsciiText(buffer_type, &text);
    if (!text.empty())
      return ASCIIToUTF16(text);
  }

  return WebString();
}

WebString WebClipboardImpl::readHTML(Buffer buffer, WebURL* source_url) {
  ui::Clipboard::Buffer buffer_type;
  if (!ConvertBufferType(buffer, &buffer_type))
    return WebString();

  string16 html_stdstr;
  GURL gurl;
  ClipboardReadHTML(buffer_type, &html_stdstr, &gurl);
  *source_url = gurl;
  return html_stdstr;
}

WebData WebClipboardImpl::readImage(Buffer buffer) {
  ui::Clipboard::Buffer buffer_type;
  if (!ConvertBufferType(buffer, &buffer_type))
    return WebData();

  std::string png_data;
  ClipboardReadImage(buffer_type, &png_data);
  return WebData(png_data);
}

void WebClipboardImpl::writeHTML(
    const WebString& html_text, const WebURL& source_url,
    const WebString& plain_text, bool write_smart_paste) {
  ScopedClipboardWriterGlue scw(ClipboardGetClipboard());
  scw.WriteHTML(html_text, source_url.spec());
  scw.WriteText(plain_text);

  if (write_smart_paste)
    scw.WriteWebSmartPaste();
}

void WebClipboardImpl::writePlainText(const WebString& plain_text) {
  ScopedClipboardWriterGlue scw(ClipboardGetClipboard());
  scw.WriteText(plain_text);
}

void WebClipboardImpl::writeURL(const WebURL& url, const WebString& title) {
  ScopedClipboardWriterGlue scw(ClipboardGetClipboard());

  scw.WriteBookmark(title, url.spec());
  scw.WriteHTML(UTF8ToUTF16(URLToMarkup(url, title)), "");
  scw.WriteText(UTF8ToUTF16(std::string(url.spec())));
}

void WebClipboardImpl::writeImage(
    const WebImage& image, const WebURL& url, const WebString& title) {
  ScopedClipboardWriterGlue scw(ClipboardGetClipboard());

  if (!image.isNull()) {
#if WEBKIT_USING_SKIA
    const SkBitmap& bitmap = image.getSkBitmap();
#elif WEBKIT_USING_CG
    const SkBitmap& bitmap = gfx::CGImageToSkBitmap(image.getCGImageRef());
#endif
    SkAutoLockPixels locked(bitmap);
    scw.WriteBitmapFromPixels(bitmap.getPixels(), image.size());
  }

  // When writing the image, we also write the image markup so that pasting
  // into rich text editors, such as Gmail, reveals the image. We also don't
  // want to call writeText(), since some applications (WordPad) don't pick the
  // image if there is also a text format on the clipboard.
  if (!url.isEmpty()) {
    scw.WriteBookmark(title, url.spec());
    scw.WriteHTML(UTF8ToUTF16(URLToImageMarkup(url, title)), "");
  }
}

void WebClipboardImpl::writeData(const WebString& type,
                                 const WebString& data,
                                 const WebString& metadata) {
  // TODO(dcheng): Implement this stub.
}

WebVector<WebString> WebClipboardImpl::readAvailableTypes(
    Buffer buffer, bool* contains_filenames) {
  ui::Clipboard::Buffer buffer_type;
  std::vector<string16> types;
  if (ConvertBufferType(buffer, &buffer_type)) {
    ClipboardReadAvailableTypes(buffer_type, &types, contains_filenames);
  }
  return types;
}

bool WebClipboardImpl::readData(Buffer buffer, const WebString& type,
                                WebString* data, WebString* metadata) {
  ui::Clipboard::Buffer buffer_type;
  if (!ConvertBufferType(buffer, &buffer_type))
    return false;

  string16 data_out;
  string16 metadata_out;
  bool result = ClipboardReadData(buffer_type, type, &data_out, &metadata_out);
  if (result) {
    *data = data_out;
    *metadata = metadata_out;
  }
  return result;
}

WebVector<WebString> WebClipboardImpl::readFilenames(Buffer buffer) {
  ui::Clipboard::Buffer buffer_type;
  std::vector<string16> filenames;
  if (ConvertBufferType(buffer, &buffer_type)) {
    ClipboardReadFilenames(buffer_type, &filenames);
  }
  return filenames;
}

bool WebClipboardImpl::ConvertBufferType(Buffer buffer,
                                         ui::Clipboard::Buffer* result) {
  switch (buffer) {
    case BufferStandard:
      *result = ui::Clipboard::BUFFER_STANDARD;
      break;
    case BufferDrag:
      *result = ui::Clipboard::BUFFER_DRAG;
    case BufferSelection:
#if defined(USE_X11)
      *result = ui::Clipboard::BUFFER_SELECTION;
      break;
#endif
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

}  // namespace webkit_glue
