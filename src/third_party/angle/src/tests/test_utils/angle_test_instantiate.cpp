//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angle_test_instantiate.cpp: Adds support for filtering parameterized
// tests by platform, so we skip unsupported configs.

#include "test_utils/angle_test_instantiate.h"

#include <iostream>
#include <map>

#include "angle_gl.h"
#include "common/platform.h"
#include "gpu_info_util/SystemInfo.h"
#include "test_utils/angle_test_configs.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/system_utils.h"

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

namespace angle
{
namespace
{
SystemInfo *GetTestSystemInfo()
{
    static SystemInfo *sSystemInfo = nullptr;
    if (sSystemInfo == nullptr)
    {
        sSystemInfo = new SystemInfo;
        GetSystemInfo(sSystemInfo);
    }
    return sSystemInfo;
}

bool IsANGLEConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
    std::unique_ptr<angle::Library> eglLibrary;

#if defined(ANGLE_USE_UTIL_LOADER)
    eglLibrary.reset(angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME));
#endif

    EGLWindow *eglWindow =
        EGLWindow::New(param.majorVersion, param.minorVersion, param.eglParameters);
    bool result = eglWindow->initializeGL(osWindow, eglLibrary.get());
    eglWindow->destroyGL();
    EGLWindow::Delete(&eglWindow);
    return result;
}

bool IsWGLConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
#if defined(ANGLE_PLATFORM_WINDOWS) && defined(ANGLE_USE_UTIL_LOADER)
    std::unique_ptr<angle::Library> openglLibrary(angle::OpenSharedLibrary("opengl32"));

    WGLWindow *wglWindow = WGLWindow::New(param.majorVersion, param.minorVersion);
    bool result          = wglWindow->initializeGL(osWindow, openglLibrary.get());
    wglWindow->destroyGL();
    WGLWindow::Delete(&wglWindow);
    return result;
#else
    return false;
#endif  // defined(ANGLE_PLATFORM_WINDOWS) && defined(ANGLE_USE_UTIL_LOADER)
}

bool IsNativeConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
    // Not yet implemented.
    return false;
}
}  // namespace

bool IsAndroid()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return true;
#else
    return false;
#endif
}

