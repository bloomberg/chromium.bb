// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#endif

using content::RenderFrameHost;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace tab_util {

content::WebContents* GetWebContentsByID(int render_process_id,
                                         int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;
  return WebContents::FromRenderViewHost(render_view_host);
}

content::WebContents* GetWebContentsByFrameID(int render_process_id,
                                              int render_frame_id) {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return NULL;
  return WebContents::FromRenderFrameHost(render_frame_host);
}

scoped_refptr<SiteInstance> GetSiteInstanceForNewTab(Profile* profile,
                                                     GURL url) {
  // Rewrite the |url| if necessary, to ensure that the SiteInstance is
  // associated with a |url| that will actually be loaded.  For example,
  // |url| set to chrome://newtab/ might actually result in a navigation to a
  // different URL like chrome-search://local-ntp/local-ntp.html
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(&url,
                                                                   profile);

  // If |url| is a WebUI or extension, we set the SiteInstance up front so that
  // we don't end up with an extra process swap on the first navigation.
  if (ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(profile, url))
    return SiteInstance::CreateForURL(profile, url);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetHostedAppByURL(url))
    return SiteInstance::CreateForURL(profile, url);
#endif

  // We used to share the SiteInstance for same-site links opened in new tabs,
  // to leverage the in-memory cache and reduce process creation.  It now
  // appears that it is more useful to have such links open in a new process,
  // so we create new tabs in a new BrowsingInstance.
  // Create a new SiteInstance for the |url| unless it is not desirable.
  if (!SiteInstance::ShouldAssignSiteForURL(url))
    return nullptr;

  return SiteInstance::CreateForURL(profile, url);
}

}  // namespace tab_util
