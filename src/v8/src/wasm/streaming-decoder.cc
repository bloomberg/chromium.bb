// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/streaming-decoder.h"

#include "src/base/template-utils.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/wasm/decoder.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

#define TRACE_STREAMING(...)                            \
  do {                                                  \
    if (FLAG_trace_wasm_streaming) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

void StreamingDecoder::OnBytesReceived(Vector<const uint8_t> bytes) {
  if (deserializing()) {
    wire_bytes_for_deserializing_.insert(wire_bytes_for_deserializing_.end(),
                                         bytes.begin(), bytes.end());
    return;
  }

  TRACE_STREAMING("OnBytesReceived(%zu bytes)\n", bytes.size());

  size_t current = 0;
  while (ok() && current < bytes.size()) {
    size_t num_bytes =
        state_->ReadBytes(this, bytes.SubVector(current, bytes.size()));
    current += num_bytes;
    module_offset_ += num_bytes;
    if (state_->offset() == state_->buffer().size()) {
      state_ = state_->Next(this);
    }
  }
  total_size_ += bytes.size();
  if (ok()) {
    processor_->OnFinishedChunk();
  }
}

size_t StreamingDecoder::DecodingState::ReadBytes(StreamingDecoder* streaming,
                                                  Vector<const uint8_t> bytes) {
  Vector<uint8_t> remaining_buf = buffer() + offset();
  size_t num_bytes = std::min(bytes.size(), remaining_buf.size());
  TRACE_STREAMING("ReadBytes(%zu bytes)\n", num_bytes);
  memcpy(remaining_buf.start(), &bytes.first(), num_bytes);
  set_offset(offset() + num_bytes);
  return num_bytes;
}

void StreamingDecoder::Finish() {
  TRACE_STREAMING("Finish\n");
  if (!ok()) return;

  if (deserializing()) {
    Vector<const uint8_t> wire_bytes = VectorOf(wire_bytes_for_deserializing_);
    // Try to deserialize the module from wire bytes and module bytes.
    if (processor_->Deserialize(compiled_module_bytes_, wire_bytes)) return;

    // Deserialization failed. Restart decoding using |wire_bytes|.
    compiled_module_bytes_ = {};
    DCHECK(!deserializing());
    OnBytesReceived(wire_bytes);
    // The decoder has received all wire bytes; fall through and finish.
  }

  if (!state_->is_finishing_allowed()) {
    // The byte stream ended too early, we report an error.
    Error("unexpected end of stream");
    return;
  }

  OwnedVector<uint8_t> bytes = OwnedVector<uint8_t>::New(total_size_);
  uint8_t* cursor = bytes.start();
  {
#define BYTES(x) (x & 0xFF), (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF
    uint8_t module_header[]{BYTES(kWasmMagic), BYTES(kWasmVersion)};
#undef BYTES
    memcpy(cursor, module_header, arraysize(module_header));
    cursor += arraysize(module_header);
  }
  for (const auto& buffer : section_buffers_) {
    DCHECK_LE(cursor - bytes.start() + buffer->length(), total_size_);
    memcpy(cursor, buffer->bytes().start(), buffer->length());
    cursor += buffer->length();
  }
  processor_->OnFinishedStream(std::move(bytes));
}

void StreamingDecoder::Abort() {
  TRACE_STREAMING("Abort\n");
  if (!ok()) return;  // Failed already.
  processor_->OnAbort();
  Fail();
}

void StreamingDecoder::SetModuleCompiledCallback(
    ModuleCompiledCallback callback) {
  DCHECK_NULL(module_compiled_callback_);
  module_compiled_callback_ = callback;
}

bool StreamingDecoder::SetCompiledModuleBytes(
    Vector<const uint8_t> compiled_module_bytes) {
  compiled_module_bytes_ = compiled_module_bytes;
  return true;
}

// An abstract class to share code among the states which decode VarInts. This
// class takes over the decoding of the VarInt and then calls the actual decode
// code with the decoded value.
class StreamingDecoder::DecodeVarInt32 : public DecodingState {
 public:
  explicit DecodeVarInt32(size_t max_value, const char* field_name)
      : max_value_(max_value), field_name_(field_name) {}

  Vector<uint8_t> buffer() override { return ArrayVector(byte_buffer_); }

  size_t ReadBytes(StreamingDecoder* streaming,
                   Vector<const uint8_t> bytes) override;

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

  virtual std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) = 0;

 protected:
  uint8_t byte_buffer_[kMaxVarInt32Size];
  // The maximum valid value decoded in this state. {Next} returns an error if
  // this value is exceeded.
  const size_t max_value_;
  const char* const field_name_;
  size_t value_ = 0;
  size_t bytes_consumed_ = 0;
};

class StreamingDecoder::DecodeModuleHeader : public DecodingState {
 public:
  Vector<uint8_t> buffer() override { return ArrayVector(byte_buffer_); }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  // Checks if the magic bytes of the module header are correct.
  void CheckHeader(Decoder* decoder);

