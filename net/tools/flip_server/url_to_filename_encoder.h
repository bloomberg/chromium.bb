// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_URL_TO_FILE_ENCODER_H__
#define NET_TOOLS_FLIP_SERVER_URL_TO_FILE_ENCODER_H__

#include <string>
#include "net/tools/flip_server/url_utilities.h"

namespace net {

// Helper class for converting a URL into a filename.
class UrlToFilenameEncoder {
 public:
  // Given a |url| and a |base_path|, returns a string which represents this
  // |url|.
  static std::string Encode(const std::string& url, std::string base_path) {
    std::string clean_url(url);
    if (clean_url.length() && clean_url[clean_url.length()-1] == '/')
      clean_url.append("index.html");

    std::string host = UrlUtilities::GetUrlHost(clean_url);
    std::string filename(base_path);
    filename = filename.append(host + "/");

    std::string url_filename = UrlUtilities::GetUrlPath(clean_url);
    // Strip the leading '/'
    if (url_filename[0] == '/')
      url_filename = url_filename.substr(1);

    // replace '/' with '\'
    ConvertToSlashes(url_filename);

    // strip double slashes ("\\")
    StripDoubleSlashes(url_filename);

    // Save path as filesystem-safe characters
    url_filename = Escape(url_filename);
    filename = filename.append(url_filename);

#ifndef WIN32
    // Last step - convert to native slashes!
    const std::string slash("/");
    const std::string backslash("\\");
    ReplaceAll(filename, backslash, slash);
#endif

    return filename;
  }

 private:
  static const unsigned int kMaximumSubdirectoryLength = 128;


  // Escape the given input |path| and chop any individual components
  // of the path which are greater than kMaximumSubdirectoryLength characters
  // into two chunks.
  static std::string Escape(const std::string& path) {
    std::string output;

    // Note:  We also chop paths into medium sized 'chunks'.
    //        This is due to the incompetence of the windows
    //        filesystem, which still hasn't figured out how
    //        to deal with long filenames.
    unsigned int last_slash = 0;
    for (size_t index = 0; index < path.length(); index++) {
      char ch = path[index];
      if (ch == 0x5C)
        last_slash = index;
      if ((ch == 0x2D) ||                    // hyphen
          (ch == 0x5C) || (ch == 0x5F) ||    // backslash, underscore
          ((0x30 <= ch) && (ch <= 0x39)) ||  // Digits [0-9]
          ((0x41 <= ch) && (ch <= 0x5A)) ||  // Uppercase [A-Z]
          ((0x61 <= ch) && (ch <= 0x7A))) {  // Lowercase [a-z]
        output.append(&path[index],1);
      } else {
        char encoded[3];
        encoded[0] = 'x';
        encoded[1] = ch / 16;
        encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
        encoded[2] = ch % 16;
        encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
        output.append(encoded, 3);
      }
      if (index - last_slash > kMaximumSubdirectoryLength) {
#ifdef WIN32
        char slash = '\\';
#else
        char slash = '/';
#endif
        output.append(&slash, 1);
        last_slash = index;
      }
    }
    return output;
  }

  // Replace all instances of |from| within |str| as |to|.
  static void ReplaceAll(std::string& str, const std::string& from,
                  const std::string& to) {
    std::string::size_type pos(0);
    while ((pos = str.find(from, pos)) != std::string::npos) {
      str.replace(pos, from.size(), to);
      pos += from.size();
    }
  }

  // Replace all instances of "/" with "\" in |path|.
  static void ConvertToSlashes(std::string& path) {
    const std::string slash("/");
    const std::string backslash("\\");
    ReplaceAll(path, slash, backslash);
  }

  // Replace all instances of "\\" with "%5C%5C" in |path|.
  static void StripDoubleSlashes(std::string& path) {
    const std::string doubleslash("\\\\");
    const std::string escaped_doubleslash("%5C%5C");
    ReplaceAll(path, doubleslash, escaped_doubleslash);
  }
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_URL_TO_FILE_ENCODER_H__

