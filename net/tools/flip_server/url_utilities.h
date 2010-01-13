// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__
#define NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__

#include <string>

namespace net {

struct UrlUtilities {
  // Get the host from an url
  static std::string GetUrlHost(const std::string& url) {
    size_t b = url.find("//");
    if (b == std::string::npos)
      b = 0;
    else
      b += 2;
    size_t next_slash = url.find_first_of('/', b);
    size_t next_colon = url.find_first_of(':', b);
    if (next_slash != std::string::npos
        && next_colon != std::string::npos
        && next_colon < next_slash) {
      return std::string(url, b, next_colon - b);
    }
    if (next_slash == std::string::npos) {
      if (next_colon != std::string::npos) {
        return std::string(url, next_colon - b);
      } else {
        next_slash = url.size();
      }
    }
    return std::string(url, b, next_slash - b);
  }

  // Get the host + path portion of an url
  // e.g   http://www.foo.com/path
  //       returns www.foo.com/path
  static std::string GetUrlHostPath(const std::string& url) {
    size_t b = url.find("//");
    if (b == std::string::npos)
      b = 0;
    else
      b += 2;
    return std::string(url, b);
  }

  // Get the path portion of an url
  // e.g   http://www.foo.com/path
  //       returns /path
  static std::string GetUrlPath(const std::string& url) {
    size_t b = url.find("//");
    if (b == std::string::npos)
      b = 0;
    else
      b += 2;
    b = url.find("/", b+1);
    if (b == std::string::npos)
      return "/";

    return std::string(url, b);
  }
};

} // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_URL_UTILITIES_H__

