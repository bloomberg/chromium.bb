// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webcore_convert.h"

namespace WebKit {

WebCore::FloatRect convert(const WebFloatRect& rect)
{
    return WebCore::FloatRect(rect.x, rect.y, rect.width, rect.height);
}

WebCore::FloatPoint convert(const WebFloatPoint& point)
{
    return WebCore::FloatPoint(point.x, point.y);
}

WebCore::IntRect convert(const WebRect& rect)
{
    return WebCore::IntRect(rect.x, rect.y, rect.width, rect.height);
}

WebCore::IntPoint convert(const WebPoint& point)
{
    return WebCore::IntPoint(point.x, point.y);
}

WebCore::IntSize convert(const WebSize& size)
{
    return WebCore::IntSize(size.width, size.height);
}

WebRect convert(const WebCore::IntRect& rect)
{
    return WebRect(rect.x(), rect.y(), rect.width(), rect.height());
}

WebSize convert(const WebCore::IntSize& size)
{
    return WebSize(size.width(), size.height());
}

}


