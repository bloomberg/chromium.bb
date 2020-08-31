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
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
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

// static
base::subtle::Atomic32 GLContext::total_gl_contexts_ = 0;
// static
bool GLContext::switchable_gpus_supported_ = false;
// static
GpuPreference GLContext::forced_gpu_preference_ = GpuPreference::kDefault;

GLContext::ScopedReleaseCurrent::ScopedReleaseCurrent() : canceled_(false) {}

GLContext::ScopedReleaseCurrent::~ScopedReleaseCurrent() {
  if (!canceled_ && GetCurrent()) {
    GetCurrent()->ReleaseCurrent(nullptr);
  }
}

void GLContext::ScopedReleaseCurrent::Cancel() {
  canceled_ = true;
}

GLContextAttribs::GLContextAttribs() = default;
GLContextAttribs::GLContextAttribs(const GLContextAttribs& other) = default;
GLContextAttribs::GLContextAttribs(GLContextAttribs&& other) = default;
GLContextAttribs::~GLContextAttribs() = default;

GLContextAttribs& GLContextAttribs::operator=(const GLContextAttribs& other) =
    default;
GLContextAttribs& GLContextAttribs::operator=(GLContextAttribs&& other) =
    default;

GLContext::GLContext(GLShareGroup* share_group) : share_group_(share_group) {
  if (!share_group_.get())
    share_group_ = new gl::GLShareGroup();
  share_group_->AddContext(this);
  base::subtle::NoBarrier_AtomicIncrement(&total_gl_contexts_, 1);
}

GLContext::~GLContext() {
#if defined(OS_MACOSX)
  DCHECK(!HasBackpressureFences());
#endif
  share_group_->RemoveContext(this);
  if (GetCurrent() == this) {
    SetCurrent(nullptr);
    SetCurrentGL(nullptr);
  }
  base::subtle::Atomic32 after_value =
      base::subtle::NoBarrier_AtomicIncrement(&total_gl_contexts_, -1);
  DCHECK(after_value >= 0);
}

// static
int32_t GLContext::TotalGLContexts() {
  return static_cast<int32_t>(
      base::subtle::NoBarrier_Load(&total_gl_contexts_));
}

// static
bool GLContext::SwitchableGPUsSupported() {
  return switchable_gpus_supported_;
}

// static
void GLContext::SetSwitchableGPUsSupported() {
  DCHECK(!switchable_gpus_supported_);
  switchable_gpus_supported_ = true;
}

// static
void GLContext::SetForcedGpuPreference(GpuPreference gpu_preference) {
  DCHECK_EQ(GpuPreference::kDefault, forced_gpu_preference_);
  forced_gpu_preference_ = gpu_preference;
}

// static
GpuPreference GLContext::AdjustGpuPreference(GpuPreference gpu_preference) {
  switch (forced_gpu_preference_) {
    case GpuPreference::kDefault:
      return gpu_preference;
    case GpuPreference::kLowPower:
    case GpuPreference::kHighPerformance:
      return forced_gpu_preference_;
    default:
      NOTREACHED();
      return GpuPreference::kDefault;
  }
}

GLApi* GLContext::CreateGLApi(DriverGL* driver) {
  real_gl_api_ = new RealGLApi;
  real_gl_api_->set_gl_workarounds(gl_workarounds_);
  real_gl_api_->SetDisabledExtensions(disabled_gl_extensions_);
  real_gl_api_->Initialize(driver);
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

std::string GLContext::GetGLVersion() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(gl_api_wrapper_);
  const char* version = reinterpret_cast<const char*>(
      gl_api_wrapper_->api()->glGetStringFn(GL_VERSION));
  return std::string(version ? version : "");
}

std::string GLContext::GetGLRenderer() {
  DCHECK(IsCurrent(nullptr));
  DCHECK(gl_api_wrapper_);
  const char* renderer = reinterpret_cast<const char*>(
      gl_api_wrapper_->api()->glGetStringFn(GL_RENDERER));
  return std::string(renderer ? renderer : "");
}

YUVToRGBConverter* GLContext::GetYUVToRGBConverter(
    const gfx::ColorSpace& color_space) {
  return nullptr;
}

CurrentGL* GLContext::GetCurrentGL() {
  if (!static_bindings_initialized_) {
    driver_gl_ = std::make_unique<DriverGL>();
    driver_gl_->InitializeStaticBindings();

    auto gl_api = base::WrapUnique<GLApi>(CreateGLApi(driver_gl_.get()));
    gl_api_wrapper_ =
        std::make_unique<GL_IMPL_WRAPPER_TYPE(GL)>(std::move(gl_api));

    current_gl_ = std::make_unique<CurrentGL>();
    current_gl_->Driver = driver_gl_.get();
    current_gl_->Api = gl_api_wrapper_->api();
    current_gl_->Version = version_info_.get();

    static_bindings_initialized_ = true;
  }

  return current_gl_.get();
}

void GLContext::ReinitializeDynamicBindings() {
  DCHECK(IsCurrent(nullptr));
  dynamic_bindings_initialized_ = false;
  ResetExtensions();
  InitializeDynamicBindings();
}

void GLContext::ForceReleaseVirtuallyCurrent() {
  NOTREACHED();
}

void GLContext::DirtyVirtualContextState() {
  current_virtual_context_ = nullptr;
}

#if defined(OS_MACOSX)
constexpr uint64_t kInvalidFenceId = 0;

