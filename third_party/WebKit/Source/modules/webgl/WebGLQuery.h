// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLQuery_h
#define WebGLQuery_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "platform/WebTaskRunner.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGL2RenderingContextBase;
class WebTaskRunner;

class WebGLQuery : public WebGLSharedPlatform3DObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLQuery() override;

  static WebGLQuery* Create(WebGL2RenderingContextBase*);

  void SetTarget(GLenum);
  bool HasTarget() const { return target_ != 0; }
  GLenum GetTarget() const { return target_; }

  void ResetCachedResult();
  void UpdateCachedResult(gpu::gles2::GLES2Interface*);

  bool IsQueryResultAvailable();
  GLuint GetQueryResult();

 protected:
  explicit WebGLQuery(WebGL2RenderingContextBase*);

  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

 private:
  bool IsQuery() const override { return true; }

  void ScheduleAllowAvailabilityUpdate();
  void AllowAvailabilityUpdate();

  GLenum target_;

  bool can_update_availability_;
  bool query_result_available_;
  GLuint query_result_;

  scoped_refptr<WebTaskRunner> task_runner_;
  TaskHandle task_handle_;
};

}  // namespace blink

#endif  // WebGLQuery_h
