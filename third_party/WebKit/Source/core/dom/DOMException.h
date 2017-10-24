/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef DOMException_h
#define DOMException_h

#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT DOMException final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMException* Create(ExceptionCode,
                              const String& sanitized_message = String(),
                              const String& unsanitized_message = String());

  // Constructor exposed to script.
  static DOMException* Create(const String& message, const String& name);

  unsigned short code() const { return code_; }
  String name() const { return name_; }

  // This is the message that's exposed to JavaScript: never return unsanitized
  // data.
  String message() const { return sanitized_message_; }

  // This is the message that's exposed to the console: if an unsanitized
  // message is present, we prefer it.
  String MessageForConsole() const {
    return !unsanitized_message_.IsEmpty() ? unsanitized_message_
                                           : sanitized_message_;
  }
  String ToStringForConsole() const;

  static String GetErrorName(ExceptionCode);
  static String GetErrorMessage(ExceptionCode);

 private:
  DOMException(unsigned short code,
               const String& name,
               const String& sanitized_message,
               const String& unsanitized_message);

  unsigned short code_;
  String name_;
  String sanitized_message_;
  String unsanitized_message_;
};

}  // namespace blink

#endif  // DOMException_h
