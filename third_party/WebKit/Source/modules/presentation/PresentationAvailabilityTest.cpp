// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/presentation/PresentationRequest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom-blink.h"
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
