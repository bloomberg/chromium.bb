// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/chrome_browser_context.h"

#include "libcef/browser/thread_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/off_the_record_profile_impl.h"

ChromeBrowserContext::ChromeBrowserContext(
    const CefRequestContextSettings& settings)
    : CefBrowserContext(settings), weak_ptr_factory_(this) {}

ChromeBrowserContext::~ChromeBrowserContext() = default;

content::BrowserContext* ChromeBrowserContext::AsBrowserContext() {
  return profile_;
}

Profile* ChromeBrowserContext::AsProfile() {
  return profile_;
}

bool ChromeBrowserContext::IsInitialized() const {
  CEF_REQUIRE_UIT();
  return !!profile_;
}

void ChromeBrowserContext::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  CEF_REQUIRE_UIT();
  if (IsInitialized()) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

void ChromeBrowserContext::InitializeAsync(base::OnceClosure initialized_cb) {
  init_callbacks_.emplace_back(std::move(initialized_cb));

  CefBrowserContext::Initialize();

  if (!cache_path_.empty()) {
    auto* profile_manager = g_browser_process->profile_manager();
    const auto& user_data_dir = profile_manager->user_data_dir();

    if (cache_path_ == user_data_dir) {
      // Use the default disk-based profile.
      auto profile = profile_manager->GetPrimaryUserProfile();
      ProfileCreated(profile, Profile::CreateStatus::CREATE_STATUS_INITIALIZED);
      return;
    } else if (cache_path_.DirName() == user_data_dir) {
      // Create or load a specific disk-based profile. May continue
      // synchronously or asynchronously.
      profile_manager->CreateProfileAsync(
          cache_path_, base::Bind(&ChromeBrowserContext::ProfileCreated,
                                  weak_ptr_factory_.GetWeakPtr()));
      return;
    } else {
      // All profile directories must be relative to |user_data_dir|.
      LOG(ERROR) << "Cannot create profile at path "
                 << cache_path_.AsUTF8Unsafe();
    }
  }

  // Default to creating a new/unique OffTheRecord profile.
  ProfileCreated(nullptr, Profile::CreateStatus::CREATE_STATUS_CANCELED);
}

void ChromeBrowserContext::Shutdown() {
  CefBrowserContext::Shutdown();
  // |g_browser_process| may be nullptr during shutdown.
  if (should_destroy_ && g_browser_process) {
    g_browser_process->profile_manager()
        ->GetPrimaryUserProfile()
        ->DestroyOffTheRecordProfile(profile_);
  }
  profile_ = nullptr;
}

void ChromeBrowserContext::ProfileCreated(Profile* profile,
                                          Profile::CreateStatus status) {
  Profile* parent_profile = nullptr;
  OffTheRecordProfileImpl* otr_profile = nullptr;

  if (status != Profile::CreateStatus::CREATE_STATUS_CREATED &&
      status != Profile::CreateStatus::CREATE_STATUS_INITIALIZED) {
    CHECK(!profile);
    CHECK(!profile_);

    // Creation of a disk-based profile failed for some reason. Create a
    // new/unique OffTheRecord profile instead.
    const auto& profile_id = Profile::OTRProfileID::CreateUniqueForCEF();
    parent_profile =
        g_browser_process->profile_manager()->GetPrimaryUserProfile();
    profile_ = parent_profile->GetOffTheRecordProfile(profile_id);
    otr_profile = static_cast<OffTheRecordProfileImpl*>(profile_);
    status = Profile::CreateStatus::CREATE_STATUS_INITIALIZED;
    should_destroy_ = true;
  } else if (profile && !profile_) {
    // May be CREATE_STATUS_CREATED or CREATE_STATUS_INITIALIZED since
    // *CREATED isn't always sent for a disk-based profile that already
    // exists.
    profile_ = profile;
  }

  if (status == Profile::CreateStatus::CREATE_STATUS_INITIALIZED) {
    CHECK(profile_);

    // Must set |profile_| before Init() calls
    // ChromeContentBrowserClientCef::ConfigureNetworkContextParams so that
    // CefBrowserContext::FromBrowserContext can find us.
    if (otr_profile) {
      otr_profile->Init();
      parent_profile->NotifyOffTheRecordProfileCreated(otr_profile);
    }

    if (!init_callbacks_.empty()) {
      for (auto& callback : init_callbacks_) {
        std::move(callback).Run();
      }
      init_callbacks_.clear();
    }
  }
}
