// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_PERMISSION_OVERRIDES_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_PERMISSION_OVERRIDES_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

namespace content {

// Maintains permission overrides for each origin.
class CONTENT_EXPORT DevToolsPermissionOverrides {
 public:
  explicit DevToolsPermissionOverrides();
  ~DevToolsPermissionOverrides();
  DevToolsPermissionOverrides(DevToolsPermissionOverrides&& other);
  DevToolsPermissionOverrides& operator=(DevToolsPermissionOverrides&& other);

  DevToolsPermissionOverrides(const DevToolsPermissionOverrides&) = delete;
  DevToolsPermissionOverrides& operator=(const DevToolsPermissionOverrides&) =
      delete;

  using PermissionOverrides =
      base::flat_map<PermissionType, blink::mojom::PermissionStatus>;

  // Set permission override for |permission| at |origin| to |status|.
  // Null |origin| specifies global overrides.
  void Set(const base::Optional<url::Origin>& origin,
           PermissionType permission,
           const blink::mojom::PermissionStatus& status);

  // Get override for |origin| set for |permission|, if specified.
  base::Optional<blink::mojom::PermissionStatus> Get(
      const url::Origin& origin,
      PermissionType permission) const;

  // Get all overrides for particular |origin|, stored in |overrides|
  // if found. Will return empty overrides if none previously existed. Returns
  // global overrides when |origin| is nullptr.
  const PermissionOverrides& GetAll(
      const base::Optional<url::Origin>& origin) const;

  // Resets overrides for |origin|.
  // Null |origin| resets global overrides.
  void Reset(const base::Optional<url::Origin>& origin);

  // Sets status for |permissions| to GRANTED in |origin|, and DENIED
  // for all others.
  // Null |origin| grants permissions globally for context.
  void GrantPermissions(const base::Optional<url::Origin>& origin,
                        const std::vector<PermissionType>& permissions);

 private:
  url::Origin global_overrides_origin_;
  base::flat_map<url::Origin, PermissionOverrides> overrides_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_PERMISSION_OVERRIDES_H_
