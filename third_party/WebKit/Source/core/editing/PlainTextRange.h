/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef PlainTextRange_h
#define PlainTextRange_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/NotFound.h"

namespace blink {

class ContainerNode;
class Range;

class CORE_EXPORT PlainTextRange {
  STACK_ALLOCATED();

 public:
  PlainTextRange();
  PlainTextRange(const PlainTextRange&);
  explicit PlainTextRange(int location);
  PlainTextRange(int start, int end);

  size_t End() const {
    DCHECK(IsNotNull());
    return end_;
  }
  size_t Start() const {
    DCHECK(IsNotNull());
    return start_;
  }
  bool IsNull() const { return start_ == kNotFound; }
  bool IsNotNull() const { return start_ != kNotFound; }
  size_t length() const {
    DCHECK(IsNotNull());
    return end_ - start_;
  }

  EphemeralRange CreateRange(const ContainerNode& scope) const;
  EphemeralRange CreateRangeForSelection(const ContainerNode& scope) const;

  static PlainTextRange Create(const ContainerNode& scope,
                               const EphemeralRange&);
  static PlainTextRange Create(const ContainerNode& scope, const Range&);

 private:
  PlainTextRange& operator=(const PlainTextRange&) = delete;

  enum GetRangeFor { kForGeneric, kForSelection };
  EphemeralRange CreateRangeFor(const ContainerNode& scope, GetRangeFor) const;

  const size_t start_;
  const size_t end_;
};

}  // namespace blink

#endif  // PlainTextRange_h
