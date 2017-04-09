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

#include "public/web/linux/WebFontRendering.h"

#include "core/layout/LayoutThemeFontProvider.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/linux/FontRenderStyle.h"

using blink::FontDescription;
using blink::FontPlatformData;

namespace blink {

// static
void WebFontRendering::SetSkiaFontManager(sk_sp<SkFontMgr> font_mgr) {
  FontCache::SetFontManager(std::move(font_mgr));
}

// static
void WebFontRendering::SetHinting(SkPaint::Hinting hinting) {
  FontRenderStyle::SetHinting(hinting);
}

// static
void WebFontRendering::SetAutoHint(bool use_auto_hint) {
  FontRenderStyle::SetAutoHint(use_auto_hint);
}

// static
void WebFontRendering::SetUseBitmaps(bool use_bitmaps) {
  FontRenderStyle::SetUseBitmaps(use_bitmaps);
}

// static
void WebFontRendering::SetAntiAlias(bool use_anti_alias) {
  FontRenderStyle::SetAntiAlias(use_anti_alias);
}

// static
void WebFontRendering::SetSubpixelRendering(bool use_subpixel_rendering) {
  FontRenderStyle::SetSubpixelRendering(use_subpixel_rendering);
}

// static
void WebFontRendering::SetSubpixelPositioning(bool use_subpixel_positioning) {
  FontDescription::SetSubpixelPositioning(use_subpixel_positioning);
}

// static
void WebFontRendering::SetDefaultFontSize(int size) {
  LayoutThemeFontProvider::SetDefaultFontSize(size);
}

// static
void WebFontRendering::SetSystemFontFamily(const WebString& name) {
  FontCache::SetSystemFontFamily(name);
}

}  // namespace blink
