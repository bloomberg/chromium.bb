// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasContextCreationAttributesHelpers_h
#define CanvasContextCreationAttributesHelpers_h

namespace blink {

class CanvasContextCreationAttributesCore;
class CanvasContextCreationAttributesModule;

CanvasContextCreationAttributesCore ToCanvasContextCreationAttributes(
    const CanvasContextCreationAttributesModule&);

}  // namespace blink

#endif  // CanvasContextCreationAttributesHelpers_h
