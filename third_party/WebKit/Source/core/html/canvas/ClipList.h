// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipList_h
#define ClipList_h

#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/Path.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;
class AffineTransform;

class ClipList {
public:
    ClipList() { }
    ClipList(const ClipList&);
    ~ClipList() { }

    void clipPath(const Path&, WindRule, AntiAliasingMode, const AffineTransform&);
    void playback(GraphicsContext*) const;

private:

    struct ClipOp {
        Path m_path;
        AntiAliasingMode m_antiAliasingMode;
        WindRule m_windRule;

        ClipOp();
        ClipOp(const ClipOp&);
    };

    // Number of clip ops that can be stored in a ClipList without resorting to dynamic allocation
    static const size_t cInlineClipOpCapacity = 4;

    WTF::Vector<ClipOp, cInlineClipOpCapacity> m_clipList;
};

} // namespace blink

#endif
