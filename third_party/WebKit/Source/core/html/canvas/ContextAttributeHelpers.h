// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContextAttributeHelpers_h
#define ContextAttributeHelpers_h

#include "core/html/canvas/Canvas2DContextAttributes.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/WebGLContextAttributes.h"
#include "public/platform/WebGraphicsContext3D.h"

namespace blink {

class Settings;

Canvas2DContextAttributes to2DContextAttributes(const CanvasContextCreationAttributes&);
WebGLContextAttributes toWebGLContextAttributes(const CanvasContextCreationAttributes&);

// Set up the attributes that can be used to initialize a WebGraphicsContext3D.
// It's mostly based on WebGLContextAttributes, but may be adjusted based
// on settings.
WebGraphicsContext3D::Attributes toWebGraphicsContext3DAttributes(const WebGLContextAttributes&, const blink::WebString&, Settings*, unsigned webGLVersion);

} // namespace blink

#endif // ContextAttributeHelpers_h
