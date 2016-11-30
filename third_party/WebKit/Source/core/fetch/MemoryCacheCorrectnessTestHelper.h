// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryCacheCorrectnessTestHelper_h
#define MemoryCacheCorrectnessTestHelper_h

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MemoryCache;

class MemoryCacheCorrectnessTestHelper : public ::testing::Test {
 protected:
  static void advanceClock(double seconds) { s_timeElapsed += seconds; }

  Resource* resourceFromResourceResponse(ResourceResponse,
                                         Resource::Type = Resource::Raw);
  Resource* resourceFromResourceRequest(ResourceRequest);
  // TODO(toyoshim): Consider to return sub-class pointer, e.g., RawResource*
  // for fetch() and createResource() below. See http://crrev.com/2509593003.
  Resource* fetch();
  ResourceFetcher* fetcher() const { return m_fetcher.get(); }

  static const char kResourceURL[];
  static const char kOriginalRequestDateAsString[];
  static const double kOriginalRequestDateAsDouble;
  static const char kOneDayBeforeOriginalRequest[];
  static const char kOneDayAfterOriginalRequest[];

 private:
  static double returnMockTime();

  // Overrides ::testing::Test.
  void SetUp() override;
  void TearDown() override;

  virtual Resource* createResource(const ResourceRequest&, Resource::Type) = 0;

  Persistent<MemoryCache> m_globalMemoryCache;
  Persistent<ResourceFetcher> m_fetcher;
  TimeFunction m_originalTimeFunction;

  static double s_timeElapsed;
};

}  // namespace blink

#endif  // MemoryCacheCorrectnessTestHelper
