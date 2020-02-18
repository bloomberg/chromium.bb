// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PIXEL_SKIA_GOLD_PIXEL_DIFF_H_
#define CHROME_TEST_PIXEL_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "ui/gfx/native_widget_types.h"

class BrowserWindow;
class SkBitmap;

namespace views {
class View;
}

namespace gfx {
class Rect;
class Image;
}

// This is the utility class for Skia Gold pixeltest.
// For an example on how to write pixeltests, please refer to the demo.
class SkiaGoldPixelDiff {
 public:
  SkiaGoldPixelDiff();
  ~SkiaGoldPixelDiff();
  // Call Init method before using this class.
  // Args:
  // browser The Browser instance you plan to take screenshots with.
  // screenshot_prefix The prefix for your screenshot name on GCS.
  //   For every screenshot you take, it should have a unique name
  //   across Chromium, because all screenshots (aka golden images) stores
  //   in one bucket on GCS. The standard convention is to use the browser
  //   test class name as the prefix. The name will be
  //   |screenshot_prefix| + "_" + |screenshot_name|.'
  //   E.g. 'ToolbarTest_BackButtonHover'.
  void Init(BrowserWindow* browser,
       const std::string& screenshot_prefix);

  // Take a screenshot, upload to Skia Gold and compare with the remote
  // golden image. Returns true if the screenshot is the same as the golden
  // image (compared with hashcode).
  // Args:
  // screenshot_name Make sure |screenshot_prefix| + "_" + |screenshot_name|
  //                 is unique.
  // view The view you want to take screenshot. If the screen is not what
  //      you want, you can use the other method.
  bool CompareScreenshot(const std::string& screenshot_name,
                      const views::View* view);

  // Using this method is discouraged.
  // Only use this method when the screen you need is not a views::View.
  bool CompareScreenshot(const std::string& screenshot_name,
                      const SkBitmap& bitmap);

 protected:
  // Upload the local file to Skia Gold server. Return true if the screenshot
  // is the same as the remote golden image.
  virtual bool UploadToSkiaGoldServer(const base::FilePath& local_file_path,
                              const std::string& remote_golden_image_name);

  virtual bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image);
  void InitSkiaGold();
  virtual int LaunchProcess(base::CommandLine::StringType& cmdline);

 private:
  std::string prefix_;
  BrowserWindow* browser_;
  bool initialized_ = false;
  // Use luci auth on bots. Don't use luci auth for local development.
  bool luci_auth_ = true;
  std::string build_revision_;
  std::string issue_;
  std::string patchset_;
  std::string job_id_;
  // The working dir for goldctl. It's the dir for storing temporary files.
  base::FilePath working_dir_;
};

#endif  // CHROME_TEST_PIXEL_SKIA_GOLD_PIXEL_DIFF_H_
