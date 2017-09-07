// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_result_struct_traits.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// This class retains the release_callback_ of the CopyOutputResult that is
// being sent over mojo. A TextureMailboxReleaserPtr that talks to this impl
// object will be sent over mojo instead of the release_callback_ (which is not
// serializable). Once the client calls Release, the release_callback_ will be
// called. An object of this class will remain alive until the MessagePipe
// attached to it goes away (i.e. StrongBinding is used).
class TextureMailboxReleaserImpl : public viz::mojom::TextureMailboxReleaser {
 public:
  explicit TextureMailboxReleaserImpl(
      std::unique_ptr<viz::SingleReleaseCallback> release_callback)
      : release_callback_(std::move(release_callback)) {
    DCHECK(release_callback_);
  }

  ~TextureMailboxReleaserImpl() override {
    // If the client fails to call Release, we should do it ourselves because
    // release_callback_ will fail if it's never called.
    if (release_callback_)
      release_callback_->Run(gpu::SyncToken(), true);
  }

  // mojom::TextureMailboxReleaser implementation:
  void Release(const gpu::SyncToken& sync_token, bool is_lost) override {
    if (!release_callback_)
      return;
    release_callback_->Run(sync_token, is_lost);
    release_callback_.reset();
  }

 private:
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
};

void Release(viz::mojom::TextureMailboxReleaserPtr ptr,
             const gpu::SyncToken& sync_token,
             bool is_lost) {
  ptr->Release(sync_token, is_lost);
}

}  // namespace

namespace mojo {

// static
viz::mojom::TextureMailboxReleaserPtr
StructTraits<viz::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    releaser(const std::unique_ptr<viz::CopyOutputResult>& result) {
  viz::mojom::TextureMailboxReleaserPtr releaser;
  if (HasTextureResult(*result)) {
    MakeStrongBinding(std::make_unique<TextureMailboxReleaserImpl>(
                          result->TakeTextureOwnership()),
                      MakeRequest(&releaser));
  }
  return releaser;
}

// static
bool StructTraits<viz::mojom::CopyOutputResultDataView,
                  std::unique_ptr<viz::CopyOutputResult>>::
    HasTextureResult(const viz::CopyOutputResult& result) {
  return result.GetTextureMailbox() && result.GetTextureMailbox()->IsTexture();
}

// static
bool StructTraits<viz::mojom::CopyOutputResultDataView,
                  std::unique_ptr<viz::CopyOutputResult>>::
    Read(viz::mojom::CopyOutputResultDataView data,
         std::unique_ptr<viz::CopyOutputResult>* out_p) {
  // First read into local variables, and then instantiate an appropriate
  // implementation of viz::CopyOutputResult.
  viz::CopyOutputResult::Format format;
  gfx::Rect rect;

  if (!data.ReadFormat(&format) || !data.ReadRect(&rect))
    return false;

  switch (format) {
    case viz::CopyOutputResult::Format::RGBA_BITMAP: {
      SkBitmap bitmap;
      if (!rect.IsEmpty() &&
          (!data.ReadBitmap(&bitmap) || !bitmap.readyToDraw())) {
        return false;  // Missing image data!
      }
      out_p->reset(new viz::CopyOutputSkBitmapResult(rect, bitmap));
      return true;
    }

    case viz::CopyOutputResult::Format::RGBA_TEXTURE: {
      base::Optional<viz::TextureMailbox> texture_mailbox;
      if (!data.ReadTextureMailbox(&texture_mailbox))
        return false;
      if (texture_mailbox && texture_mailbox->IsTexture()) {
        auto releaser =
            data.TakeReleaser<viz::mojom::TextureMailboxReleaserPtr>();
        if (!releaser)
          return false;  // Illegal to provide texture without Releaser.
        out_p->reset(new viz::CopyOutputTextureResult(
            rect, *texture_mailbox,
            viz::SingleReleaseCallback::Create(
                base::Bind(Release, base::Passed(&releaser)))));
      } else {
        if (!rect.IsEmpty())
          return false;  // Missing image data!
        out_p->reset(new viz::CopyOutputTextureResult(
            rect, viz::TextureMailbox(), nullptr));
      }
      return true;
    }
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
