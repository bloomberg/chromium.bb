// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_HEALTH_RESOURCE_PROVIDER_H_
#define CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_HEALTH_RESOURCE_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace network_health {

// Adds the resources needed for network health elements to |html_source|.
void AddResources(content::WebUIDataSource* html_source);

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NETWORK_UI_NETWORK_HEALTH_RESOURCE_PROVIDER_H_
