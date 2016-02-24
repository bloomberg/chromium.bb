// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasTemp_h
#define OffscreenCanvasTemp_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT OffscreenCanvasTemp final : public GarbageCollectedFinalized<OffscreenCanvasTemp>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static OffscreenCanvasTemp* create(unsigned width, unsigned height);

    IntSize size() const { return m_size; }
    unsigned width() const { return m_size.width(); }
    unsigned height() const { return m_size.height(); }

    void setWidth(unsigned);
    void setHeight(unsigned);

    DECLARE_VIRTUAL_TRACE();

private:
    OffscreenCanvasTemp(const IntSize&);

    IntSize m_size;
};

} // namespace blink

#endif // OffscreenCanvasTemp_h
