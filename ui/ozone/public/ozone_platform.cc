// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_platform.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

namespace {

bool g_platform_initialized_ui = false;
bool g_platform_initialized_gpu = false;
base::LazyInstance<base::OnceCallback<void(OzonePlatform*)>>::Leaky
    instance_callback = LAZY_INSTANCE_INITIALIZER;

const OzonePlatform::PlatformProperties kDefaultPlatformProperties;

base::Lock& GetOzoneInstanceLock() {
  static base::Lock lock;
  return lock;
}

}  // namespace

OzonePlatform::PlatformProperties::PlatformProperties() = default;

OzonePlatform::PlatformProperties::PlatformProperties(
    bool needs_request,
    bool custom_frame_default,
    bool can_use_system_title_bar,
    std::vector<gfx::BufferFormat> buffer_formats)
    : needs_view_owner_request(needs_request),
      custom_frame_pref_default(custom_frame_default),
      use_system_title_bar(can_use_system_title_bar),
      supported_buffer_formats(buffer_formats) {}

OzonePlatform::PlatformProperties::~PlatformProperties() = default;

OzonePlatform::PlatformProperties::PlatformProperties(
    const PlatformProperties& other) = default;

OzonePlatform::OzonePlatform() {
  GetOzoneInstanceLock().AssertAcquired();
  DCHECK(!instance_) << "There should only be a single OzonePlatform.";
  instance_ = this;
  g_platform_initialized_ui = false;
  g_platform_initialized_gpu = false;
}

OzonePlatform::~OzonePlatform() = default;

// static
void OzonePlatform::InitializeForUI(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_ui)
    return;
  g_platform_initialized_ui = true;
  instance_->InitializeUI(args);
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
  if (!instance_callback.Get().is_null())
    std::move(instance_callback.Get()).Run(instance_);
}

// static
void OzonePlatform::InitializeForGPU(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_gpu)
    return;
  g_platform_initialized_gpu = true;
  instance_->InitializeGPU(args);
  if (!args.single_process && !instance_callback.Get().is_null())
    std::move(instance_callback.Get()).Run(instance_);
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  base::AutoLock lock(GetOzoneInstanceLock());
  DCHECK(instance_) << "OzonePlatform is not initialized";
  return instance_;
}

// static
OzonePlatform* OzonePlatform::EnsureInstance() {
  base::AutoLock lock(GetOzoneInstanceLock());
  if (!instance_) {
    TRACE_EVENT1("ozone",
                 "OzonePlatform::Initialize",
                 "platform",
                 GetOzonePlatformName());
    std::unique_ptr<OzonePlatform> platform =
        PlatformObject<OzonePlatform>::Create();

    // TODO(spang): Currently need to leak this object.
    OzonePlatform* pl = platform.release();
    DCHECK_EQ(instance_, pl);
  }
  return instance_;
}

// static
void OzonePlatform::RegisterStartupCallback(
    base::OnceCallback<void(OzonePlatform*)> callback) {
  OzonePlatform* inst = nullptr;
  {
    base::AutoLock lock(GetOzoneInstanceLock());
    if (!instance_ || !g_platform_initialized_ui) {
      instance_callback.Get() = std::move(callback);
      return;
    }
    inst = instance_;
  }
  std::move(callback).Run(inst);
}

// static
OzonePlatform* OzonePlatform::instance_ = nullptr;

IPC::MessageFilter* OzonePlatform::GetGpuMessageFilter() {
  return nullptr;
}

std::unique_ptr<PlatformScreen> OzonePlatform::CreateScreen() {
  return nullptr;
}

const OzonePlatform::PlatformProperties&
OzonePlatform::GetPlatformProperties() {
  return kDefaultPlatformProperties;
}

base::MessageLoop::Type OzonePlatform::GetMessageLoopTypeForGpu() {
  return base::MessageLoop::TYPE_DEFAULT;
}

void OzonePlatform::AddInterfaces(service_manager::BinderRegistry* registry) {}

void OzonePlatform::AfterSandboxEntry() {}

}  // namespace ui
