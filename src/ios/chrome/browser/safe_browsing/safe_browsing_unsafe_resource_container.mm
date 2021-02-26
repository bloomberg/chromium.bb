// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"

#include <list>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::UnsafeResource;

#pragma mark - UnsafeSubresourceContainer

namespace {
// Helper object storing unsafe subresources for a NavigationItem.
class UnsafeSubresourceContainer : public base::SupportsUserData::Data {
 public:
  ~UnsafeSubresourceContainer() override = default;

  // Lazily instantiates and returns the UnsafeSubresourceContainer for |item|.
  static UnsafeSubresourceContainer* FromNavigationItem(
      web::NavigationItem* item) {
    DCHECK(item);
    static void* kUserDataKey = &kUserDataKey;
    UnsafeSubresourceContainer* stack =
        static_cast<UnsafeSubresourceContainer*>(
            item->GetUserData(kUserDataKey));
    if (!stack) {
      auto new_stack = base::WrapUnique(new UnsafeSubresourceContainer());
      stack = new_stack.get();
      item->SetUserData(kUserDataKey, std::move(new_stack));
    }
    return stack;
  }

  // Returns a pointer to the first pending unsafe resource, if any.  Clears any
  // stored resources that are no longer pending.
  const UnsafeResource* GetUnsafeResource() {
    auto it = unsafe_resources_.begin();
    while (it != unsafe_resources_.end()) {
      const UnsafeResource* resource = it->resource();
      if (resource)
        return resource;
      it = unsafe_resources_.erase(it);
    }
    return nullptr;
  }

  // Stores |resource| in the container.
  void StoreUnsafeResource(const UnsafeResource& resource) {
    unsafe_resources_.push_back(PendingUnsafeResourceStorage(resource));
  }

 private:
  UnsafeSubresourceContainer() = default;

  std::list<PendingUnsafeResourceStorage> unsafe_resources_;
};
}  // namespace

#pragma mark - SafeBrowsingUnsafeResourceContainer

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingUnsafeResourceContainer)

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    web::WebState* web_state)
    : web_state_(web_state) {}

SafeBrowsingUnsafeResourceContainer::SafeBrowsingUnsafeResourceContainer(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer&
SafeBrowsingUnsafeResourceContainer::operator=(
    SafeBrowsingUnsafeResourceContainer&& other) = default;

SafeBrowsingUnsafeResourceContainer::~SafeBrowsingUnsafeResourceContainer() =
    default;

void SafeBrowsingUnsafeResourceContainer::StoreMainFrameUnsafeResource(
    const security_interstitials::UnsafeResource& resource) {
  DCHECK(!resource.web_state_getter.is_null() &&
         resource.web_state_getter.Run() == web_state_);
  DCHECK_EQ(safe_browsing::ResourceType::kMainFrame, resource.resource_type);

  // For main frame navigations, the copy is stored in
  // |main_frame_unsafe_resource_|.  It corresponds with the pending
  // NavigationItem, which may have not been created yet and will be discarded
  // after navigation failures.
  main_frame_unsafe_resource_ = PendingUnsafeResourceStorage(resource);
}

void SafeBrowsingUnsafeResourceContainer::StoreSubFrameUnsafeResource(
    const security_interstitials::UnsafeResource& resource,
    web::NavigationItem* main_frame_item) {
  DCHECK(!resource.web_state_getter.is_null() &&
         resource.web_state_getter.Run() == web_state_);
  DCHECK_EQ(safe_browsing::ResourceType::kSubFrame, resource.resource_type);
  DCHECK(main_frame_item);

  // Unsafe sub frame resources are caused by loads triggered by a committed
  // main frame navigation.  These are associated with the NavigationItem so
  // that they persist past reloads.
  UnsafeSubresourceContainer::FromNavigationItem(main_frame_item)
      ->StoreUnsafeResource(resource);
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetMainFrameUnsafeResource() const {
  return main_frame_unsafe_resource_.resource();
}

const security_interstitials::UnsafeResource*
SafeBrowsingUnsafeResourceContainer::GetSubFrameUnsafeResource(
    web::NavigationItem* item) const {
  return UnsafeSubresourceContainer::FromNavigationItem(item)
      ->GetUnsafeResource();
}
