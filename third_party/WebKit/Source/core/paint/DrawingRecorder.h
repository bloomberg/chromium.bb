// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingRecorder_h
#define DrawingRecorder_h

#include "core/paint/ViewDisplayList.h"
#include "platform/graphics/DisplayList.h"

namespace blink {

class DrawingDisplayItem : public DisplayItem {
public:
    DrawingDisplayItem(PassRefPtr<SkPicture> picture, const FloatPoint& location, PaintPhase phase, const RenderObject* renderer)
        : DisplayItem(renderer, (Type)phase), m_picture(picture), m_location(location) { }

    PassRefPtr<SkPicture> picture() const { return m_picture; }
    const FloatPoint& location() const { return m_location; }

private:
    virtual void replay(GraphicsContext*);

    RefPtr<SkPicture> m_picture;
    const FloatPoint m_location;
#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};


class DrawingRecorder {
public:
    explicit DrawingRecorder(GraphicsContext*, const RenderObject*, PaintPhase, const FloatRect&);
    ~DrawingRecorder();

private:
    GraphicsContext* m_context;
    const RenderObject* m_renderer;
    const PaintPhase m_phase;
    const FloatRect m_bounds;
};

} // namespace blink

#endif // DrawingRecorder_h
