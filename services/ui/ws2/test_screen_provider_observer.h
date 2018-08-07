// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_TEST_SCREEN_PROVIDER_OBSERVER_H_
#define SERVICES_UI_WS2_TEST_SCREEN_PROVIDER_OBSERVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "services/ui/public/interfaces/screen_provider_observer.mojom.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event.h"

namespace ui {
namespace ws2 {

// Test implementation of ScreenProviderObserver. Tracks most recent call to
// OnDisplaysChanged().
class TestScreenProviderObserver : public mojom::ScreenProviderObserver {
 public:
  TestScreenProviderObserver();
  ~TestScreenProviderObserver() override;

  std::vector<mojom::WsDisplayPtr>& displays() { return displays_; }
  std::string& display_ids() { return display_ids_; }
  int64_t primary_display_id() const { return primary_display_id_; }
  int64_t internal_display_id() const { return internal_display_id_; }
  int64_t display_id_for_new_windows() const {
    return display_id_for_new_windows_;
  }

  // mojom::ScreenProviderObserver:
  void OnDisplaysChanged(std::vector<mojom::WsDisplayPtr> displays,
                         int64_t primary_display_id,
                         int64_t internal_display_id,
                         int64_t display_id_for_new_windows) override;

 private:
  std::vector<mojom::WsDisplayPtr> displays_;
  std::string display_ids_;
  int64_t primary_display_id_ = display::kInvalidDisplayId;
  int64_t internal_display_id_ = display::kInvalidDisplayId;
  int64_t display_id_for_new_windows_ = display::kInvalidDisplayId;

  DISALLOW_COPY_AND_ASSIGN(TestScreenProviderObserver);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_TEST_SCREEN_PROVIDER_OBSERVER_H_
