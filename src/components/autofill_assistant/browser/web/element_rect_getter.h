// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_RECT_GETTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_RECT_GETTER_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace autofill_assistant {

// Worker class to get an element's bounding rectangle in viewport coordinates.
// This returns the global coordinates of the element rect, summing up (and
// cutting) throughout the iframe stack.
//
// 0/0  _____________
//     |   _______   |
//     |  |       |  |
//     |  |   X   |  |
//     |  |_______|  |
//     |_____________|
//                    100/100
//
// X inside the iFrame has a local position of 20/20. This getter returns the
// global position of 50/50.
class ElementRectGetter : public WebControllerWorker {
 public:
  // |devtools_client| must be valid for the lifetime of the instance.
  ElementRectGetter(DevtoolsClient* devtools_client);
  ~ElementRectGetter() override;

  // Callback that receives the bounding rect of the element.
  //
  // If the first element is false, the call failed. Otherwise, the second
  // element contains the rect.
  using ElementRectCallback = base::OnceCallback<void(bool, const RectF&)>;

  void Start(std::unique_ptr<ElementFinder::Result> element,
             ElementRectCallback callback);

 private:
  void GetBoundingClientRect(std::unique_ptr<ElementFinder::Result> element,
                             size_t index,
                             const RectF& stacked_rect,
                             ElementRectCallback callback);

  void OnGetClientRectResult(
      ElementRectCallback callback,
      std::unique_ptr<ElementFinder::Result> element,
      size_t index,
      const RectF& stacked_rect,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  DevtoolsClient* devtools_client_ = nullptr;
  base::WeakPtrFactory<ElementRectGetter> weak_ptr_factory_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_POSITION_GETTER_H_
