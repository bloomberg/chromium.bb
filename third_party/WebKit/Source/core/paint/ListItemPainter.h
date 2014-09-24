// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListItemPainter_h
#define ListItemPainter_h

namespace blink {

struct PaintInfo;
class Path;
class LayoutPoint;
class RenderListItem;

class ListItemPainter {
public:
    ListItemPainter(RenderListItem& renderListItem) : m_renderListItem(renderListItem) { }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);

private:
    Path getCanonicalPath() const;
    Path getPath(const LayoutPoint& origin) const;

    RenderListItem& m_renderListItem;
};

} // namespace blink

#endif // ListItemPainter_h
