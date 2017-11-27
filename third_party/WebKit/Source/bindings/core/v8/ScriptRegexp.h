/*
 * Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ScriptRegexp_h
#define ScriptRegexp_h

#include "core/CoreExport.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

enum MultilineMode { kMultilineDisabled, kMultilineEnabled };

class CORE_EXPORT ScriptRegexp {
  USING_FAST_MALLOC(ScriptRegexp);
  WTF_MAKE_NONCOPYABLE(ScriptRegexp);

 public:
  enum CharacterMode {
    BMP,    // NOLINT
    UTF16,  // NOLINT
  };

  // For TextCaseSensitivity argument, TextCaseASCIIInsensitive and
  // TextCaseUnicodeInsensitive has identical behavior. They just add "i" flag.
  ScriptRegexp(const String&,
               TextCaseSensitivity,
               MultilineMode = kMultilineDisabled,
               CharacterMode = BMP);

  int Match(const String&,
            int start_from = 0,
            int* match_length = nullptr) const;

  bool IsValid() const { return !regex_.IsEmpty(); }
  // exceptionMessage is available only if !isValid().
  String ExceptionMessage() const { return exception_message_; }

 private:
  ScopedPersistent<v8::RegExp> regex_;
  String exception_message_;
};

}  // namespace blink

#endif  // ScriptRegexp_h
