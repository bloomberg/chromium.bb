// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_style_variant_h
#define ng_style_variant_h

namespace blink {

// LayoutObject can have multiple style variations.
enum class NGStyleVariant { kStandard, kFirstLine, kEllipsis };

}  // namespace blink

#endif  // ng_style_variant_h
