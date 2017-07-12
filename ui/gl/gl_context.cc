// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context.h"

#include <string>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_local.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

namespace {
base::LazyInstance<base::ThreadLocalPointer<GLContext>>::Leaky
    current_context_ = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ThreadLocalPointer<GLContext>>::Leaky
    current_real_context_ = LAZY_INSTANCE_INITIALIZER;
}  // namespace

GLContext::ScopedReleaseCurrent::ScopedReleaseCurrent() : canceled_(false) {}

GLContext::ScopedReleaseCurrent::~ScopedReleaseCurrent() {
  if (!canceled_ && GetCurrent()) {
    GetCurrent()->ReleaseCurrent(nullptr);
  }
}

void GLContext::ScopedReleaseCurrent::Cancel() {
  canceled_ = true;
}

GLContext::GLContext(GLShareGroup* share_group) : share_group_(share_group) {
  if (!share_group_.get())
    share_group_ = new gl::GLShareGroup();
  share_group_->AddContext(this);
}

GLContext::~GLContext() {
  share_group_->RemoveContext(this);
  if (GetCurrent() == this) {
    SetCurrent(nullptr);
    SetCurrentGL(nullptr);
  }
}

GLApi* GLContext::CreateGLApi(DriverGL* driver) {
  real_gl_api_ = new RealGLApi;
  real_gl_api_->Initialize(driver);
  real_gl_api_->set_gl_workarounds(gl_workarounds_);
  return real_gl_api_;
}

void GLContext::SetSafeToForceGpuSwitch() {
}

bool GLContext::ForceGpuSwitchIfNeeded() {
  return true;
}

void GLContext::SetUnbindFboOnMakeCurrent() {
  NOTIMPLEMENTED();
}

std::string GLContext::GetExtensions() {
  DCHECK(IsCurrent(nullptr));
  return GetGLExtensionsFromCurrentContext(gl_api_.get());
}

std::string GLContext::GetGLVersion() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(gl_api_ != nullptr);
  const char* version =
      reinterpret_cast<const char*>(gl_api_->glGetStringFn(GL_VERSION));
  return std::string(version ? version : "");
}

std::string GLContext::GetGLRenderer() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(gl_api_ != nullptr);
  const char* renderer =
      reinterpret_cast<const char*>(gl_api_->glGetStringFn(GL_RENDERER));
  return std::string(renderer ? renderer : "");
}

YUVToRGBConverter* GLContext::GetYUVToRGBConverter() {
  return nullptr;
}

CurrentGL* GLContext::GetCurrentGL() {
  if (!static_bindings_initialized_) {
    driver_gl_.reset(new DriverGL);
    driver_gl_->InitializeStaticBindings();

    gl_api_.reset(CreateGLApi(driver_gl_.get()));
    GLApi* final_api = gl_api_.get();

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableGPUServiceTracing)) {
      trace_gl_api_.reset(new TraceGLApi(final_api));
      final_api = trace_gl_api_.get();
    }

    if (GetDebugGLBindingsInitializedGL()) {
      debug_gl_api_.reset(new DebugGLApi(final_api));
      final_api = debug_gl_api_.get();
    }

    current_gl_.reset(new CurrentGL);
    current_gl_->Driver = driver_gl_.get();
    current_gl_->Api = final_api;
    current_gl_->Version = version_info_.get();

    static_bindings_initialized_ = true;
  }

  return current_gl_.get();
}

void GLContext::ReinitializeDynamicBindings() {
  DCHECK(IsCurrent(nullptr));
  dynamic_bindings_initialized_ = false;
  InitializeDynamicBindings();
}

void GLContext::ForceReleaseVirtuallyCurrent() {
  NOTREACHED();
}

bool GLContext::HasExtension(const char* name) {
  std::string extensions = GetExtensions();
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

const GLVersionInfo* GLContext::GetVersionInfo() {
  if (!version_info_) {
    version_info_ = GenerateGLVersionInfo();

    // current_gl_ may be null for virtual contexts
    if (current_gl_) {
      current_gl_->Version = version_info_.get();
    }
  }
  return version_info_.get();
}

GLShareGroup* GLContext::share_group() {
  return share_group_.get();
}

bool GLContext::LosesAllContextsOnContextLost() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return false;
    case kGLImplementationEGLGLES2:
    case kGLImplementationSwiftShaderGL:
      return true;
    case kGLImplementationOSMesaGL:
    case kGLImplementationAppleGL:
      return false;
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return false;
    default:
      NOTREACHED();
      return true;
  }
}

GLContext* GLContext::GetCurrent() {
  return current_context_.Pointer()->Get();
}

GLContext* GLContext::GetRealCurrent() {
  return current_real_context_.Pointer()->Get();
}

std::unique_ptr<gl::GLVersionInfo> GLContext::GenerateGLVersionInfo() {
  return base::MakeUnique<GLVersionInfo>(
      GetGLVersion().c_str(), GetGLRenderer().c_str(), GetExtensions().c_str());
}

void GLContext::SetCurrent(GLSurface* surface) {
  current_context_.Pointer()->Set(surface ? this : nullptr);
  GLSurface::SetCurrent(surface);
  // Leave the real GL api current so that unit tests work correctly.
  // TODO(sievers): Remove this, but needs all gpu_unittest classes
  // to create and make current a context.
  if (!surface && GetGLImplementation() != kGLImplementationMockGL &&
      GetGLImplementation() != kGLImplementationStubGL) {
    SetCurrentGL(nullptr);
  }
}

