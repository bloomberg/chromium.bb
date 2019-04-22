// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

class KeyedService;
class Profile;

namespace badging {

// The maximum value of badge contents before saturation occurs.
constexpr uint64_t kMaxBadgeContent = 99u;

// Determines the text to put on the badge based on some badge_content.
std::string GetBadgeString(base::Optional<uint64_t> badge_content);

// Maintains a record of badge contents and dispatches badge changes to a
// delegate.
class BadgeManager : public KeyedService {
 public:
  explicit BadgeManager(Profile* profile);
  ~BadgeManager() override;

  // Records badge contents for an app and notifies the delegate if the badge
  // contents have changed.
  void UpdateBadge(const extensions::ExtensionId&, base::Optional<uint64_t>);

  // Clears badge contents for an app (if existing) and notifies the delegate.
  void ClearBadge(const extensions::ExtensionId&);

  // Sets the delegate used for setting/clearing badges.
  void SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate);

 private:
  std::unique_ptr<BadgeManagerDelegate> delegate_;

  // Maps extension id to badge contents.
  std::map<extensions::ExtensionId, base::Optional<uint64_t>> badged_apps_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManager);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
