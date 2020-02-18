//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncVk:
//    Defines the class interface for SyncVk, implementing SyncImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
#define LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_

#include "libANGLE/renderer/EGLSyncImpl.h"
#include "libANGLE/renderer/SyncImpl.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"

namespace egl
{
class AttributeMap;
}

namespace rx
{
namespace vk
{
// The behaviors of SyncImpl and EGLSyncImpl as fence syncs (only supported type) are currently
// identical for the Vulkan backend, and this class implements both interfaces.
class SyncHelper
{
  public:
    SyncHelper();
    ~SyncHelper();

    void releaseToRenderer(RendererVk *renderer);

    angle::Result initialize(ContextVk *contextVk);
    angle::Result clientWait(Context *context,
                             ContextVk *contextVk,
                             bool flushCommands,
                             uint64_t timeout,
                             VkResult *outResult);
    void serverWait(ContextVk *contextVk);
    angle::Result getStatus(Context *context, bool *signaled);

  private:
    // The vkEvent that's signaled on `init` and can be waited on in `serverWait`, or queried with
    // `getStatus`.
    Event mEvent;
    // The fence is signaled once the CB including the `init` signal is executed.
    // `clientWait` waits on this fence.
    Shared<Fence> mFence;

    SharedResourceUse mUse;
};
}  // namespace vk

class SyncVk final : public SyncImpl
{
  public:
    SyncVk();
    ~SyncVk() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result set(const gl::Context *context, GLenum condition, GLbitfield flags) override;
    angle::Result clientWait(const gl::Context *context,
                             GLbitfield flags,
                             GLuint64 timeout,
                             GLenum *outResult) override;
    angle::Result serverWait(const gl::Context *context,
                             GLbitfield flags,
                             GLuint64 timeout) override;
    angle::Result getStatus(const gl::Context *context, GLint *outResult) override;

  private:
    vk::SyncHelper mFenceSync;
};

class EGLSyncVk final : public EGLSyncImpl
{
  public:
    EGLSyncVk(const egl::AttributeMap &attribs);
    ~EGLSyncVk() override;

    void onDestroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display,
                          const gl::Context *context,
                          EGLenum type) override;
    egl::Error clientWait(const egl::Display *display,
                          const gl::Context *context,
                          EGLint flags,
                          EGLTime timeout,
                          EGLint *outResult) override;
    egl::Error serverWait(const egl::Display *display,
                          const gl::Context *context,
                          EGLint flags) override;
    egl::Error getStatus(const egl::Display *display, EGLint *outStatus) override;

    egl::Error dupNativeFenceFD(const egl::Display *display, EGLint *result) const override;

  private:
    vk::SyncHelper mFenceSync;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FENCESYNCVK_H_
