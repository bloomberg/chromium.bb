// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "content/public/common/mhtml_generation_params.h"

namespace content {

class WebContents;

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
//
// MHTMLGenerationManager is a singleton.  Each call to SaveMHTML method creates
// a new instance of MHTMLGenerationManager::Job that continues with the MHTML
// serialization process on its own, eventually deleting itself.
class MHTMLGenerationManager {
 public:
  static MHTMLGenerationManager* GetInstance();

  // GenerateMHTMLCallback is called to report completion and status of MHTML
  // generation.  On success |file_size| indicates the size of the
  // generated file.  On failure |file_size| is -1.
  using GenerateMHTMLCallback = base::OnceCallback<void(int64_t file_size)>;

  // Instructs the RenderFrames in |web_contents| to generate a MHTML
  // representation of the current page.
  void SaveMHTML(WebContents* web_contents,
                 const MHTMLGenerationParams& params,
                 GenerateMHTMLCallback callback);

 private:
  friend struct base::DefaultSingletonTraits<MHTMLGenerationManager>;
  class Job;

  MHTMLGenerationManager();
  virtual ~MHTMLGenerationManager();

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
