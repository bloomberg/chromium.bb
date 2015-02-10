// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSampler_h
#define WebGLSampler_h

#include "core/html/canvas/WebGLSharedObject.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSampler : public WebGLSharedObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLSampler() override;

    static PassRefPtrWillBeRawPtr<WebGLSampler> create(WebGL2RenderingContextBase*);

protected:
    explicit WebGLSampler(WebGL2RenderingContextBase*);

    void deleteObjectImpl(blink::WebGraphicsContext3D*, Platform3DObject) override;

private:
    bool isSampler() const override { return true; }
};

} // namespace blink

#endif // WebGLSampler_h
