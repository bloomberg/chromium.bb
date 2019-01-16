// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace image_annotation {

namespace {

constexpr size_t kMaxResponseSize = 1024 * 1024;  // 1MB.

// The minimum confidence value needed to return an OCR result.
// TODO(crbug.com/916420): tune this value.
constexpr double kMinOcrConfidence = 0.7;

// Constructs and returns a JSON string representing an OCR request for the
// given image bytes.
std::string FormatJsonOcrRequest(const std::string& source_id,
                                 const std::vector<uint8_t>& image_bytes) {
  // Re-encode image bytes into base64, which can be represented in JSON.
  std::string base64_data;
  Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(image_bytes.data()),
                        image_bytes.size()),
      &base64_data);

  base::Value image_request(base::Value::Type::DICTIONARY);
  image_request.SetKey("image_id", base::Value(source_id));
  image_request.SetKey("image_bytes", base::Value(std::move(base64_data)));

  // TODO(crbug.com/916420): batch multiple images into one request.
  base::Value image_request_list(base::Value::Type::LIST);
  image_request_list.GetList().push_back(std::move(image_request));

  // TODO(crbug.com/916420): accept and propagate page language info to improve
  //                         OCR accuracy.
  base::Value feature_request(base::Value::Type::DICTIONARY);
  feature_request.SetKey("ocr_feature",
                         base::Value(base::Value::Type::DICTIONARY));

  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("image_requests", base::Value(std::move(image_request_list)));
  request.SetKey("feature_request", std::move(feature_request));

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);

  return json_request;
}

// Creates a URL loader that calls the image annotation server with an OCR
// request for the given image bytes.
std::unique_ptr<network::SimpleURLLoader> MakeOcrRequestLoader(
    const GURL& server_url,
    const std::string& source_id,
    const std::vector<uint8_t>& image_bytes) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";

  // TODO(crbug.com/916420): accept and pass API key when the server is
  //                         configured to require it.
  resource_request->url = server_url;

  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // TODO(crbug.com/916420): update this annotation to be more general and to
  //                         reflect specfics of the UI when it is implemented.
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("image_annotation", R"(
        semantics {
          sender: "Image Annotation"
          description:
            "Chrome can identify text inside images and provide this text to "
            "screen readers (for visually-impaired users) by sending images to "
            "Google's servers. If image text extraction is enabled for a page, "
            "Chrome will send the URLs and pixels of all images on the "
            "page to Google's servers, which will return any textual content "
            "identified inside the images. This content is made accessible to "
            "screen reading software."
          trigger: "A page containing images is loaded for a user who has "
                   "automatic image text extraction enabled."
          data: "Image pixels and URLs. No user identifier is sent along with "
                "the data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the context menu "
            "for images, or via 'Image Labeling' in Chrome's settings under "
            "Accessibility. This feature is disabled by default."
          policy_exception_justification: "Policy to come; feature not yet "
                                          "complete."
        })");

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader->AttachStringForUpload(
      FormatJsonOcrRequest(source_id, image_bytes), "application/json");

  return url_loader;
}

// Attempts to extract OCR results from the server response, returning true and
// setting |out| to these results if successful.
base::Optional<std::string> ParseJsonOcrResponse(
    const std::string* const json_response) {
  if (!json_response)
    return base::nullopt;

  const std::unique_ptr<base::Value> response =
      base::JSONReader::Read(*json_response);
  if (!response || !response->is_dict())
    return base::nullopt;

  const base::Value* const results = response->FindKey("results");
  if (!results || !results->is_list() || results->GetList().size() != 1 ||
      !results->GetList()[0].is_dict())
    return base::nullopt;

  const base::Value* const annotations =
      results->GetList()[0].FindKey("annotations");
  if (!annotations || !annotations->is_dict())
    return base::nullopt;

  const base::Value* const ocr_list = annotations->FindKey("ocr");
  if (!ocr_list || !ocr_list->is_list())
    return base::nullopt;

  // The server returns separate OCR results for each region of the image; we
  // naively concatenate these into one response string.
  std::string out;
  for (const base::Value& ocr : ocr_list->GetList()) {
    if (!ocr.is_dict())
      return base::nullopt;

    const base::Value* const detected_text = ocr.FindKey("detected_text");
    if (!detected_text || !detected_text->is_string())
      return base::nullopt;

    const base::Value* const confidence = ocr.FindKey("confidence_score");
    if (!confidence || !confidence->is_double())
      return base::nullopt;

    if (confidence->GetDouble() < kMinOcrConfidence)
      continue;

    const std::string& detected_text_str = detected_text->GetString();

    if (!out.empty() && !detected_text_str.empty())
      out += "\n";
    out += detected_text_str;
  }

  return out;
}

}  // namespace

