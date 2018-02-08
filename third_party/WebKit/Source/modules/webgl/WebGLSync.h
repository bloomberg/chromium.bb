// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSync_h
#define WebGLSync_h

#include "base/single_thread_task_runner.h"
#include "modules/webgl/WebGLSharedObject.h"
#include "platform/WebTaskRunner.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSync : public WebGLSharedObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLSync() override;

  GLsync Object() const { return object_; }

  void UpdateCache(gpu::gles2::GLES2Interface*);
  GLint GetCachedResult(GLenum pname);
  bool IsSignaled() const;

 protected:
  WebGLSync(WebGL2RenderingContextBase*, GLsync, GLenum object_type);

  bool HasObject() const override { return object_ != nullptr; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  GLenum ObjectType() const { return object_type_; }

 private:
  bool IsSync() const override { return true; }

  void ScheduleAllowCacheUpdate();
  void AllowCacheUpdate();

  bool allow_cache_update_ = false;
  // Initialized in cpp file to avoid including gl3.h in this header.
  GLint sync_status_;

  GLsync object_;
  GLenum object_type_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TaskHandle task_handle_;
};

}  // namespace blink

#endif  // WebGLSync_h
