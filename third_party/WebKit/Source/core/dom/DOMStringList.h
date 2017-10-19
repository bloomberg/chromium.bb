/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#ifndef DOMStringList_h
#define DOMStringList_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// FIXME: Some consumers of this class may benefit from lazily fetching items
// rather than creating the list statically as is currently the only option.
class CORE_EXPORT DOMStringList final
    : public GarbageCollectedFinalized<DOMStringList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMStringList* Create() { return new DOMStringList(); }

  bool IsEmpty() const { return strings_.IsEmpty(); }
  void clear() { strings_.clear(); }
  void Append(const String& string) { strings_.push_back(string); }
  void Sort();

  // Implements the IDL.
  size_t length() const { return strings_.size(); }

  String item(unsigned index) const;
  bool contains(const String&) const;

  operator const Vector<String>&() const { return strings_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit DOMStringList() {}

  Vector<String> strings_;
};

}  // namespace blink

#endif  // DOMStringList_h
