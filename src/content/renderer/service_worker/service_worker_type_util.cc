// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_util.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/common/service_worker/service_worker_types.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

namespace {

template <typename T>
class HeaderVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  explicit HeaderVisitor(T* headers) : headers_(headers) {}
  ~HeaderVisitor() override {}

  void VisitHeader(const blink::WebString& name,
                   const blink::WebString& value) override {
    // Headers are ISO Latin 1.
    const std::string& header_name = name.Latin1();
    const std::string& header_value = value.Latin1();
    CHECK(header_name.find('\0') == std::string::npos);
    CHECK(header_value.find('\0') == std::string::npos);
    headers_->insert(typename T::value_type(header_name, header_value));
  }

 private:
  T* const headers_;
};

template <typename T>
std::unique_ptr<HeaderVisitor<T>> MakeHeaderVisitor(T* headers) {
  return std::make_unique<HeaderVisitor<T>>(headers);
}

std::vector<std::string> GetHeaderList(
    const blink::WebVector<blink::WebString>& web_headers) {
  auto result = std::vector<std::string>(web_headers.size());
  std::transform(web_headers.begin(), web_headers.end(), result.begin(),
                 [](const blink::WebString& s) { return s.Latin1(); });
  return result;
}

std::vector<GURL> GetURLList(
    const blink::WebVector<blink::WebURL>& web_url_list) {
  std::vector<GURL> result = std::vector<GURL>(web_url_list.size());
  std::transform(web_url_list.begin(), web_url_list.end(), result.begin(),
                 [](const blink::WebURL& url) { return url; });
  return result;
}

}  // namespace

void GetServiceWorkerHeaderMapFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request,
    ServiceWorkerHeaderMap* headers) {
  DCHECK(headers);
  DCHECK(headers->empty());
  web_request.VisitHTTPHeaderFields(MakeHeaderVisitor(headers).get());
}

blink::mojom::FetchAPIResponsePtr GetFetchAPIResponseFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response) {
  blink::mojom::SerializedBlobPtr blob;
  if (!web_response.BlobUUID().IsEmpty()) {
    blob = blink::mojom::SerializedBlob::New();
    blob->uuid = web_response.BlobUUID().Utf8();
    blob->size = web_response.BlobSize();
    auto blob_pipe = web_response.CloneBlobPtr();
    DCHECK(blob_pipe.is_valid());
    blob->blob = blink::mojom::BlobPtrInfo(std::move(blob_pipe),
                                           blink::mojom::Blob::Version_);
  }

  blink::mojom::SerializedBlobPtr side_data_blob;
  if (web_response.SideDataBlobSize() != 0) {
    side_data_blob = blink::mojom::SerializedBlob::New();
    side_data_blob->uuid = web_response.SideDataBlobUUID().Utf8();
    side_data_blob->size = web_response.SideDataBlobSize();
    auto side_data_blob_pipe = web_response.CloneSideDataBlobPtr();
    DCHECK(side_data_blob_pipe.is_valid());
    side_data_blob->blob = blink::mojom::BlobPtrInfo(
        std::move(side_data_blob_pipe), blink::mojom::Blob::Version_);
  }

  base::flat_map<std::string, std::string> headers;
  web_response.VisitHTTPHeaderFields(MakeHeaderVisitor(&headers).get());

  return blink::mojom::FetchAPIResponse::New(
      GetURLList(web_response.UrlList()), web_response.Status(),
      web_response.StatusText().Utf8(), web_response.ResponseType(), headers,
      std::move(blob), web_response.GetError(), web_response.ResponseTime(),
      web_response.CacheStorageCacheName().Utf8(),
      GetHeaderList(web_response.CorsExposedHeaderNames()),
      !web_response.CacheStorageCacheName().IsNull(),
      std::move(side_data_blob));
}

}  // namespace content
