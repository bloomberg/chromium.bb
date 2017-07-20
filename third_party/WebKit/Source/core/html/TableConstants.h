// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableConstants_h
#define TableConstants_h

namespace blink {

// https://html.spec.whatwg.org/multipage/tables.html#dom-colgroup-span
// https://html.spec.whatwg.org/multipage/tables.html#dom-col-span
// https://html.spec.whatwg.org/multipage/tables.html#dom-tdth-colspan
constexpr unsigned kDefaultColSpan = 1u;
constexpr unsigned kMinColSpan = 1u;
constexpr unsigned kMaxColSpan = 1000u;

// https://html.spec.whatwg.org/multipage/tables.html#dom-tdth-rowspan
constexpr unsigned kDefaultRowSpan = 1u;
constexpr unsigned kMaxRowSpan = 65534u;
// The minimum value is 1 though the standard says it's 0. It's intentional
// because we don't implement rowSpan=0 behavior.
constexpr unsigned kMinRowSpan = 1;

}  // namespace blink

#endif  // TableConstants_h