Annotator::Annotator(
    GURL server_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(std::move(server_url)) {}

Annotator::~Annotator() {}

void Annotator::BindRequest(mojom::AnnotatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Annotator::AnnotateImage(const std::string& source_id,
                              mojom::ImageProcessorPtr image_processor,
                              AnnotateImageCallback callback) {
  // Return cached results if they exist.
  const auto cache_lookup = cached_results_.find(source_id);
  if (cache_lookup != cached_results_.end()) {
    std::move(callback).Run(
        mojom::AnnotateImageResult::NewOcrText(cache_lookup->second));
    return;
  }

  // Register the ImageProcessor and callback to be used for this request.
  RequestInfoList& request_info_list = request_infos_[source_id];
  request_info_list.push_back(
      {std::move(image_processor), std::move(callback)});

  // If the image processor dies: automatically delete the request info and
  // reassign local processing (for other interested clients) if the dead image
  // processor was responsible for some ongoing work.
  request_info_list.back().first.set_connection_error_handler(base::BindOnce(
      &Annotator::RemoveRequestInfo, base::Unretained(this), source_id,
      --request_info_list.end(), mojom::AnnotateImageError::kCanceled));

  // Don't start local work if it would duplicate some ongoing or already-
  // completed work.
  if (base::ContainsKey(local_processors_, source_id) ||
      base::ContainsKey(url_loaders_, source_id))
    return;

  local_processors_.insert(
      std::make_pair(source_id, &request_info_list.back().first));

  // TODO(crbug.com/916420): first query the public result cache by URL to
  // improve latency.

  request_info_list.back().first->GetJpgImageData(
      base::BindOnce(&Annotator::OnJpgImageDataReceived, base::Unretained(this),
                     source_id, --request_info_list.end()));
}

void Annotator::OnJpgImageDataReceived(
    const std::string& source_id,
    const RequestInfoList::iterator request_info_it,
    const std::vector<uint8_t>& image_bytes) {
  // Failed to retrieve bytes from local processor; remove dead processor and
  // reschedule processing.
  if (image_bytes.empty()) {
    RemoveRequestInfo(source_id, request_info_it,
                      mojom::AnnotateImageError::kFailure);
    return;
  }

  // Local processing is no longer ongoing.
  local_processors_.erase(source_id);

  // Kick off server communication.
  // TODO(crbug.com/916420): add request to a queue here and batch them up
  //                         to limit number of concurrent HTTP requests made.
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      MakeOcrRequestLoader(server_url_, source_id, image_bytes);
  const auto url_loader_it =
      url_loaders_.insert({source_id, std::move(url_loader)}).first;
  url_loader_it->second->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&Annotator::OnServerResponseReceived,
                     base::Unretained(this), source_id, url_loader_it),
      kMaxResponseSize);
}

void Annotator::OnServerResponseReceived(
    const std::string& source_id,
    const URLLoaderMap::iterator url_loader_it,
    const std::unique_ptr<std::string> json_response) {
  url_loaders_.erase(url_loader_it);

  // Extract OCR results into a string.
  const base::Optional<std::string> ocr_text =
      ParseJsonOcrResponse(json_response.get());

  if (ocr_text.has_value())
    cached_results_.insert({source_id, *ocr_text});

  const auto request_info_it = request_infos_.find(source_id);
  if (request_info_it == request_infos_.end())
    return;

  // Notify clients of success or failure.
  // TODO(crbug.com/916420): explore server retry strategies.
  for (auto& info : request_info_it->second) {
    auto result = ocr_text.has_value()
                      ? mojom::AnnotateImageResult::NewOcrText(*ocr_text)
                      : mojom::AnnotateImageResult::NewErrorCode(
                            mojom::AnnotateImageError::kFailure);
    std::move(info.second).Run(std::move(result));
  }
  request_infos_.erase(request_info_it);
}

void Annotator::RemoveRequestInfo(
    const std::string& source_id,
    const RequestInfoList::iterator request_info_it,
    const mojom::AnnotateImageError error) {
  // Check whether we are deleting the ImageProcessor responsible for current
  // local processing.
  auto lookup = local_processors_.find(source_id);
  const bool should_reassign = lookup != local_processors_.end() &&
                               lookup->second == &request_info_it->first;

  // Notify client of cancellation / failure.
  std::move(request_info_it->second)
      .Run(mojom::AnnotateImageResult::NewErrorCode(error));

  // Delete the specified ImageProcessor.
  RequestInfoList& request_info_list = request_infos_[source_id];
  request_info_list.erase(request_info_it);

  // If necessary, reassign local processing.
  if (!request_info_list.empty() && should_reassign) {
    lookup->second = &request_info_list.front().first;

    request_info_list.front().first->GetJpgImageData(base::BindOnce(
        &Annotator::OnJpgImageDataReceived, base::Unretained(this), source_id,
        request_info_list.begin()));
  }
}

}  // namespace image_annotation
