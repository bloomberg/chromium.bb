// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_set.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

PermissionSet::PermissionSet() {}
PermissionSet::PermissionSet(APIPermissionSet apis,
                             ManifestPermissionSet manifest_permissions,
                             URLPatternSet explicit_hosts,
                             URLPatternSet scriptable_hosts)
    : apis_(std::move(apis)),
      manifest_permissions_(std::move(manifest_permissions)),
      explicit_hosts_(std::move(explicit_hosts)),
      scriptable_hosts_(std::move(scriptable_hosts)) {
  // For explicit hosts, we require the path to be "/*". This is a little
  // tricky, since URLPatternSets are backed by a std::set<>, where the
  // elements are immutable (because they themselves are the keys). In order to
  // work around this, we find the patterns we need to update, collect updated
  // versions, remove them, and then insert the updated ones. This isn't very
  // clean, but does mean that all the other URLPatterns in the set (which is
  // likely the majority) get to be std::move'd efficiently.
  // NOTE(devlin): This would be non-issue if URLPatternSet() was backed by e.g.
  // a vector.
  std::vector<URLPattern> modified_patterns;
  for (auto iter = explicit_hosts_.begin(); iter != explicit_hosts_.end();) {
    if (iter->path() == "/*") {
      ++iter;
      continue;
    }
    URLPattern modified_pattern(*iter);
    modified_pattern.SetPath("/*");
    modified_patterns.push_back(std::move(modified_pattern));
    iter = explicit_hosts_.erase(iter);
  }

  for (URLPattern& pattern : modified_patterns)
    explicit_hosts_.AddPattern(std::move(pattern));

  InitImplicitPermissions();
  InitEffectiveHosts();
}

