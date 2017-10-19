/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMError_h
#define DOMError_h

#include "core/dom/DOMException.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT DOMError : public GarbageCollectedFinalized<DOMError>,
                                public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMError* Create(const String& name) { return new DOMError(name); }
  static DOMError* Create(const String& name, const String& message) {
    return new DOMError(name, message);
  }

  static DOMError* Create(ExceptionCode ec) {
    return new DOMError(DOMException::GetErrorName(ec),
                        DOMException::GetErrorMessage(ec));
  }

  static DOMError* Create(ExceptionCode ec, const String& message) {
    return new DOMError(DOMException::GetErrorName(ec), message);
  }

  virtual ~DOMError();

  const String& name() const { return name_; }
  const String& message() const { return message_; }

  void Trace(blink::Visitor* visitor) {}

 protected:
  explicit DOMError(const String& name);
  DOMError(const String& name, const String& message);

 private:
  const String name_;
  const String message_;
};

}  // namespace blink

#endif  // DOMError_h
