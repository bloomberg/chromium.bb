// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_
#define IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_

// Contains keys present in dictionary created by __gCrWeb.findElementAtPoint
// to represent the DOM element.

namespace web {

// Required key. Represents a unique string request ID that is passed through
// directly from a call to findElementAtPoint to the response dictionary. The
// request ID should be used to correlate a response with a previous call to
// findElementAtPoint.
extern const char kContextMenuElementRequestId[];

// Optional key. Represents element's href attribute if present or parent's href
// if element is an image.
extern const char kContextMenuElementHyperlink[];

// Optional key. Represents element's src attribute if present (<img> element
// only).
extern const char kContextMenuElementSource[];

// Optional key. Represents element's title attribute if present (<img> element
// only).
extern const char kContextMenuElementTitle[];

// Optional key. Represents referrer policy to use for navigations away from the
// current page. Key is present if |kContextMenuElementError| is |NO_ERROR|.
extern const char kContextMenuElementReferrerPolicy[];

// Optional key. Represents element's innerText attribute if present (<a>
// elements with href only).
extern const char kContextMenuElementInnerText[];

// Optional key. Represents element's alt attribute if present (<img> element
// only).
extern const char kContextMenuElementAlt[];

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_
