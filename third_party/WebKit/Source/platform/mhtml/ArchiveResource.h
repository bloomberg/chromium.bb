/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ArchiveResource_h
#define ArchiveResource_h

#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT ArchiveResource final
    : public GarbageCollectedFinalized<ArchiveResource> {
 public:
  static ArchiveResource* Create(RefPtr<SharedBuffer>,
                                 const KURL&,
                                 const String& content_id,
                                 const AtomicString& mime_type,
                                 const AtomicString& text_encoding);

  ~ArchiveResource();

  const KURL& Url() const { return url_; }
  const String& ContentID() const { return content_id_; }
  SharedBuffer* Data() const { return data_.get(); }
  const AtomicString& MimeType() const { return mime_type_; }
  const AtomicString& TextEncoding() const { return text_encoding_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  ArchiveResource(RefPtr<SharedBuffer>,
                  const KURL&,
                  const String& content_id,
                  const AtomicString& mime_type,
                  const AtomicString& text_encoding);

  KURL url_;
  String content_id_;
  RefPtr<SharedBuffer> data_;
  AtomicString mime_type_;
  AtomicString text_encoding_;
};

}  // namespace blink

#endif  // ArchiveResource_h
