// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_

#include <string>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace arc {

class ArcIntentHelperObserver {
 public:
  virtual ~ArcIntentHelperObserver() = default;
  // Called when a new entry has been added to the MediaStore.Downloads
  // collection of downloaded items in ARC with the specified metadata.
  // |relative_path|      relative path of the download within the Download/
  //                      folder (e.g. "Download/foo/bar.pdf").
  // |owner_package_name| package name that contributed the download (e.g.
  //                      "com.bar.foo").
  virtual void OnArcDownloadAdded(const base::FilePath& relative_path,
                                  const std::string& owner_package_name) {}
  // Called when intent filters are added, removed or updated.
  // A absl::nullopt |package_name| indicates that intent filters were updated
  // for all packages. Otherwise, |package_name| contains the name of the
  // package whose filters were changed.
  virtual void OnIntentFiltersUpdated(
      const absl::optional<std::string>& package_name) {}

  // Called when the supported links setting ("Open Supported Links" under
  // "Open by default" in ARC Settings) is changed for one or more packages.
  // |added_packages| contains packages for which the setting was enabled,
  // |removed_packages| contains packages for which the setting was disabled.
  virtual void OnArcSupportedLinksChanged(
      const std::vector<arc::mojom::SupportedLinksPtr>& added_packages,
      const std::vector<arc::mojom::SupportedLinksPtr>& removed_packages,
      arc::mojom::SupportedLinkChangeSource source) {}

  virtual void OnIconInvalidated(const std::string& package_name) {}

  // Called when ArcIntentHelperBridge is shut down.
  virtual void OnArcIntentHelperBridgeShutdown() {}
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
