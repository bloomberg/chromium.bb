// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementAllow.h"

#include "core/dom/Document.h"
#include "core/html/HTMLIFrameElement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace blink {

TEST(HTMLIFrameElementAllowTest, ParseAllowedFeatureNamesValid) {
  Document* document = Document::create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::create(*document);
  HTMLIFrameElementAllow* allow = HTMLIFrameElementAllow::create(iframe);
  String errorMessage;
  Vector<WebFeaturePolicyFeature> result;

  allow->setValue("");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_TRUE(result.isEmpty());

  allow->setValue("fullscreen");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_THAT(result,
              UnorderedElementsAre(WebFeaturePolicyFeature::Fullscreen));

  allow->setValue("fullscreen payment vibrate");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_THAT(result, UnorderedElementsAre(WebFeaturePolicyFeature::Fullscreen,
                                           WebFeaturePolicyFeature::Payment,
                                           WebFeaturePolicyFeature::Vibrate));
}

TEST(HTMLIFrameElementAllowTest, ParseAllowedFeatureNamesInvalid) {
  Document* document = Document::create();
  HTMLIFrameElement* iframe = HTMLIFrameElement::create(*document);
  HTMLIFrameElementAllow* allow = HTMLIFrameElementAllow::create(iframe);
  String errorMessage;
  Vector<WebFeaturePolicyFeature> result;

  allow->setValue("invalid");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_TRUE(result.isEmpty());
  EXPECT_EQ("'invalid' is an invalid feature name.", errorMessage);

  allow->setValue("fullscreen invalid1 invalid2");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_THAT(result,
              UnorderedElementsAre(WebFeaturePolicyFeature::Fullscreen));
  EXPECT_EQ("'invalid1', 'invalid2' are invalid feature names.", errorMessage);

  allow->setValue("fullscreen invalid vibrate fullscreen");
  result = allow->parseAllowedFeatureNames(errorMessage);
  EXPECT_EQ("'invalid' is an invalid feature name.", errorMessage);
  EXPECT_THAT(result, UnorderedElementsAre(WebFeaturePolicyFeature::Fullscreen,
                                           WebFeaturePolicyFeature::Vibrate));
}

}  // namespace blink
