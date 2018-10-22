// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

namespace {

// Returns a PermissionSet that has the active permissions of the extension,
// bounded to its current manifest.
std::unique_ptr<const PermissionSet> GetBoundedActivePermissions(
    const Extension* extension,
    const PermissionSet* active_permissions) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  if (!active_permissions)
    return extension->permissions_data()->active_permissions().Clone();

  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);

  // We restrict the active permissions to be within the bounds defined in the
  // extension's manifest.
  //  a) active permissions must be a subset of optional + default permissions
  //  b) active permissions must contains all default permissions
  std::unique_ptr<const PermissionSet> total_permissions =
      PermissionSet::CreateUnion(
          required_permissions,
          PermissionsParser::GetOptionalPermissions(extension));

  // Make sure the active permissions contain no more than optional + default.
  std::unique_ptr<const PermissionSet> adjusted_active =
      PermissionSet::CreateIntersection(*total_permissions,
                                        *active_permissions);

  // Make sure the active permissions contain the default permissions.
  adjusted_active =
      PermissionSet::CreateUnion(required_permissions, *adjusted_active);

  return adjusted_active;
}

PermissionsUpdater::Delegate* g_delegate = nullptr;

}  // namespace

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context)
    : browser_context_(browser_context), init_flag_(INIT_FLAG_NONE) {
}

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context,
                                       InitFlag init_flag)
    : browser_context_(browser_context), init_flag_(init_flag) {
}

PermissionsUpdater::~PermissionsUpdater() {}

// static
void PermissionsUpdater::SetPlatformDelegate(Delegate* delegate) {
  // Make sure we're setting it only once (allow setting to nullptr, but then
  // take special care of actually freeing it).
  CHECK(!g_delegate || !delegate);
  g_delegate = delegate;
}

void PermissionsUpdater::GrantOptionalPermissions(
    const Extension& extension,
    const PermissionSet& permissions) {
  // TODO(devlin): Ideally, we'd have this CHECK in place, but unit tests are
  // currently violating it.
  // CHECK(PermissionsParser::GetOptionalPermissions(&extension).Contains(
  //     permissions))
  //     << "Cannot add optional permissions that are not "
  //     << "specified in the manifest.";

  // Granted optional permissions are stored in both the granted permissions (so
  // we don't later disable the extension when we check the active permissions
  // against the granted set to determine if there's a permissions increase) and
  // the granted runtime permissions (so they don't get withheld with runtime
  // host permissions enabled). They're also added to the active set, which is
  // the permission set stored in preferences representing the extension's
  // currently-desired permission state.
  constexpr int permissions_store_mask =
      kActivePermissions | kGrantedPermissions | kRuntimeGrantedPermissions;
  AddPermissionsImpl(extension, permissions, permissions_store_mask,
                     permissions);
}

void PermissionsUpdater::GrantRuntimePermissions(
    const Extension& extension,
    const PermissionSet& permissions) {
  DCHECK(base::FeatureList::IsEnabled(features::kRuntimeHostPermissions));

  // We don't want to grant the extension object/process more privilege than it
  // requested, even if the user grants additional permission. For instance, if
  // the extension requests https://maps.google.com and the user grants
  // https://*.google.com, we only want to grant the extension itself
  // https://maps.google.com. Since we updated the prefs with the exact
  // granted permissions (*.google.com), if the extension later requests
  // increased permissions that are already covered, they will be auto-granted.

  // Determine which permissions to add to the extension.
  const PermissionSet& withheld =
      extension.permissions_data()->withheld_permissions();

  // We add the intersection of any permissions that were withheld and the
  // permissions that were granted. Since these might not be directly
  // overlapping, we need to use a detailed intersection behavior here.
  std::unique_ptr<const PermissionSet> active_permissions_to_add =
      PermissionSet::CreateIntersection(
          withheld, permissions,
          URLPatternSet::IntersectionBehavior::kDetailed);
  CHECK(extension.permissions_data()->withheld_permissions().Contains(
      *active_permissions_to_add))
      << "Cannot add runtime granted permissions that were not withheld.";

  // Adding runtime granted permissions does not add permissions to the
  // granted or active permissions store, so that behavior taken with the
  // runtime host permissions feature is confined to when the experiment is
  // enabled.
  constexpr int permissions_store_mask = kRuntimeGrantedPermissions;
  AddPermissionsImpl(extension, *active_permissions_to_add,
                     permissions_store_mask, permissions);
}

