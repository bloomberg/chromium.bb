// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_TEST_BROWSER_CONTEXT_H_
#define ASH_COMPONENTS_ARC_TEST_TEST_BROWSER_CONTEXT_H_

#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_context.h"

class BrowserContextDependencyManager;

namespace arc {

// A browser context for testing that can be used for getting objects
// through ArcBrowserContextKeyedServiceFactoryBase<>.
class TestBrowserContext : public content::TestBrowserContext {
 public:
  TestBrowserContext();

  TestBrowserContext(const TestBrowserContext&) = delete;
  TestBrowserContext& operator=(const TestBrowserContext&) = delete;

  ~TestBrowserContext() override;

  PrefService* prefs() { return &prefs_; }
  PrefRegistrySimple* pref_registry() { return prefs_.registry(); }

 private:
  BrowserContextDependencyManager* const browser_context_dependency_manager_;
  TestingPrefServiceSimple prefs_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_TEST_BROWSER_CONTEXT_H_
