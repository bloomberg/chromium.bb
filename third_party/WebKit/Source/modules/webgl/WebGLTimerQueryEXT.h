// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTimerQueryEXT_h
#define WebGLTimerQueryEXT_h

#include "modules/webgl/WebGLContextObject.h"

namespace blink {

class WebGLTimerQueryEXT : public WebGLContextObject {
    DEFINE_WRAPPERTYPEINFO();

public:
    static WebGLTimerQueryEXT* create(WebGLRenderingContextBase*);
    ~WebGLTimerQueryEXT() override;

    void setTarget(GLenum target) { m_target = target; }

    GLuint object() const { return m_queryId; }
    bool hasTarget() const { return m_target != 0; }
    GLenum target() const { return m_target; }

protected:
    WebGLTimerQueryEXT(WebGLRenderingContextBase*);

private:
    bool hasObject() const override { return m_queryId != 0; }
    void deleteObjectImpl(WebGraphicsContext3D*) override;

    GLenum m_target;
    GLuint m_queryId;
};

} // namespace blink

#endif // WebGLTimerQueryEXT_h