void PermissionsUpdater::RevokeOptionalPermissions(
    const Extension& extension,
    const PermissionSet& permissions,
    RemoveType remove_type) {
  // TODO(devlin): Ideally, we'd have this CHECK in place, but unit tests are
  // currently violating it.
  // CHECK(PermissionsParser::GetOptionalPermissions(&extension).Contains(
  //     permissions))
  //     << "Cannot remove optional permissions that are not "
  //     << "specified in the manifest.";

  // Revoked optional permissions are removed from granted and runtime-granted
  // permissions only if the user, and not the extension, removed them. This
  // allows the extension to add them again without prompting the user. They are
  // always removed from the active set, which is the set of permissions the
  // the extension currently requests.
  int permissions_store_mask = kActivePermissions;
  if (remove_type == REMOVE_HARD)
    permissions_store_mask |= kGrantedPermissions | kRuntimeGrantedPermissions;

  RemovePermissionsImpl(extension, permissions, permissions_store_mask,
                        permissions);
}

void PermissionsUpdater::RevokeRuntimePermissions(
    const Extension& extension,
    const PermissionSet& permissions) {
  DCHECK(base::FeatureList::IsEnabled(features::kRuntimeHostPermissions));
  // Similar to the process in adding permissions, we might be revoking more
  // permissions than the extension currently has explicit access to. For
  // instance, we might be revoking https://*.google.com/* even if the extension
  // only has https://maps.google.com/*.
  const PermissionSet& active =
      extension.permissions_data()->active_permissions();
  // Unlike adding permissions, we should know that any permissions we remove
  // are a superset of the permissions the extension has active (because we only
  // allow removal origins and the extension can't have a broader origin than
  // what it has granted).
  std::unique_ptr<const PermissionSet> active_permissions_to_remove =
      PermissionSet::CreateIntersection(
          active, permissions,
          URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth);
  // One exception: If we're revoking a permission like "<all_urls>", we need
  // to make sure it doesn't revoke the included chrome://favicon permission.
  std::set<URLPattern> removable_explicit_hosts;
  bool needs_adjustment = false;
  for (const auto& pattern : active_permissions_to_remove->explicit_hosts()) {
    if (pattern.host() == "favicon" && pattern.scheme() == "chrome")
      needs_adjustment = true;
    else
      removable_explicit_hosts.insert(pattern);
  }
  if (needs_adjustment) {
    // Tedious, because PermissionSets are const. :(
    active_permissions_to_remove = std::make_unique<PermissionSet>(
        active_permissions_to_remove->apis(),
        active_permissions_to_remove->manifest_permissions(),
        URLPatternSet(removable_explicit_hosts),
        active_permissions_to_remove->scriptable_hosts());
  }

  CHECK(extension.permissions_data()->active_permissions().Contains(
      *active_permissions_to_remove))
      << "Cannot remove permissions that are not active.";
  CHECK(GetRevokablePermissions(&extension)->Contains(permissions))
      << "Cannot remove non-revokable permissions.";

  // Removing runtime-granted permissions does not remove permissions from
  // the granted permissions store. This is done to ensure behavior taken with
  // the runtime host permissions feature is confined to when the experiment is
  // enabled. Similarly, since the runtime-granted permissions were never added
  // to the active permissions stored in prefs, they are also not removed.
  constexpr int permissions_store_mask = kRuntimeGrantedPermissions;
  RemovePermissionsImpl(extension, *active_permissions_to_remove,
                        permissions_store_mask, permissions);
}

