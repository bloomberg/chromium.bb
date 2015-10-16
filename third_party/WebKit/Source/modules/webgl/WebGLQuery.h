// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLQuery_h
#define WebGLQuery_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLQuery : public WebGLSharedPlatform3DObject, public WebThread::TaskObserver {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLQuery() override;

    static WebGLQuery* create(WebGL2RenderingContextBase*);

    void setTarget(GLenum);
    bool hasTarget() const { return m_target != 0; }
    GLenum getTarget() const { return m_target; }

    void resetCachedResult();
    void updateCachedResult(WebGraphicsContext3D*);

    bool isQueryResultAvailable();
    GLuint getQueryResult();

protected:
    explicit WebGLQuery(WebGL2RenderingContextBase*);

    void deleteObjectImpl(WebGraphicsContext3D*) override;

private:
    bool isQuery() const override { return true; }

    void registerTaskObserver();
    void unregisterTaskObserver();

    // TaskObserver implementation.
    void didProcessTask() override;
    void willProcessTask() override { }

    GLenum m_target;

    bool m_taskObserverRegistered;
    bool m_canUpdateAvailability;
    bool m_queryResultAvailable;
    GLuint m_queryResult;
};

} // namespace blink

#endif // WebGLQuery_h
