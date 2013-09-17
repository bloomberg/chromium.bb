/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "SkDeferredCanvas.h"
#include "SkImage.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/IntSize.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebExternalTextureLayerClient.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "wtf/DoublyLinkedList.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class Canvas2DLayerBridge;
class PassCanvas2DLayerBridgePtr;

class Canvas2DLayerBridgePtr {
public:
    Canvas2DLayerBridgePtr() { }
    Canvas2DLayerBridgePtr(const PassRefPtr<Canvas2DLayerBridge>& ptr) { m_ptr = ptr; }
    ~Canvas2DLayerBridgePtr() { clear(); }
    Canvas2DLayerBridge* operator->() const { return m_ptr.get(); }
    Canvas2DLayerBridgePtr& operator=(const PassRefPtr<Canvas2DLayerBridge>&);
    Canvas2DLayerBridge* get() const { return m_ptr.get(); }
    operator bool () const { return m_ptr; }
    void clear();
    PassRefPtr<Canvas2DLayerBridge> release() WARN_UNUSED_RETURN { return m_ptr.release(); }
private:
    RefPtr<Canvas2DLayerBridge> m_ptr;
};

class Canvas2DLayerBridge : public WebKit::WebExternalTextureLayerClient, public SkDeferredCanvas::NotificationClient, public DoublyLinkedListNode<Canvas2DLayerBridge>, public RefCounted<Canvas2DLayerBridge> {
    WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);
public:
    enum OpacityMode {
        Opaque,
        NonOpaque
    };

    static PassRefPtr<Canvas2DLayerBridge> create(PassRefPtr<GraphicsContext3D>, const IntSize&, OpacityMode);

    virtual ~Canvas2DLayerBridge();

    // WebKit::WebExternalTextureLayerClient implementation.
    virtual WebKit::WebGraphicsContext3D* context() OVERRIDE;
    virtual bool prepareMailbox(WebKit::WebExternalTextureMailbox*, WebKit::WebExternalBitmap*) OVERRIDE;
    virtual void mailboxReleased(const WebKit::WebExternalTextureMailbox&) OVERRIDE;

    // SkDeferredCanvas::NotificationClient implementation
    virtual void prepareForDraw() OVERRIDE;
    virtual void storageAllocatedForRecordingChanged(size_t) OVERRIDE;
    virtual void flushedDrawCommands() OVERRIDE;
    virtual void skippedPendingDrawCommands() OVERRIDE;

    // Methods used by Canvas2DLayerManager
    virtual size_t freeMemoryIfPossible(size_t); // virtual for mocking
    virtual void flush(); // virtual for mocking
    virtual size_t storageAllocatedForRecording(); // virtual for faking
    size_t bytesAllocated() const {return m_bytesAllocated;}
    void limitPendingFrames();

    WebKit::WebLayer* layer();
    void contextAcquired();
    PassRefPtr<SkCanvas> getCanvas() { return PassRefPtr<SkCanvas>(m_canvas); }

    unsigned backBufferTexture();

    bool isValid();

protected:
    void destroy();
    friend class Canvas2DLayerBridgePtr;
    Canvas2DLayerBridge(PassRefPtr<GraphicsContext3D>, SkDeferredCanvas*, OpacityMode);
    void setRateLimitingEnabled(bool);

    RefPtr<SkDeferredCanvas> m_canvas;
    OwnPtr<WebKit::WebExternalTextureLayer> m_layer;
    RefPtr<GraphicsContext3D> m_context;
    size_t m_bytesAllocated;
    bool m_didRecordDrawCommand;
    bool m_surfaceIsValid;
    int m_framesPending;
    bool m_destructionInProgress;
    bool m_rateLimitingEnabled;

    friend class WTF::DoublyLinkedListNode<Canvas2DLayerBridge>;
    Canvas2DLayerBridge* m_next;
    Canvas2DLayerBridge* m_prev;

    enum MailboxStatus {
        MailboxInUse,
        MailboxReleased,
        MailboxAvailable,
    };

    struct MailboxInfo {
        WebKit::WebExternalTextureMailbox m_mailbox;
        SkAutoTUnref<SkImage> m_image;
        MailboxStatus m_status;
        RefPtr<Canvas2DLayerBridge> m_parentLayerBridge;

        MailboxInfo(const MailboxInfo&);
        MailboxInfo() {}
    };
    MailboxInfo* createMailboxInfo();

    uint32_t m_lastImageId;
    Vector<MailboxInfo> m_mailboxes;
};
}
#endif
