/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleScrollSnapData_h
#define StyleScrollSnapData_h

#include "core/style/ScrollSnapPoints.h"
#include "platform/LengthPoint.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

namespace blink {

class StyleScrollSnapData : public RefCounted<StyleScrollSnapData> {
 public:
  static PassRefPtr<StyleScrollSnapData> Create() {
    return AdoptRef(new StyleScrollSnapData);
  }
  PassRefPtr<StyleScrollSnapData> Copy() {
    return AdoptRef(new StyleScrollSnapData(*this));
  }

  ScrollSnapPoints x_points_;
  ScrollSnapPoints y_points_;
  LengthPoint destination_;
  Vector<LengthPoint> coordinates_;

 private:
  StyleScrollSnapData();
  StyleScrollSnapData(const StyleScrollSnapData&);
};

bool operator==(const StyleScrollSnapData&, const StyleScrollSnapData&);
inline bool operator!=(const StyleScrollSnapData& a,
                       const StyleScrollSnapData& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // StyleScrollSnapData_h
