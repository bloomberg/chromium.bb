//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVk.cpp:
//    Implements the class methods for DisplayVk.
//

#include "libANGLE/renderer/vulkan/DisplayVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"

namespace rx
{

DisplayVk::DisplayVk(const egl::DisplayState &state)
    : DisplayImpl(state), vk::Context(new RendererVk())
{
}

DisplayVk::~DisplayVk()
{
    delete mRenderer;
}

egl::Error DisplayVk::initialize(egl::Display *display)
{
    ASSERT(mRenderer != nullptr && display != nullptr);
    angle::Result result = mRenderer->initialize(this, display->getAttributeMap(), getWSIName());
    ANGLE_TRY(angle::ToEGL(result, this, EGL_NOT_INITIALIZED));
    return egl::NoError();
}

void DisplayVk::terminate()
{
    ASSERT(mRenderer);
    mRenderer->onDestroy(this);
}

egl::Error DisplayVk::makeCurrent(egl::Surface * /*drawSurface*/,
                                  egl::Surface * /*readSurface*/,
                                  gl::Context * /*context*/)
{
    return egl::NoError();
}

bool DisplayVk::testDeviceLost()
{
    // TODO(jmadill): Figure out how to do device lost in Vulkan.
    return false;
}

egl::Error DisplayVk::restoreLostDevice(const egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

std::string DisplayVk::getVendorString() const
{
    std::string vendorString = "Google Inc.";
    if (mRenderer)
    {
        vendorString += " " + mRenderer->getVendorString();
    }

    return vendorString;
}

DeviceImpl *DisplayVk::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayVk::waitClient(const gl::Context *context)
{
    // TODO(jmadill): Call flush instead of finish once it is implemented in RendererVK.
    // http://anglebug.com/2504
    UNIMPLEMENTED();

    return angle::ToEGL(mRenderer->finish(this), this, EGL_BAD_ACCESS);
}

egl::Error DisplayVk::waitNative(const gl::Context *context, EGLint engine)
{
    UNIMPLEMENTED();
    return egl::EglBadAccess();
}

SurfaceImpl *DisplayVk::createWindowSurface(const egl::SurfaceState &state,
                                            EGLNativeWindowType window,
                                            const egl::AttributeMap &attribs)
{
    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);

    return createWindowSurfaceVk(state, window, width, height);
}

SurfaceImpl *DisplayVk::createPbufferSurface(const egl::SurfaceState &state,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer);

    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);

    return new OffscreenSurfaceVk(state, width, height);
}

SurfaceImpl *DisplayVk::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                      EGLenum buftype,
                                                      EGLClientBuffer clientBuffer,
                                                      const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

SurfaceImpl *DisplayVk::createPixmapSurface(const egl::SurfaceState &state,
                                            NativePixmapType nativePixmap,
                                            const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<SurfaceImpl *>(0);
}

ImageImpl *DisplayVk::createImage(const egl::ImageState &state,
                                  const gl::Context *context,
                                  EGLenum target,
                                  const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<ImageImpl *>(0);
}

ContextImpl *DisplayVk::createContext(const gl::ContextState &state,
                                      const egl::Config *configuration,
                                      const gl::Context *shareContext,
                                      const egl::AttributeMap &attribs)
{
    return new ContextVk(state, mRenderer);
}

StreamProducerImpl *DisplayVk::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return static_cast<StreamProducerImpl *>(0);
}

gl::Version DisplayVk::getMaxSupportedESVersion() const
{
    UNIMPLEMENTED();
    return gl::Version(0, 0);
}

void DisplayVk::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->surfaceOrientation       = true;
    outExtensions->displayTextureShareGroup = true;

    // TODO(geofflang): Extension is exposed but not implemented so that other aspects of the Vulkan
    // backend can be tested in Chrome. http://anglebug.com/2722
    outExtensions->robustResourceInitialization = true;
}

void DisplayVk::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

void DisplayVk::handleError(VkResult result, const char *file, unsigned int line)
{
    std::stringstream errorStream;
    errorStream << "Internal Vulkan error: " << VulkanResultString(result) << ", in " << file
                << ", line " << line << ".";
    mStoredErrorString = errorStream.str();
}

// TODO(jmadill): Remove this. http://anglebug.com/2491
egl::Error DisplayVk::getEGLError(EGLint errorCode)
{
    return egl::Error(errorCode, 0, std::move(mStoredErrorString));
}
}  // namespace rx
