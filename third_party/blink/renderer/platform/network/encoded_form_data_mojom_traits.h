// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_

#include <string>

#include "services/network/public/mojom/url_loader.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<network::mojom::DataElementDataView, blink::FormDataElement> {
  static network::mojom::DataElementType type(
      const blink::FormDataElement& data);

  static base::span<const uint8_t> buf(const blink::FormDataElement& data);

  static base::File file(const blink::FormDataElement& data);

  static base::FilePath path(const blink::FormDataElement& data);

  static const WTF::String& blob_uuid(const blink::FormDataElement& data) {
    return data.blob_uuid_;
  }

  static network::mojom::blink::DataPipeGetterPtrInfo data_pipe_getter(
      const blink::FormDataElement& data);

  static network::mojom::blink::ChunkedDataPipeGetterPtrInfo
  chunked_data_pipe_getter(const blink::FormDataElement& data) {
    return nullptr;
  }

  static uint64_t offset(const blink::FormDataElement& data) {
    return data.file_start_;
  }

  static uint64_t length(const blink::FormDataElement& data) {
    if (data.type_ == blink::FormDataElement::kEncodedBlob &&
        data.optional_blob_data_handle_) {
      return data.optional_blob_data_handle_->size();
    }
    return data.file_length_;
  }

  static base::Time expected_modification_time(
      const blink::FormDataElement& data);

  static bool Read(network::mojom::DataElementDataView data,
                   blink::FormDataElement* out);
};

template <>
struct PLATFORM_EXPORT StructTraits<network::mojom::URLRequestBodyDataView,
                                    scoped_refptr<blink::EncodedFormData>> {
  static const WTF::Vector<blink::FormDataElement>& elements(
      const scoped_refptr<blink::EncodedFormData>& data) {
    return data->elements_;
  }
  static int64_t identifier(const scoped_refptr<blink::EncodedFormData>& data) {
    return data->identifier_;
  }
  static bool contains_sensitive_info(
      const scoped_refptr<blink::EncodedFormData>& data) {
    return data->contains_password_data_;
  }

  static bool Read(network::mojom::URLRequestBodyDataView in,
                   scoped_refptr<blink::EncodedFormData>* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_
