// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_

#include <memory>

namespace base {
class DictionaryValue;
}

std::unique_ptr<base::DictionaryValue> GetFileManagerStrings();

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_STRING_UTIL_H_
