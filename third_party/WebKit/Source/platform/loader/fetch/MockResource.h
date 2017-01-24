// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockResource_h
#define MockResource_h

#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"

namespace blink {

class FetchRequest;
class ResourceFetcher;
struct ResourceLoaderOptions;

// Mocked Resource sub-class for testing. MockResource class can pretend a type
// of Resource sub-class in a simple way. You should not expect anything
// complicated to emulate actual sub-resources, but you may be able to use this
// class to verify classes that consume Resource sub-classes in a simple way.
class MockResource final : public Resource {
 public:
  static MockResource* fetch(FetchRequest&, ResourceFetcher*);
  static MockResource* create(const ResourceRequest&);
  MockResource(const ResourceRequest&, const ResourceLoaderOptions&);
};

}  // namespace blink

#endif