  // The size of the module header.
  static constexpr size_t kModuleHeaderSize = 8;
  uint8_t byte_buffer_[kModuleHeaderSize];
};

class StreamingDecoder::DecodeSectionID : public DecodingState {
 public:
  explicit DecodeSectionID(uint32_t module_offset)
      : module_offset_(module_offset) {}

  Vector<uint8_t> buffer() override { return {&id_, 1}; }
  bool is_finishing_allowed() const override { return true; }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  uint8_t id_ = 0;
  // The start offset of this section in the module.
  const uint32_t module_offset_;
};

class StreamingDecoder::DecodeSectionLength : public DecodeVarInt32 {
 public:
  explicit DecodeSectionLength(uint8_t id, uint32_t module_offset)
      : DecodeVarInt32(kV8MaxWasmModuleSize, "section length"),
        section_id_(id),
        module_offset_(module_offset) {}

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  const uint8_t section_id_;
  // The start offset of this section in the module.
  const uint32_t module_offset_;
};

class StreamingDecoder::DecodeSectionPayload : public DecodingState {
 public:
  explicit DecodeSectionPayload(SectionBuffer* section_buffer)
      : section_buffer_(section_buffer) {}

  Vector<uint8_t> buffer() override { return section_buffer_->payload(); }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
};

class StreamingDecoder::DecodeNumberOfFunctions : public DecodeVarInt32 {
 public:
  explicit DecodeNumberOfFunctions(SectionBuffer* section_buffer)
      : DecodeVarInt32(kV8MaxWasmFunctions, "functions count"),
        section_buffer_(section_buffer) {}

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
};

class StreamingDecoder::DecodeFunctionLength : public DecodeVarInt32 {
 public:
  explicit DecodeFunctionLength(SectionBuffer* section_buffer,
                                size_t buffer_offset,
                                size_t num_remaining_functions)
      : DecodeVarInt32(kV8MaxWasmFunctionSize, "body size"),
        section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        // We are reading a new function, so one function less is remaining.
        num_remaining_functions_(num_remaining_functions - 1) {
    DCHECK_GT(num_remaining_functions, 0);
  }

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
  const size_t buffer_offset_;
  const size_t num_remaining_functions_;
};

class StreamingDecoder::DecodeFunctionBody : public DecodingState {
 public:
  explicit DecodeFunctionBody(SectionBuffer* section_buffer,
                              size_t buffer_offset, size_t function_body_length,
                              size_t num_remaining_functions,
                              uint32_t module_offset)
      : section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        function_body_length_(function_body_length),
        num_remaining_functions_(num_remaining_functions),
        module_offset_(module_offset) {}

  Vector<uint8_t> buffer() override {
    Vector<uint8_t> remaining_buffer =
        section_buffer_->bytes() + buffer_offset_;
    return remaining_buffer.SubVector(0, function_body_length_);
  }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
  const size_t buffer_offset_;
  const size_t function_body_length_;
  const size_t num_remaining_functions_;
  const uint32_t module_offset_;
};

