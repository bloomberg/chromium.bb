// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include <array>
#include <memory>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientBackgroundImageView;
class AmbientViewDelegate;
struct PhotoWithDetails;

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View,
                             public AmbientBackendModelObserver,
                             public ui::ImplicitAnimationObserver {
 public:
  METADATA_HEADER(PhotoView);

  explicit PhotoView(AmbientViewDelegate* delegate);
  PhotoView(const PhotoView&) = delete;
  PhotoView& operator=(PhotoView&) = delete;
  ~PhotoView() override;

  // AmbientBackendModelObserver:
  void OnImageAdded() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  friend class AmbientAshTestBase;

  void Init();

  void UpdateImage(const PhotoWithDetails& image);

  void StartTransitionAnimation();

  // Return if can start transition animation.
  bool NeedToAnimateTransition() const;

  gfx::ImageSkia GetVisibleImageForTesting();

  // Note that we should be careful when using |delegate_|, as there is no
  // strong guarantee on the life cycle.
  AmbientViewDelegate* const delegate_ = nullptr;

  // Image containers used for animation. Owned by view hierarchy.
  std::array<AmbientBackgroundImageView*, 2> image_views_{nullptr, nullptr};

  // The index of |image_views_| to update the next image.
  int image_index_ = 0;

  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_backend_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