void PermissionsUpdater::SetPolicyHostRestrictions(
    const Extension* extension,
    const URLPatternSet& runtime_blocked_hosts,
    const URLPatternSet& runtime_allowed_hosts) {
  extension->permissions_data()->SetPolicyHostRestrictions(
      runtime_blocked_hosts, runtime_allowed_hosts);

  // Send notification to the currently running renderers of the runtime block
  // hosts settings.
  const PermissionSet perms;
  NotifyPermissionsUpdated(POLICY, extension, perms);
}

void PermissionsUpdater::SetUsesDefaultHostRestrictions(
    const Extension* extension) {
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  const PermissionSet perms;
  NotifyPermissionsUpdated(POLICY, extension, perms);
}

void PermissionsUpdater::SetDefaultPolicyHostRestrictions(
    const URLPatternSet& default_runtime_blocked_hosts,
    const URLPatternSet& default_runtime_allowed_hosts) {
  PermissionsData::SetDefaultPolicyHostRestrictions(
      default_runtime_blocked_hosts, default_runtime_allowed_hosts);

  // Send notification to the currently running renderers of the runtime block
  // hosts settings.
  NotifyDefaultPolicyHostRestrictionsUpdated(default_runtime_blocked_hosts,
                                             default_runtime_allowed_hosts);
}

void PermissionsUpdater::RemovePermissionsUnsafe(
    const Extension* extension,
    const PermissionSet& to_remove) {
  const PermissionSet& active =
      extension->permissions_data()->active_permissions();
  std::unique_ptr<const PermissionSet> total =
      PermissionSet::CreateDifference(active, to_remove);
  // |successfully_removed| might not equal |to_remove| if |to_remove| contains
  // permissions the extension didn't have.
  std::unique_ptr<const PermissionSet> successfully_removed =
      PermissionSet::CreateDifference(active, *total);

  // TODO(devlin): This seems wrong. Since these permissions are being removed
  // by enterprise policy, we should not update the active permissions set in
  // preferences. That way, if the enterprise policy is changed, the removed
  // permissions would be re-added.
  constexpr bool update_active_prefs = true;
  SetPermissions(extension, std::move(total), update_active_prefs);
  NotifyPermissionsUpdated(REMOVED, extension, *successfully_removed);
}

std::unique_ptr<const PermissionSet>
PermissionsUpdater::GetRevokablePermissions(const Extension* extension) const {
  // Any permissions not required by the extension are revokable.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  std::unique_ptr<const PermissionSet> revokable_permissions =
      PermissionSet::CreateDifference(
          extension->permissions_data()->active_permissions(), required);

  // Additionally, some required permissions may be revokable if they can be
  // withheld by the ScriptingPermissionsModifier.
  std::unique_ptr<const PermissionSet> revokable_scripting_permissions =
      ScriptingPermissionsModifier(browser_context_,
                                   base::WrapRefCounted(extension))
          .GetRevokablePermissions();

  if (revokable_scripting_permissions) {
    revokable_permissions = PermissionSet::CreateUnion(
        *revokable_permissions, *revokable_scripting_permissions);
  }
  return revokable_permissions;
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  ExtensionPrefs::Get(browser_context_)
      ->AddGrantedPermissions(
          extension->id(), extension->permissions_data()->active_permissions());
}

