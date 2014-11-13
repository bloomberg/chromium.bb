// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPrintPresetOptions_h
#define WebPrintPresetOptions_h

#include <vector>

namespace blink {

struct WebPageRange;
typedef std::vector<WebPageRange> WebPageRanges;

enum WebDuplexMode {
    WebUnknownDuplexMode = -1,
    WebSimplex,
    WebLongEdge,
    WebShortEdge
};

struct WebPageRange {
    int from;
    int to;
};

struct WebPrintPresetOptions {
    WebPrintPresetOptions()
        : isScalingDisabled(false)
        , copies(0)
        , duplexMode(WebUnknownDuplexMode) { }

    // Specifies whether scaling is disabled.
    bool isScalingDisabled;

    // Specifies the number of copies to be printed.
    int copies;

    // Specifies duplex mode to be used for printing.
    WebDuplexMode duplexMode;

    // Specifies page range to be used for printing.
    WebPageRanges pageRanges;
};

} // namespace blink

#endif