void GLContext::SetGLWorkarounds(const GLWorkarounds& workarounds) {
  DCHECK(IsCurrent(nullptr));
  gl_workarounds_ = workarounds;
  if (real_gl_api_) {
    real_gl_api_->set_gl_workarounds(gl_workarounds_);
  }
}

GLStateRestorer* GLContext::GetGLStateRestorer() {
  return state_restorer_.get();
}

void GLContext::SetGLStateRestorer(GLStateRestorer* state_restorer) {
  state_restorer_ = base::WrapUnique(state_restorer);
}

void GLContext::SetSwapInterval(int interval) {
  if (swap_interval_ == interval)
    return;
  swap_interval_ = interval;
  OnSetSwapInterval(force_swap_interval_zero_ ? 0 : swap_interval_);
}

void GLContext::ForceSwapIntervalZero(bool force) {
  if (force_swap_interval_zero_ == force)
    return;
  force_swap_interval_zero_ = force;
  OnSetSwapInterval(force_swap_interval_zero_ ? 0 : swap_interval_);
}

bool GLContext::WasAllocatedUsingRobustnessExtension() {
  return false;
}

void GLContext::InitializeDynamicBindings() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(static_bindings_initialized_);
  if (!dynamic_bindings_initialized_) {
    if (real_gl_api_) {
      real_gl_api_->InitializeFilteredExtensions();
      real_gl_api_->set_version(GenerateGLVersionInfo());
    }

    driver_gl_->InitializeDynamicBindings(GetVersionInfo(), GetExtensions());
    dynamic_bindings_initialized_ = true;
  }
}

bool GLContext::MakeVirtuallyCurrent(
    GLContext* virtual_context, GLSurface* surface) {
  if (!ForceGpuSwitchIfNeeded())
    return false;
  bool switched_real_contexts = GLContext::GetRealCurrent() != this;
  GLSurface* current_surface = GLSurface::GetCurrent();
  if (switched_real_contexts || surface != current_surface) {
    // MakeCurrent 'lite' path that avoids potentially expensive MakeCurrent()
    // calls if the GLSurface uses the same underlying surface or renders to
    // an FBO.
    if (switched_real_contexts || !current_surface ||
        !virtual_context->IsCurrent(surface)) {
      if (!MakeCurrent(surface)) {
        return false;
      }
    }
  }

  DCHECK_EQ(this, GLContext::GetRealCurrent());
  DCHECK(IsCurrent(NULL));
  DCHECK(virtual_context->IsCurrent(surface));

  if (switched_real_contexts || virtual_context != current_virtual_context_) {
#if DCHECK_IS_ON()
    GLenum error = glGetError();
    // Accepting a context loss error here enables using debug mode to work on
    // context loss handling in virtual context mode.
    // There should be no other errors from the previous context leaking into
    // the new context.
    DCHECK(error == GL_NO_ERROR || error == GL_CONTEXT_LOST_KHR) <<
        "GL error was: " << error;
#endif

    // Set all state that is different from the real state
    if (virtual_context->GetGLStateRestorer()->IsInitialized()) {
      GLStateRestorer* virtual_state = virtual_context->GetGLStateRestorer();
      GLStateRestorer* current_state =
          current_virtual_context_
              ? current_virtual_context_->GetGLStateRestorer()
              : nullptr;
      if (current_state)
        current_state->PauseQueries();
      virtual_state->ResumeQueries();

      virtual_state->RestoreState(
          (current_state && !switched_real_contexts) ? current_state : NULL);
    }
    current_virtual_context_ = virtual_context;
  }

  virtual_context->SetCurrent(surface);
  if (!surface->OnMakeCurrent(virtual_context)) {
    LOG(ERROR) << "Could not make GLSurface current.";
    return false;
  }
  return true;
}

void GLContext::OnReleaseVirtuallyCurrent(GLContext* virtual_context) {
  if (current_virtual_context_ == virtual_context)
    current_virtual_context_ = nullptr;
}

void GLContext::BindGLApi() {
  SetCurrentGL(GetCurrentGL());
}

GLContextReal::GLContextReal(GLShareGroup* share_group)
    : GLContext(share_group) {}

scoped_refptr<GPUTimingClient> GLContextReal::CreateGPUTimingClient() {
  if (!gpu_timing_) {
    gpu_timing_.reset(GPUTiming::CreateGPUTiming(this));
  }
  return gpu_timing_->CreateGPUTimingClient();
}

GLContextReal::~GLContextReal() {
  if (GetRealCurrent() == this)
    current_real_context_.Pointer()->Set(nullptr);
}

void GLContextReal::SetCurrent(GLSurface* surface) {
  GLContext::SetCurrent(surface);
  current_real_context_.Pointer()->Set(surface ? this : nullptr);
}

scoped_refptr<GLContext> InitializeGLContext(scoped_refptr<GLContext> context,
                                             GLSurface* compatible_surface,
                                             const GLContextAttribs& attribs) {
  if (!context->Initialize(compatible_surface, attribs))
    return nullptr;
  return context;
}

}  // namespace gl
