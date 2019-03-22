// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_extensions.h"

namespace chrome_cleaner {

TestRegistryEntry::TestRegistryEntry(HKEY hkey,
                                     const base::string16& path,
                                     const base::string16& name,
                                     const base::string16& value)
    : hkey(hkey), path(path), name(name), value(value) {}
TestRegistryEntry::TestRegistryEntry(const TestRegistryEntry& other) = default;
TestRegistryEntry& TestRegistryEntry::operator=(
    const TestRegistryEntry& other) = default;

}  // namespace chrome_cleaner
