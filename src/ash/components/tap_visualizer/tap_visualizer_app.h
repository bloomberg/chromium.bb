// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_
#define ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "ash/components/tap_visualizer/public/mojom/tap_visualizer.mojom.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_observer.h"

namespace views {
class AuraInit;
}  // namespace views

namespace tap_visualizer {

class TapRenderer;

// Application that paints touch tap points as circles. Creates a fullscreen
// transparent widget on each display to draw the taps.
class TapVisualizerApp : public service_manager::Service,
                         public tap_visualizer::mojom::TapVisualizer,
                         public ui::EventObserver,
                         public display::DisplayObserver {
 public:
  explicit TapVisualizerApp(service_manager::mojom::ServiceRequest request);

  ~TapVisualizerApp() override;

 private:
  friend class TapVisualizerAppTestApi;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::TapVisualizer:
  void Show() override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // Creates the touch HUD widget for a display.
  void CreateWidgetForDisplay(int64_t display_id);

  void AddBinding(mojom::TapVisualizerRequest request);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::Binding<mojom::TapVisualizer> tap_visualizer_binding_{this};

  // Must be released after |display_id_to_renderer_| which indirectly depends
  // on aura.
  std::unique_ptr<views::AuraInit> aura_init_;

  // True after the first Show().
  bool is_showing_ = false;

  // Maps display::Display::id() to the renderer for that display.
  std::map<int64_t, std::unique_ptr<TapRenderer>> display_id_to_renderer_;

  DISALLOW_COPY_AND_ASSIGN(TapVisualizerApp);
};

}  // namespace tap_visualizer

#endif  // ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_
