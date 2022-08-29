// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>
#include <utility>

#include "GLFW/glfw3.h"
#include "dawn/common/Platform.h"
#include "dawn/utils/GLFWUtils.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#if defined(DAWN_USE_X11)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#if defined(DAWN_USE_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include "GLFW/glfw3native.h"

namespace utils {

void SetupGLFWWindowHintsForBackend(wgpu::BackendType type) {
    if (type == wgpu::BackendType::OpenGL) {
        // Ask for OpenGL 4.4 which is what the GL backend requires for compute shaders and
        // texture views.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    } else if (type == wgpu::BackendType::OpenGLES) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    } else {
        // Without this GLFW will initialize a GL context on the window, which prevents using
        // the window with other APIs (by crashing in weird ways).
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
}

wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window) {
    std::unique_ptr<wgpu::ChainedStruct> chainedDescriptor =
        SetupWindowAndGetSurfaceDescriptor(window);

    wgpu::SurfaceDescriptor descriptor;
    descriptor.nextInChain = chainedDescriptor.get();
    wgpu::Surface surface = instance.CreateSurface(&descriptor);

    return surface;
}

// SetupWindowAndGetSurfaceDescriptorCocoa defined in GLFWUtils_metal.mm
std::unique_ptr<wgpu::ChainedStruct> SetupWindowAndGetSurfaceDescriptorCocoa(GLFWwindow* window);

std::unique_ptr<wgpu::ChainedStruct> SetupWindowAndGetSurfaceDescriptor(GLFWwindow* window) {
    switch (glfwGetPlatform()) {
#if DAWN_PLATFORM_IS(WINDOWS)
        case GLFW_PLATFORM_WIN32: {
            std::unique_ptr<wgpu::SurfaceDescriptorFromWindowsHWND> desc =
                std::make_unique<wgpu::SurfaceDescriptorFromWindowsHWND>();
            desc->hwnd = glfwGetWin32Window(window);
            desc->hinstance = GetModuleHandle(nullptr);
            return std::move(desc);
        }
#endif
#if defined(DAWN_ENABLE_BACKEND_METAL)
        case GLFW_PLATFORM_COCOA:
            return SetupWindowAndGetSurfaceDescriptorCocoa(window);
#endif
#if defined(DAWN_USE_WAYLAND)
        case GLFW_PLATFORM_WAYLAND: {
            std::unique_ptr<wgpu::SurfaceDescriptorFromWaylandSurface> desc =
                std::make_unique<wgpu::SurfaceDescriptorFromWaylandSurface>();
            desc->display = glfwGetWaylandDisplay();
            desc->surface = glfwGetWaylandWindow(window);
            return std::move(desc);
        }
#endif
#if defined(DAWN_USE_X11)
        case GLFW_PLATFORM_X11: {
            std::unique_ptr<wgpu::SurfaceDescriptorFromXlibWindow> desc =
                std::make_unique<wgpu::SurfaceDescriptorFromXlibWindow>();
            desc->display = glfwGetX11Display();
            desc->window = glfwGetX11Window(window);
            return std::move(desc);
        }
#endif
        default:
            return nullptr;
    }
}

}  // namespace utils
