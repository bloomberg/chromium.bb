// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureWindowControllerImpl;

// Receives Picture-in-Picture messages from a given RenderFrame. There is one
// PictureInPictureServiceImpl per RenderFrameHost. The service pipes the
// `StartSession()` call to the PictureInPictureWindowControllerImpl which owns
// the created session. The same object will get notified when the service is
// killed given that the PictureInPictureWindowControllerImpl is
// WebContents-bound instead of RenderFrameHost.
// PictureInPictureServiceImpl owns itself. It self-destruct as needed, see the
// DocumentService's documentation for more information.
class CONTENT_EXPORT PictureInPictureServiceImpl final
    : public content::DocumentService<blink::mojom::PictureInPictureService> {
 public:
  static void Create(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::PictureInPictureService>);

  static PictureInPictureServiceImpl* CreateForTesting(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::PictureInPictureService>);

  PictureInPictureServiceImpl(const PictureInPictureServiceImpl&) = delete;
  PictureInPictureServiceImpl& operator=(const PictureInPictureServiceImpl&) =
      delete;

  // PictureInPictureService implementation.
  void StartSession(
      uint32_t player_id,
      mojo::PendingAssociatedRemote<media::mojom::MediaPlayer> player_remote,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size,
      bool show_play_pause_button,
      mojo::PendingRemote<blink::mojom::PictureInPictureSessionObserver>,
      StartSessionCallback) final;

 private:
  friend class PictureInPictureSession;

  PictureInPictureServiceImpl(
      RenderFrameHost*,
      mojo::PendingReceiver<blink::mojom::PictureInPictureService>);
  ~PictureInPictureServiceImpl() override;

  PictureInPictureWindowControllerImpl& GetController();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_
