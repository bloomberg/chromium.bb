// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvas_h
#define OffscreenCanvas_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class CanvasContextCreationAttributes;
class OffscreenCanvasRenderingContext;
class OffscreenCanvasRenderingContext2D;
class OffscreenCanvasRenderingContextFactory;

class MODULES_EXPORT OffscreenCanvas final : public GarbageCollectedFinalized<OffscreenCanvas>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static OffscreenCanvas* create(unsigned width, unsigned height);
    ~OffscreenCanvas() {}

    IntSize size() const { return m_size; }
    unsigned width() const { return m_size.width(); }
    unsigned height() const { return m_size.height(); }
    void setWidth(unsigned);
    void setHeight(unsigned);

    OffscreenCanvasRenderingContext2D* getContext(const String&, const CanvasContextCreationAttributes&);
    OffscreenCanvasRenderingContext2D* renderingContext() const;

    static void registerRenderingContextFactory(PassOwnPtr<OffscreenCanvasRenderingContextFactory>);

    DECLARE_VIRTUAL_TRACE();

private:
    OffscreenCanvas(const IntSize&);

    using ContextFactoryVector = Vector<OwnPtr<OffscreenCanvasRenderingContextFactory>>;
    static ContextFactoryVector& renderingContextFactories();
    static OffscreenCanvasRenderingContextFactory* getRenderingContextFactory(int);

    Member<OffscreenCanvasRenderingContext> m_context;
    IntSize m_size;
};

} // namespace blink

#endif // OffscreenCanvas_h
