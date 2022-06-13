// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_H_
#define UI_COLOR_COLOR_PROVIDER_H_

#include <forward_list>
#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"

namespace ui {

// A ColorProvider holds the complete pipeline of ColorMixers that compute
// result colors for UI elements.  ColorProvider is meant to be a long-lived
// object whose internal list of mixers does not change after initial
// construction.  Separate ColorProviders should be instantiated for e.g.
// windows with different themes.
// TODO(pkasting): Figure out ownership model and lifetime.
class COMPONENT_EXPORT(COLOR) ColorProvider {
 public:
  using ColorMap = base::flat_map<ColorId, SkColor>;

  ColorProvider();
  // There should be no reason to copy or move a ColorProvider.
  ColorProvider(const ColorProvider&) = delete;
  ColorProvider& operator=(const ColorProvider&) = delete;
  ~ColorProvider();

  // Adds a mixer to the current color pipeline after all other
  // non-"postprocessing" mixers.  Returns a reference to the added mixer so
  // callers can subsequently add sets and/or recipes.
  ColorMixer& AddMixer();

  // Like AddMixer(), but adds at the very end of the color pipeline.
  // "Postprocessing" mixers are meant to run after all other mixers and are
  // skipped when calling GetUnprocessedColor().
  ColorMixer& AddPostprocessingMixer();

  // Returns the result color for |id| by applying the effects of each mixer in
  // order.  Returns gfx::kPlaceholderColor if no mixer knows how to construct
  // |id|.
  SkColor GetColor(ColorId id) const;

  // Generates the `color_map_` used by this provider for all ColorIds defined
  // by attached mixers. After the map is generated attached mixers and their
  // associated objects are discarded. Mixers should not be added to the
  // provider after this has been called.
  void GenerateColorMap();

 private:
  using Mixers = std::forward_list<ColorMixer>;

  // Returns the last mixer in the chain that is not a "postprocessing" mixer,
  // or nullptr.
  const ColorMixer* GetLastNonPostprocessingMixer() const;

  // The entire color pipeline, in reverse order (that is, the "last" mixer is
  // at the front).
  Mixers mixers_;

  // The first mixer in the chain that is a "postprocessing" mixer.
  Mixers::iterator first_postprocessing_mixer_ = mixers_.before_begin();

  // A cached map of ColorId => SkColor mappings for this provider. This will be
  // generated in the call to `GenerateColorMap()`.
  std::unique_ptr<ColorMap> color_map_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_H_
