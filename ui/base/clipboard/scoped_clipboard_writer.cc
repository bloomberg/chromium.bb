// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ScopedClipboardWriter class. Documentation on its
// purpose can be found in our header. Documentation on the format of the
// parameters for each clipboard target can be found in clipboard.h.

#include "ui/base/clipboard/scoped_clipboard_writer.h"

#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/size.h"

namespace ui {

ScopedClipboardWriter::ScopedClipboardWriter(Clipboard* clipboard)
    : clipboard_(clipboard) {
}

ScopedClipboardWriter::~ScopedClipboardWriter() {
  if (!objects_.empty() && clipboard_) {
    clipboard_->WriteObjects(objects_);
    if (url_text_.length())
      clipboard_->DidWriteURL(url_text_);
  }
}

void ScopedClipboardWriter::WriteText(const string16& text) {
  WriteTextOrURL(text, false);
}

void ScopedClipboardWriter::WriteURL(const string16& text) {
  WriteTextOrURL(text, true);
}

void ScopedClipboardWriter::WriteHTML(const string16& markup,
                                      const std::string& source_url) {
  if (markup.empty())
    return;

  std::string utf8_markup = UTF16ToUTF8(markup);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(
      Clipboard::ObjectMapParam(utf8_markup.begin(),
                                utf8_markup.end()));
  if (!source_url.empty()) {
    parameters.push_back(Clipboard::ObjectMapParam(source_url.begin(),
                                                   source_url.end()));
  }

  objects_[Clipboard::CBF_HTML] = parameters;
}

void ScopedClipboardWriter::WriteBookmark(const string16& bookmark_title,
                                          const std::string& url) {
  if (bookmark_title.empty() || url.empty())
    return;

  std::string utf8_markup = UTF16ToUTF8(bookmark_title);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                                 utf8_markup.end()));
  parameters.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::CBF_BOOKMARK] = parameters;
}

void ScopedClipboardWriter::WriteHyperlink(const string16& anchor_text,
                                           const std::string& url) {
  if (anchor_text.empty() || url.empty())
    return;

  // Construct the hyperlink.
  std::string html("<a href=\"");
  html.append(url);
  html.append("\">");
  html.append(UTF16ToUTF8(anchor_text));
  html.append("</a>");
  WriteHTML(UTF8ToUTF16(html), std::string());
}

void ScopedClipboardWriter::WriteWebSmartPaste() {
  objects_[Clipboard::CBF_WEBKIT] = Clipboard::ObjectMapParams();
}

void ScopedClipboardWriter::WriteBitmapFromPixels(const void* pixels,
                                                  const gfx::Size& size) {
  Clipboard::ObjectMapParam pixels_parameter, size_parameter;
  const char* pixels_data = static_cast<const char*>(pixels);
  size_t pixels_length = 4 * size.width() * size.height();
  for (size_t i = 0; i < pixels_length; i++)
    pixels_parameter.push_back(pixels_data[i]);

  const char* size_data = reinterpret_cast<const char*>(&size);
  size_t size_length = sizeof(gfx::Size);
  for (size_t i = 0; i < size_length; i++)
    size_parameter.push_back(size_data[i]);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(pixels_parameter);
  parameters.push_back(size_parameter);
  objects_[Clipboard::CBF_BITMAP] = parameters;
}

void ScopedClipboardWriter::WritePickledData(const Pickle& pickle,
                                             Clipboard::FormatType format) {
  Clipboard::ObjectMapParam format_parameter(format.begin(), format.end());
  Clipboard::ObjectMapParam data_parameter;

  data_parameter.resize(pickle.size());
  memcpy(const_cast<char*>(&data_parameter.front()),
         pickle.data(), pickle.size());

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(format_parameter);
  parameters.push_back(data_parameter);
  objects_[Clipboard::CBF_DATA] = parameters;
}

void ScopedClipboardWriter::WriteTextOrURL(const string16& text, bool is_url) {
  if (text.empty())
    return;

  std::string utf8_text = UTF16ToUTF8(text);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_text.begin(),
                                                 utf8_text.end()));
  objects_[Clipboard::CBF_TEXT] = parameters;

  if (is_url) {
    url_text_ = utf8_text;
  } else {
    url_text_.clear();
  }
}

}  // namespace ui
