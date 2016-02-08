// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameContentDumper_h
#define WebFrameContentDumper_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebLocalFrame;
class WebString;

// Functions in this class should only be used for
// testing purposes.
// The exceptions to this rule are tracked in http://crbug.com/585164.
class WebFrameContentDumper {
public:
    // Control of layoutTreeAsText output
    enum LayoutAsTextControl {
        LayoutAsTextNormal = 0,
        LayoutAsTextDebug = 1 << 0,
        LayoutAsTextPrinting = 1 << 1,
        LayoutAsTextWithLineTrees = 1 << 2
    };
    typedef unsigned LayoutAsTextControls;

    // Returns the contents of this frame as a string.  If the text is
    // longer than maxChars, it will be clipped to that length.  WARNING:
    // This function may be slow depending on the number of characters
    // retrieved and page complexity.  For a typically sized page, expect
    // it to take on the order of milliseconds.
    //
    // If there is room, subframe text will be recursively appended. Each
    // frame will be separated by an empty line.
    BLINK_EXPORT static WebString dumpFrameTreeAsText(WebLocalFrame*, size_t maxChars);

    // Returns HTML text for the contents of this frame, generated
    // from the DOM.
    BLINK_EXPORT static WebString dumpAsMarkup(WebLocalFrame*);

    // Returns a text representation of the render tree.  This method is used
    // to support layout tests.
    BLINK_EXPORT static WebString dumpLayoutTreeAsText(WebLocalFrame*, LayoutAsTextControls toShow = LayoutAsTextNormal);
};

} // namespace blink

#endif
