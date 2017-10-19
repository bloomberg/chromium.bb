/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/blob/BlobURL.h"

#include "platform/UUID.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

const char BlobURL::kBlobProtocol[] = "blob";

KURL BlobURL::CreatePublicURL(SecurityOrigin* security_origin) {
  DCHECK(security_origin);
  return CreateBlobURL(security_origin->ToString());
}

String BlobURL::GetOrigin(const KURL& url) {
  DCHECK(url.ProtocolIs(kBlobProtocol));

  unsigned start_index = url.PathStart();
  unsigned end_index = url.PathAfterLastSlash();
  return url.GetString().Substring(start_index, end_index - start_index - 1);
}

KURL BlobURL::CreateInternalStreamURL() {
  return CreateBlobURL("blobinternal://");
}

KURL BlobURL::CreateBlobURL(const String& origin_string) {
  DCHECK(!origin_string.IsEmpty());
  String url_string =
      "blob:" + origin_string + '/' + CreateCanonicalUUIDString();
  return KURL::CreateIsolated(url_string);
}

}  // namespace blink
