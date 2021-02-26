// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_PREVIEWS_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_PREVIEWS_STATE_H_

namespace blink {

typedef int PreviewsState;

// The Previews types which determines whether to request a Preview version of
// the resource. Previews are optimizations that change the format and
// content of web pages to improve data savings and / or performance. This enum
// determines which Previews types to request.
// Deprecated values should be commented out and not reused since this bitmask
// is persisted on disk.
// TODO(nhiroki): Remove snake-case enum values (e.g., PREVIEWS_UNSPECIFIED) in
// favor of camel-case enum values (e.g., kPreviewsUnspecified).
enum PreviewsTypes {
  PREVIEWS_UNSPECIFIED = 0,  // Let the browser process decide whether or
                             // not to request Preview types.
  kPreviewsUnspecified = PREVIEWS_UNSPECIFIED,

  // DEPRECATED: SERVER_LOFI_ON = 1 << 0, Request a Lo-Fi version of the
  // resource from the server. This preview type has been deprecated and should
  // no longer be used.
  // DEPRECATED: CLIENT_LOFI_ON = 1 << 1, Request a Lo-Fi version of the
  // resource from the client. This preview type has been deprecated and should
  // no longer be used.
  CLIENT_LOFI_AUTO_RELOAD = 1 << 2,  // Request the original version of the
                                     // resource after a decoding error occurred
                                     // when attempting to use Client Lo-Fi.
  kClientLoFiAutoReload = CLIENT_LOFI_AUTO_RELOAD,

  SERVER_LITE_PAGE_ON = 1 << 3,  // Request a Lite Page version of the
                                 // resource from the server.
  kServiceLitePageOn = SERVER_LITE_PAGE_ON,

  PREVIEWS_NO_TRANSFORM = 1 << 4,  // Explicitly forbid Previews
                                   // transformations.
  kPreviewsNoTransform = PREVIEWS_NO_TRANSFORM,

  PREVIEWS_OFF = 1 << 5,  // Request a normal (non-Preview) version of
                          // the resource. Server transformations may
                          // still happen if the page is heavy.
  kPreviewsOff = PREVIEWS_OFF,

  NOSCRIPT_ON = 1 << 6,  // Request that script be disabled for page load.
  kNoScriptOn = NOSCRIPT_ON,

  RESOURCE_LOADING_HINTS_ON =
      1 << 7,  // Request that resource loading hints be used during pageload.
  kResourceLoadingHintsOn = RESOURCE_LOADING_HINTS_ON,

  OFFLINE_PAGE_ON =
      1 << 8,  // Request that an offline page be used if one is stored.
  kOfflinePageOn = OFFLINE_PAGE_ON,

  // DEPRECATED: LITE_PAGE_REDIRECT_ON = 1 << 9,  // Allow the browser to
  // redirect the resource to a Lite Page server. Support for this functionality
  // has been removed.
  DEFER_ALL_SCRIPT_ON = 1 << 10,  // Request that script execution be deferred
                                  // until parsing completes.
  kDeferAllScriptOn = DEFER_ALL_SCRIPT_ON,

  SUBRESOURCE_REDIRECT_ON =
      1 << 11,  // Allow the subresources in the page to be redirected to
                // serve better optimized resources. Set on subresources.
  kSubresourceRedirectOn = SUBRESOURCE_REDIRECT_ON,

  PREVIEWS_STATE_LAST = SUBRESOURCE_REDIRECT_ON,
  kPreviewsStateLast = PREVIEWS_STATE_LAST
};

// Combination of all previews that are guaranteed not to provide partial
// content.
// const PreviewsState PARTIAL_CONTENT_SAFE_PREVIEWS = SERVER_LOFI_ON;
// deprecated

// Combination of all currently supported previews.
const PreviewsState ALL_SUPPORTED_PREVIEWS = SERVER_LITE_PAGE_ON | NOSCRIPT_ON |
                                             RESOURCE_LOADING_HINTS_ON |
                                             OFFLINE_PAGE_ON;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_PREVIEWS_STATE_H_
