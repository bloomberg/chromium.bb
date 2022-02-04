// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AmbientAnimationStaticResources;
class AmbientViewDelegate;

// Container view to display all Ambient Mode related views, i.e. photo frame,
// weather info.
class ASH_EXPORT AmbientContainerView : public views::View {
 public:
  METADATA_HEADER(AmbientContainerView);

  // |animation_static_resources| contains the Lottie animation file to render
  // along with its accompanying static image assets. If null, that means the
  // slideshow UI should be rendered instead.
  AmbientContainerView(AmbientViewDelegate* delegate,
                       std::unique_ptr<AmbientAnimationStaticResources>
                           animation_static_resources);
  ~AmbientContainerView() override;

 private:
  friend class AmbientAshTestBase;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
