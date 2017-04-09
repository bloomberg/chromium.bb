/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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

#include "core/style/StyleFlexibleBoxData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

StyleFlexibleBoxData::StyleFlexibleBoxData()
    : flex_grow_(ComputedStyle::InitialFlexGrow()),
      flex_shrink_(ComputedStyle::InitialFlexShrink()),
      flex_basis_(ComputedStyle::InitialFlexBasis()),
      flex_direction_(ComputedStyle::InitialFlexDirection()),
      flex_wrap_(ComputedStyle::InitialFlexWrap()) {}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
    : RefCounted<StyleFlexibleBoxData>(),
      flex_grow_(o.flex_grow_),
      flex_shrink_(o.flex_shrink_),
      flex_basis_(o.flex_basis_),
      flex_direction_(o.flex_direction_),
      flex_wrap_(o.flex_wrap_) {}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const {
  return flex_grow_ == o.flex_grow_ && flex_shrink_ == o.flex_shrink_ &&
         flex_basis_ == o.flex_basis_ && flex_direction_ == o.flex_direction_ &&
         flex_wrap_ == o.flex_wrap_;
}

}  // namespace blink
