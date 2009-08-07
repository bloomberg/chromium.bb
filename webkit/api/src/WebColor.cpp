/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "WebColor.h"

#include "CSSValueKeywords.h"
#include "UnusedParam.h"
#include "WebColorName.h"

namespace WebKit {

static int toCSSValueKeyword(WebColorName in_value)
{
    switch (in_value) {
        case WebColorActiveBorder:
            return CSSValueActiveborder;
            break;
        case WebColorActiveCaption:
            return CSSValueActivecaption;
            break;
        case WebColorAppworkspace:
            return CSSValueAppworkspace;
            break;
        case WebColorBackground:
            return CSSValueBackground;
            break;
        case WebColorButtonFace:
            return CSSValueButtonface;
            break;
        case WebColorButtonHighlight:
            return CSSValueButtonhighlight;
            break;
        case WebColorButtonShadow:
            return CSSValueButtonshadow;
            break;
        case WebColorButtonText:
            return CSSValueButtontext;
            break;
        case WebColorCaptionText:
            return CSSValueCaptiontext;
            break;
        case WebColorGrayText:
            return CSSValueGraytext;
            break;
        case WebColorHighlight:
            return CSSValueHighlight;
            break;
        case WebColorHighlightText:
            return CSSValueHighlighttext;
            break;
        case WebColorInactiveBorder:
            return CSSValueInactiveborder;
            break;
        case WebColorInactiveCaption:
            return CSSValueInactivecaption;
            break;
        case WebColorInactiveCaptionText:
            return CSSValueInactivecaptiontext;
            break;
        case WebColorInfoBackground:
            return CSSValueInfobackground;
            break;
        case WebColorInfoText:
            return CSSValueInfotext;
            break;
        case WebColorMenu:
            return CSSValueMenu;
            break;
        case WebColorMenuText:
            return CSSValueMenutext;
            break;
        case WebColorScrollbar:
            return CSSValueScrollbar;
            break;
        case WebColorText:
            return CSSValueText;
            break;
        case WebColorThreedDarkShadow:
            return CSSValueThreeddarkshadow;
            break;
        case WebColorThreedShadow:
            return CSSValueThreedshadow;
            break;
        case WebColorThreedFace:
            return CSSValueThreedface;
            break;
        case WebColorThreedHighlight:
            return CSSValueThreedhighlight;
            break;
        case WebColorThreedLightShadow:
            return CSSValueThreedlightshadow;
            break;
        case WebColorWebkitFocusRingColor:
            return CSSValueWebkitFocusRingColor;
            break;
        case WebColorWindow:
            return CSSValueWindow;
            break;
        case WebColorWindowFrame:
            return CSSValueWindowframe;
            break;
        case WebColorWindowText:
            return CSSValueWindowtext;
            break;
        default:
            return CSSValueInvalid;
            break;
    }
}

void setNamedColors(const WebColorName* colorNames, const WebColor* colors, size_t length)
{
  for (size_t i = 0; i < length; ++i) {
      WebColorName colorName = colorNames[i];
      WebColor color = colors[i];

      // Convert color to internal value identifier.
      int internalColorName = toCSSValueKeyword(colorName);
      
      // TODO(jeremy): Set Color in RenderTheme.
      UNUSED_PARAM(internalColorName);  // Suppress compiler warnings for now.
      UNUSED_PARAM(colorName);
      UNUSED_PARAM(color);
  }
  
  // TODO(jeremy): Tell RenderTheme to update colors.
}

}  // namespace webkit_glue
