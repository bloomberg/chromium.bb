// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebPageOverlay.h"

#include "public/platform/WebSize.h"
#include "public/web/WebGraphicsContext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

void WebPageOverlay::paintPageOverlay(WebGraphicsContext* context, const WebSize& webViewSize)
{
    WebFloatRect rect(0, 0, webViewSize.width, webViewSize.height);
    WebCanvas* canvas = context->beginDrawing(rect);
    int saveCount = canvas->save();
    canvas->clipRect(SkRect::MakeWH(webViewSize.width, webViewSize.height));
    paintPageOverlay(canvas);
    canvas->restoreToCount(saveCount);
    context->endDrawing();
}

void WebPageOverlay::paintPageOverlay(WebCanvas*)
{
}

} // namespace blink
