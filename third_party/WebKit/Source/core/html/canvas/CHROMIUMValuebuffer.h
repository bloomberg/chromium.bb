// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMValuebuffer_h
#define CHROMIUMValuebuffer_h

#include "core/html/canvas/WebGLSharedObject.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class CHROMIUMValuebuffer final : public WebGLSharedObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~CHROMIUMValuebuffer();

    static PassRefPtrWillBeRawPtr<CHROMIUMValuebuffer> create(WebGLRenderingContextBase*);

    bool hasEverBeenBound() const { return m_hasEverBeenBound; }

    void setHasEverBeenBound() { m_hasEverBeenBound = true; }

protected:
    explicit CHROMIUMValuebuffer(WebGLRenderingContextBase*);

    virtual void deleteObjectImpl(blink::WebGraphicsContext3D*, Platform3DObject) override;

private:

    bool m_hasEverBeenBound;
};

} // namespace blink

#endif // CHROMIUMValuebuffer_h