PermissionSet::~PermissionSet() {}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateDifference(
    const PermissionSet& set1,
    const PermissionSet& set2) {
  APIPermissionSet apis;
  APIPermissionSet::Difference(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Difference(set1.manifest_permissions(),
                                    set2.manifest_permissions(),
                                    &manifest_permissions);

  URLPatternSet explicit_hosts = URLPatternSet::CreateDifference(
      set1.explicit_hosts(), set2.explicit_hosts());

  URLPatternSet scriptable_hosts = URLPatternSet::CreateDifference(
      set1.scriptable_hosts(), set2.scriptable_hosts());

  return base::WrapUnique(new PermissionSet(
      std::move(apis), std::move(manifest_permissions),
      std::move(explicit_hosts), std::move(scriptable_hosts)));
}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateIntersection(
    const PermissionSet& set1,
    const PermissionSet& set2,
    URLPatternSet::IntersectionBehavior intersection_behavior) {
  APIPermissionSet apis;
  APIPermissionSet::Intersection(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Intersection(set1.manifest_permissions(),
                                      set2.manifest_permissions(),
                                      &manifest_permissions);

  URLPatternSet explicit_hosts = URLPatternSet::CreateIntersection(
      set1.explicit_hosts(), set2.explicit_hosts(), intersection_behavior);
  URLPatternSet scriptable_hosts = URLPatternSet::CreateIntersection(
      set1.scriptable_hosts(), set2.scriptable_hosts(), intersection_behavior);

  return base::WrapUnique(new PermissionSet(
      std::move(apis), std::move(manifest_permissions),
      std::move(explicit_hosts), std::move(scriptable_hosts)));
}

// static
std::unique_ptr<const PermissionSet> PermissionSet::CreateUnion(
    const PermissionSet& set1,
    const PermissionSet& set2) {
  APIPermissionSet apis;
  APIPermissionSet::Union(set1.apis(), set2.apis(), &apis);

  ManifestPermissionSet manifest_permissions;
  ManifestPermissionSet::Union(set1.manifest_permissions(),
                               set2.manifest_permissions(),
                               &manifest_permissions);

  URLPatternSet explicit_hosts =
      URLPatternSet::CreateUnion(set1.explicit_hosts(), set2.explicit_hosts());

  URLPatternSet scriptable_hosts = URLPatternSet::CreateUnion(
      set1.scriptable_hosts(), set2.scriptable_hosts());

  return base::WrapUnique(new PermissionSet(
      std::move(apis), std::move(manifest_permissions),
      std::move(explicit_hosts), std::move(scriptable_hosts)));
}

bool PermissionSet::operator==(
    const PermissionSet& rhs) const {
  return apis_ == rhs.apis_ &&
      manifest_permissions_ == rhs.manifest_permissions_ &&
      scriptable_hosts_ == rhs.scriptable_hosts_ &&
      explicit_hosts_ == rhs.explicit_hosts_;
}

bool PermissionSet::operator!=(const PermissionSet& rhs) const {
  return !(*this == rhs);
}

std::unique_ptr<const PermissionSet> PermissionSet::Clone() const {
  return base::WrapUnique(new PermissionSet(*this));
}

bool PermissionSet::Contains(const PermissionSet& set) const {
  return apis_.Contains(set.apis()) &&
         manifest_permissions_.Contains(set.manifest_permissions()) &&
         explicit_hosts().Contains(set.explicit_hosts()) &&
         scriptable_hosts().Contains(set.scriptable_hosts());
}

std::set<std::string> PermissionSet::GetAPIsAsStrings() const {
  std::set<std::string> apis_str;
  for (APIPermissionSet::const_iterator i = apis_.begin();
       i != apis_.end(); ++i) {
    apis_str.insert(i->name());
  }
  return apis_str;
}

bool PermissionSet::IsEmpty() const {
  // Not default if any host permissions are present.
  if (!(explicit_hosts().is_empty() && scriptable_hosts().is_empty()))
    return false;

  // Or if it has no api permissions.
  return apis().empty() && manifest_permissions().empty();
}

bool PermissionSet::HasAPIPermission(
    APIPermission::ID id) const {
  return apis().find(id) != apis().end();
}

bool PermissionSet::HasAPIPermission(const std::string& permission_name) const {
  const APIPermissionInfo* permission =
      PermissionsInfo::GetInstance()->GetByName(permission_name);
  // Ensure our PermissionsProvider is aware of this permission.
  CHECK(permission) << permission_name;
  return (permission && apis_.count(permission->id()));
}

bool PermissionSet::CheckAPIPermission(APIPermission::ID permission) const {
  return CheckAPIPermissionWithParam(permission, NULL);
}

bool PermissionSet::CheckAPIPermissionWithParam(
    APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  APIPermissionSet::const_iterator iter = apis().find(permission);
  if (iter == apis().end())
    return false;
  return iter->Check(param);
}

bool PermissionSet::HasExplicitAccessToOrigin(
    const GURL& origin) const {
  return explicit_hosts().MatchesURL(origin);
}

bool PermissionSet::HasEffectiveAccessToAllHosts() const {
  // There are two ways this set can have effective access to all hosts:
  //  1) it has an <all_urls> URL pattern.
  //  2) it has a named permission with implied full URL access.
  if (effective_hosts().MatchesAllURLs())
    return true;

  for (APIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    if (i->info()->implies_full_url_access())
      return true;
  }
  return false;
}

bool PermissionSet::ShouldWarnAllHosts(bool include_api_permissions) const {
  if (host_permissions_should_warn_all_hosts_ == UNINITIALIZED)
    InitShouldWarnAllHostsForHostPermissions();

  if (host_permissions_should_warn_all_hosts_ == WARN_ALL_HOSTS)
    return true;

  if (!include_api_permissions)
    return false;

  if (api_permissions_should_warn_all_hosts_ == UNINITIALIZED)
    InitShouldWarnAllHostsForAPIPermissions();

  return api_permissions_should_warn_all_hosts_ == WARN_ALL_HOSTS;
}

bool PermissionSet::HasEffectiveAccessToURL(const GURL& url) const {
  return effective_hosts().MatchesURL(url);
}

PermissionSet::PermissionSet(const PermissionSet& other)
    : apis_(other.apis_.Clone()),
      manifest_permissions_(other.manifest_permissions_.Clone()),
      explicit_hosts_(other.explicit_hosts_.Clone()),
      scriptable_hosts_(other.scriptable_hosts_.Clone()),
      effective_hosts_(other.effective_hosts_.Clone()) {}

void PermissionSet::InitImplicitPermissions() {
  // The downloads permission implies the internal version as well.
  if (apis_.find(APIPermission::kDownloads) != apis_.end())
    apis_.insert(APIPermission::kDownloadsInternal);

  // The fileBrowserHandler permission implies the internal version as well.
  if (apis_.find(APIPermission::kFileBrowserHandler) != apis_.end())
    apis_.insert(APIPermission::kFileBrowserHandlerInternal);
}

void PermissionSet::InitEffectiveHosts() {
  effective_hosts_ =
      URLPatternSet::CreateUnion(explicit_hosts(), scriptable_hosts());
}

void PermissionSet::InitShouldWarnAllHostsForHostPermissions() const {
  DCHECK_EQ(UNINITIALIZED, host_permissions_should_warn_all_hosts_);
  host_permissions_should_warn_all_hosts_ = DONT_WARN_ALL_HOSTS;
  if (effective_hosts().MatchesAllURLs()) {
    host_permissions_should_warn_all_hosts_ = WARN_ALL_HOSTS;
    return;
  }

  for (const auto& pattern : effective_hosts_) {
    if (pattern.MatchesEffectiveTld()) {
      host_permissions_should_warn_all_hosts_ = WARN_ALL_HOSTS;
      break;
    }
  }
}

void PermissionSet::InitShouldWarnAllHostsForAPIPermissions() const {
  DCHECK_EQ(UNINITIALIZED, api_permissions_should_warn_all_hosts_);
  api_permissions_should_warn_all_hosts_ = DONT_WARN_ALL_HOSTS;

  for (const auto* api : apis_) {
    if (api->info()->implies_full_url_access()) {
      api_permissions_should_warn_all_hosts_ = WARN_ALL_HOSTS;
      break;
    }
  }
}

}  // namespace extensions
