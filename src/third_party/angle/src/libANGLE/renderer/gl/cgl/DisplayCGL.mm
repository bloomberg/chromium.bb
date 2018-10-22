//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayCGL.mm: CGL implementation of egl::Display

#include "libANGLE/renderer/gl/cgl/DisplayCGL.h"

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <EGL/eglext.h>

#include "common/debug.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/cgl/IOSurfaceSurfaceCGL.h"
#include "libANGLE/renderer/gl/cgl/PbufferSurfaceCGL.h"
#include "libANGLE/renderer/gl/cgl/WindowSurfaceCGL.h"

namespace
{

const char *kDefaultOpenGLDylibName =
    "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib";
const char *kFallbackOpenGLDylibName = "GL";

}

namespace rx
{

class FunctionsGLCGL : public FunctionsGL
{
  public:
    FunctionsGLCGL(void *dylibHandle) : mDylibHandle(dylibHandle) {}

    ~FunctionsGLCGL() override { dlclose(mDylibHandle); }

  private:
    void *loadProcAddress(const std::string &function) const override
    {
        return dlsym(mDylibHandle, function.c_str());
    }

    void *mDylibHandle;
};

DisplayCGL::DisplayCGL(const egl::DisplayState &state)
    : DisplayGL(state), mEGLDisplay(nullptr), mContext(nullptr)
{
}

DisplayCGL::~DisplayCGL()
{
}

egl::Error DisplayCGL::initialize(egl::Display *display)
{
    mEGLDisplay = display;

    CGLPixelFormatObj pixelFormat;
    {
        // TODO(cwallez) investigate which pixel format we want
        CGLPixelFormatAttribute attribs[] = {
            kCGLPFAOpenGLProfile, static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            static_cast<CGLPixelFormatAttribute>(0)};
        GLint nVirtualScreens = 0;
        CGLChoosePixelFormat(attribs, &pixelFormat, &nVirtualScreens);

        if (pixelFormat == nullptr)
        {
            return egl::EglNotInitialized() << "Could not create the context's pixel format.";
        }
    }

    CGLCreateContext(pixelFormat, nullptr, &mContext);
    if (mContext == nullptr)
    {
        return egl::EglNotInitialized() << "Could not create the CGL context.";
    }
    CGLSetCurrentContext(mContext);

    // There is no equivalent getProcAddress in CGL so we open the dylib directly
    void *handle = dlopen(kDefaultOpenGLDylibName, RTLD_NOW);
    if (!handle)
    {
        handle = dlopen(kFallbackOpenGLDylibName, RTLD_NOW);
    }
    if (!handle)
    {
        return egl::EglNotInitialized() << "Could not open the OpenGL Framework.";
    }

    std::unique_ptr<FunctionsGL> functionsGL(new FunctionsGLCGL(handle));
    functionsGL->initialize(display->getAttributeMap());

    mRenderer.reset(new RendererGL(std::move(functionsGL), display->getAttributeMap()));

    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::EglNotInitialized() << "OpenGL ES 2.0 is not supportable.";
    }

    return DisplayGL::initialize(display);
}

void DisplayCGL::terminate()
{
    DisplayGL::terminate();

    mRenderer.reset();

    if (mContext != nullptr)
    {
        CGLSetCurrentContext(nullptr);
        CGLReleaseContext(mContext);
        mContext = nullptr;
    }
}

SurfaceImpl *DisplayCGL::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    return new WindowSurfaceCGL(state, mRenderer.get(), window, mContext);
}

SurfaceImpl *DisplayCGL::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    EGLint width  = static_cast<EGLint>(attribs.get(EGL_WIDTH, 0));
    EGLint height = static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0));
    return new PbufferSurfaceCGL(state, mRenderer.get(), width, height);
}

SurfaceImpl *DisplayCGL::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    ASSERT(buftype == EGL_IOSURFACE_ANGLE);

    return new IOSurfaceSurfaceCGL(state, mContext, clientBuffer, attribs);
}

SurfaceImpl *DisplayCGL::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ContextImpl *DisplayCGL::createContext(const gl::ContextState &state,
                                       const egl::Config *configuration,
                                       const gl::Context *shareContext,
                                       const egl::AttributeMap &attribs)
{
    return new ContextGL(state, mRenderer);
}

