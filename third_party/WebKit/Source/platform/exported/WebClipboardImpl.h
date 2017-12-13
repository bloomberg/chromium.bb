// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebClipboardImpl_h
#define WebClipboardImpl_h

#include <stdint.h>

#include <string>

#include "third_party/WebKit/common/clipboard/clipboard.mojom-blink.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"

namespace blink {

class WebClipboardImpl : public blink::WebClipboard {
 public:
  WebClipboardImpl();
  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  uint64_t SequenceNumber(mojom::ClipboardBuffer) override;
  bool IsFormatAvailable(mojom::ClipboardFormat,
                         mojom::ClipboardBuffer) override;
  blink::WebVector<blink::WebString> ReadAvailableTypes(
      mojom::ClipboardBuffer,
      bool* contains_filenames) override;
  blink::WebString ReadPlainText(mojom::ClipboardBuffer) override;
  blink::WebString ReadHTML(mojom::ClipboardBuffer,
                            blink::WebURL* source_url,
                            unsigned* fragment_start,
                            unsigned* fragment_end) override;
  blink::WebString ReadRTF(mojom::ClipboardBuffer) override;
  blink::WebBlobInfo ReadImage(mojom::ClipboardBuffer) override;
  blink::WebString ReadCustomData(mojom::ClipboardBuffer,
                                  const blink::WebString& type) override;
  void WritePlainText(const blink::WebString& plain_text) override;
  void WriteHTML(const blink::WebString& html_text,
                 const blink::WebURL& source_url,
                 const blink::WebString& plain_text,
                 bool write_smart_paste) override;
  void WriteImage(const blink::WebImage&,
                  const blink::WebURL& source_url,
                  const blink::WebString& title) override;
  void WriteDataObject(const blink::WebDragData&) override;

 private:
  bool IsValidBufferType(mojom::ClipboardBuffer);
  bool WriteImageToClipboard(mojom::ClipboardBuffer, const SkBitmap&);
  mojom::blink::ClipboardHostPtr clipboard_;
};

}  // namespace blink

#endif  // WebClipboardImpl_h
