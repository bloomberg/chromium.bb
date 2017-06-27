// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FormDataBytesConsumer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/TextCodec.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

bool IsSimple(const EncodedFormData* form_data) {
  for (const auto& element : form_data->Elements()) {
    if (element.type_ != FormDataElement::kData)
      return false;
  }
  return true;
}

class SimpleFormDataBytesConsumer : public BytesConsumer {
 public:
  explicit SimpleFormDataBytesConsumer(PassRefPtr<EncodedFormData> form_data)
      : form_data_(std::move(form_data)) {}

  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    if (form_data_) {
      form_data_->Flatten(flatten_form_data_);
      form_data_ = nullptr;
      DCHECK_EQ(flatten_form_data_offset_, 0u);
    }
    if (flatten_form_data_offset_ == flatten_form_data_.size())
      return Result::kDone;
    *buffer = flatten_form_data_.data() + flatten_form_data_offset_;
    *available = flatten_form_data_.size() - flatten_form_data_offset_;
    return Result::kOk;
  }
  Result EndRead(size_t read_size) override {
    DCHECK(!form_data_);
    DCHECK_LE(flatten_form_data_offset_ + read_size, flatten_form_data_.size());
    flatten_form_data_offset_ += read_size;
    if (flatten_form_data_offset_ == flatten_form_data_.size()) {
      state_ = PublicState::kClosed;
      return Result::kDone;
    }
    return Result::kOk;
  }
  PassRefPtr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (!form_data_)
      return nullptr;

    Vector<char> data;
    form_data_->Flatten(data);
    form_data_ = nullptr;
    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->AppendBytes(data.data(), data.size());
    auto length = blob_data->length();
    state_ = PublicState::kClosed;
    return BlobDataHandle::Create(std::move(blob_data), length);
  }
  PassRefPtr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_)
      return nullptr;

    state_ = PublicState::kClosed;
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override { DCHECK(client); }
  void ClearClient() override {}
  void Cancel() override {
    state_ = PublicState::kClosed;
    form_data_ = nullptr;
    flatten_form_data_.clear();
    flatten_form_data_offset_ = 0;
  }
  PublicState GetPublicState() const override { return state_; }
  Error GetError() const override {
    NOTREACHED();
    return Error();
  }
  String DebugName() const override { return "SimpleFormDataBytesConsumer"; }

 private:
  // either one of |m_formData| and |m_flattenFormData| is usable at a time.
  RefPtr<EncodedFormData> form_data_;
  Vector<char> flatten_form_data_;
  size_t flatten_form_data_offset_ = 0;
  PublicState state_ = PublicState::kReadableOrWaiting;
};

class ComplexFormDataBytesConsumer final : public BytesConsumer {
 public:
  ComplexFormDataBytesConsumer(ExecutionContext* execution_context,
                               PassRefPtr<EncodedFormData> form_data,
                               BytesConsumer* consumer)
      : form_data_(std::move(form_data)) {
    if (consumer) {
      // For testing.
      blob_bytes_consumer_ = consumer;
      return;
    }

    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    for (const auto& element : form_data_->Elements()) {
      switch (element.type_) {
        case FormDataElement::kData:
          blob_data->AppendBytes(element.data_.data(), element.data_.size());
          break;
        case FormDataElement::kEncodedFile: {
          auto file_length = element.file_length_;
          if (file_length < 0) {
            if (!GetFileSize(element.filename_, file_length)) {
              form_data_ = nullptr;
              blob_bytes_consumer_ = BytesConsumer::CreateErrored(
                  Error("Cannot determine a file size"));
              return;
            }
          }
          blob_data->AppendFile(element.filename_, element.file_start_,
                                file_length,
                                element.expected_file_modification_time_);
          break;
        }
        case FormDataElement::kEncodedBlob:
          if (element.optional_blob_data_handle_)
            blob_data->AppendBlob(element.optional_blob_data_handle_, 0,
                                  element.optional_blob_data_handle_->size());
          break;
        case FormDataElement::kEncodedFileSystemURL:
          if (element.file_length_ < 0) {
            form_data_ = nullptr;
            blob_bytes_consumer_ = BytesConsumer::CreateErrored(
                Error("Cannot determine a file size"));
            return;
          }
          blob_data->AppendFileSystemURL(
              element.file_system_url_, element.file_start_,
              element.file_length_, element.expected_file_modification_time_);
          break;
      }
    }
    // Here we handle m_formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    blob_data->SetContentType(AtomicString("multipart/form-data; boundary=") +
                              form_data_->Boundary().data());
    auto size = blob_data->length();
    blob_bytes_consumer_ = new BlobBytesConsumer(
        execution_context, BlobDataHandle::Create(std::move(blob_data), size));
  }

  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    form_data_ = nullptr;
    // Delegate the operation to the underlying consumer. This relies on
    // the fact that we appropriately notify the draining information to
    // the underlying consumer.
    return blob_bytes_consumer_->BeginRead(buffer, available);
  }
  Result EndRead(size_t read_size) override {
    return blob_bytes_consumer_->EndRead(read_size);
  }
  PassRefPtr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    RefPtr<BlobDataHandle> handle =
        blob_bytes_consumer_->DrainAsBlobDataHandle(policy);
    if (handle)
      form_data_ = nullptr;
    return handle;
  }
  PassRefPtr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_)
      return nullptr;
    blob_bytes_consumer_->Cancel();
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override {
    blob_bytes_consumer_->SetClient(client);
  }
  void ClearClient() override { blob_bytes_consumer_->ClearClient(); }
  void Cancel() override {
    form_data_ = nullptr;
    blob_bytes_consumer_->Cancel();
  }
  PublicState GetPublicState() const override {
    return blob_bytes_consumer_->GetPublicState();
  }
  Error GetError() const override { return blob_bytes_consumer_->GetError(); }
  String DebugName() const override { return "ComplexFormDataBytesConsumer"; }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(blob_bytes_consumer_);
    BytesConsumer::Trace(visitor);
  }

 private:
  RefPtr<EncodedFormData> form_data_;
  Member<BytesConsumer> blob_bytes_consumer_;
};

}  // namespace

FormDataBytesConsumer::FormDataBytesConsumer(const String& string)
    : impl_(new SimpleFormDataBytesConsumer(EncodedFormData::Create(
          UTF8Encoding().Encode(string, WTF::kEntitiesForUnencodables)))) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBuffer* buffer)
    : FormDataBytesConsumer(buffer->Data(), buffer->ByteLength()) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBufferView* view)
    : FormDataBytesConsumer(view->BaseAddress(), view->byteLength()) {}

FormDataBytesConsumer::FormDataBytesConsumer(const void* data, size_t size)
    : impl_(new SimpleFormDataBytesConsumer(
          EncodedFormData::Create(data, size))) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    PassRefPtr<EncodedFormData> form_data)
    : FormDataBytesConsumer(execution_context, std::move(form_data), nullptr) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    PassRefPtr<EncodedFormData> form_data,
    BytesConsumer* consumer)
    : impl_(IsSimple(form_data.Get())
                ? static_cast<BytesConsumer*>(
                      new SimpleFormDataBytesConsumer(std::move(form_data)))
                : static_cast<BytesConsumer*>(
                      new ComplexFormDataBytesConsumer(execution_context,
                                                       std::move(form_data),
                                                       consumer))) {}

}  // namespace blink
