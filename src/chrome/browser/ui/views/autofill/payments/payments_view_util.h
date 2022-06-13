// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace views {
class Label;
class Textfield;
class Throbber;
}  // namespace views

namespace autofill {

// Defines a title view with an icon, a separator, and a label, to be used
// by dialogs that need to present the Google or Google Pay logo with a
// separator and custom horizontal padding.
class TitleWithIconAndSeparatorView : public views::View {
 public:
  METADATA_HEADER(TitleWithIconAndSeparatorView);

  enum class Icon {
    // Google Pay icon. The "Pay" portion is recolored for light/dark mode.
    GOOGLE_PAY,
    // Google super G.
    GOOGLE_G,
  };

  TitleWithIconAndSeparatorView(const std::u16string& window_title,
                                Icon icon_to_show);
  ~TitleWithIconAndSeparatorView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Creates and returns a small Textfield intended to be used for CVC entry.
std::unique_ptr<views::Textfield> CreateCvcTextfield();

// Defines a view with legal message. This class handles the legal message
// parsing and the links clicking events.
class LegalMessageView : public views::View {
 public:
  METADATA_HEADER(LegalMessageView);

  using LinkClickedCallback = base::RepeatingCallback<void(const GURL&)>;

  LegalMessageView(const LegalMessageLines& legal_message_lines,
                   LinkClickedCallback callback);
  ~LegalMessageView() override;
};

PaymentsBubbleClosedReason GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
    views::Widget::ClosedReason reason);

// TODO(crbug.com/1249665): Replace all payments' progress bar usages with this.
// Creates a progress bar with an explanatory text below.
class ProgressBarWithTextView : public views::View {
 public:
  METADATA_HEADER(ProgressBarWithTextView);

  explicit ProgressBarWithTextView(const std::u16string& progress_bar_text);
  ~ProgressBarWithTextView() override;

 private:
  // views::View:
  void OnThemeChanged() override;
  void AddedToWidget() override;

  raw_ptr<views::Label> progress_label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
