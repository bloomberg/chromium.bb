// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_
#define WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_

#include "base/string16.h"

class FilePath;

namespace delegate_execute {

// Finalizes a previously updated installation.
void UpdateChromeIfNeeded(const FilePath& chrome_exe);

// Returns the appid of the Chrome pointed to by |chrome_exe|.
string16 GetAppId(const FilePath& chrome_exe);

}  // namespace delegate_execute

#endif  // WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_
