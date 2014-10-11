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

#ifndef WebScrollbarThemeClientImpl_h
#define WebScrollbarThemeClientImpl_h

#include "platform/PlatformExport.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "public/platform/WebScrollbar.h"
#include "wtf/Noncopyable.h"

namespace blink {

// Adapts a WebScrollbar to the ScrollbarThemeClient interface
class PLATFORM_EXPORT WebScrollbarThemeClientImpl : public ScrollbarThemeClient {
    WTF_MAKE_NONCOPYABLE(WebScrollbarThemeClientImpl);
public:
    // Caller must retain ownership of this pointer and ensure that its lifetime
    // exceeds this instance.
    WebScrollbarThemeClientImpl(WebScrollbar*);
    virtual ~WebScrollbarThemeClientImpl();

    // Implement ScrollbarThemeClient interface
    virtual int x() const override;
    virtual int y() const override;
    virtual int width() const override;
    virtual int height() const override;
    virtual IntSize size() const override;
    virtual IntPoint location() const override;
    virtual Widget* parent() const override;
    virtual Widget* root() const override;
    virtual void setFrameRect(const IntRect&) override;
    virtual IntRect frameRect() const override;
    virtual void invalidate() override;
    virtual void invalidateRect(const IntRect&) override;
    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const override;
    virtual void getTickmarks(Vector<IntRect>&) const override;
    virtual bool isScrollableAreaActive() const override;
    virtual IntPoint convertFromContainingWindow(const IntPoint&) override;
    virtual bool isCustomScrollbar() const override;
    virtual ScrollbarOrientation orientation() const override;
    virtual bool isLeftSideVerticalScrollbar() const override;
    virtual int value() const override;
    virtual float currentPos() const override;
    virtual int visibleSize() const override;
    virtual int totalSize() const override;
    virtual int maximum() const override;
    virtual ScrollbarControlSize controlSize() const override;
    virtual ScrollbarPart pressedPart() const override;
    virtual ScrollbarPart hoveredPart() const override;
    virtual void styleChanged() override;
    virtual bool enabled() const override;
    virtual void setEnabled(bool) override;
    virtual bool isOverlayScrollbar() const override;
    virtual bool isAlphaLocked() const override;
    virtual void setIsAlphaLocked(bool) override;

private:
    WebScrollbar* m_scrollbar;
};

} // namespace blink

#endif // WebScrollbarThemeClientImpl_h
