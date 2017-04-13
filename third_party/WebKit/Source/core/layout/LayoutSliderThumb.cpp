/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/layout/LayoutSliderThumb.h"

#include "core/html/forms/SliderThumbElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"

namespace blink {

LayoutSliderThumb::LayoutSliderThumb(SliderThumbElement* element)
    : LayoutBlockFlow(element) {}

void LayoutSliderThumb::UpdateAppearance(const ComputedStyle& parent_style) {
  if (parent_style.Appearance() == kSliderVerticalPart)
    MutableStyleRef().SetAppearance(kSliderThumbVerticalPart);
  else if (parent_style.Appearance() == kSliderHorizontalPart)
    MutableStyleRef().SetAppearance(kSliderThumbHorizontalPart);
  else if (parent_style.Appearance() == kMediaSliderPart)
    MutableStyleRef().SetAppearance(kMediaSliderThumbPart);
  else if (parent_style.Appearance() == kMediaVolumeSliderPart)
    MutableStyleRef().SetAppearance(kMediaVolumeSliderThumbPart);
  if (StyleRef().HasAppearance())
    LayoutTheme::GetTheme().AdjustSliderThumbSize(MutableStyleRef());
}

}  // namespace blink
