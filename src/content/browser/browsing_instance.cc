// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_instance.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

namespace {
const char* const kDefaultInstanceSiteURL = "http://unisolated.invalid";
}  // namespace

// Start the BrowsingInstance ID counter from 1 to avoid a conflict with the
// invalid BrowsingInstanceId value, which is 0 in its underlying IdType32.
int BrowsingInstance::next_browsing_instance_id_ = 1;

BrowsingInstance::BrowsingInstance(BrowserContext* browser_context)
    : browser_context_(browser_context),
      isolation_context_(
          BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_++),
          BrowserOrResourceContext(browser_context)),
      active_contents_count_(0u),
      default_process_(nullptr) {
  DCHECK(browser_context);
}

void BrowsingInstance::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(default_process_, host);
  // Only clear the default process if the RenderProcessHost object goes away,
  // not if the renderer process goes away while the RenderProcessHost remains.
  default_process_->RemoveObserver(this);
  default_process_ = nullptr;
}

void BrowsingInstance::SetDefaultProcess(RenderProcessHost* default_process) {
  DCHECK(!default_process_);
  DCHECK(!default_site_instance_);
  default_process_ = default_process;
  default_process_->AddObserver(this);
}

bool BrowsingInstance::IsDefaultSiteInstance(
    const SiteInstanceImpl* site_instance) const {
  return site_instance != nullptr &&
         site_instance == default_site_instance_.get();
}

bool BrowsingInstance::HasSiteInstance(const GURL& url) {
  std::string site =
      SiteInstanceImpl::GetSiteForURL(browser_context_, isolation_context_, url)
          .possibly_invalid_spec();

  return site_instance_map_.find(site) != site_instance_map_.end();
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURL(
    const GURL& url,
    bool allow_default_instance) {
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetSiteInstanceForURLHelper(url, allow_default_instance);

  if (site_instance)
    return site_instance;

  // No current SiteInstance for this site, so let's create one.
  scoped_refptr<SiteInstanceImpl> instance = new SiteInstanceImpl(this);

  // Set the site of this new SiteInstance, which will register it with us.
  instance->SetSite(url);
  return instance;
}

void BrowsingInstance::GetSiteAndLockForURL(const GURL& url,
                                            bool allow_default_instance,
                                            GURL* site_url,
                                            GURL* lock_url) {
  scoped_refptr<SiteInstanceImpl> site_instance =
      GetSiteInstanceForURLHelper(url, allow_default_instance);

  if (site_instance) {
    *site_url = site_instance->GetSiteURL();
    *lock_url = site_instance->lock_url();
    return;
  }

  BrowserOrResourceContext context(browser_context_);
  *site_url = SiteInstanceImpl::GetSiteForURL(
      context, isolation_context_, url, true /* should_use_effective_urls */);
  *lock_url = SiteInstanceImpl::DetermineProcessLockURL(
      context, isolation_context_, url);
}

scoped_refptr<SiteInstanceImpl> BrowsingInstance::GetSiteInstanceForURLHelper(
    const GURL& url,
    bool allow_default_instance) {
  std::string site =
      SiteInstanceImpl::GetSiteForURL(browser_context_, isolation_context_, url)
          .possibly_invalid_spec();

  auto i = site_instance_map_.find(site);
  if (i != site_instance_map_.end())
    return i->second;

  // Check to see if we can use the default SiteInstance for sites that don't
  // need to be isolated in their own process. The default instance allows us to
  // have multiple unisolated sites share a process. We don't use the default
  // instance when kProcessSharingWithStrictSiteInstances is enabled because in
  // that case we want each site to have their own SiteInstance object and logic
  // elsewhere ensures that those SiteInstances share a process.
  if (allow_default_instance &&
      !base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances) &&
      !SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
          browser_context_, isolation_context_, url)) {
    DCHECK(!default_process_);
    if (!default_site_instance_) {
      default_site_instance_ = new SiteInstanceImpl(this);
      default_site_instance_->SetSite(GURL(kDefaultInstanceSiteURL));
    }
    return default_site_instance_;
  }

  return nullptr;
}

void BrowsingInstance::RegisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());

  // Explicitly prevent the |default_site_instance_| from being added since
  // the map is only supposed to contain instances that map to a single site.
  if (site_instance == default_site_instance_.get())
    return;

  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();

  // Only register if we don't have a SiteInstance for this site already.
  // It's possible to have two SiteInstances point to the same site if two
  // tabs are navigated there at the same time.  (We don't call SetSite or
  // register them until DidNavigate.)  If there is a previously existing
  // SiteInstance for this site, we just won't register the new one.
  auto i = site_instance_map_.find(site);
  if (i == site_instance_map_.end()) {
    // Not previously registered, so register it.
    site_instance_map_[site] = site_instance;
  }
}

void BrowsingInstance::UnregisterSiteInstance(SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->browsing_instance_.get() == this);
  DCHECK(site_instance->HasSite());
  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();

  // Only unregister the SiteInstance if it is the same one that is registered
  // for the site.  (It might have been an unregistered SiteInstance.  See the
  // comments in RegisterSiteInstance.)
  auto i = site_instance_map_.find(site);
  if (i != site_instance_map_.end() && i->second == site_instance) {
    // Matches, so erase it.
    site_instance_map_.erase(i);
  }
}

// static
BrowsingInstanceId BrowsingInstance::NextBrowsingInstanceId() {
  return BrowsingInstanceId::FromUnsafeValue(next_browsing_instance_id_);
}

BrowsingInstance::~BrowsingInstance() {
  // We should only be deleted when all of the SiteInstances that refer to
  // us are gone.
  DCHECK(site_instance_map_.empty());
  DCHECK_EQ(0u, active_contents_count_);
  if (default_process_)
    default_process_->RemoveObserver(this);
}

}  // namespace content
