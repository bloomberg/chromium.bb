// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_UTIL_H_

#include <memory>

class GURL;

namespace base {
class FilePath;
}

namespace chromeos {
namespace customization_wallpaper_util {

// First checks if the file paths exist for both large and small sizes, then
// calls |SetCustomizedDefaultWallpaperAfterCheck| with |both_sizes_exist|.
void StartSettingCustomizedDefaultWallpaper(const GURL& wallpaper_url,
                                            const base::FilePath& file_path);

// Gets the file paths of both small and large sizes of the customized default
// wallpaper. Returns true on success.
bool GetCustomizedDefaultWallpaperPaths(base::FilePath* small_path_out,
                                        base::FilePath* large_path_out);

// Whether customized default wallpaper should be used wherever a default
// wallpaper is needed.
bool ShouldUseCustomizedDefaultWallpaper();

}  // namespace customization_wallpaper_util
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_CUSTOMIZATION_WALLPAPER_UTIL_H_
