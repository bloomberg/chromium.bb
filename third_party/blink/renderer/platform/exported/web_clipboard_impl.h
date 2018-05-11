// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_EXPORTED_WEB_CLIPBOARD_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_EXPORTED_WEB_CLIPBOARD_IMPL_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class WebDragData;
class WebImage;
class WebURL;

class PLATFORM_EXPORT WebClipboardImpl {
 public:
  WebClipboardImpl();
  ~WebClipboardImpl();

  uint64_t SequenceNumber(mojom::ClipboardBuffer);
  bool IsFormatAvailable(mojom::ClipboardFormat, mojom::ClipboardBuffer);
  blink::WebVector<blink::WebString> ReadAvailableTypes(
      mojom::ClipboardBuffer,
      bool* contains_filenames);
  blink::WebString ReadPlainText(mojom::ClipboardBuffer);
  blink::WebString ReadHTML(mojom::ClipboardBuffer,
                            blink::WebURL* source_url,
                            unsigned* fragment_start,
                            unsigned* fragment_end);
  blink::WebString ReadRTF(mojom::ClipboardBuffer);
  blink::WebBlobInfo ReadImage(mojom::ClipboardBuffer);
  blink::WebString ReadCustomData(mojom::ClipboardBuffer,
                                  const blink::WebString& type);
  void WritePlainText(const blink::WebString& plain_text);
  void WriteHTML(const blink::WebString& html_text,
                 const blink::WebURL& source_url,
                 const blink::WebString& plain_text,
                 bool write_smart_paste);
  void WriteImage(const blink::WebImage&,
                  const blink::WebURL& source_url,
                  const blink::WebString& title);
  void WriteDataObject(const blink::WebDragData&);

 private:
  bool IsValidBufferType(mojom::ClipboardBuffer);
  bool WriteImageToClipboard(mojom::ClipboardBuffer, const SkBitmap&);
  mojom::blink::ClipboardHostPtr clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_EXPORTED_WEB_CLIPBOARD_IMPL_H_
