// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/message.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/safe_math.h"

namespace ipcz {

namespace {

// Helper to transform a driver object attached to `message` into its serialized
// form within the message by running it through the driver's serializer.
//
// Metadata is placed into a DriverObjectData structure at `data_offset` bytes
// from the begining of the message. Serialized data bytes are stored in an
// array appended to `message` and referenced by the DriverObjectData, and any
// transmissible handles emitted by the driver are appended to
// `transmissible_handles`, with relevant index and count also stashed in the
// DriverObjectData.
IpczResult SerializeDriverObject(
    DriverObject object,
    const DriverTransport& transport,
    Message& message,
    internal::DriverObjectData& data,
    absl::InlinedVector<IpczDriverHandle, 2>& transmissible_handles) {
  if (!object.is_valid()) {
    // This is not a valid driver handle and it cannot be serialized.
    data.num_driver_handles = 0;
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  uint32_t driver_data_array = 0;
  DriverObject::SerializedDimensions dimensions =
      object.GetSerializedDimensions(transport);
  if (dimensions.num_bytes > 0) {
    driver_data_array = message.AllocateArray<uint8_t>(dimensions.num_bytes);
  }

  const uint32_t first_handle =
      static_cast<uint32_t>(transmissible_handles.size());
  absl::Span<uint8_t> driver_data =
      message.GetArrayView<uint8_t>(driver_data_array);
  data.driver_data_array = driver_data_array;
  data.num_driver_handles = dimensions.num_driver_handles;
  data.first_driver_handle = first_handle;

  transmissible_handles.resize(transmissible_handles.size() +
                               dimensions.num_driver_handles);

  auto handles_view = absl::MakeSpan(transmissible_handles);
  object.Serialize(
      transport, driver_data,
      handles_view.subspan(first_handle, dimensions.num_driver_handles));
  return IPCZ_RESULT_OK;
}

// Returns `true` if and only if it will be safe to use GetArrayView() to access
// the contents of a serialized array beginning at `array_offset` bytes from
// the start of `message`, where each element is `element_size` bytes wide.
bool IsArrayValid(Message& message,
                  uint32_t array_offset,
                  size_t element_size) {
  if (array_offset == 0) {
    return true;
  }

  const absl::Span<uint8_t> data = message.data_view();
  if (array_offset >= data.size()) {
    return false;
  }

  size_t bytes_available = data.size() - array_offset;
  if (bytes_available < sizeof(internal::ArrayHeader)) {
    return false;
  }

  auto& header = *reinterpret_cast<internal::ArrayHeader*>(&data[array_offset]);
  if (bytes_available < header.num_bytes ||
      header.num_bytes < sizeof(internal::ArrayHeader)) {
    return false;
  }

  size_t max_num_elements =
      (header.num_bytes - sizeof(internal::ArrayHeader)) / element_size;
  if (header.num_elements > max_num_elements) {
    return false;
  }

  return true;
}

// Deserializes a driver object encoded within `message`, returning the object
// on success. On failure, an invalid DriverObject is returned.
DriverObject DeserializeDriverObject(
    Message& message,
    const internal::DriverObjectData& object_data,
    absl::Span<const IpczDriverHandle> handles,
    const DriverTransport& transport) {
  if (!IsArrayValid(message, object_data.driver_data_array, sizeof(uint8_t))) {
    return {};
  }

  auto driver_data =
      message.GetArrayView<uint8_t>(object_data.driver_data_array);
  if (object_data.num_driver_handles > handles.size()) {
    return {};
  }

  if (handles.size() - object_data.num_driver_handles <
      object_data.first_driver_handle) {
    return {};
  }

  return DriverObject::Deserialize(
      transport, driver_data,
      handles.subspan(object_data.first_driver_handle,
                      object_data.num_driver_handles));
}

}  // namespace

Message::Message() = default;

Message::Message(uint8_t message_id, size_t params_size)
    : data_(sizeof(internal::MessageHeader) + params_size) {
  internal::MessageHeader& h = header();
  h.size = sizeof(h);
  h.version = 0;
  h.message_id = message_id;
  h.driver_object_data_array = 0;
}

Message::~Message() = default;

uint32_t Message::AllocateGenericArray(size_t element_size,
                                       size_t num_elements) {
  if (num_elements == 0) {
    return 0;
  }
  size_t offset = Align(data_.size());
  size_t num_bytes = Align(CheckAdd(sizeof(internal::ArrayHeader),
                                    CheckMul(element_size, num_elements)));
  data_.resize(CheckAdd(offset, num_bytes));
  auto& header = *reinterpret_cast<internal::ArrayHeader*>(&data_[offset]);
  header.num_bytes = checked_cast<uint32_t>(num_bytes);
  header.num_elements = checked_cast<uint32_t>(num_elements);
  return offset;
}

uint32_t Message::AppendDriverObject(DriverObject object) {
  const uint32_t index = checked_cast<uint32_t>(driver_objects_.size());
  driver_objects_.push_back(std::move(object));
  return index;
}

internal::DriverObjectArrayData Message::AppendDriverObjects(
    absl::Span<DriverObject> objects) {
  const internal::DriverObjectArrayData data = {
      .first_object_index = checked_cast<uint32_t>(driver_objects_.size()),
      .num_objects = checked_cast<uint32_t>(objects.size()),
  };
  driver_objects_.reserve(driver_objects_.size() + objects.size());
  for (auto& object : objects) {
    driver_objects_.push_back(std::move(object));
  }
  return data;
}

DriverObject Message::TakeDriverObject(uint32_t index) {
  // Note that `index` has already been validated by now.
  ABSL_HARDENING_ASSERT(index < driver_objects_.size());
  return std::move(driver_objects_[index]);
}

absl::Span<DriverObject> Message::GetDriverObjectArrayView(
    const internal::DriverObjectArrayData& data) {
  return absl::MakeSpan(driver_objects_)
      .subspan(data.first_object_index, data.num_objects);
}

bool Message::CanTransmitOn(const DriverTransport& transport) {
  for (DriverObject& object : driver_objects_) {
    if (!object.CanTransmitOn(transport)) {
      return false;
    }
  }
  return true;
}

void Message::Serialize(const DriverTransport& transport) {
  ABSL_ASSERT(CanTransmitOn(transport));
  if (driver_objects_.empty()) {
    return;
  }

  const uint32_t array_offset =
      AllocateArray<internal::DriverObjectData>(driver_objects_.size());
  header().driver_object_data_array = array_offset;

  // NOTE: In Chromium, a vast majority of IPC messages have 0, 1, or 2 OS
  // handles attached. Since these objects are small, we inline some storage on
  // the stack to avoid some heap allocation in the most common cases.
  absl::InlinedVector<IpczDriverHandle, 2> transmissible_handles;
  for (size_t i = 0; i < driver_objects().size(); ++i) {
    internal::DriverObjectData data = {};
    const IpczResult result =
        SerializeDriverObject(std::move(driver_objects()[i]), transport, *this,
                              data, transmissible_handles);
    ABSL_ASSERT(result == IPCZ_RESULT_OK);
    GetArrayView<internal::DriverObjectData>(array_offset)[i] = data;
  }

  transmissible_driver_handles_ = std::move(transmissible_handles);
}

bool Message::DeserializeUnknownType(const DriverTransport::RawMessage& message,
                                     const DriverTransport& transport) {
  // Copy the data into a local message object to avoid any TOCTOU issues in
  // case `data` is in unsafe shared memory.
  data_.resize(message.data.size());
  memcpy(data_.data(), message.data.data(), message.data.size());

  // Validate the header. The message must at least be large enough to encode a
  // v0 MessageHeader, and the encoded header size and version must make sense
  // (e.g. version 0 size must be sizeof(MessageHeader))
  if (data_.size() < sizeof(internal::MessageHeaderV0)) {
    return false;
  }

  const auto& message_header =
      *reinterpret_cast<const internal::MessageHeaderV0*>(data_.data());
  if (message_header.version == 0) {
    if (message_header.size != sizeof(internal::MessageHeaderV0)) {
      return false;
    }
  } else {
    if (message_header.size < sizeof(internal::MessageHeaderV0)) {
      return false;
    }
  }

  if (message_header.size > data_.size()) {
    return false;
  }

  // Validate and deserialize the DriverObject array.
  const uint32_t driver_object_array_offset =
      message_header.driver_object_data_array;
  bool all_driver_objects_ok = true;
  if (driver_object_array_offset > 0) {
    if (!IsArrayValid(*this, driver_object_array_offset,
                      sizeof(internal::DriverObjectData))) {
      // The header specified an invalid DriverObjectData array offset, or the
      // array itself was invalid or out-of-bounds.
      return false;
    }

    auto driver_object_data =
        GetArrayView<internal::DriverObjectData>(driver_object_array_offset);
    driver_objects_.reserve(driver_object_data.size());
    for (const internal::DriverObjectData& object_data : driver_object_data) {
      DriverObject object = DeserializeDriverObject(*this, object_data,
                                                    message.handles, transport);
      if (object.is_valid()) {
        driver_objects_.push_back(std::move(object));
      } else {
        // We don't fail immediately so we can try to deserialize the remaining
        // objects anyway, since doing so may free additional resources.
        all_driver_objects_ok = false;
      }
    }
  }

  return all_driver_objects_ok;
}

bool Message::DeserializeFromTransport(
    size_t params_size,
    uint32_t params_current_version,
    absl::Span<const internal::ParamMetadata> params_metadata,
    const DriverTransport::RawMessage& message,
    const DriverTransport& transport) {
  if (!DeserializeUnknownType(message, transport)) {
    return false;
  }

  // Validate parameter data. There must be at least enough bytes following the
  // header to encode a StructHeader and to account for all parameter data.
  absl::Span<uint8_t> params_data = params_data_view();
  if (params_data.size() < sizeof(internal::StructHeader)) {
    return false;
  }

  auto& params_header =
      *reinterpret_cast<internal::StructHeader*>(params_data.data());
  if (params_current_version < params_header.version) {
    params_header.version = params_current_version;
  }

  // The param struct's header claims to consist of more data than is present in
  // the message. Not good.
  if (params_data.size() < params_header.size) {
    return false;
  }

  // NOTE: In Chromium, a vast majority of IPC messages have 0, 1, or 2 OS
  // handles attached. Since these objects are small, we inline some storage on
  // the stack to avoid some heap allocation in the most common cases.
  absl::InlinedVector<bool, 2> is_object_claimed(driver_objects_.size());

  // Finally, validate each parameter and claim driver objects. We track the
  // index of every object claimed by a parameter to ensure that no object is
  // claimed more than once.
  //
  // Note that it is not an error for some objects to go unclaimed, as they may
  // be provided for fields from a newer version of the protocol that isn't
  // known to this receipient.
  for (const internal::ParamMetadata& param : params_metadata) {
    if (param.offset >= params_header.size ||
        param.offset + param.size > params_header.size) {
      return false;
    }

    if (param.array_element_size > 0) {
      const uint32_t array_offset =
          *reinterpret_cast<uint32_t*>(&params_data[param.offset]);
      if (!IsArrayValid(*this, array_offset, param.array_element_size)) {
        return false;
      }
    }

    switch (param.type) {
      case internal::ParamType::kDriverObject: {
        const uint32_t index = GetParamValueAt<uint32_t>(param.offset);
        if (is_object_claimed[index]) {
          return false;
        }
        is_object_claimed[index] = true;
        break;
      }

      case internal::ParamType::kDriverObjectArray: {
        const internal::DriverObjectArrayData array_data =
            GetParamValueAt<internal::DriverObjectArrayData>(param.offset);
        const size_t begin = array_data.first_object_index;
        for (size_t i = begin; i < begin + array_data.num_objects; ++i) {
          if (is_object_claimed[i]) {
            return false;
          }
          is_object_claimed[i] = true;
        }
        break;
      }

      default:
        break;
    }
  }

  return true;
}

}  // namespace ipcz
