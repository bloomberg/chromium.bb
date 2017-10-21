// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTimerQueryEXT_h
#define WebGLTimerQueryEXT_h

#include "modules/webgl/WebGLContextObject.h"
#include "platform/WebTaskRunner.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGLTimerQueryEXT : public WebGLContextObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLTimerQueryEXT* Create(WebGLRenderingContextBase*);
  ~WebGLTimerQueryEXT() override;

  void SetTarget(GLenum target) { target_ = target; }

  GLuint Object() const { return query_id_; }
  bool HasTarget() const { return target_ != 0; }
  GLenum Target() const { return target_; }

  void ResetCachedResult();
  void UpdateCachedResult(gpu::gles2::GLES2Interface*);

  bool IsQueryResultAvailable();
  GLuint64 GetQueryResult();

 protected:
  WebGLTimerQueryEXT(WebGLRenderingContextBase*);

 private:
  bool HasObject() const override { return query_id_ != 0; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  void ScheduleAllowAvailabilityUpdate();
  void AllowAvailabilityUpdate();

  GLenum target_;
  GLuint query_id_;

  bool can_update_availability_;
  bool query_result_available_;
  GLuint64 query_result_;

  scoped_refptr<WebTaskRunner> task_runner_;
  TaskHandle task_handle_;
};

}  // namespace blink

#endif  // WebGLTimerQueryEXT_h
