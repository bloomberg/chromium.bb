// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_

constexpr char kIsHistoryClustersVisibleKey[] = "isHistoryClustersVisible";
constexpr char kIsHistoryClustersVisibleManagedByPolicyKey[] =
    "isHistoryClustersVisibleManagedByPolicy";

class Profile;

namespace content {
class WebUIDataSource;
}

class HistoryClustersUtil {
 public:
  static void PopulateSource(content::WebUIDataSource* source,
                             Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_
