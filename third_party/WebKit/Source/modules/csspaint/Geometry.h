// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Geometry_h
#define Geometry_h

namespace blink {

class Geometry : public GarbageCollectedFinalized<Geometry>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(Geometry);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Geometry* create(IntSize size)
    {
        return new Geometry(size);
    }
    virtual ~Geometry() {}

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }

    DEFINE_INLINE_TRACE() { }
private:
    explicit Geometry(IntSize size) : m_size(size) { }

    IntSize m_size;
};

} // namespace blink

#endif // Geometry_h
