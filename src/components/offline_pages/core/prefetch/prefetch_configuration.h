// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_CONFIGURATION_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_CONFIGURATION_H_

namespace offline_pages {

// Class that interfaces with configuration systems to provide prefetching
// specific configuration information.
class PrefetchConfiguration {
 public:
  virtual ~PrefetchConfiguration() = default;

  // Returns true if all needed flags and settings allow the prefetching of
  // offline pages to run. Note that this a runtime value that can change in the
  // course of the application lifetime. Should not be overridden by subclasses.
  bool IsPrefetchingEnabled();

  // Returns true if the user controlled setting allows the prefetching of
  // offline pages to run. Note that this a runtime value that can change in the
  // course of the application lifetime.
  virtual bool IsPrefetchingEnabledBySettings() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_CONFIGURATION_H_
