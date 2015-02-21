// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTransformFeedback_h
#define WebGLTransformFeedback_h

#include "core/html/canvas/WebGLSharedPlatform3DObject.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLTransformFeedback : public WebGLSharedPlatform3DObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLTransformFeedback() override;

    static PassRefPtrWillBeRawPtr<WebGLTransformFeedback> create(WebGL2RenderingContextBase*);

protected:
    explicit WebGLTransformFeedback(WebGL2RenderingContextBase*);

    void deleteObjectImpl(blink::WebGraphicsContext3D*) override;

private:
    bool isTransformFeedback() const override { return true; }
};

} // namespace blink

#endif // WebGLTransformFeedback_h
