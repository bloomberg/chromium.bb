// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/exported/WebFormElementObserverImpl.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/StaticNodeList.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebInputElement.h"
#include "public/web/modules/password_manager/WebFormElementObserverCallback.h"

namespace blink {

class WebFormElementObserverImpl::ObserverCallback
    : public MutationObserver::Delegate {
 public:
  ObserverCallback(HTMLElement&,
                   std::unique_ptr<WebFormElementObserverCallback>);

  ExecutionContext* GetExecutionContext() const override;

  void Deliver(const MutationRecordVector& records, MutationObserver&) override;

  void Disconnect();

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<HTMLElement> element_;
  Member<MutationObserver> mutation_observer_;
  std::unique_ptr<WebFormElementObserverCallback> callback_;
};

WebFormElementObserverImpl::ObserverCallback::ObserverCallback(
    HTMLElement& element,
    std::unique_ptr<WebFormElementObserverCallback> callback)
    : element_(element),
      mutation_observer_(MutationObserver::Create(this)),
      callback_(std::move(callback)) {
  {
    MutationObserverInit init;
    init.setAttributes(true);
    init.setAttributeFilter({"action", "class", "style"});
    mutation_observer_->observe(element_, init, ASSERT_NO_EXCEPTION);
  }
  if (element_->parentElement()) {
    MutationObserverInit init;
    init.setChildList(true);
    mutation_observer_->observe(element_->parentElement(), init,
                                ASSERT_NO_EXCEPTION);
  }
}

ExecutionContext*
WebFormElementObserverImpl::ObserverCallback::GetExecutionContext() const {
  return &element_->GetDocument();
}

void WebFormElementObserverImpl::ObserverCallback::Deliver(
    const MutationRecordVector& records,
    MutationObserver&) {
  for (const auto& record : records) {
    if (record->type() == "childList") {
      for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
        if (record->removedNodes()->item(i) != element_)
          continue;
        callback_->ElementWasHiddenOrRemoved();
        Disconnect();
        return;
      }
    } else {
      HTMLElement& element = *ToHTMLElement(record->target());
      if (record->attributeName() == "action") {
        // If the action was modified, we just assume that the form as
        // submitted.
        callback_->ElementWasHiddenOrRemoved();
        Disconnect();
        return;
      }
      // Otherwise, either "style" or "class" was modified. Check the
      // computed style.
      CSSComputedStyleDeclaration* style =
          CSSComputedStyleDeclaration::Create(&element);
      if (style->GetPropertyValue(CSSPropertyDisplay) == "none") {
        callback_->ElementWasHiddenOrRemoved();
        Disconnect();
        return;
      }
    }
  }
}

void WebFormElementObserverImpl::ObserverCallback::Disconnect() {
  mutation_observer_->disconnect();
  callback_.reset();
}

DEFINE_TRACE(WebFormElementObserverImpl::ObserverCallback) {
  visitor->Trace(element_);
  visitor->Trace(mutation_observer_);
  MutationObserver::Delegate::Trace(visitor);
}

WebFormElementObserver* WebFormElementObserver::Create(
    WebFormElement& element,
    std::unique_ptr<WebFormElementObserverCallback> callback) {
  return new WebFormElementObserverImpl(*element.Unwrap<HTMLFormElement>(),
                                        std::move(callback));
}

WebFormElementObserver* WebFormElementObserver::Create(
    WebInputElement& element,
    std::unique_ptr<WebFormElementObserverCallback> callback) {
  return new WebFormElementObserverImpl(*element.Unwrap<HTMLInputElement>(),
                                        std::move(callback));
}

WebFormElementObserverImpl::WebFormElementObserverImpl(
    HTMLElement& element,
    std::unique_ptr<WebFormElementObserverCallback> callback)
    : self_keep_alive_(this) {
  mutation_callback_ = new ObserverCallback(element, std::move(callback));
}

WebFormElementObserverImpl::~WebFormElementObserverImpl() {}

void WebFormElementObserverImpl::Disconnect() {
  mutation_callback_->Disconnect();
  mutation_callback_ = nullptr;
  self_keep_alive_.Clear();
}

DEFINE_TRACE(WebFormElementObserverImpl) {
  visitor->Trace(mutation_callback_);
}

}  // namespace blink