uint64_t GLContext::BackpressureFenceCreate() {
  TRACE_EVENT0("gpu", "GLContextEGL::BackpressureFenceCreate");

  // This flush will trigger a crash if FlushForDriverCrashWorkaround is not
  // called sufficiently frequently.
  glFlush();

  if (gl::GLFence::IsSupported()) {
    next_backpressure_fence_ += 1;
    backpressure_fences_[next_backpressure_fence_] = GLFence::Create();
    return next_backpressure_fence_;
  }
  glFinish();
  return kInvalidFenceId;
}

void GLContext::BackpressureFenceWait(uint64_t fence_id) {
  TRACE_EVENT0("gpu", "GLContext::BackpressureFenceWait");
  if (fence_id == kInvalidFenceId) {
    return;
  }

  // If a fence is not found, then it has already been waited on.
  auto found = backpressure_fences_.find(fence_id);
  if (found == backpressure_fences_.end())
    return;
  std::unique_ptr<GLFence> fence = std::move(found->second);
  backpressure_fences_.erase(found);

  // While we could call GLFence::ClientWait, this performs a busy wait on
  // Mac, leading to high CPU usage. Instead we poll with a 1ms delay. This
  // should have minimal impact, as we will only hit this path when we are
  // more than one frame (16ms) behind.
  //
  // Note that on some platforms (10.9), fences appear to sometimes get
  // lost and will never pass. Add a 32ms timeout to prevent these
  // situations from causing a GPU process hang.
  // https://crbug.com/618075
  bool fence_completed = false;
  for (int poll_iter = 0; !fence_completed && poll_iter < 32; ++poll_iter) {
    if (poll_iter > 0) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
    {
      TRACE_EVENT0("gpu", "GLFence::HasCompleted");
      fence_completed = fence->HasCompleted();
    }
  }
  if (!fence_completed) {
    TRACE_EVENT0("gpu", "Finish");
    // We timed out waiting for the above fence, just issue a glFinish.
    glFinish();
  }
  fence.reset();

  // Waiting on |fence_id| has implicitly waited on all previous fences, so
  // remove them.
  while (backpressure_fences_.begin()->first < fence_id)
    backpressure_fences_.erase(backpressure_fences_.begin());
}

bool GLContext::HasBackpressureFences() const {
  return !backpressure_fences_.empty();
}

void GLContext::DestroyBackpressureFences() {
  backpressure_fences_.clear();
}

void GLContext::FlushForDriverCrashWorkaround() {
  if (!IsCurrent(nullptr))
    return;
  TRACE_EVENT0("gpu", "GLContext::FlushForDriverCrashWorkaround");
  glFlush();
}
#endif

bool GLContext::HasExtension(const char* name) {
  return gfx::HasExtension(GetExtensions(), name);
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
    case kGLImplementationEGLANGLE:
    case kGLImplementationSwiftShaderGL:
      return true;
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
  return std::make_unique<GLVersionInfo>(
      GetGLVersion().c_str(), GetGLRenderer().c_str(), GetExtensions());
}

void GLContext::SetCurrent(GLSurface* surface) {
  current_context_.Pointer()->Set(surface ? this : nullptr);
  if (surface) {
    surface->SetCurrent();
  } else {
    GLSurface::ClearCurrent();
  }

  // Leave the real GL api current so that unit tests work correctly.
  // TODO(sievers): Remove this, but needs all gpu_unittest classes
  // to create and make current a context.
  if (!surface && GetGLImplementation() != kGLImplementationMockGL &&
      GetGLImplementation() != kGLImplementationStubGL)
    SetCurrentGL(nullptr);
}

void GLContext::SetGLWorkarounds(const GLWorkarounds& workarounds) {
  DCHECK(!real_gl_api_);
  gl_workarounds_ = workarounds;
}

void GLContext::SetDisabledGLExtensions(
    const std::string& disabled_extensions) {
  DCHECK(!real_gl_api_);
  disabled_gl_extensions_ = disabled_extensions;
}

GLStateRestorer* GLContext::GetGLStateRestorer() {
  return state_restorer_.get();
}

void GLContext::SetGLStateRestorer(GLStateRestorer* state_restorer) {
  state_restorer_ = base::WrapUnique(state_restorer);
}

GLenum GLContext::CheckStickyGraphicsResetStatus() {
  DCHECK(IsCurrent(nullptr));
  return GL_NO_ERROR;
}

void GLContext::InitializeDynamicBindings() {
  DCHECK(IsCurrent(nullptr));
  BindGLApi();
  DCHECK(static_bindings_initialized_);
  if (!dynamic_bindings_initialized_) {
    if (real_gl_api_) {
      // This is called everytime DoRequestExtensionCHROMIUM() is called in
      // passthrough command buffer. So the underlying ANGLE driver will have
      // different GL extensions, therefore we need to clear the cache and
      // recompute on demand later.
      real_gl_api_->ClearCachedGLExtensions();
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
  if (switched_real_contexts || !surface->IsCurrent()) {
    GLSurface* current_surface = GLSurface::GetCurrent();
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

const gfx::ExtensionSet& GLContextReal::GetExtensions() {
  DCHECK(IsCurrent(nullptr));
  if (!extensions_initialized_) {
    SetExtensionsFromString(GetGLExtensionsFromCurrentContext(gl_api()));
  }
  return extensions_;
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

void GLContextReal::SetExtensionsFromString(std::string extensions) {
  extensions_string_ = std::move(extensions);
  extensions_ = gfx::MakeExtensionSet(extensions_string_);
  extensions_initialized_ = true;
}

void GLContextReal::ResetExtensions() {
  extensions_.clear();
  extensions_string_.clear();
  extensions_initialized_ = false;
}

}  // namespace gl
