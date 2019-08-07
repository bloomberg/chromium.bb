// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PARSER_H_
#define CONTENT_BROWSER_SMS_SMS_PARSER_H_

#include "base/optional.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

// Extracts the origin from SMS messages according to the server-side
// convention of https://github.com/samuelgoto/sms-receiver.
// Returns an empty result if the formatting doesn't match.
class CONTENT_EXPORT SmsParser {
 public:
  static base::Optional<url::Origin> Parse(base::StringPiece sms);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PARSER_H_
