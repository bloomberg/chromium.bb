// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextResource_h
#define TextResource_h

#include <memory>
#include "core/CoreExport.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"

namespace blink {

class CORE_EXPORT TextResource : public Resource {
 public:
  // Returns the decoded data in text form. The data has to be available at
  // call time.
  String DecodedText() const;

  WTF::TextEncoding Encoding() const override;

  void SetEncodingForTest(const String& encoding) { SetEncoding(encoding); }

 protected:
  TextResource(const ResourceRequest&,
               Type,
               const ResourceLoaderOptions&,
               const TextResourceDecoderOptions&);
  ~TextResource() override;

  void SetEncoding(const String&) override;

 private:
  std::unique_ptr<TextResourceDecoder> decoder_;
};

}  // namespace blink

#endif  // TextResource_h
