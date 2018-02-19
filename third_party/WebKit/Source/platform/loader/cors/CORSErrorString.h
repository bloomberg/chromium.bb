// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORSErrorString_h
#define CORSErrorString_h

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace blink {

// CORS error strings related utility functions.
namespace CORS {

// A struct to pass error dependent arguments for |GetErrorString|.
struct PLATFORM_EXPORT ErrorParameter {
  // Creates an ErrorParameter for generic cases. Use this function if |error|
  // can contain any.
  static ErrorParameter Create(const network::mojom::CORSError,
                               const KURL& first_url,
                               const KURL& second_url,
                               const int status_code,
                               const HTTPHeaderMap&,
                               const SecurityOrigin&,
                               const WebURLRequest::RequestContext);

  // Creates an ErrorParameter for kDisallowedByMode.
  static ErrorParameter CreateForDisallowedByMode(const KURL& request_url);

  // Creates an ErrorParameter for kInvalidResponse.
  static ErrorParameter CreateForInvalidResponse(const KURL& request_url,
                                                 const SecurityOrigin&);

  // Creates an ErrorParameter for an error that CORS::CheckAccess() returns.
  // |error| for redirect check needs to specify a valid |redirect_url|. The
  // |redirect_url| can be omitted not to include redirect related information.
  static ErrorParameter CreateForAccessCheck(
      const network::mojom::CORSError,
      const KURL& request_url,
      int response_status_code,
      const HTTPHeaderMap& response_header_map,
      const SecurityOrigin&,
      const WebURLRequest::RequestContext,
      const KURL& redirect_url = KURL());

  // Creates an ErrorParameter for kPreflightInvalidStatus that
  // CORS::CheckPreflight() returns.
  static ErrorParameter CreateForPreflightStatusCheck(int response_status_code);

  // Creates an ErrorParameter for an error that CORS::CheckExternalPreflight()
  // returns.
  static ErrorParameter CreateForExternalPreflightCheck(
      const network::mojom::CORSError,
      const HTTPHeaderMap& response_header_map);

  // Creates an ErrorParameter for CORS::CheckRedirectLocation() returns.
  static ErrorParameter CreateForRedirectCheck(network::mojom::CORSError,
                                               const KURL& request_url,
                                               const KURL& redirect_url);

  // Should not be used directly by external callers. Use Create functions
  // above.
  ErrorParameter(const network::mojom::CORSError,
                 const KURL& first_url,
                 const KURL& second_url,
                 const int status_code,
                 const HTTPHeaderMap&,
                 const SecurityOrigin&,
                 const WebURLRequest::RequestContext,
                 bool unknown);

  // Members that this struct carries.
  const network::mojom::CORSError error;
  const KURL& first_url;
  const KURL& second_url;
  const int status_code;
  const HTTPHeaderMap& header_map;
  const SecurityOrigin& origin;
  const WebURLRequest::RequestContext context;

  // Set to true when an ErrorParameter was created in a wrong way. Used in
  // GetErrorString() to be robust for coding errors.
  const bool unknown;
};

// Stringify CORSError mainly for inspector messages. Generated string should
// not be exposed to JavaScript for security reasons.
PLATFORM_EXPORT String GetErrorString(const ErrorParameter&);

}  // namespace CORS

}  // namespace blink

#endif  // CORSErrorString_h
