// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
#define ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_

#include <memory>

#include "ash/public/interfaces/arc_custom_tab.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ws/remote_view_host/server_remote_view_host.h"
#include "ui/views/view.h"

namespace ash {

// Implementation of ArcCustomTabView interface.
class ArcCustomTabView : public views::View, public mojom::ArcCustomTabView {
 public:
  // Creates a new ArcCustomTabView instance. The instance will be deleted when
  // the pointer is closed. Returns null when the arguments are invalid.
  static mojom::ArcCustomTabViewPtr Create(int32_t task_id,
                                           int32_t surface_id,
                                           int32_t top_margin);

  // mojom::ArcCustomTabView:
  void EmbedUsingToken(const base::UnguessableToken& token) override;

  // views::View:
  void Layout() override;

 private:
  ArcCustomTabView(int32_t surface_id, int32_t top_margin);
  ~ArcCustomTabView() override;

  // Binds this instance to the pointer.
  void Bind(mojom::ArcCustomTabViewPtr* ptr);

  // Deletes this object when the mojo connection is closed.
  void Close();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  mojo::Binding<mojom::ArcCustomTabView> binding_;
  ws::ServerRemoteViewHost* remote_view_host_;
  int32_t surface_id_, top_margin_;
  base::WeakPtrFactory<ArcCustomTabView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomTabView);
};

}  // namespace ash

#endif  // ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
