// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_
#define MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "media/base/media_export.h"
#include "media/base/media_log.h"

namespace media {

class StreamParser;

class MEDIA_EXPORT StreamParserFactory {
 public:
  // Checks to see if the specified |type| and |codecs| list are supported.
  // Returns true if |type| and all codecs listed in |codecs| are supported.
  static bool IsTypeSupported(
      const std::string& type, const std::vector<std::string>& codecs);

  // Creates a new StreamParser object if the specified |type| and |codecs| list
  // are supported. |media_log| can be used to report errors if there is
  // something wrong with |type| or the codec IDs in |codecs|.
  // Returns a new StreamParser object if |type| and all codecs listed in
  //   |codecs| are supported.
  // Returns NULL otherwise.
  static std::unique_ptr<StreamParser> Create(
      const std::string& type,
      const std::vector<std::string>& codecs,
      MediaLog* media_log);
};

}  // namespace media

#endif  // MEDIA_FILTERS_STREAM_PARSER_FACTORY_H_
