// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGFilterPainter_h
#define SVGFilterPainter_h

namespace blink {

class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceFilter;

class SVGFilterPainter {
public:
    SVGFilterPainter(LayoutSVGResourceFilter& filter) : m_filter(filter) { }

    // Returns the context that should be used to paint the filter contents, or
    // null if the content should not be recorded.
    GraphicsContext* prepareEffect(LayoutObject*, GraphicsContext*);
    void finishEffect(LayoutObject*, GraphicsContext*);

private:
    LayoutSVGResourceFilter& m_filter;
};

} // namespace blink

#endif // SVGFilterPainter_h
