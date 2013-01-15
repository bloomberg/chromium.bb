// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLRESPONSE_EXTRADATA_IMPL_H_
#define WEBKIT_GLUE_WEBURLRESPONSE_EXTRADATA_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

// Base class for Chrome's implementation of the "extra data".
class WEBKIT_GLUE_EXPORT WebURLResponseExtraDataImpl
    : NON_EXPORTED_BASE(public WebKit::WebURLResponse::ExtraData) {
 public:
  explicit WebURLResponseExtraDataImpl(
      const std::string& npn_negotiated_protocol);
  virtual ~WebURLResponseExtraDataImpl();

  const std::string& npn_negotiated_protocol() const {
    return npn_negotiated_protocol_;
  }
  bool is_ftp_directory_listing() const { return is_ftp_directory_listing_; }
  void set_is_ftp_directory_listing(bool is_ftp_directory_listing) {
    is_ftp_directory_listing_ = is_ftp_directory_listing;
  }


 private:
  std::string npn_negotiated_protocol_;
  bool is_ftp_directory_listing_;

  DISALLOW_COPY_AND_ASSIGN(WebURLResponseExtraDataImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBURLRESPONSE_EXTRADATA_IMPL_H_