size_t StreamingDecoder::DecodeVarInt32::ReadBytes(
    StreamingDecoder* streaming, Vector<const uint8_t> bytes) {
  Vector<uint8_t> buf = buffer();
  Vector<uint8_t> remaining_buf = buf + offset();
  size_t new_bytes = std::min(bytes.size(), remaining_buf.size());
  TRACE_STREAMING("ReadBytes of a VarInt\n");
  memcpy(remaining_buf.start(), &bytes.first(), new_bytes);
  buf.Truncate(offset() + new_bytes);
  Decoder decoder(buf, streaming->module_offset());
  value_ = decoder.consume_u32v(field_name_);
  // The number of bytes we actually needed to read.
  DCHECK_GT(decoder.pc(), buffer().start());
  bytes_consumed_ = static_cast<size_t>(decoder.pc() - buf.start());
  TRACE_STREAMING("  ==> %zu bytes consumed\n", bytes_consumed_);

  if (decoder.failed()) {
    if (new_bytes == remaining_buf.size()) {
      // We only report an error if we read all bytes.
      streaming->Error(decoder.toResult(nullptr));
    }
    set_offset(offset() + new_bytes);
    return new_bytes;
  }

  // We read all the bytes we needed.
  DCHECK_GT(bytes_consumed_, offset());
  new_bytes = bytes_consumed_ - offset();
  // Set the offset to the buffer size to signal that we are at the end of this
  // section.
  set_offset(buffer().size());
  return new_bytes;
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeVarInt32::Next(StreamingDecoder* streaming) {
  if (!streaming->ok()) return nullptr;

  if (value_ > max_value_) {
    std::ostringstream oss;
    oss << "function size > maximum function size: " << value_ << " < "
        << max_value_;
    return streaming->Error(oss.str());
  }

  return NextWithValue(streaming);
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeModuleHeader::Next(StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeModuleHeader\n");
  streaming->ProcessModuleHeader();
  if (!streaming->ok()) return nullptr;
  return base::make_unique<DecodeSectionID>(streaming->module_offset());
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionID::Next(StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionID: %s section\n",
                  SectionName(static_cast<SectionCode>(id_)));
  return base::make_unique<DecodeSectionLength>(id_, module_offset_);
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionLength::NextWithValue(
    StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionLength(%zu)\n", value_);
  SectionBuffer* buf =
      streaming->CreateNewBuffer(module_offset_, section_id_, value_,
                                 buffer().SubVector(0, bytes_consumed_));
  if (!buf) return nullptr;
  if (value_ == 0) {
    if (section_id_ == SectionCode::kCodeSectionCode) {
      return streaming->Error("Code section cannot have size 0");
    }
    streaming->ProcessSection(buf);
    if (!streaming->ok()) return nullptr;
    // There is no payload, we go to the next section immediately.
    return base::make_unique<DecodeSectionID>(streaming->module_offset_);
  } else {
    if (section_id_ == SectionCode::kCodeSectionCode) {
      // We reached the code section. All functions of the code section are put
      // into the same SectionBuffer.
      return base::make_unique<DecodeNumberOfFunctions>(buf);
    }
    return base::make_unique<DecodeSectionPayload>(buf);
  }
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionPayload::Next(StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionPayload\n");
  streaming->ProcessSection(section_buffer_);
  if (!streaming->ok()) return nullptr;
  return base::make_unique<DecodeSectionID>(streaming->module_offset());
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeNumberOfFunctions::NextWithValue(
    StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeNumberOfFunctions(%zu)\n", value_);
  // Copy the bytes we read into the section buffer.
  Vector<uint8_t> payload_buf = section_buffer_->payload();
  if (payload_buf.size() < bytes_consumed_) {
    return streaming->Error("Invalid code section length");
  }
  memcpy(payload_buf.start(), buffer().start(), bytes_consumed_);

  // {value} is the number of functions.
  if (value_ == 0) {
    if (payload_buf.size() != bytes_consumed_) {
      return streaming->Error("not all code section bytes were consumed");
    }
    return base::make_unique<DecodeSectionID>(streaming->module_offset());
  }

  streaming->StartCodeSection(value_, streaming->section_buffers_.back());
  if (!streaming->ok()) return nullptr;
  return base::make_unique<DecodeFunctionLength>(
      section_buffer_, section_buffer_->payload_offset() + bytes_consumed_,
      value_);
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeFunctionLength::NextWithValue(
    StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeFunctionLength(%zu)\n", value_);
  // Copy the bytes we consumed into the section buffer.
  Vector<uint8_t> fun_length_buffer = section_buffer_->bytes() + buffer_offset_;
  if (fun_length_buffer.size() < bytes_consumed_) {
    return streaming->Error("Invalid code section length");
  }
  memcpy(fun_length_buffer.start(), buffer().start(), bytes_consumed_);

  // {value} is the length of the function.
  if (value_ == 0) return streaming->Error("Invalid function length (0)");

  if (buffer_offset_ + bytes_consumed_ + value_ > section_buffer_->length()) {
    return streaming->Error("not enough code section bytes");
  }

  return base::make_unique<DecodeFunctionBody>(
      section_buffer_, buffer_offset_ + bytes_consumed_, value_,
      num_remaining_functions_, streaming->module_offset());
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeFunctionBody::Next(StreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeFunctionBody\n");
  streaming->ProcessFunctionBody(buffer(), module_offset_);
  if (!streaming->ok()) return nullptr;

  size_t end_offset = buffer_offset_ + function_body_length_;
  if (num_remaining_functions_ > 0) {
    return base::make_unique<DecodeFunctionLength>(section_buffer_, end_offset,
                                                   num_remaining_functions_);
  }
  // We just read the last function body. Continue with the next section.
  if (end_offset != section_buffer_->length()) {
    return streaming->Error("not all code section bytes were used");
  }
  return base::make_unique<DecodeSectionID>(streaming->module_offset());
}

StreamingDecoder::StreamingDecoder(
    std::unique_ptr<StreamingProcessor> processor)
    : processor_(std::move(processor)),
      // A module always starts with a module header.
      state_(new DecodeModuleHeader()) {}

StreamingDecoder::SectionBuffer* StreamingDecoder::CreateNewBuffer(
    uint32_t module_offset, uint8_t section_id, size_t length,
    Vector<const uint8_t> length_bytes) {
  // Check the order of sections. Unknown sections can appear at any position.
  if (section_id != kUnknownSectionCode) {
    if (section_id < next_section_id_) {
      Error("Unexpected section");
      return nullptr;
    }
    next_section_id_ = section_id + 1;
  }
  section_buffers_.emplace_back(std::make_shared<SectionBuffer>(
      module_offset, section_id, length, length_bytes));
  return section_buffers_.back().get();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE_STREAMING
