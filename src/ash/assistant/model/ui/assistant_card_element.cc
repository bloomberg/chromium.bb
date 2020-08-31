// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_card_element.h"

#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/public/cpp/assistant/assistant_web_view_factory.h"
#include "base/base64.h"

namespace ash {

// AssistantCardElement::Processor ---------------------------------------------

class AssistantCardElement::Processor : public AssistantWebView::Observer {
 public:
  Processor(AssistantCardElement* card_element, ProcessingCallback callback)
      : card_element_(card_element), callback_(std::move(callback)) {}

  Processor(const Processor& copy) = delete;
  Processor& operator=(const Processor& assign) = delete;

  ~Processor() override {
    if (contents_view_)
      contents_view_->RemoveObserver(this);

    if (callback_)
      std::move(callback_).Run();
  }

  void Process() {
    // TODO(dmblack): Find a better way of determining desired card size.
    const int width_dip =
        kPreferredWidthDip - 2 * kUiElementHorizontalMarginDip;

    // Configure parameters for the card.
    AssistantWebView::InitParams contents_params;
    contents_params.enable_auto_resize = true;
    contents_params.min_size = gfx::Size(width_dip, 1);
    contents_params.max_size = gfx::Size(width_dip, INT_MAX);
    contents_params.suppress_navigation = true;

    // Create |contents_view_| and retain ownership until it is added to the
    // view hierarchy. If that never happens, it will be still be cleaned up.
    contents_view_ = AssistantWebViewFactory::Get()->Create(contents_params);

    // Observe |contents_view_| so that we are notified when loading is
    // complete.
    contents_view_->AddObserver(this);

    // Encode the html string to be URL-safe.
    std::string encoded_html;
    base::Base64Encode(card_element_->html(), &encoded_html);

    // Navigate to the data URL which represents the card.
    constexpr char kDataUriPrefix[] = "data:text/html;base64,";
    contents_view_->Navigate(GURL(kDataUriPrefix + encoded_html));
  }

 private:
  // AssistantWebView::Observer:
  void DidStopLoading() override {
    contents_view_->RemoveObserver(this);

    // Pass ownership of |contents_view_| to the card element that was being
    // processed and notify our |callback_| of the completion.
    card_element_->set_contents_view(std::move(contents_view_));
    std::move(callback_).Run();
  }

  // |card_element_| should outlive the Processor.
  AssistantCardElement* const card_element_;
  ProcessingCallback callback_;

  std::unique_ptr<AssistantWebView> contents_view_;
};

// AssistantCardElement --------------------------------------------------------

AssistantCardElement::AssistantCardElement(const std::string& html,
                                           const std::string& fallback)
    : AssistantUiElement(AssistantUiElementType::kCard),
      html_(html),
      fallback_(fallback) {}

AssistantCardElement::~AssistantCardElement() {
  // |processor_| should be destroyed before |this| has been deleted.
  processor_.reset();
}

void AssistantCardElement::Process(ProcessingCallback callback) {
  processor_ = std::make_unique<Processor>(this, std::move(callback));
  processor_->Process();
}

}  // namespace ash
