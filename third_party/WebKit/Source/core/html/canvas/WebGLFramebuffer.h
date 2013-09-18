/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef WebGLFramebuffer_h
#define WebGLFramebuffer_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/canvas/WebGLContextObject.h"
#include "core/html/canvas/WebGLSharedObject.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class WebGLRenderbuffer;
class WebGLTexture;

class WebGLFramebuffer : public WebGLContextObject, public ScriptWrappable {
public:
    class WebGLAttachment : public RefCounted<WebGLAttachment> {
    public:
        virtual ~WebGLAttachment();

        virtual GC3Dsizei width() const = 0;
        virtual GC3Dsizei height() const = 0;
        virtual GC3Denum format() const = 0;
        // For texture attachment, type() returns the type of the attached texture.
        // For renderbuffer attachment, the type of the renderbuffer may vary with GL implementation.
        // To avoid confusion, it would be better to not implement type() for renderbuffer attachment and
        // we should always use the internalformat of the renderbuffer and avoid using type() API.
        virtual GC3Denum type() const = 0;
        virtual WebGLSharedObject* object() const = 0;
        virtual bool isSharedObject(WebGLSharedObject*) const = 0;
        virtual bool valid() const = 0;
        virtual bool initialized() const = 0;
        virtual void setInitialized() = 0;
        virtual void onDetached(GraphicsContext3D*) = 0;
        virtual void attach(GraphicsContext3D*, GC3Denum attachment) = 0;
        virtual void unattach(GraphicsContext3D*, GC3Denum attachment) = 0;

    protected:
        WebGLAttachment();
    };

    virtual ~WebGLFramebuffer();

    static PassRefPtr<WebGLFramebuffer> create(WebGLRenderingContext*);

    void setAttachmentForBoundFramebuffer(GC3Denum attachment, GC3Denum texTarget, WebGLTexture*, GC3Dint level);
    void setAttachmentForBoundFramebuffer(GC3Denum attachment, WebGLRenderbuffer*);
    // If an object is attached to the currently bound framebuffer, remove it.
    void removeAttachmentFromBoundFramebuffer(WebGLSharedObject*);
    // If a given attachment point for the currently bound framebuffer is not null, remove the attached object.
    void removeAttachmentFromBoundFramebuffer(GC3Denum);
    WebGLSharedObject* getAttachmentObject(GC3Denum) const;

    GC3Denum colorBufferFormat() const;
    GC3Dsizei colorBufferWidth() const;
    GC3Dsizei colorBufferHeight() const;

    // This should always be called before drawArray, drawElements, clear,
    // readPixels, copyTexImage2D, copyTexSubImage2D if this framebuffer is
    // currently bound.
    // Return false if the framebuffer is incomplete; otherwise initialize
    // the buffers if they haven't been initialized and
    // needToInitializeAttachments is true.
    bool onAccess(GraphicsContext3D*, const char** reason);

    // Software version of glCheckFramebufferStatus(), except that when
    // FRAMEBUFFER_COMPLETE is returned, it is still possible for
    // glCheckFramebufferStatus() to return FRAMEBUFFER_UNSUPPORTED,
    // depending on hardware implementation.
    GC3Denum checkStatus(const char** reason) const;

    bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }

    void setHasEverBeenBound() { m_hasEverBeenBound = true; }

    bool hasStencilBuffer() const;

    // Wrapper for drawBuffersEXT/drawBuffersARB to work around a driver bug.
    void drawBuffers(const Vector<GC3Denum>& bufs);

    GC3Denum getDrawBuffer(GC3Denum);

protected:
    WebGLFramebuffer(WebGLRenderingContext*);

    virtual void deleteObjectImpl(GraphicsContext3D*, Platform3DObject);

private:
    virtual bool isFramebuffer() const { return true; }

    WebGLAttachment* getAttachment(GC3Denum) const;
    bool isAttachmentComplete(WebGLAttachment* attachedObject, GC3Denum attachment, const char** reason) const;

    // Check if the framebuffer is currently bound.
    bool isBound() const;

    // attach 'attachment' at 'attachmentPoint'.
    void attach(GC3Denum attachment, GC3Denum attachmentPoint);

    // Check if a new drawBuffers call should be issued. This is called when we add or remove an attachment.
    void drawBuffersIfNecessary(bool force);

    typedef WTF::HashMap<GC3Denum, RefPtr<WebGLAttachment> > AttachmentMap;

    AttachmentMap m_attachments;

    bool m_hasEverBeenBound;

    Vector<GC3Denum> m_drawBuffers;
    Vector<GC3Denum> m_filteredDrawBuffers;
};

} // namespace WebCore

#endif // WebGLFramebuffer_h
