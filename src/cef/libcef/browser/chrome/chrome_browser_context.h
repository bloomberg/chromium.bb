// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
#pragma once

#include "libcef/browser/browser_context.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_manager.h"

// See CefBrowserContext documentation for usage. Only accessed on the UI thread
// unless otherwise indicated.
class ChromeBrowserContext : public CefBrowserContext {
 public:
  explicit ChromeBrowserContext(const CefRequestContextSettings& settings);

  void InitializeAsync(base::OnceClosure initialized_cb);

  // CefBrowserContext overrides.
  content::BrowserContext* AsBrowserContext() override;
  Profile* AsProfile() override;
  bool IsInitialized() const override;
  void StoreOrTriggerInitCallback(base::OnceClosure callback) override;
  void Shutdown() override;

 private:
  ~ChromeBrowserContext() override;

  void ProfileCreated(Profile* profile, Profile::CreateStatus status);

  base::OnceClosure initialized_cb_;
  Profile* profile_ = nullptr;
  bool should_destroy_ = false;

  std::vector<base::OnceClosure> init_callbacks_;

  base::WeakPtrFactory<ChromeBrowserContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserContext);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
