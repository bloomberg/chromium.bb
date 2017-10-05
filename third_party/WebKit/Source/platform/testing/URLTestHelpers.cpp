/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "platform/testing/URLTestHelpers.h"

#include <string>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "platform/http_names.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoadTiming.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {
namespace URLTestHelpers {

WebURL RegisterMockedURLLoadFromBase(const WebString& base_url,
                                     const WebString& base_path,
                                     const WebString& file_name,
                                     const WebString& mime_type) {
  // fullURL = baseURL + fileName.
  std::string full_url = std::string(base_url.Utf8().data()) +
                         std::string(file_name.Utf8().data());

  // filePath = basePath + ("/" +) fileName.
  base::FilePath file_path =
      WebStringToFilePath(base_path).Append(WebStringToFilePath(file_name));

  KURL url = ToKURL(full_url);
  RegisterMockedURLLoad(url, FilePathToWebString(file_path), mime_type);
  return WebURL(url);
}

void RegisterMockedURLLoad(const WebURL& full_url,
                           const WebString& file_path,
                           const WebString& mime_type) {
  WebURLLoadTiming timing;
  timing.Initialize();

  WebURLResponse response(full_url);
  response.SetMIMEType(mime_type);
  response.SetHTTPHeaderField(HTTPNames::Content_Type, mime_type);
  response.SetHTTPStatusCode(200);
  response.SetLoadTiming(timing);

  RegisterMockedURLLoadWithCustomResponse(full_url, file_path, response);
}

void RegisterMockedErrorURLLoad(const WebURL& full_url) {
  WebURLLoadTiming timing;
  timing.Initialize();

  WebURLResponse response;
  response.SetMIMEType("image/png");
  response.SetHTTPHeaderField(HTTPNames::Content_Type, "image/png");
  response.SetHTTPStatusCode(404);
  response.SetLoadTiming(timing);

  WebURLError error;
  error.domain = WebURLError::Domain::kTest;
  error.reason = 404;
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      full_url, response, error);
}

void RegisterMockedURLLoadWithCustomResponse(const WebURL& full_url,
                                             const WebString& file_path,
                                             WebURLResponse response) {
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      full_url, response, file_path);
}

}  // namespace URLTestHelpers
}  // namespace blink
