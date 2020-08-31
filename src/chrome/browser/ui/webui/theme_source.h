// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/url_data_source.h"

class Profile;

class ThemeSource : public content::URLDataSource {
 public:
  explicit ThemeSource(Profile* profile);
  ThemeSource(Profile* profile, bool serve_untrusted);
  ~ThemeSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;

 private:
  // Fetches and sends the theme bitmap.
  void SendThemeBitmap(content::URLDataSource::GotDataCallback callback,
                       int resource_id,
                       float scale);

  // Used in place of SendThemeBitmap when the desired scale is larger than
  // what the resource bundle supports.  This can rescale the provided bitmap up
  // to the desired size.
  void SendThemeImage(content::URLDataSource::GotDataCallback callback,
                      int resource_id,
                      float scale);

  // The profile this object was initialized with.
  Profile* profile_;

  // Whether this source services chrome-unstrusted://theme.
  bool serve_untrusted_;

  DISALLOW_COPY_AND_ASSIGN(ThemeSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_
