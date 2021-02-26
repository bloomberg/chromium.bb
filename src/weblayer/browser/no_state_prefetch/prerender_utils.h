// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_UTILS_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_UTILS_H_

namespace content {
class WebContents;
}

namespace prerender {
class PrerenderContents;
}

namespace weblayer {

prerender::PrerenderContents* PrerenderContentsFromWebContents(
    content::WebContents* web_contents);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_UTILS_H_
