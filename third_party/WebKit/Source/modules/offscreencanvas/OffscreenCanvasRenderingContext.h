// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasRenderingContext_h
#define OffscreenCanvasRenderingContext_h

#include "modules/ModulesExport.h"
#include "modules/offscreencanvas/OffscreenCanvas.h"

namespace blink {

class MODULES_EXPORT OffscreenCanvasRenderingContext : public GarbageCollectedFinalized<OffscreenCanvasRenderingContext>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(OffscreenCanvasRenderingContext);
public:
    virtual ~OffscreenCanvasRenderingContext() { }
    enum ContextType {
        Context2d = 0,
        ContextWebgl = 1,
        ContextWebgl2 = 2,
        ContextTypeCount
    };
    static ContextType contextTypeFromId(const String& id);

    OffscreenCanvas* getOffscreenCanvas() const { return m_offscreenCanvas; }
    virtual ContextType getContextType() const = 0;

    virtual bool is2d() const { return false; }

protected:
    OffscreenCanvasRenderingContext(OffscreenCanvas*);
    DECLARE_VIRTUAL_TRACE();

private:
    Member<OffscreenCanvas> m_offscreenCanvas;
};

} // namespace blink

#endif // OffscreenCanvasRenderingContext_h
