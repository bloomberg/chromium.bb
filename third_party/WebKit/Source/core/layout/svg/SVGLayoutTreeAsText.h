/*
 * Copyright (C) 2004, 2005, 2009 Apple Inc. All rights reserved.
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

#ifndef SVGLayoutTreeAsText_h
#define SVGLayoutTreeAsText_h

#include "platform/text/TextStream.h"

namespace blink {

class LayoutObject;
class LayoutSVGImage;
class LayoutSVGInline;
class LayoutSVGInlineText;
class LayoutSVGShape;
class LayoutSVGRoot;
class LayoutSVGText;

// functions used by the main LayoutTreeAsText code
void Write(TextStream&, const LayoutSVGShape&, int indent);
void Write(TextStream&, const LayoutSVGRoot&, int indent);
void WriteSVGResourceContainer(TextStream&, const LayoutObject&, int indent);
void WriteSVGContainer(TextStream&, const LayoutObject&, int indent);
void WriteSVGImage(TextStream&, const LayoutSVGImage&, int indent);
void WriteSVGInlineText(TextStream&, const LayoutSVGInlineText&, int indent);
void WriteSVGText(TextStream&, const LayoutSVGText&, int indent);
void WriteSVGInline(TextStream&, const LayoutSVGInline&, int indent);
void WriteResources(TextStream&, const LayoutObject&, int indent);

}  // namespace blink

#endif  // SVGLayoutTreeAsText_h
