// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/decode_image.h"

#include "services/data_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace data_decoder {

namespace {

// Helper callback which owns an ImageDecoderPtr until invoked. This keeps the
// ImageDecoder pipe open just long enough to dispatch a reply, at which point
// the reply is forwarded to the wrapped |callback|.
void OnDecodeImage(mojom::ImageDecoderPtr decoder,
                   mojom::ImageDecoder::DecodeImageCallback callback,
                   const SkBitmap& bitmap) {
  std::move(callback).Run(bitmap);
}

}  // namespace

void DecodeImage(service_manager::Connector* connector,
                 const std::vector<uint8_t>& encoded_bytes,
                 mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 uint64_t max_size_in_bytes,
                 const gfx::Size& desired_image_frame_size,
                 mojom::ImageDecoder::DecodeImageCallback callback) {
  mojom::ImageDecoderPtr decoder;
  connector->BindInterface(mojom::kServiceName, &decoder);

  // |call_once| runs |callback| on its first invocation.
  auto call_once = base::AdaptCallbackForRepeating(std::move(callback));
  decoder.set_connection_error_handler(base::Bind(call_once, SkBitmap()));

  mojom::ImageDecoder* raw_decoder = decoder.get();
  raw_decoder->DecodeImage(
      encoded_bytes, codec, shrink_to_fit, max_size_in_bytes,
      desired_image_frame_size,
      base::BindOnce(&OnDecodeImage, std::move(decoder), std::move(call_once)));
}

}  // namespace data_decoder
