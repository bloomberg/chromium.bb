// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReplicaPainter_h
#define ReplicaPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutReplica;

class ReplicaPainter {
public:
    ReplicaPainter(LayoutReplica& renderReplica) : m_renderReplica(renderReplica) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    LayoutReplica& m_renderReplica;
};

} // namespace blink

#endif // ReplicaPainter_h