void PermissionsUpdater::InitializePermissions(const Extension* extension) {
  std::unique_ptr<const PermissionSet> bounded_wrapper;
  const PermissionSet* bounded_active = nullptr;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  // If |extension| is a transient dummy extension, we do not want to look for
  // it in preferences.
  if (init_flag_ & INIT_FLAG_TRANSIENT) {
    bounded_active = &extension->permissions_data()->active_permissions();
  } else {
    std::unique_ptr<const PermissionSet> active_permissions =
        prefs->GetActivePermissions(extension->id());
    bounded_wrapper =
        GetBoundedActivePermissions(extension, active_permissions.get());
    bounded_active = bounded_wrapper.get();
  }

  std::unique_ptr<const PermissionSet> granted_permissions;
  ScriptingPermissionsModifier::WithholdPermissionsIfNecessary(
      *extension, *prefs, *bounded_active, &granted_permissions);

  if (g_delegate)
    g_delegate->InitializePermissions(extension, &granted_permissions);

  bool update_active_permissions = false;
  if ((init_flag_ & INIT_FLAG_TRANSIENT) == 0) {
    update_active_permissions = true;
    // Apply per-extension policy if set.
    ExtensionManagement* management =
        ExtensionManagementFactory::GetForBrowserContext(browser_context_);
    if (!management->UsesDefaultPolicyHostRestrictions(extension)) {
      SetPolicyHostRestrictions(extension,
                                management->GetPolicyBlockedHosts(extension),
                                management->GetPolicyAllowedHosts(extension));
    }
  }

  SetPermissions(extension, std::move(granted_permissions),
                 update_active_permissions);
}

void PermissionsUpdater::AddPermissionsForTesting(
    const Extension& extension,
    const PermissionSet& permissions) {
  AddPermissionsImpl(extension, permissions, kNone, permissions);
}

void PermissionsUpdater::SetPermissions(
    const Extension* extension,
    std::unique_ptr<const PermissionSet> new_active,
    bool update_prefs) {
  // Calculate the withheld permissions as any permissions that were required,
  // but are not in the active set.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  // TODO(https://crbug.com/869403): Currently, withheld permissions should only
  // contain permissions withheld by the runtime host permissions feature.
  // However, there could possibly be API permissions that were removed from the
  // active set by enterprise policy. These shouldn't go in the withheld
  // permission set, since withheld permissions are generally supposed to be
  // grantable. Currently, we can deal with this because all permissions
  // withheld by runtime host permissions are explicit or scriptable hosts, and
  // all permissions blocked by enterprise are API permissions. So to get the
  // set of runtime-hosts-withheld permissions, we just look at the delta in the
  // URLPatternSets. However, this is very fragile, and should be dealt with
  // more robustly.
  std::unique_ptr<const PermissionSet> new_withheld =
      PermissionSet::CreateDifference(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        required.explicit_hosts(), required.scriptable_hosts()),
          *new_active);

  extension->permissions_data()->SetPermissions(std::move(new_active),
                                                std::move(new_withheld));

  if (update_prefs) {
    ExtensionPrefs::Get(browser_context_)
        ->SetActivePermissions(
            extension->id(),
            extension->permissions_data()->active_permissions());
  }
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const char* event_name,
    const PermissionSet& changed_permissions) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  std::unique_ptr<base::ListValue> value(new base::ListValue());
  std::unique_ptr<api::permissions::Permissions> permissions =
      PackPermissionSet(changed_permissions);
  value->Append(permissions->ToValue());
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(value), browser_context_);
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void PermissionsUpdater::NotifyPermissionsUpdated(
    EventType event_type,
    const Extension* extension,
    const PermissionSet& changed) {
  DCHECK_EQ(0, init_flag_ & INIT_FLAG_TRANSIENT);

  if (changed.IsEmpty() && event_type != POLICY)
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = NULL;
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    histogram_value = events::PERMISSIONS_ON_REMOVED;
    event_name = permissions::OnRemoved::kEventName;
  } else if (event_type == ADDED) {
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    histogram_value = events::PERMISSIONS_ON_ADDED;
    event_name = permissions::OnAdded::kEventName;
  } else {
    DCHECK_EQ(POLICY, event_type);
    reason = UpdatedExtensionPermissionsInfo::POLICY;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info =
      UpdatedExtensionPermissionsInfo(extension, changed, reason);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(profile),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  ExtensionMsg_UpdatePermissions_Params params;
  params.extension_id = extension->id();
  params.active_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->active_permissions());
  params.withheld_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->withheld_permissions());
  params.uses_default_policy_host_restrictions =
      extension->permissions_data()->UsesDefaultPolicyHostRestrictions();
  if (!params.uses_default_policy_host_restrictions) {
    params.policy_blocked_hosts =
        extension->permissions_data()->policy_blocked_hosts();
    params.policy_allowed_hosts =
        extension->permissions_data()->policy_allowed_hosts();
  }

  // Send the new permissions to the renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    if (profile->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_UpdatePermissions(params));
    }
  }

  // Trigger the onAdded and onRemoved events in the extension. We explicitly
  // don't do this for policy-related events.
  if (event_name)
    DispatchEvent(extension->id(), histogram_value, event_name, changed);
}

