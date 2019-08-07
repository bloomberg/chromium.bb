// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_APP_ORDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_APP_ORDER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"

namespace chromeos {
namespace default_app_order {

// ExternalLoader checks FILE_DEFAULT_APP_ORDER and loads it if the file
// exists. Otherwise, it uses the default built-in order. The file loading runs
// asynchronously on start up except for the browser restart path, in which
// case, start up will wait for the file check to finish because user profile
// might need to access the ordinals data.
class ExternalLoader {
 public:
  // Constructs an ExternalLoader and starts file loading. |async| is true to
  // load the file asynchronously on the blocking pool.
  explicit ExternalLoader(bool async);
  ~ExternalLoader();

  const std::vector<std::string>& GetAppIds();
  const std::string& GetOemAppsFolderName();

 private:
  void Load();

  // A vector of app id strings that defines the default order of apps.
  std::vector<std::string> app_ids_;

  std::string oem_apps_folder_name_;

  base::WaitableEvent loaded_;

  DISALLOW_COPY_AND_ASSIGN(ExternalLoader);
};

// Gets the ordered list of app ids.
void Get(std::vector<std::string>* app_ids);

// Get the name of OEM apps folder in app launcher.
std::string GetOemAppsFolderName();

// Number of apps in hard-coded apps order.
extern const size_t kDefaultAppOrderCount;

}  // namespace default_app_order
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_APP_ORDER_H_
