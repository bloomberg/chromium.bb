// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_
#define CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class ScopedFeatureList;
}  // namespace test
}  // namespace base

namespace content {

void InitAndEnableRenderDocumentFeature(
    base::test::ScopedFeatureList* feature_list,
    std::string level);

// The list of values to test for the "level" parameter.
std::vector<std::string> RenderDocumentFeatureLevelValues();

}  // namespace content

#endif  // CONTENT_TEST_RENDER_DOCUMENT_FEATURE_H_
