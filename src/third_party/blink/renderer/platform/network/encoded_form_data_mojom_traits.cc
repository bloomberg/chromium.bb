// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/platform/network/encoded_form_data_mojom_traits.h"

#include "base/feature_list.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace mojo {

// static
network::mojom::DataElementType
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::type(
    const blink::FormDataElement& data) {
  switch (data.type_) {
    case blink::FormDataElement::kData:
      return network::mojom::DataElementType::kBytes;
    case blink::FormDataElement::kEncodedFile:
      return network::mojom::DataElementType::kFile;
    case blink::FormDataElement::kEncodedBlob: {
      if (data.optional_blob_data_handle_)
        return network::mojom::DataElementType::kDataPipe;
      return network::mojom::DataElementType::kBlob;
    }
    case blink::FormDataElement::kDataPipe:
      return network::mojom::DataElementType::kDataPipe;
  }
  NOTREACHED();
  return network::mojom::DataElementType::kUnknown;
}

// static
base::span<const uint8_t>
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::buf(
    const blink::FormDataElement& data) {
  return base::make_span(reinterpret_cast<const uint8_t*>(data.data_.data()),
                         data.data_.size());
}

// static
base::File
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::file(
    const blink::FormDataElement& data) {
  return base::File();
}

// static
base::FilePath
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::path(
    const blink::FormDataElement& data) {
  return base::FilePath::FromUTF8Unsafe(
      std::string(data.filename_.Utf8().data()));
}

// static
network::mojom::blink::DataPipeGetterPtrInfo
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::
    data_pipe_getter(const blink::FormDataElement& data) {
  if (data.type_ == blink::FormDataElement::kDataPipe) {
    if (!data.data_pipe_getter_)
      return nullptr;
    network::mojom::blink::DataPipeGetterPtr data_pipe_getter;
    (*data.data_pipe_getter_->GetPtr())
        ->Clone(mojo::MakeRequest(&data_pipe_getter));
    return data_pipe_getter.PassInterface();
  }
  if (data.type_ == blink::FormDataElement::kEncodedBlob) {
    if (data.optional_blob_data_handle_) {
      blink::mojom::blink::BlobPtr blob_ptr(blink::mojom::blink::BlobPtrInfo(
          data.optional_blob_data_handle_->CloneBlobPtr()
              .PassInterface()
              .PassHandle(),
          blink::mojom::blink::Blob::Version_));
      network::mojom::blink::DataPipeGetterPtr data_pipe_getter_ptr;
      blob_ptr->AsDataPipeGetter(MakeRequest(&data_pipe_getter_ptr));
      return data_pipe_getter_ptr.PassInterface();
    }
  }
  return nullptr;
}

// static
base::Time
StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::
    expected_modification_time(const blink::FormDataElement& data) {
  if (data.type_ == blink::FormDataElement::kEncodedFile)
    return base::Time::FromDoubleT(data.expected_file_modification_time_);
  return base::Time();
}

// static
bool StructTraits<network::mojom::DataElementDataView, blink::FormDataElement>::
    Read(network::mojom::DataElementDataView data,
         blink::FormDataElement* out) {
  network::mojom::DataElementType data_type;
  if (!data.ReadType(&data_type)) {
    return false;
  }
  out->file_start_ = data.offset();
  out->file_length_ = data.length();

  switch (data_type) {
    case network::mojom::DataElementType::kBytes: {
      out->type_ = blink::FormDataElement::kData;
      // TODO(richard.li): Delete this workaround when type of
      // blink::FormDataElement::data_ is changed to WTF::Vector<uint8_t>
      WTF::Vector<uint8_t> buf;
      if (!data.ReadBuf(&buf)) {
        return false;
      }
      out->data_.AppendRange(buf.begin(), buf.end());
      break;
    }
    case network::mojom::DataElementType::kFile: {
      out->type_ = blink::FormDataElement::kEncodedFile;
      base::FilePath file_path;
      base::Time expected_time;
      if (!data.ReadPath(&file_path) ||
          !data.ReadExpectedModificationTime(&expected_time)) {
        return false;
      }
      out->expected_file_modification_time_ = expected_time.ToDoubleT();
      out->filename_ =
          WTF::String(file_path.value().data(), file_path.value().size());
      break;
    }
    case network::mojom::DataElementType::kBlob: {
      // Blobs are actually passed around as kDataPipe elements when network
      // service is enabled, which keeps the blobs alive.
      DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
      out->type_ = blink::FormDataElement::kEncodedBlob;
      if (!data.ReadBlobUuid(&out->blob_uuid_)) {
        return false;
      }
      out->optional_blob_data_handle_ = blink::BlobDataHandle::Create(
          out->blob_uuid_, "" /* type is not necessary */, data.length());
      break;
    }
    case network::mojom::DataElementType::kDataPipe: {
      out->type_ = blink::FormDataElement::kDataPipe;
      auto data_pipe_ptr_info = data.TakeDataPipeGetter<
          network::mojom::blink::DataPipeGetterPtrInfo>();
      DCHECK(data_pipe_ptr_info.is_valid());

      network::mojom::blink::DataPipeGetterPtr data_pipe_getter;
      data_pipe_getter.Bind(std::move(data_pipe_ptr_info));
      out->data_pipe_getter_ =
          base::MakeRefCounted<blink::WrappedDataPipeGetter>(
              std::move(data_pipe_getter));
      break;
    }
    case network::mojom::DataElementType::kUnknown:
    case network::mojom::DataElementType::kChunkedDataPipe:
    case network::mojom::DataElementType::kRawFile:
      NOTREACHED();
      return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::URLRequestBodyDataView,
                  scoped_refptr<blink::EncodedFormData>>::
    Read(network::mojom::URLRequestBodyDataView in,
         scoped_refptr<blink::EncodedFormData>* out) {
  *out = blink::EncodedFormData::Create();
  if (!in.ReadElements(&((*out)->elements_))) {
    return false;
  }
  (*out)->identifier_ = in.identifier();
  (*out)->contains_password_data_ = in.contains_sensitive_info();
  (*out)->SetBoundary(blink::FormDataEncoder::GenerateUniqueBoundaryString());

  return true;
}

}  // namespace mojo
