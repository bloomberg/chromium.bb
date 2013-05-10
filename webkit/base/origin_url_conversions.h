// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BASE_ORIGIN_URL_CONVERSIONS_H_
#define WEBKIT_BASE_ORIGIN_URL_CONVERSIONS_H_

#include "base/string16.h"
#include "webkit/base/webkit_base_export.h"

class GURL;

namespace webkit_base {

// Returns the origin url for the given |identifier| and vice versa.
// The origin identifier string is a serialized form of a security origin
// and can be used as a path name as it contains no "/" or other possibly
// unsafe characters. (See WebKit's SecurityOrigin code for more details.)
//
// Example:
//   "http://www.example.com:80/"'s identifier should look like:
//   "http_www.example.host_80"
WEBKIT_BASE_EXPORT GURL GetOriginURLFromIdentifier(
    const base::string16& identifier);

WEBKIT_BASE_EXPORT base::string16 GetOriginIdentifierFromURL(const GURL& url);

}  // namespace webkit_base

#endif  // WEBKIT_BASE_ORIGIN_URL_CONVERSIONS_H_
