// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSync_h
#define WebGLSync_h

#include "core/html/canvas/WebGLSharedObject.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSync : public WebGLSharedObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLSync() override;

protected:
    WebGLSync(WebGL2RenderingContextBase*, GLenum objectType);

    void deleteObjectImpl(blink::WebGraphicsContext3D*, Platform3DObject) override;

    GLenum objectType() const { return m_objectType; }

private:
    bool isSync() const override { return true; }

    GLenum m_objectType;
};

} // namespace blink

#endif // WebGLSync_h
