/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SubstituteData_h
#define SubstituteData_h

#include "platform/SharedBuffer.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

enum SubstituteDataLoadPolicy { kLoadNormally, kForceSynchronousLoad };

class SubstituteData {
  DISALLOW_NEW();

 public:
  SubstituteData() : substitute_data_load_policy_(kLoadNormally) {}

  SubstituteData(
      RefPtr<SharedBuffer> content,
      const AtomicString& mime_type,
      const AtomicString& text_encoding,
      const KURL& failing_url,
      SubstituteDataLoadPolicy substitute_data_load_policy = kLoadNormally)
      : content_(std::move(content)),
        mime_type_(mime_type),
        text_encoding_(text_encoding),
        failing_url_(failing_url),
        substitute_data_load_policy_(substitute_data_load_policy) {}

  bool IsValid() const { return content_.Get(); }

  SharedBuffer* Content() const { return content_.Get(); }
  const AtomicString& MimeType() const { return mime_type_; }
  const AtomicString& TextEncoding() const { return text_encoding_; }
  const KURL& FailingURL() const { return failing_url_; }
  bool ForceSynchronousLoad() const {
    return substitute_data_load_policy_ == kForceSynchronousLoad;
  }

 private:
  RefPtr<SharedBuffer> content_;
  AtomicString mime_type_;
  AtomicString text_encoding_;
  KURL failing_url_;
  SubstituteDataLoadPolicy substitute_data_load_policy_;
};

}  // namespace blink

#endif  // SubstituteData_h
