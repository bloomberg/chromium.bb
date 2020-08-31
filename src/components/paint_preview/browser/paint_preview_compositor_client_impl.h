// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

using CompositorPtr =
    std::unique_ptr<mojo::Remote<mojom::PaintPreviewCompositor>,
                    base::OnTaskRunnerDeleter>;

// The implementation of the PaintPreviewCompositorClient class.
// The public interface should be invoked only on the |default_task_runner_|
// which is the the runner returned by base::SequencedTaskRunnerHandle::Get()
// when this is constructed.
class PaintPreviewCompositorClientImpl : public PaintPreviewCompositorClient {
 public:
  using OnCompositorCreatedCallback =
      base::OnceCallback<void(const base::UnguessableToken&)>;

  explicit PaintPreviewCompositorClientImpl(
      scoped_refptr<base::SequencedTaskRunner> compositor_task_runner,
      base::WeakPtr<PaintPreviewCompositorServiceImpl> service);
  ~PaintPreviewCompositorClientImpl() override;

  // PaintPreviewCompositorClient implementation.
  const base::Optional<base::UnguessableToken>& Token() const override;
  void SetDisconnectHandler(base::OnceClosure closure) override;
  void BeginComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginCompositeCallback callback) override;
  void BitmapForFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForFrameCallback callback) override;
  void SetRootFrameUrl(const GURL& url) override;

  // Exposes underlying BindNewPipeAndPassReceiver method of |compositor_|.
  mojo::PendingReceiver<mojom::PaintPreviewCompositor>
  BindNewPipeAndPassReceiver();

  void IsBoundAndConnected(base::OnceCallback<void(bool)> callback);

  OnCompositorCreatedCallback BuildCompositorCreatedCallback(
      base::OnceClosure user_closure,
      OnCompositorCreatedCallback service_callback);

  PaintPreviewCompositorClientImpl(const PaintPreviewCompositorClientImpl&) =
      delete;
  PaintPreviewCompositorClientImpl& operator=(
      const PaintPreviewCompositorClientImpl&) = delete;

 private:
  void OnCompositorCreated(base::OnceClosure user_closure,
                           OnCompositorCreatedCallback service_callback,
                           const base::UnguessableToken& token);

  void NotifyServiceOfInvalidation();

  void DisconnectHandler();

  scoped_refptr<base::SequencedTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> default_task_runner_;
  base::WeakPtr<PaintPreviewCompositorServiceImpl> service_;
  CompositorPtr compositor_;

  base::Optional<base::UnguessableToken> token_;
  base::OnceClosure user_disconnect_closure_;

  base::WeakPtrFactory<PaintPreviewCompositorClientImpl> weak_ptr_factory_{
      this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_CLIENT_IMPL_H_
