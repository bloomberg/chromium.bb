// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_snapshot.h"

#include <inttypes.h>

#include <sstream>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace display {

namespace {

static const float kInchInMm = 25.4f;

// Get pixel pitch in millimeters from DPI.
float PixelPitchMmFromDPI(float dpi) {
  return (1.0f / dpi) * kInchInMm;
}

std::string ModeListString(
    const std::vector<std::unique_ptr<const ui::DisplayMode>>& modes) {
  std::stringstream stream;
  bool first = true;
  for (auto& mode : modes) {
    if (!first)
      stream << ", ";
    stream << mode->ToString();
    first = false;
  }
  return stream.str();
}

std::string DisplayConnectionTypeString(ui::DisplayConnectionType type) {
  switch (type) {
    case ui::DISPLAY_CONNECTION_TYPE_NONE:
      return "none";
    case ui::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return "unknown";
    case ui::DISPLAY_CONNECTION_TYPE_INTERNAL:
      return "internal";
    case ui::DISPLAY_CONNECTION_TYPE_VGA:
      return "vga";
    case ui::DISPLAY_CONNECTION_TYPE_HDMI:
      return "hdmi";
    case ui::DISPLAY_CONNECTION_TYPE_DVI:
      return "dvi";
    case ui::DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return "dp";
    case ui::DISPLAY_CONNECTION_TYPE_NETWORK:
      return "network";
    case ui::DISPLAY_CONNECTION_TYPE_VIRTUAL:
      return "virtual";
  }
  NOTREACHED();
  return "";
}

}  // namespace

using Builder = FakeDisplaySnapshot::Builder;

Builder::Builder() {}

Builder::~Builder() {}

std::unique_ptr<FakeDisplaySnapshot> Builder::Build() {
  if (modes_.empty() || id_ == Display::kInvalidDisplayID) {
    NOTREACHED() << "Display modes or display ID missing";
    return nullptr;
  }

  // If there is no native mode set, use the first display mode.
  if (!native_mode_)
    native_mode_ = modes_.back().get();

  // Calculate physical size to match set DPI.
  gfx::Size physical_size =
      gfx::ScaleToRoundedSize(native_mode_->size(), PixelPitchMmFromDPI(dpi_));

  return base::WrapUnique(new FakeDisplaySnapshot(
      id_, origin_, physical_size, type_, is_aspect_preserving_scaling_,
      has_overscan_, has_color_correction_matrix_, name_, product_id_,
      std::move(modes_), current_mode_, native_mode_));
}

Builder& Builder::SetId(int64_t id) {
  id_ = id;
  return *this;
}

Builder& Builder::SetNativeMode(const gfx::Size& size) {
  native_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetCurrentMode(const gfx::Size& size) {
  current_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::AddMode(const gfx::Size& size) {
  AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetOrigin(const gfx::Point& origin) {
  origin_ = origin;
  return *this;
}

Builder& Builder::SetType(ui::DisplayConnectionType type) {
  type_ = type;
  return *this;
}

Builder& Builder::SetIsAspectPerservingScaling(bool val) {
  is_aspect_preserving_scaling_ = val;
  return *this;
}

Builder& Builder::SetHasOverscan(bool has_overscan) {
  has_overscan_ = has_overscan;
  return *this;
}

Builder& Builder::SetHasColorCorrectionMatrix(bool val) {
  has_color_correction_matrix_ = val;
  return *this;
}

Builder& Builder::SetName(const std::string& name) {
  name_ = name;
  return *this;
}

Builder& Builder::SetProductId(int64_t product_id) {
  product_id_ = product_id;
  return *this;
}

Builder& Builder::SetDPI(int dpi) {
  dpi_ = static_cast<float>(dpi);
  return *this;
}

Builder& Builder::SetLowDPI() {
  return SetDPI(96);
}

Builder& Builder::SetHighDPI() {
  return SetDPI(326);  // Retina-ish.
}

const ui::DisplayMode* Builder::AddOrFindDisplayMode(const gfx::Size& size) {
  for (auto& mode : modes_) {
    if (mode->size() == size)
      return mode.get();
  }

  // Not found, insert a mode with the size and return.
  modes_.push_back(base::MakeUnique<ui::DisplayMode>(size, false, 60.0f));
  return modes_.back().get();
}

FakeDisplaySnapshot::FakeDisplaySnapshot(
    int64_t display_id,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    ui::DisplayConnectionType type,
    bool is_aspect_preserving_scaling,
    bool has_overscan,
    bool has_color_correction_matrix,
    std::string display_name,
    int64_t product_id,
    std::vector<std::unique_ptr<const ui::DisplayMode>> modes,
    const ui::DisplayMode* current_mode,
    const ui::DisplayMode* native_mode)
    : DisplaySnapshot(display_id,
                      origin,
                      physical_size,
                      type,
                      is_aspect_preserving_scaling,
                      has_overscan,
                      has_color_correction_matrix,
                      display_name,
                      base::FilePath(),
                      std::move(modes),
                      std::vector<uint8_t>(),
                      current_mode,
                      native_mode) {
  product_id_ = product_id;
}

FakeDisplaySnapshot::~FakeDisplaySnapshot() {}

std::string FakeDisplaySnapshot::ToString() const {
  return base::StringPrintf(
      "id=%" PRId64
      " current_mode=%s native_mode=%s origin=%s"
      " physical_size=%s, type=%s name=\"%s\" modes=(%s)",
      display_id_,
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      native_mode_ ? native_mode_->ToString().c_str() : "nullptr",
      origin_.ToString().c_str(), physical_size_.ToString().c_str(),
      DisplayConnectionTypeString(type_).c_str(), display_name_.c_str(),
      ModeListString(modes_).c_str());
}

}  // namespace display
