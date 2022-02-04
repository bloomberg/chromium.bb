// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/oop_arc_video_accelerator_factory.h"

#include "ash/components/arc/mojom/protected_buffer_manager.mojom.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_decoder.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace arc {

namespace {

class MojoProtectedBufferManager : public DecoderProtectedBufferManager {
 public:
  MojoProtectedBufferManager(
      mojo::PendingRemote<mojom::ProtectedBufferManager> pending_remote)
      : remote_(std::move(pending_remote)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Version 1 introduced GetProtectedNativePixmapHandleFromHandle() and a
    // GetProtectedSharedMemoryFromHandle() variant that returns a nullable
    // value.
    remote_.RequireVersion(1);
  }
  MojoProtectedBufferManager(const MojoProtectedBufferManager&) = delete;
  MojoProtectedBufferManager& operator=(const MojoProtectedBufferManager&) =
      delete;

  // DecoderProtectedBufferManager implementation.
  void GetProtectedSharedMemoryRegionFor(
      base::ScopedFD dummy_fd,
      GetProtectedSharedMemoryRegionForResponseCB response_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    remote_->GetProtectedSharedMemoryFromHandle(
        mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(dummy_fd))),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            media::BindToCurrentLoop(base::BindOnce(
                &OnGetProtectedSharedMemoryRegionFor, std::move(response_cb))),
            mojo::ScopedSharedBufferHandle()));
  }
  void GetProtectedNativePixmapHandleFor(
      base::ScopedFD dummy_fd,
      GetProtectedNativePixmapHandleForResponseCB response_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(b/195769334): do we need to validate anything about the response
    // gfx::NativePixmapHandle before calling |response_cb|.
    remote_->GetProtectedNativePixmapHandleFromHandle(
        mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(dummy_fd))),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            media::BindToCurrentLoop(base::BindOnce(
                &OnGetProtectedNativePixmapHandleFor, std::move(response_cb))),
            absl::nullopt));
  }

 private:
  ~MojoProtectedBufferManager() override = default;

  static void OnGetProtectedSharedMemoryRegionFor(
      GetProtectedSharedMemoryRegionForResponseCB response_cb,
      mojo::ScopedSharedBufferHandle shared_memory_mojo_handle) {
    if (!shared_memory_mojo_handle.is_valid()) {
      return std::move(response_cb)
          .Run(base::subtle::PlatformSharedMemoryRegion());
    }

    // TODO(b/195769334): does anything need to be validated here?
    std::move(response_cb)
        .Run(mojo::UnwrapPlatformSharedMemoryRegion(
            std::move(shared_memory_mojo_handle)));
  }

  static void OnGetProtectedNativePixmapHandleFor(
      GetProtectedNativePixmapHandleForResponseCB response_cb,
      absl::optional<gfx::NativePixmapHandle> native_pixmap_handle) {
    if (!native_pixmap_handle)
      return std::move(response_cb).Run(gfx::NativePixmapHandle());
    // TODO(b/195769334): does anything need to be validated here?
    std::move(response_cb).Run(std::move(*native_pixmap_handle));
  }

  mojo::Remote<mojom::ProtectedBufferManager> remote_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

// TODO(b/195769334): we should plumb meaningful gpu::GpuPreferences and
// gpu::GpuDriverBugWorkarounds so that we can use them to control behaviors of
// the hardware decoder.
OOPArcVideoAcceleratorFactory::OOPArcVideoAcceleratorFactory(
    mojo::PendingReceiver<mojom::VideoAcceleratorFactory> receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OOPArcVideoAcceleratorFactory::~OOPArcVideoAcceleratorFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Reset |receiver_| now to ensure it doesn't use a partially destroyed
  // *|this|.
  receiver_.reset();
}

void OOPArcVideoAcceleratorFactory::CreateDecodeAccelerator(
    mojo::PendingReceiver<mojom::VideoDecodeAccelerator> receiver,
    mojo::PendingRemote<mojom::ProtectedBufferManager>
        protected_buffer_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(2);
  // Note that a well-behaved client should not reach this point twice because
  // there should be one process for each GpuArcVideoDecodeAccelerator. This is
  // guaranteed by arc::GpuArcVideoServiceHost in the browser process. Thus, we
  // don't bother validating that here because if the browser process is
  // compromised, we have bigger problems.
  protected_buffer_manager_ = base::MakeRefCounted<MojoProtectedBufferManager>(
      std::move(protected_buffer_manager));
  auto decoder = std::make_unique<GpuArcVideoDecodeAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
      protected_buffer_manager_);
  auto decoder_receiver =
      mojo::MakeSelfOwnedReceiver(std::move(decoder), std::move(receiver));
  CHECK(decoder_receiver);
  decoder_receiver->set_connection_error_handler(
      base::BindOnce(&OOPArcVideoAcceleratorFactory::OnDecoderDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OOPArcVideoAcceleratorFactory::CreateVideoDecoder(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver) {
  // TODO(b/195769334): plumb a ProtectedBufferManager.
  auto decoder = std::make_unique<GpuArcVideoDecoder>(
      /*protected_buffer_manager=*/nullptr);
  auto decoder_receiver =
      mojo::MakeSelfOwnedReceiver(std::move(decoder), std::move(receiver));
  CHECK(decoder_receiver);
  decoder_receiver->set_connection_error_handler(
      base::BindOnce(&OOPArcVideoAcceleratorFactory::OnDecoderDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OOPArcVideoAcceleratorFactory::CreateEncodeAccelerator(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) {
  NOTIMPLEMENTED();
}

void OOPArcVideoAcceleratorFactory::CreateProtectedBufferAllocator(
    mojo::PendingReceiver<mojom::VideoProtectedBufferAllocator> receiver) {
  NOTREACHED();
}

void OOPArcVideoAcceleratorFactory::OnDecoderDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(2);
  receiver_.reset();
}

}  // namespace arc
