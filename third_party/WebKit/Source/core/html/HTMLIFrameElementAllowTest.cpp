// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementAllow.h"

#include "core/dom/Document.h"
#include "core/html/HTMLIFrameElement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAre;

namespace blink {

TEST(HTMLIFrameElementAllowTest, ParseAllowedFeatureNamesValid) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);
  HTMLIFrameElementAllow* allow =
      static_cast<HTMLIFrameElementAllow*>(iframe->allow());
  String error_message;
  Vector<WebFeaturePolicyFeature> result;

  allow->setValue("");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_TRUE(result.IsEmpty());

  allow->setValue("fullscreen");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_THAT(result,
              UnorderedElementsAre(WebFeaturePolicyFeature::kFullscreen));

  allow->setValue("fullscreen payment");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_THAT(result, UnorderedElementsAre(WebFeaturePolicyFeature::kFullscreen,
                                           WebFeaturePolicyFeature::kPayment));
}

TEST(HTMLIFrameElementAllowTest, ParseAllowedFeatureNamesInvalid) {
  Document* document = Document::Create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::Create(*document);
  HTMLIFrameElementAllow* allow =
      static_cast<HTMLIFrameElementAllow*>(iframe->allow());
  String error_message;
  Vector<WebFeaturePolicyFeature> result;

  allow->setValue("invalid");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_EQ("'invalid' is an invalid feature name.", error_message);

  allow->setValue("fullscreen invalid1 invalid2");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_THAT(result,
              UnorderedElementsAre(WebFeaturePolicyFeature::kFullscreen));
  EXPECT_EQ("'invalid1', 'invalid2' are invalid feature names.", error_message);

  allow->setValue("fullscreen invalid payment fullscreen");
  result = allow->ParseAllowedFeatureNames(error_message);
  EXPECT_EQ("'invalid' is an invalid feature name.", error_message);
  EXPECT_THAT(result, UnorderedElementsAre(WebFeaturePolicyFeature::kFullscreen,
                                           WebFeaturePolicyFeature::kPayment));
}

}  // namespace blink
