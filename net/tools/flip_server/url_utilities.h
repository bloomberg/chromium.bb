// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__
#define NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__

#include <string>

namespace net {

struct UrlUtilities {
  // Get the host from an url
  static string GetUrlHost(const string& url) {
    size_t b = url.find("//");
    if (b == string::npos)
      b = 0;
    else
      b += 2;
    size_t next_slash = url.find_first_of('/', b);
    size_t next_colon = url.find_first_of(':', b);
    if (next_slash != string::npos
        && next_colon != string::npos
        && next_colon < next_slash) {
      return string(url, b, next_colon - b);
    }
    if (next_slash == string::npos) {
      if (next_colon != string::npos) {
        return string(url, next_colon - b);
      } else {
        next_slash = url.size();
      }
    }
    return string(url, b, next_slash - b);
  }

  // Get the host + path portion of an url
  // e.g   http://www.foo.com/path
  //       returns www.foo.com/path
  static string GetUrlHostPath(const string& url) {
    size_t b = url.find("//");
    if (b == string::npos)
      b = 0;
    else
      b += 2;
    return string(url, b);
  }

  // Get the path portion of an url
  // e.g   http://www.foo.com/path
  //       returns /path
  static string GetUrlPath(const string& url) {
    size_t b = url.find("//");
    if (b == string::npos)
      b = 0;
    else
      b += 2;
    b = url.find("/", b+1);
    if (b == string::npos)
      return "/";

    return string(url, b);
  }
};

} // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__

