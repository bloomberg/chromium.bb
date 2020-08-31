// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_
#define ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"

#include "base/memory/weak_ptr.h"

namespace ash {

class AmbientController;

class AmbientViewDelegateImpl : public AmbientViewDelegate {
 public:
  explicit AmbientViewDelegateImpl(AmbientController* ambient_controller);
  AmbientViewDelegateImpl(const AmbientViewDelegateImpl&) = delete;
  AmbientViewDelegateImpl& operator=(AmbientViewDelegateImpl&) = delete;
  ~AmbientViewDelegateImpl() override;

  // AmbientViewDelegate:
  AmbientBackendModel* GetAmbientBackendModel() override;
  void OnBackgroundPhotoEvents() override;

 private:
  AmbientController* const ambient_controller_;  // Owned by Shell.

  base::WeakPtrFactory<AmbientViewDelegateImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_VIEW_DELEGATE_IMPL_H_
