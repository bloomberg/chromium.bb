// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_loader.h"

#include <utility>

#include "base/json/json_writer.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace quick_answers {
namespace {

using base::Value;

// The JSON we generate looks like this:
// {
//   "query": {
//     "raw_query": "23 cm"
//   }
//   "client_id": {
//     "client_type": "EXPERIMENTAL"
//   }
//   "options": {
//     "page_size": "1"
//   }
// }
//
// Which is:
// DICT
//   "query": DICT
//      "raw_query": STRING
//   "client_id": DICT
//       "client_type": STRING
//   "options": DICT
//       "page_size": STRING

constexpr base::StringPiece kQueryKey = "query";
constexpr base::StringPiece kRawQueryKey = "rawQuery";
constexpr base::StringPiece kClientTypeKey = "clientType";
constexpr base::StringPiece kClientIdKey = "clientId";
constexpr base::StringPiece kClientType = "QUICK_ANSWERS_CROS";
constexpr base::StringPiece kPageSizeKey = "pageSize";
constexpr base::StringPiece kOptionsKey = "options";
constexpr base::StringPiece kPageSize = "1";

std::string BuildSearchRequestPayload(const std::string& selected_text) {
  Value payload(Value::Type::DICTIONARY);

  Value query(Value::Type::DICTIONARY);
  query.SetStringKey(kRawQueryKey, selected_text);
  payload.SetKey(kQueryKey, std::move(query));

  // TODO(llin): Change the client type.
  Value client_id(Value::Type::DICTIONARY);
  client_id.SetKey(kClientTypeKey, Value(kClientType));
  payload.SetKey(kClientIdKey, std::move(client_id));

  Value options(Value::Type::DICTIONARY);
  options.SetKey(kPageSizeKey, Value(kPageSize));
  payload.SetKey(kOptionsKey, std::move(options));

  std::string request_payload_str;
  base::JSONWriter::Write(payload, &request_payload_str);

  return request_payload_str;
}

}  // namespace

SearchResultLoader::SearchResultLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ResultLoaderDelegate* delegate)
    : ResultLoader(url_loader_factory, delegate) {}

SearchResultLoader::~SearchResultLoader() = default;

void SearchResultLoader::BuildRequest(
    const PreprocessedOutput& preprocessed_output,
    BuildRequestCallback callback) const {
  GURL url = GURL(ash::assistant::kKnowledgeApiEndpoint);

  // Add encoded request payload.
  url = net::AppendOrReplaceQueryParameter(
      url, ash::assistant::kPayloadParamName,
      BuildSearchRequestPayload(preprocessed_output.query));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  std::move(callback).Run(std::move(resource_request), std::string());
}

void SearchResultLoader::ProcessResponse(
    const PreprocessedOutput& preprocessed_output,
    std::unique_ptr<std::string> response_body,
    ResponseParserCallback complete_callback) {
  search_response_parser_ =
      std::make_unique<SearchResponseParser>(std::move(complete_callback));
  search_response_parser_->ProcessResponse(std::move(response_body));
}

}  // namespace quick_answers
