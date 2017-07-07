/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/clipboard/Pasteboard.h"

#include "build/build_config.h"
#include "core/clipboard/DataObject.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "platform/graphics/Image.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

Pasteboard* Pasteboard::GeneralPasteboard() {
  static Pasteboard* pasteboard = new Pasteboard;
  return pasteboard;
}

Pasteboard::Pasteboard() : buffer_(WebClipboard::kBufferStandard) {}

bool Pasteboard::IsSelectionMode() const {
  return buffer_ == WebClipboard::kBufferSelection;
}

void Pasteboard::SetSelectionMode(bool selection_mode) {
  buffer_ = selection_mode ? WebClipboard::kBufferSelection
                           : WebClipboard::kBufferStandard;
}

void Pasteboard::WritePlainText(const String& text, SmartReplaceOption) {
// FIXME: add support for smart replace
#if defined(OS_WIN)
  String plain_text(text);
  ReplaceNewlinesWithWindowsStyleNewlines(plain_text);
  Platform::Current()->Clipboard()->WritePlainText(plain_text);
#else
  Platform::Current()->Clipboard()->WritePlainText(text);
#endif
}

void Pasteboard::WriteImage(Image* image,
                            const KURL& url,
                            const String& title) {
  DCHECK(image);

  const WebImage web_image(image);
  if (web_image.IsNull())
    return;

  Platform::Current()->Clipboard()->WriteImage(web_image, WebURL(url),
                                               WebString(title));
}

void Pasteboard::WriteDataObject(DataObject* data_object) {
  Platform::Current()->Clipboard()->WriteDataObject(
      data_object->ToWebDragData());
}

bool Pasteboard::CanSmartReplace() {
  return Platform::Current()->Clipboard()->IsFormatAvailable(
      WebClipboard::kFormatSmartPaste, buffer_);
}

bool Pasteboard::IsHTMLAvailable() {
  return Platform::Current()->Clipboard()->IsFormatAvailable(
      WebClipboard::kFormatHTML, buffer_);
}

String Pasteboard::PlainText() {
  return Platform::Current()->Clipboard()->ReadPlainText(buffer_);
}

String Pasteboard::ReadHTML(KURL& url,
                            unsigned& fragment_start,
                            unsigned& fragment_end) {
  WebURL web_url;
  WebString markup = Platform::Current()->Clipboard()->ReadHTML(
      buffer_, &web_url, &fragment_start, &fragment_end);
  if (!markup.IsEmpty()) {
    url = web_url;
    // fragmentStart and fragmentEnd are populated by WebClipboard::readHTML.
  } else {
    url = KURL();
    fragment_start = 0;
    fragment_end = 0;
  }
  return markup;
}

void Pasteboard::WriteHTML(const String& markup,
                           const KURL& document_url,
                           const String& plain_text,
                           bool can_smart_copy_or_delete) {
  String text = plain_text;
#if defined(OS_WIN)
  ReplaceNewlinesWithWindowsStyleNewlines(text);
#endif
  ReplaceNBSPWithSpace(text);

  Platform::Current()->Clipboard()->WriteHTML(markup, document_url, text,
                                              can_smart_copy_or_delete);
}

}  // namespace blink
