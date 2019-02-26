// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_HELPERS_H_
#define CHROME_BROWSER_POLICY_POLICY_HELPERS_H_

class PrefRegistrySimple;

namespace policy {

// Register policy related preferences in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_HELPERS_H_
