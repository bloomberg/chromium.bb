// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

TEST(PresentationAvailabilityTest, NoPageVisibilityChangeAfterDetach) {
  V8TestingScope scope;
  WTF::Vector<KURL> urls;
  urls.push_back(URLTestHelpers::ToKURL("https://example.com"));
  urls.push_back(URLTestHelpers::ToKURL("https://another.com"));

  Persistent<PresentationAvailabilityProperty> resolver =
      new PresentationAvailabilityProperty(
          scope.GetExecutionContext(), nullptr,
          PresentationAvailabilityProperty::kReady);
  Persistent<PresentationAvailability> availability =
      PresentationAvailability::Take(resolver, urls, false);

  // These two calls should not crash.
  scope.GetFrame().Detach(FrameDetachType::kRemove);
  scope.GetPage().SetVisibilityState(mojom::PageVisibilityState::kHidden,
                                     false);
}

}  // anonymous namespace
}  // namespace blink
