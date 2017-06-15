// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/TextResource.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "platform/SharedBuffer.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

TextResource::TextResource(const ResourceRequest& resource_request,
                           Resource::Type type,
                           const ResourceLoaderOptions& options,
                           const String& mime_type,
                           const String& charset)
    : Resource(resource_request, type, options),
      decoder_(
          TextResourceDecoder::Create(mime_type, WTF::TextEncoding(charset))) {}

TextResource::~TextResource() {}

void TextResource::SetEncoding(const String& chs) {
  decoder_->SetEncoding(WTF::TextEncoding(chs),
                        TextResourceDecoder::kEncodingFromHTTPHeader);
}

WTF::TextEncoding TextResource::Encoding() const {
  return decoder_->Encoding();
}

String TextResource::DecodedText() const {
  DCHECK(Data());

  StringBuilder builder;
  const char* segment;
  size_t position = 0;
  while (size_t length = Data()->GetSomeData(segment, position)) {
    builder.Append(decoder_->Decode(segment, length));
    position += length;
  }
  builder.Append(decoder_->Flush());
  return builder.ToString();
}

}  // namespace blink
