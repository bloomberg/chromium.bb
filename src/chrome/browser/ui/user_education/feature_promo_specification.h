// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_SPECIFICATION_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_SPECIFICATION_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace base {
struct Feature;
}

namespace gfx {
struct VectorIcon;
}

// Specifies the parameters for a feature promo and its associated bubble.
class FeaturePromoSpecification {
 public:
  // The body text (specified by |bubble_body_string_id|) can have parameters
  // that can be specified situationally. When specifying these parameters,
  // use a |StringReplacements| object.
  using StringReplacements = std::vector<std::u16string>;

  // Optional method that filters a set of potential `elements` to choose and
  // return the anchor element, or null if none of the inputs is appropriate.
  // This method can return an element different from the input list, or null
  // if no valid element is found (this will cause the IPH not to run).
  using AnchorElementFilter = base::RepeatingCallback<ui::TrackedElement*(
      const ui::ElementTracker::ElementList& elements)>;

  // Describes the type of promo. Used to configure defaults for the promo's
  // bubble.
  enum class PromoType {
    // Uninitialized/invalid specification.
    kUnspecifiied,
    // A toast-style promo.
    kToast,
    // A snooze-style promo.
    kSnooze,
    // A tutorial promo.
    kTutorial,
    // A simple promo that acts like a toast but without the required
    // accessibility data.
    kLegacy,
  };

  // Mirrors most values of views::BubbleBorder::Arrow.
  // All values except kNone show a visible arrow between the bubble and the
  // anchor element.
  enum class BubbleArrow {
    kNone,  // Positions the bubble directly beneath the anchor with no arrow.
    kTopLeft,
    kTopRight,
    kBottomLeft,
    kBottomRight,
    kLeftTop,
    kRightTop,
    kLeftBottom,
    kRightBottom,
    kTopCenter,
    kBottomCenter,
    kLeftCenter,
    kRightCenter,
  };

  // Represents a command or command accelerator. Can be valueless (falsy) if
  // neither a command ID nor an explicit accelerator is specified.
  class AcceleratorInfo {
   public:
    // You can assign either an int (command ID) or a ui::Accelerator to an
    // AcceleratorInfo object.
    using ValueType = absl::variant<int, ui::Accelerator>;

    AcceleratorInfo();
    AcceleratorInfo(const AcceleratorInfo& other);
    ~AcceleratorInfo();

    explicit AcceleratorInfo(ValueType value);
    AcceleratorInfo& operator=(ValueType value);
    AcceleratorInfo& operator=(const AcceleratorInfo& other);

    explicit operator bool() const;
    bool operator!() const { return !static_cast<bool>(*this); }

    ui::Accelerator GetAccelerator(
        const ui::AcceleratorProvider* provider) const;

   private:
    ValueType value_;
  };

  FeaturePromoSpecification();
  FeaturePromoSpecification(FeaturePromoSpecification&& other);
  ~FeaturePromoSpecification();
  FeaturePromoSpecification& operator=(FeaturePromoSpecification&& other);

  // Specifies a standard toast promo.
  // Because toasts are transient, they expect a separate screen reader prompt.
  // It is recommended that the prompt include an
  static FeaturePromoSpecification CreateForToastPromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      int accessible_text_string_id,
      AcceleratorInfo accessible_accelerator);

  // Specifies a promo with snooze buttons.
  static FeaturePromoSpecification CreateForSnoozePromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id);

  // Specifies a promo that launches a tutorial.
  static FeaturePromoSpecification CreateForTutorialPromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      TutorialIdentifier tutorial_id);

  // Specifies a text-only promo without additional accessibility information.
  // Deprecated. Only included for backwards compatibility with existing
  // promos. This is the only case in which |feature| can be null, and if it is
  // the result can only be used for a critical promo.
  static FeaturePromoSpecification CreateForLegacyPromo(
      const base::Feature* feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id);

  // Set the optional bubble title. This text appears above the body text in a
  // slightly larger font.
  FeaturePromoSpecification& SetBubbleTitleText(int title_text_string_id);

  // Set the optional bubble icon. This is displayed next to the title or body
  // text.
  FeaturePromoSpecification& SetBubbleIcon(const gfx::VectorIcon* bubble_icon);

  // Set the bubble arrow. Default is top-left.
  FeaturePromoSpecification& SetBubbleArrow(BubbleArrow bubble_arrow);

  // Set the anchor element filter.
  FeaturePromoSpecification& SetAnchorElementFilter(
      AnchorElementFilter anchor_element_filter);

  // Get the anchor element based on `anchor_element_id`,
  // `anchor_element_filter`, and `context`.
  ui::TrackedElement* GetAnchorElement(ui::ElementContext context) const;

  const base::Feature* feature() const { return feature_; }
  PromoType promo_type() const { return promo_type_; }
  ui::ElementIdentifier anchor_element_id() const { return anchor_element_id_; }
  const AnchorElementFilter& anchor_element_filter() const {
    return anchor_element_filter_;
  }
  int bubble_body_string_id() const { return bubble_body_string_id_; }
  const std::u16string& bubble_title_text() const { return bubble_title_text_; }
  const gfx::VectorIcon* bubble_icon() const { return bubble_icon_; }
  BubbleArrow bubble_arrow() const { return bubble_arrow_; }
  int screen_reader_string_id() const { return screen_reader_string_id_; }
  const AcceleratorInfo& screen_reader_accelerator() const {
    return screen_reader_accelerator_;
  }
  const TutorialIdentifier& tutorial_id() const { return tutorial_id_; }

 private:
  static constexpr BubbleArrow kDefaultBubbleArrow = BubbleArrow::kTopRight;

  FeaturePromoSpecification(const base::Feature* feature,
                            PromoType promo_type,
                            ui::ElementIdentifier anchor_element_id,
                            int bubble_body_string_id);

  raw_ptr<const base::Feature> feature_ = nullptr;

  // The type of promo. A promo with type kUnspecified is not valid.
  PromoType promo_type_ = PromoType::kUnspecifiied;

  // The element identifier of the element to attach the promo to.
  ui::ElementIdentifier anchor_element_id_;

  // The filter to use if there is more than one matching element, or
  // additional processing is needed (default is to always use the first
  // matching element).
  AnchorElementFilter anchor_element_filter_;

  // Text to be displayed in the promo bubble body. Should not be zero for
  // valid bubbles. We keep the string ID around because we can specify format
  // parameters when we actually create the bubble.
  int bubble_body_string_id_ = 0;

  // Optional text that is displayed at the top of the bubble, in a slightly
  // more prominent font.
  std::u16string bubble_title_text_;

  // Optional icon that is displayed next to bubble text.
  raw_ptr<const gfx::VectorIcon> bubble_icon_ = nullptr;

  // Optional arrow pointing to the promo'd element. Defaults to top left.
  BubbleArrow bubble_arrow_ = kDefaultBubbleArrow;

  // Optional screen reader announcement that replaces bubble text when the
  // bubble is first announced.
  int screen_reader_string_id_ = 0;

  // Accelerator that is used to fill in a parametric field in
  // screen_reader_string_id_.
  AcceleratorInfo screen_reader_accelerator_;

  // Tutorial identifier if the user decides to view a tutorial.
  TutorialIdentifier tutorial_id_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_SPECIFICATION_H_