DeviceImpl *DisplayCGL::createDevice()
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::ConfigSet DisplayCGL::generateConfigs()
{
    // TODO(cwallez): generate more config permutations
    egl::ConfigSet configs;

    const gl::Version &maxVersion = getMaxSupportedESVersion();
    ASSERT(maxVersion >= gl::Version(2, 0));
    bool supportsES3 = maxVersion >= gl::Version(3, 0);

    egl::Config config;

    // Native stuff
    config.nativeVisualID   = 0;
    config.nativeVisualType = 0;
    config.nativeRenderable = EGL_TRUE;

    // Buffer sizes
    config.redSize     = 8;
    config.greenSize   = 8;
    config.blueSize    = 8;
    config.alphaSize   = 8;
    config.depthSize   = 24;
    config.stencilSize = 8;

    config.colorBufferType = EGL_RGB_BUFFER;
    config.luminanceSize   = 0;
    config.alphaMaskSize   = 0;

    config.bufferSize = config.redSize + config.greenSize + config.blueSize + config.alphaSize;

    config.transparentType = EGL_NONE;

    // Pbuffer
    config.maxPBufferWidth  = 4096;
    config.maxPBufferHeight = 4096;
    config.maxPBufferPixels = 4096 * 4096;

    // Caveat
    config.configCaveat = EGL_NONE;

    // Misc
    config.sampleBuffers     = 0;
    config.samples           = 0;
    config.level             = 0;
    config.bindToTextureRGB  = EGL_FALSE;
    config.bindToTextureRGBA = EGL_FALSE;

    config.surfaceType = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

    config.minSwapInterval = 1;
    config.maxSwapInterval = 1;

    config.renderTargetFormat = GL_RGBA8;
    config.depthStencilFormat = GL_DEPTH24_STENCIL8;

    config.conformant     = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
    config.renderableType = config.conformant;

    config.matchNativePixmap = EGL_NONE;

    config.colorComponentType = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

    configs.add(config);
    return configs;
}

bool DisplayCGL::testDeviceLost()
{
    // TODO(cwallez) investigate implementing this
    return false;
}

egl::Error DisplayCGL::restoreLostDevice(const egl::Display *display)
{
    UNIMPLEMENTED();
    return egl::EglBadDisplay();
}

bool DisplayCGL::isValidNativeWindow(EGLNativeWindowType window) const
{
    NSObject *layer = reinterpret_cast<NSObject *>(window);
    return [layer isKindOfClass:[CALayer class]];
}

egl::Error DisplayCGL::validateClientBuffer(const egl::Config *configuration,
                                            EGLenum buftype,
                                            EGLClientBuffer clientBuffer,
                                            const egl::AttributeMap &attribs) const
{
    ASSERT(buftype == EGL_IOSURFACE_ANGLE);

    if (!IOSurfaceSurfaceCGL::validateAttributes(clientBuffer, attribs))
    {
        return egl::EglBadAttribute();
    }

    return egl::NoError();
}

std::string DisplayCGL::getVendorString() const
{
    // TODO(cwallez) find a useful vendor string
    return "";
}

CGLContextObj DisplayCGL::getCGLContext() const
{
    return mContext;
}

void DisplayCGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->iosurfaceClientBuffer = true;
    outExtensions->surfacelessContext = true;

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    DisplayGL::generateExtensions(outExtensions);
}

void DisplayCGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

egl::Error DisplayCGL::waitClient(const gl::Context *context)
{
    // TODO(cwallez) UNIMPLEMENTED()
    return egl::NoError();
}

egl::Error DisplayCGL::waitNative(const gl::Context *context, EGLint engine)
{
    // TODO(cwallez) UNIMPLEMENTED()
    return egl::NoError();
}

gl::Version DisplayCGL::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

egl::Error DisplayCGL::makeCurrentSurfaceless(gl::Context *context)
{
    // We have nothing to do as mContext is always current, and that CGL is surfaceless by
    // default.
    return egl::NoError();
}
}
