// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEBCORE_CONVERT_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEBCORE_CONVERT_H_

#include "FloatPoint.h"
#include "FloatRect.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include <public/WebFloatRect.h>
#include <public/WebFloatPoint.h>
#include <public/WebRect.h>
#include <public/WebPoint.h>
#include <public/WebSize.h>

namespace WebKit {

WebCore::FloatRect convert(const WebFloatRect& rect);
WebCore::FloatPoint convert(const WebFloatPoint& point);
WebCore::IntRect convert(const WebRect& rect);
WebCore::IntPoint convert(const WebPoint& point);
WebCore::IntSize convert(const WebSize& size);
WebRect convert(const WebCore::IntRect& rect);
WebSize convert(const WebCore::IntSize& size);

}

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEBCORE_CONVERT_H_
