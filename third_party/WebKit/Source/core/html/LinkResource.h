/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef LinkResource_h
#define LinkResource_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

class Document;
class HTMLLinkElement;
class LocalFrame;

class CORE_EXPORT LinkResource
    : public GarbageCollectedFinalized<LinkResource> {
  WTF_MAKE_NONCOPYABLE(LinkResource);

 public:
  enum LinkResourceType { kStyle, kImport, kManifest, kOther };

  explicit LinkResource(HTMLLinkElement*);
  virtual ~LinkResource();

  bool ShouldLoadResource() const;
  LocalFrame* LoadingFrame() const;

  virtual LinkResourceType GetType() const = 0;
  virtual void Process() = 0;
  virtual void OwnerRemoved() {}
  virtual void OwnerInserted() {}
  virtual bool HasLoaded() const = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  void Load();

  Document& GetDocument();
  const Document& GetDocument() const;
  WTF::TextEncoding GetCharset() const;

  Member<HTMLLinkElement> owner_;
};

}  // namespace blink

#endif  // LinkResource_h
