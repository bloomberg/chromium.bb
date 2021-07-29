// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

// MediaFoundation-specific MojoMediaClient implementation for
// MediaFoundationService running in the "Media Foundation Service" utility
// process hosting MediaFoundationRenderer and MediaFoundationCdm.
class MediaFoundationMojoMediaClient : public MojoMediaClient {
 public:
  explicit MediaFoundationMojoMediaClient(const base::FilePath& user_data_dir);
  ~MediaFoundationMojoMediaClient() final;

  // MojoMediaClient implementation.
  std::unique_ptr<Renderer> CreateMediaFoundationRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) final;

 private:
  base::FilePath user_data_dir_;
  DISALLOW_COPY_AND_ASSIGN(MediaFoundationMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
