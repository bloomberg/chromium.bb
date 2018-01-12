// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFlatTreeShorthands_h
#define NGFlatTreeShorthands_h

#include "core/editing/Forward.h"

namespace blink {

class LayoutBlockFlow;

// This file contains shorthands that converts FlatTree-variants of editing
// objects into DOM tree variants, and then pass them to LayoutNG utility
// functions that accept DOM tree variants only.

const LayoutBlockFlow* NGInlineFormattingContextOf(const PositionInFlatTree&);

}  // namespace blink

#endif
