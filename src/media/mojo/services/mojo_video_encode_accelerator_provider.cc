// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"

#include <memory>
#include <utility>

#include "base/task/thread_pool.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {
void BindVEAProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        receiver,
    media::MojoVideoEncodeAcceleratorProvider::
        CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    gpu::GpuPreferences gpu_preferences,
    gpu::GpuDriverBugWorkarounds gpu_workarounds) {
  auto vea_provider =
      std::make_unique<media::MojoVideoEncodeAcceleratorProvider>(
          std::move(create_vea_callback), gpu_preferences, gpu_workarounds);
  mojo::MakeSelfOwnedReceiver(std::move(vea_provider), std::move(receiver));
}
}  // namespace

namespace media {

// static
void MojoVideoEncodeAcceleratorProvider::Create(
    mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver,
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
  // Offload VEA providers to a dedicated runner. Things like loading profiles
  // and creating encoder might take quite some time, and they might block
  // processing of other mojo calls if executed on the current runner.
  //
  // MayBlock() because MF VEA can take long time running GetSupportedProfiles()
  auto runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
  runner->PostTask(
      FROM_HERE, base::BindOnce(BindVEAProvider, std::move(receiver),
                                std::move(create_vea_callback), gpu_preferences,
                                gpu_workarounds));
}

MojoVideoEncodeAcceleratorProvider::MojoVideoEncodeAcceleratorProvider(
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
    : create_vea_callback_(std::move(create_vea_callback)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds) {}

MojoVideoEncodeAcceleratorProvider::~MojoVideoEncodeAcceleratorProvider() =
    default;

void MojoVideoEncodeAcceleratorProvider::CreateVideoEncodeAccelerator(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) {
  MojoVideoEncodeAcceleratorService::Create(std::move(receiver),
                                            create_vea_callback_,
                                            gpu_preferences_, gpu_workarounds_);
}

void MojoVideoEncodeAcceleratorProvider::
    GetVideoEncodeAcceleratorSupportedProfiles(
        GetVideoEncodeAcceleratorSupportedProfilesCallback callback) {
  std::move(callback).Run(
      GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(gpu_preferences_,
                                                             gpu_workarounds_));
}

}  // namespace media
