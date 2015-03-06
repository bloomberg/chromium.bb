/*
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/dom/Document.h"
#include "core/html/HTMLElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/heap/Handle.h"

#define CanvasDefaultInterpolationQuality InterpolationLow

namespace blink {

class AffineTransform;
class CanvasContextCreationAttributes;
class CanvasRenderingContext;
class GraphicsContext;
class GraphicsContextStateSaver;
class HTMLCanvasElement;
class Image;
class ImageData;
class ImageBuffer;
class ImageBufferSurface;
class IntSize;
class RecordingImageBufferFallbackSurfaceFactory;

class CanvasObserver : public WillBeGarbageCollectedMixin {
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(CanvasObserver);
public:
    virtual void canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect) = 0;
    virtual void canvasResized(HTMLCanvasElement*) = 0;
#if !ENABLE(OILPAN)
    virtual void canvasDestroyed(HTMLCanvasElement*) = 0;
#endif

    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

class HTMLCanvasElement final : public HTMLElement, public DocumentVisibilityObserver, public CanvasImageSource, public ImageBufferClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLCanvasElement);
public:
    DECLARE_NODE_FACTORY(HTMLCanvasElement);
    virtual ~HTMLCanvasElement();

    void addObserver(CanvasObserver*);
    void removeObserver(CanvasObserver*);

    // Attributes and functions exposed to script
    int width() const { return size().width(); }
    int height() const { return size().height(); }

    const IntSize& size() const { return m_size; }

    void setWidth(int);
    void setHeight(int);
    void setAccelerationDisabled(bool accelerationDisabled) { m_accelerationDisabled = accelerationDisabled; }
    bool accelerationDisabled() const { return m_accelerationDisabled; }

    void setSize(const IntSize& newSize)
    {
        if (newSize == size())
            return;
        m_ignoreReset = true;
        setWidth(newSize.width());
        setHeight(newSize.height());
        m_ignoreReset = false;
        reset();
    }

    // Called by HTMLCanvasElement's V8 bindings.
    ScriptValue getContext(ScriptState*, const String&, const CanvasContextCreationAttributes&);
    // Called by Document::getCSSCanvasContext as well as above getContext() variant.
    void getContext(const String&, const CanvasContextCreationAttributes&, CanvasRenderingContext2DOrWebGLRenderingContext&);
    bool isPaintable() const;

    static String toEncodingMimeType(const String& mimeType);
    String toDataURL(const String& mimeType, const ScriptValue& qualityArgument, ExceptionState&) const;
    String toDataURL(const String& mimeType, ExceptionState& exceptionState) const { return toDataURL(mimeType, ScriptValue(), exceptionState); }

    // Used for rendering
    void didDraw(const FloatRect&);
    void notifyObserversCanvasChanged(const FloatRect&);

    void paint(GraphicsContext*, const LayoutRect&);

    GraphicsContext* drawingContext() const; // Deprecated: use drawingCanvas
    SkCanvas* drawingCanvas() const;
    SkCanvas* existingDrawingCanvas() const;

    CanvasRenderingContext* renderingContext() const { return m_context.get(); }

    void ensureUnacceleratedImageBuffer();
    ImageBuffer* buffer() const;
    PassRefPtr<Image> copiedImage(SourceDrawingBuffer) const;
    void clearCopiedImage();

    SecurityOrigin* securityOrigin() const;
    bool originClean() const;
    void setOriginTainted() { m_originClean = false; }

    AffineTransform baseTransform() const;

    bool is3D() const;

    bool hasImageBuffer() const { return m_imageBuffer; }
    void discardImageBuffer();

    bool shouldAccelerate(const IntSize&) const;

    bool shouldBeDirectComposited() const;

    virtual const AtomicString imageSourceURL() const override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;

    // DocumentVisibilityObserver implementation
    virtual void didChangeVisibilityState(PageVisibilityState) override;

    // CanvasImageSource implementation
    virtual PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const override;
    virtual bool wouldTaintOrigin(SecurityOrigin*) const override;
    virtual FloatSize sourceSize() const override;
    virtual bool isCanvasElement() const override { return true; }
    virtual bool isOpaque() const override;

    // ImageBufferClient implementation
    virtual void notifySurfaceInvalid() override;
    virtual bool isDirty() override { return !m_dirtyRect.isEmpty(); }
    virtual void didFinalizeFrame() override;
    virtual void restoreCanvasMatrixClipStack() override;

    void doDeferredPaintInvalidation();

    DECLARE_VIRTUAL_TRACE();

    // Methods used for testing
    void createImageBufferUsingSurface(PassOwnPtr<ImageBufferSurface>);

protected:
    virtual void didMoveToNewDocument(Document& oldDocument) override;

private:
    explicit HTMLCanvasElement(Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual LayoutObject* createLayoutObject(const LayoutStyle&) override;
    virtual void didRecalcStyle(StyleRecalcChange) override;
    virtual bool areAuthorShadowsAllowed() const override { return false; }

    void reset();

    PassOwnPtr<RecordingImageBufferFallbackSurfaceFactory> createSurfaceFactory(const IntSize& deviceSize, int* msaaSampleCount) const;
    PassOwnPtr<ImageBufferSurface> createImageBufferSurface(const IntSize& deviceSize, int* msaaSampleCount);
    void createImageBuffer();
    void createImageBufferInternal(PassOwnPtr<ImageBufferSurface> externalSurface);
    bool shouldUseDisplayList(const IntSize& deviceSize);

    void setSurfaceSize(const IntSize&);

    bool paintsIntoCanvasBuffer() const;

    void updateExternallyAllocatedMemory() const;

    String toDataURLInternal(const String& mimeType, const double* quality, SourceDrawingBuffer) const;

    WillBeHeapHashSet<RawPtrWillBeWeakMember<CanvasObserver>> m_observers;

    IntSize m_size;

    OwnPtrWillBeMember<CanvasRenderingContext> m_context;

    bool m_ignoreReset;
    bool m_accelerationDisabled;
    FloatRect m_dirtyRect;

    mutable intptr_t m_externallyAllocatedMemory;

    bool m_originClean;

    // It prevents HTMLCanvasElement::buffer() from continuously re-attempting to allocate an imageBuffer
    // after the first attempt failed.
    mutable bool m_didFailToCreateImageBuffer;
    bool m_imageBufferIsClear;
    OwnPtr<ImageBuffer> m_imageBuffer;
    mutable OwnPtr<GraphicsContextStateSaver> m_contextStateSaver;

    mutable RefPtr<Image> m_copiedImage; // FIXME: This is temporary for platforms that have to copy the image buffer to render (and for CSSCanvasValue).
};

} // namespace blink

#endif // HTMLCanvasElement_h
