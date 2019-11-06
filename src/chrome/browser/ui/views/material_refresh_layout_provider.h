// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MATERIAL_REFRESH_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_MATERIAL_REFRESH_LAYOUT_PROVIDER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"

class MaterialRefreshLayoutProvider : public ChromeLayoutProvider {
 public:
  MaterialRefreshLayoutProvider() = default;
  ~MaterialRefreshLayoutProvider() override = default;

  // ChromeLayoutProvider:
  int GetDistanceMetric(int metric) const override;
  gfx::Insets GetInsetsMetric(int metric) const override;
  int GetCornerRadiusMetric(views::EmphasisMetric emphasis_metric,
                            const gfx::Size& size = gfx::Size()) const override;
  int GetShadowElevationMetric(
      views::EmphasisMetric emphasis_metric) const override;
  gfx::ShadowValues MakeShadowValues(int elevation,
                                     SkColor color) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MATERIAL_REFRESH_LAYOUT_PROVIDER_H_
