// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common functionality for unittest and browsertest harnesses.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_

#include <memory>

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PerformanceManagerImpl;
class PerformanceManagerRegistry;

// A test harness helper. Manages the PM in a test environment.
//
// Note that in some test environments we have automatic WebContents creation
// hooks, and in others the test code must manually call "OnWebContentsCreated".
//
// Rough directions for use:
//
// In browser tests:
// - components_browsertests:
//   The PerformanceManagerBrowserTestHarness fixture brings its own
//   OnWebContentsCreated hooks.
// - browser_tests:
//   The PerformanceManagerBrowserTestHarness and
//   ChromeRenderViewHostTestHarness have their own OnWebContentsCreated hooks.
//   If using ChromeRenderViewHostTestHarness you need to embed an instance of
//   this helper in order to initialize the PM.
//
// In unit tests:
// - components_unittests:
//   The PerformanceManagerTestHarness brings its own OnWebContentsCreated
//   hooks providing you use its CreateTestWebContents helper.
// - unit_tests:
//   The ChromeRenderViewHostTestHarness brings its own OnWebContentsCreated
//   hooks, but you need to embed an instance of this helper in order to
//   initialize the PM.
class PerformanceManagerTestHarnessHelper {
 public:
  PerformanceManagerTestHarnessHelper();
  PerformanceManagerTestHarnessHelper(
      const PerformanceManagerTestHarnessHelper&) = delete;
  PerformanceManagerTestHarnessHelper& operator=(
      const PerformanceManagerTestHarnessHelper&) = delete;
  virtual ~PerformanceManagerTestHarnessHelper();

  // Sets up the PM and registry, etc.
  virtual void SetUp();

  // Tears down the PM and registry, etc. Blocks on the main thread until they
  // are torn down.
  virtual void TearDown();

  // Attaches tab helpers to the provided |contents|. This should only need to
  // be called explicitly in components_unittests. In unit_tests, browser_tests
  // and components_browsertests we have the necessary hooks into WebContents
  // creation to automatically add our observers; it suffices to ensure that the
  // PM is initialized (ie, initialize an instance of this helper).
  void OnWebContentsCreated(content::WebContents* contents);

 private:
  std::unique_ptr<PerformanceManagerImpl> perf_man_;
  std::unique_ptr<PerformanceManagerRegistry> registry_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_TEST_HARNESS_HELPER_H_