bool IsLinux()
{
#if defined(ANGLE_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

bool IsOSX()
{
#if defined(ANGLE_PLATFORM_APPLE)
    return true;
#else
    return false;
#endif
}

bool IsOzone()
{
#if defined(USE_OZONE)
    return true;
#else
    return false;
#endif
}

bool IsWindows()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

bool IsFuchsia()
{
#if defined(ANGLE_PLATFORM_FUCHSIA)
    return true;
#else
    return false;
#endif
}

bool IsConfigWhitelisted(const SystemInfo &systemInfo, const PlatformParameters &param)
{
    VendorID vendorID = systemInfo.gpus[systemInfo.primaryGPUIndex].vendorId;

    // We support the default and null back-ends on every platform.
    if (param.driver == GLESDriverType::AngleEGL)
    {
        if (param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE)
            return true;
        if (param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE)
            return true;
    }

#if ANGLE_VULKAN_CONFORMANT_CONFIGS_ONLY
    // Vulkan ES 3.0 is not yet supported.
    if (param.majorVersion > 2 && param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
    {
        return false;
    }
#endif

    if (IsWindows())
    {
        switch (param.driver)
        {
            case GLESDriverType::AngleEGL:
                switch (param.getRenderer())
                {
                    case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
                    case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
                    case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
                    case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
                        return true;
                    case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
                        // ES 3.1+ back-end is not supported properly.
                        if (param.eglParameters.majorVersion == 3 &&
                            param.eglParameters.minorVersion > 0)
                        {
                            return false;
                        }

                        // Win ES emulation is currently only supported on NVIDIA.
                        return vendorID == kVendorID_Nvidia;
                    default:
                        return false;
                }
            case GLESDriverType::SystemWGL:
                // AMD does not support the ES compatibility extensions.
                return vendorID != kVendorID_AMD;
            default:
                return false;
        }
    }

    if (IsOSX())
    {
        // Currently we only support the OpenGL back-end on OSX.
        if (param.driver != GLESDriverType::AngleEGL)
        {
            return false;
        }

        // OSX does not support ES 3.1 features.
        if (param.majorVersion == 3 && param.minorVersion > 0)
        {
            return false;
        }

        return (param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE);
    }

    if (IsFuchsia())
    {
        // Currently we only support the Vulkan back-end on Fuchsia.
        if (param.driver != GLESDriverType::AngleEGL)
        {
            return false;
        }

        return (param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);
    }

    if (IsOzone())
    {
        // Currently we only support the GLES back-end on Ozone.
        if (param.driver != GLESDriverType::AngleEGL)
            return false;

        // ES 3 configs do not work properly on Ozone.
        if (param.majorVersion > 2)
            return false;

        return (param.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE);
    }

    if (IsLinux())
    {
        // Currently we support the OpenGL and Vulkan back-ends on Linux.
        if (param.driver != GLESDriverType::AngleEGL)
        {
            return false;
        }

        switch (param.getRenderer())
        {
            case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
                // Note that system info collection depends on Vulkan support.
                return true;
            default:
                return false;
        }
    }

    if (IsAndroid())
    {
        // Currently we support the GLES and Vulkan back-ends on Linux.
        if (param.driver != GLESDriverType::AngleEGL)
        {
            return false;
        }

        // Some Android devices don't support backing 3.2 contexts. We should refine this to only
        // exclude the problematic devices.
        if (param.eglParameters.majorVersion == 3 && param.eglParameters.minorVersion == 2)
        {
            return false;
        }

        switch (param.getRenderer())
        {
            case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
                return true;
            default:
                return false;
        }
    }

    // Unknown platform.
    return false;
}

bool IsConfigSupported(const PlatformParameters &param)
{
    OSWindow *osWindow = OSWindow::New();
    bool result        = false;
    if (osWindow->initialize("CONFIG_TESTER", 1, 1))
    {
        switch (param.driver)
        {
            case GLESDriverType::AngleEGL:
                result = IsANGLEConfigSupported(param, osWindow);
                break;
            case GLESDriverType::SystemEGL:
                result = IsNativeConfigSupported(param, osWindow);
                break;
            case GLESDriverType::SystemWGL:
                result = IsWGLConfigSupported(param, osWindow);
                break;
        }

        osWindow->destroy();
    }

    OSWindow::Delete(&osWindow);
    return result;
}

bool IsPlatformAvailable(const PlatformParameters &param)
{
    switch (param.getRenderer())
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            break;

        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
#if !defined(ANGLE_ENABLE_D3D9)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
#if !defined(ANGLE_ENABLE_D3D11)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
#if !defined(ANGLE_ENABLE_OPENGL)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
#if !defined(ANGLE_ENABLE_VULKAN)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
#ifndef ANGLE_ENABLE_NULL
            return false;
#endif
            break;

        default:
            std::cout << "Unknown test platform: " << param << std::endl;
            return false;
    }

    static std::map<PlatformParameters, bool> paramAvailabilityCache;
    auto iter = paramAvailabilityCache.find(param);
    if (iter != paramAvailabilityCache.end())
    {
        return iter->second;
    }
    else
    {
        const SystemInfo *systemInfo = GetTestSystemInfo();

        bool result = false;
        if (systemInfo)
        {
            result = IsConfigWhitelisted(*systemInfo, param);
        }
        else
        {
            result = IsConfigSupported(param);
        }

        paramAvailabilityCache[param] = result;

        if (!result)
        {
            std::cout << "Skipping tests using configuration " << param
                      << " because it is not available." << std::endl;
        }

        return result;
    }
}
}  // namespace angle
