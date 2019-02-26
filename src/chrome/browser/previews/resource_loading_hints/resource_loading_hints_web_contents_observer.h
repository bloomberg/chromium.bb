// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

// Observes navigation events and sends the resource loading hints mojo message
// to the renderer.
class ResourceLoadingHintsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ResourceLoadingHintsWebContentsObserver> {
 public:
  ~ResourceLoadingHintsWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<
      ResourceLoadingHintsWebContentsObserver>;

  explicit ResourceLoadingHintsWebContentsObserver(
      content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver. If the navigation is of type
  // resource loading hints preview, then this method sends the resource loading
  // hints mojo message to the renderer.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Sends resource loading hints to the renderer.
  void SendResourceLoadingHints(content::NavigationHandle* navigation_handle,
                                bool is_reload) const;

  // Returns the pattern of resources that should be blocked when loading
  // |document_gurl|. The pattern may be a single substring to match against the
  // URL or it may be an ordered set of substrings to match where the substrings
  // are separated by the ‘*’ wildcard character (with an implicit ‘*’ at the
  // beginning and end).
  const std::vector<std::string> GetResourceLoadingHintsResourcePatternsToBlock(
      const GURL& document_gurl) const;

  // Set in constructor.
  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsWebContentsObserver);
};

#endif  // CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
