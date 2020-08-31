// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_
#define ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

class AmbientBackendModel;

class ASH_EXPORT AmbientViewDelegate {
 public:
  virtual ~AmbientViewDelegate() = default;

  // Returns the model store stores all the information we get from the backdrop
  // server to render the photo frame and the glanceable weather information on
  // Ambient Mode.
  virtual AmbientBackendModel* GetAmbientBackendModel() = 0;

  // Invoked when user interacting with the background photo using mouse,
  // touchpad, or touchscreen.
  virtual void OnBackgroundPhotoEvents() = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_
