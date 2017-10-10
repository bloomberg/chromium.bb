// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorAuditsAgent.h"

#include "core/inspector/InspectorNetworkAgent.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/wtf/text/Base64.h"
#include "public/platform/WebData.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebSize.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace EncodingEnum = protocol::Audits::GetEncodedResponse::EncodingEnum;

namespace {

static constexpr int kMaximumEncodeImageWidthInPixels = 10000;

static constexpr int kMaximumEncodeImageHeightInPixels = 10000;

static constexpr double kDefaultEncodeQuality = 1;

bool EncodeAsImage(char* body,
                   size_t size,
                   const String& encoding,
                   const double quality,
                   Vector<unsigned char>* output) {
  const WebSize maximum_size = WebSize(kMaximumEncodeImageWidthInPixels,
                                       kMaximumEncodeImageHeightInPixels);
  SkBitmap bitmap =
      WebImage::FromData(WebData(body, size), maximum_size).GetSkBitmap();
  SkImageInfo info =
      SkImageInfo::Make(bitmap.width(), bitmap.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType);
  size_t row_bytes = info.minRowBytes();
  Vector<unsigned char> pixel_storage(info.computeByteSize(row_bytes));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);

  if (!SkImage::MakeFromBitmap(bitmap)->readPixels(pixmap, 0, 0))
    return false;

  ImageDataBuffer image_to_encode = ImageDataBuffer(
      IntSize(bitmap.width(), bitmap.height()), pixel_storage.data());

  String mime_type = "image/";
  mime_type.append(encoding);
  return image_to_encode.EncodeImage(mime_type, quality, output);
}

}  // namespace

DEFINE_TRACE(InspectorAuditsAgent) {
  visitor->Trace(network_agent_);
  InspectorBaseAgent::Trace(visitor);
}

InspectorAuditsAgent::InspectorAuditsAgent(InspectorNetworkAgent* network_agent)
    : network_agent_(network_agent) {}

InspectorAuditsAgent::~InspectorAuditsAgent() {}

protocol::Response InspectorAuditsAgent::getEncodedResponse(
    const String& request_id,
    const String& encoding,
    Maybe<double> quality,
    Maybe<bool> size_only,
    Maybe<String>* out_body,
    int* out_original_size,
    int* out_encoded_size) {
  DCHECK(encoding == EncodingEnum::Jpeg || encoding == EncodingEnum::Png ||
         encoding == EncodingEnum::Webp);

  String body;
  bool is_base64_encoded;
  Response response =
      network_agent_->GetResponseBody(request_id, &body, &is_base64_encoded);
  if (!response.isSuccess())
    return response;

  Vector<char> base64_decoded_buffer;
  if (!is_base64_encoded || !Base64Decode(body, base64_decoded_buffer) ||
      base64_decoded_buffer.size() == 0) {
    return Response::Error("Failed to decode original image");
  }

  Vector<unsigned char> encoded_image;
  if (!EncodeAsImage(base64_decoded_buffer.data(), base64_decoded_buffer.size(),
                     encoding, quality.fromMaybe(kDefaultEncodeQuality),
                     &encoded_image)) {
    return Response::Error("Could not encode image with given settings");
  }

  if (!size_only.fromMaybe(false))
    *out_body = Base64Encode(encoded_image);

  *out_original_size = static_cast<int>(base64_decoded_buffer.size());
  *out_encoded_size = static_cast<int>(encoded_image.size());
  return Response::OK();
}

}  // namespace blink