// Notify the renderers that extension policy (policy_blocked_hosts) is updated
// and provide new set of hosts.
void PermissionsUpdater::NotifyDefaultPolicyHostRestrictionsUpdated(
    const URLPatternSet& default_runtime_blocked_hosts,
    const URLPatternSet& default_runtime_allowed_hosts) {
  DCHECK_EQ(0, init_flag_ & INIT_FLAG_TRANSIENT);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params params;
  params.default_policy_blocked_hosts = default_runtime_blocked_hosts;
  params.default_policy_allowed_hosts = default_runtime_allowed_hosts;

  // Send the new policy to the renderers.
  for (RenderProcessHost::iterator host_iterator(
           RenderProcessHost::AllHostsIterator());
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (profile->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_UpdateDefaultPolicyHostRestrictions(params));
    }
  }
}

void PermissionsUpdater::AddPermissionsImpl(
    const Extension& extension,
    const PermissionSet& active_permissions_to_add,
    int permissions_store_mask,
    const PermissionSet& prefs_permissions_to_add) {
  std::unique_ptr<const PermissionSet> new_active = PermissionSet::CreateUnion(
      active_permissions_to_add,
      extension.permissions_data()->active_permissions());

  bool update_active_prefs = (permissions_store_mask & kActivePermissions) != 0;
  SetPermissions(&extension, std::move(new_active), update_active_prefs);

  if ((permissions_store_mask & kGrantedPermissions) != 0) {
    // TODO(devlin): Could we only grant |permissions|, rather than all those
    // in the active permissions? In theory, all other active permissions have
    // already been granted.
    GrantActivePermissions(&extension);
  }

  if ((permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    ExtensionPrefs::Get(browser_context_)
        ->AddRuntimeGrantedPermissions(extension.id(),
                                       prefs_permissions_to_add);
  }

  NotifyPermissionsUpdated(ADDED, &extension, active_permissions_to_add);
}

void PermissionsUpdater::RemovePermissionsImpl(
    const Extension& extension,
    const PermissionSet& active_permissions_to_remove,
    int permissions_store_mask,
    const PermissionSet& prefs_permissions_to_remove) {
  std::unique_ptr<const PermissionSet> new_active =
      PermissionSet::CreateDifference(
          extension.permissions_data()->active_permissions(),
          active_permissions_to_remove);

  bool update_active_prefs = (permissions_store_mask & kActivePermissions) != 0;
  SetPermissions(&extension, std::move(new_active), update_active_prefs);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  // NOTE: Currently, this code path is only reached in unit tests. See comment
  // above REMOVE_HARD in the header file.
  if ((permissions_store_mask & kGrantedPermissions) != 0) {
    prefs->RemoveGrantedPermissions(extension.id(),
                                    prefs_permissions_to_remove);
  }

  if ((permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    prefs->RemoveRuntimeGrantedPermissions(extension.id(),
                                           prefs_permissions_to_remove);
  }

  NotifyPermissionsUpdated(REMOVED, &extension, active_permissions_to_remove);
}

}  // namespace extensions
