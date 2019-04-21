// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_VIDEO_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_VIDEO_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {

// Data source that reads videos from the file system.
// It provides resources from chrome urls of type:
//   chrome://chromeos-asset/<whitelisted directories>/*.webm
class VideoSource : public content::URLDataSource {
 public:
  VideoSource();
  ~VideoSource() override;

  // content::URLDataSource:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& got_data_callback)
      override;
  std::string GetMimeType(const std::string& path) const override;

 private:
  // Continuation from StartDataRequest().
  void StartDataRequestAfterPathExists(
      const base::FilePath& video_path,
      const content::URLDataSource::GotDataCallback& got_data_callback,
      bool path_exists);

  // The background task runner on which file I/O is performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<VideoSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_VIDEO_SOURCE_H_
