/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigatorUserMediaError_h
#define NavigatorUserMediaError_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class NavigatorUserMediaError final
    : public GarbageCollectedFinalized<NavigatorUserMediaError>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Name { kNamePermissionDenied, kNameConstraintNotSatisfied };

  static NavigatorUserMediaError* Create(Name,
                                         const String& message,
                                         const String& constraint_name);
  static NavigatorUserMediaError* Create(const String& name,
                                         const String& message,
                                         const String& constraint_name) {
    return new NavigatorUserMediaError(name, message, constraint_name);
  }

  String name() const { return name_; }
  const String& message() const { return message_; }
  const String& constraintName() const { return constraint_name_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  NavigatorUserMediaError(const String& name,
                          const String& message,
                          const String& constraint_name)
      : name_(name), message_(message), constraint_name_(constraint_name) {
    DCHECK(!name.IsEmpty());
  }

  String name_;
  String message_;
  String constraint_name_;
};

}  // namespace blink

#endif  // NavigatorUserMediaError_h
