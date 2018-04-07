/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CLIPBOARD_H_

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-shared.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class WebDragData;
class WebImage;
class WebURL;

class WebClipboard {
 public:
  // Returns an identifier which can be used to determine whether the data
  // contained within the clipboard has changed.
  virtual uint64_t SequenceNumber(mojom::ClipboardBuffer) { return 0; }

  virtual bool IsFormatAvailable(mojom::ClipboardFormat,
                                 mojom::ClipboardBuffer) {
    return false;
  }

  virtual WebVector<WebString> ReadAvailableTypes(blink::mojom::ClipboardBuffer,
                                                  bool* contains_filenames) {
    return WebVector<WebString>();
  }
  virtual WebString ReadPlainText(blink::mojom::ClipboardBuffer) {
    return WebString();
  }
  // |fragment_start| and |fragment_end| are indexes into the returned markup
  // that indicate the start and end of the fragment if the returned markup
  // contains additional context. If there is no additional context,
  // |fragment_start| will be zero and |fragment_end| will be the same as the
  // length of the returned markup.
  virtual WebString ReadHTML(blink::mojom::ClipboardBuffer buffer,
                             WebURL* page_url,
                             unsigned* fragment_start,
                             unsigned* fragment_end) {
    return WebString();
  }
  virtual WebString ReadRTF(mojom::ClipboardBuffer) { return WebString(); }
  virtual WebBlobInfo ReadImage(mojom::ClipboardBuffer) {
    return WebBlobInfo();
  }
  virtual WebString ReadCustomData(mojom::ClipboardBuffer,
                                   const WebString& type) {
    return WebString();
  }

  virtual void WritePlainText(const WebString&) {}
  virtual void WriteHTML(const WebString& html_text,
                         const WebURL&,
                         const WebString& plain_text,
                         bool write_smart_paste) {}
  virtual void WriteImage(const WebImage&,
                          const WebURL&,
                          const WebString& title) {}
  virtual void WriteDataObject(const WebDragData&) {}

 protected:
  ~WebClipboard() = default;
};

}  // namespace blink

#endif
