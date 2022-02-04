// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_promo_specification.h"

#include <string>

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo() = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(
    const AcceleratorInfo& other) = default;
FeaturePromoSpecification::AcceleratorInfo::~AcceleratorInfo() = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(ValueType value)
    : value_(value) {}
FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(
    const AcceleratorInfo& other) = default;

FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(ValueType value) {
  value_ = value;
  return *this;
}

FeaturePromoSpecification::AcceleratorInfo::operator bool() const {
  return absl::holds_alternative<ui::Accelerator>(value_) ||
         absl::get<int>(value_);
}

ui::Accelerator FeaturePromoSpecification::AcceleratorInfo::GetAccelerator(
    const ui::AcceleratorProvider* provider) const {
  if (absl::holds_alternative<ui::Accelerator>(value_))
    return absl::get<ui::Accelerator>(value_);

  const int command_id = absl::get<int>(value_);
  DCHECK_GT(command_id, 0);

  ui::Accelerator result;
  DCHECK(provider->GetAcceleratorForCommandId(command_id, &result));
  return result;
}

// static
constexpr FeaturePromoSpecification::BubbleArrow
    FeaturePromoSpecification::kDefaultBubbleArrow;

FeaturePromoSpecification::FeaturePromoSpecification() = default;

FeaturePromoSpecification::FeaturePromoSpecification(
    FeaturePromoSpecification&& other)
    : feature_(std::exchange(other.feature_, nullptr)),
      promo_type_(std::exchange(other.promo_type_, PromoType::kUnspecifiied)),
      anchor_element_id_(
          std::exchange(other.anchor_element_id_, ui::ElementIdentifier())),
      anchor_element_filter_(std::move(other.anchor_element_filter_)),
      bubble_body_string_id_(std::exchange(other.bubble_body_string_id_, 0)),
      bubble_title_text_(std::move(other.bubble_title_text_)),
      bubble_icon_(std::exchange(other.bubble_icon_, nullptr)),
      bubble_arrow_(std::exchange(other.bubble_arrow_, kDefaultBubbleArrow)),
      screen_reader_string_id_(
          std::exchange(other.screen_reader_string_id_, 0)),
      screen_reader_accelerator_(
          std::exchange(other.screen_reader_accelerator_, AcceleratorInfo())),
      tutorial_id_(std::move(other.tutorial_id_)) {}

FeaturePromoSpecification::~FeaturePromoSpecification() = default;

FeaturePromoSpecification& FeaturePromoSpecification::operator=(
    FeaturePromoSpecification&& other) {
  if (this != &other) {
    feature_ = std::exchange(other.feature_, nullptr);
    promo_type_ = std::exchange(other.promo_type_, PromoType::kUnspecifiied);
    anchor_element_id_ =
        std::exchange(other.anchor_element_id_, ui::ElementIdentifier());
    anchor_element_filter_ = std::move(other.anchor_element_filter_);
    bubble_body_string_id_ = std::exchange(other.bubble_body_string_id_, 0);
    bubble_title_text_ = std::move(other.bubble_title_text_);
    bubble_icon_ = std::exchange(other.bubble_icon_, nullptr);
    bubble_arrow_ = std::exchange(other.bubble_arrow_, kDefaultBubbleArrow);
    screen_reader_string_id_ = std::exchange(other.screen_reader_string_id_, 0);
    screen_reader_accelerator_ =
        std::exchange(other.screen_reader_accelerator_, AcceleratorInfo());
    tutorial_id_ = std::move(other.tutorial_id_);
  }
  return *this;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForToastPromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    int accessible_text_string_id,
    AcceleratorInfo accessible_accelerator) {
  FeaturePromoSpecification spec(&feature, PromoType::kToast, anchor_element_id,
                                 body_text_string_id);
  spec.screen_reader_string_id_ = accessible_text_string_id;
  spec.screen_reader_accelerator_ = std::move(accessible_accelerator);
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForSnoozePromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id) {
  return FeaturePromoSpecification(&feature, PromoType::kSnooze,
                                   anchor_element_id, body_text_string_id);
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForTutorialPromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    TutorialIdentifier tutorial_id) {
  FeaturePromoSpecification spec(&feature, PromoType::kTutorial,
                                 anchor_element_id, body_text_string_id);
  DCHECK(!tutorial_id.empty());
  spec.tutorial_id_ = tutorial_id;
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForLegacyPromo(
    const base::Feature* feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id) {
  return FeaturePromoSpecification(feature, PromoType::kLegacy,
                                   anchor_element_id, body_text_string_id);
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleTitleText(
    int title_text_string_id) {
  DCHECK_NE(promo_type_, PromoType::kUnspecifiied);
  bubble_title_text_ = l10n_util::GetStringUTF16(title_text_string_id);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleIcon(
    const gfx::VectorIcon* bubble_icon) {
  DCHECK_NE(promo_type_, PromoType::kUnspecifiied);
  bubble_icon_ = bubble_icon;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleArrow(
    BubbleArrow bubble_arrow) {
  bubble_arrow_ = bubble_arrow;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetAnchorElementFilter(
    AnchorElementFilter anchor_element_filter) {
  anchor_element_filter_ = std::move(anchor_element_filter);
  return *this;
}

ui::TrackedElement* FeaturePromoSpecification::GetAnchorElement(
    ui::ElementContext context) const {
  auto* const element_tracker = ui::ElementTracker::GetElementTracker();
  return anchor_element_filter_ ? anchor_element_filter_.Run(
                                      element_tracker->GetAllMatchingElements(
                                          anchor_element_id_, context))
                                : element_tracker->GetFirstMatchingElement(
                                      anchor_element_id_, context);
}

FeaturePromoSpecification::FeaturePromoSpecification(
    const base::Feature* feature,
    PromoType promo_type,
    ui::ElementIdentifier anchor_element_id,
    int bubble_body_string_id)
    : feature_(feature),
      promo_type_(promo_type),
      anchor_element_id_(anchor_element_id),
      bubble_body_string_id_(bubble_body_string_id) {
  DCHECK_NE(promo_type, PromoType::kUnspecifiied);
  DCHECK(bubble_body_string_id_);
}
