// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_
#define CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_

#include "components/favicon/content/content_favicon_driver.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace favicon {

// Creates a ContentFaviconDriver and associates it with |web_contents| if none
// exists yet.
//
// This is a helper method for ContentFaviconDriver::CreateForWebContents() that
// gets KeyedService factories from the Profile linked to web_contents.
void CreateContentFaviconDriverForWebContents(
    content::WebContents* web_contents);

// Generates a monogram in a colored circle. The color is based on the hash
// of the url and the monogram is the first letter of the URL domain.
SkBitmap GenerateMonogramFavicon(GURL url, int icon_size, int circle_size);

// Retrieves the favicon from given WebContents. If contents contain a
// network error, desaturate the favicon.
gfx::Image TabFaviconFromWebContents(content::WebContents* contents);

// Returns the image to use when no favicon is available, taking dark mode
// into account if necessary.
gfx::Image GetDefaultFavicon();

}  // namespace favicon

#endif  // CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_
