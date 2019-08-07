// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JOURNEY_JOURNEY_INFO_JSON_REQUEST_H_
#define COMPONENTS_JOURNEY_JOURNEY_INFO_JSON_REQUEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class HttpRequestHeaders;
}
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace journey {

// This class represents a request of journey info. It encapsulates the
// request-response lifecycle. It is also responsible for building and
// serializing the request body protos.
class JourneyInfoJsonRequest {
  // Callbacks for JSON parsing to allow injecting platform-dependent code.
  using SuccessCallback = base::OnceCallback<void(base::Value result)>;
  using ErrorCallback = base::OnceCallback<void(const std::string& error)>;
  using ParseJSONCallback =
      base::RepeatingCallback<void(const std::string& raw_json_string,
                                   SuccessCallback success_callback,
                                   ErrorCallback error_callback)>;
  using CompletedCallback =
      base::OnceCallback<void(base::Optional<base::Value> result,
                              const std::string& error_detail)>;

 public:
  // This class is used to build authenticated and non-authenticated
  // JourneyInfoJsonRequest.
  class Builder {
   public:
    Builder();
    ~Builder();

    // This method is used to builds a Request object that contains all
    // data to fetch new snippets.
    std::unique_ptr<JourneyInfoJsonRequest> Build() const;

    Builder& SetAuthentication(const std::string& auth_header);
    Builder& SetParseJsonCallback(ParseJSONCallback callback);
    Builder& SetTimestamps(const std::vector<int64_t>& timestamps);

   private:
    std::unique_ptr<network::SimpleURLLoader> BuildSimpleURLLoader() const;
    net::HttpRequestHeaders BuildSimpleURLLoaderHeaders() const;

    // The url for which we're fetching journey info.
    GURL url_;
    std::string auth_header_;
    std::string body_;
    ParseJSONCallback parse_json_callback_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  explicit JourneyInfoJsonRequest(const ParseJSONCallback& callback);
  ~JourneyInfoJsonRequest();

  // This method is used to start fetching journey info using |loader_factory|
  // to create a URLLoader, and call |callback| when finished.
  void Start(
      CompletedCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& loader_factory);

  // Get the last response as a string
  const std::string& GetResponseString() const;

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnJsonParsed(base::Value result);
  void OnJsonError(const std::string& error);

  // This callback is called to parse a json string. It contains callbacks for
  // error and success cases.
  ParseJSONCallback parse_json_callback_;

  // The loader for downloading the snippets. Only non-null if a fetch is
  // currently ongoing.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  std::string last_response_string_;

  // The callback to call when journey info are available.
  CompletedCallback completed_callback_;

  base::WeakPtrFactory<JourneyInfoJsonRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(JourneyInfoJsonRequest);
};

}  // namespace journey

#endif  // COMPONENTS_JOURNEY_JOURNEY_INFO_JSON_REQUEST_H_
