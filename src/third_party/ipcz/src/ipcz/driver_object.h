// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_DRIVER_OBJECT_H_
#define IPCZ_SRC_IPCZ_DRIVER_OBJECT_H_

#include <cstdint>

#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class DriverTransport;
class Node;

// Owns an IpczDriverHandle and exposes a generic interface for serialization
// and deserialization through the driver.
class DriverObject {
 public:
  DriverObject();
  DriverObject(Ref<Node> node, IpczDriverHandle handle);
  DriverObject(DriverObject&&);
  DriverObject& operator=(DriverObject&&);
  ~DriverObject();

  const Ref<Node>& node() const { return node_; }
  IpczDriverHandle handle() const { return handle_; }

  void reset();
  IpczDriverHandle release();

  bool is_valid() const { return handle_ != IPCZ_INVALID_DRIVER_HANDLE; }

  // Indicates whether this DriverObject can be serialized into a collection of
  // data and/or transmissible subobjects for transmission over a driver
  // transport.
  bool IsSerializable() const;

  // Indicates whether this DriverObject is (either as-is, or after some
  // serialization) transmissible over the identified transport.
  bool CanTransmitOn(const DriverTransport& transport) const;

  // Returns the data and transmissible handle capacity required to serialize
  // this object for transmission over the identified transport. Must only be
  // called on a valid object which is transmissible over that transport.
  struct SerializedDimensions {
    size_t num_bytes;
    size_t num_driver_handles;
  };
  SerializedDimensions GetSerializedDimensions(
      const DriverTransport& transport) const;

  // Serializes this object into `data` and `handles` for imminent transmission
  // over `transport`. Both input spans must be at least large enough to support
  // the object's serialized dimensions. Handles placed into `handles` will be
  // transmissible by the driver without further serialization. Must only be
  // called on valid objects which are known to be serializable and
  // transmissible over `transport`.
  void Serialize(const DriverTransport& transport,
                 absl::Span<uint8_t> data,
                 absl::Span<IpczDriverHandle> handles);

  // Attempts to deserialize a driver object from a series of bytes and
  // transmissible driver objects produced by a prior call to Serialize() and
  // received via `transport`. Returns a valid DriverObject on success, or an
  // invalid DriverObject on failure.
  static DriverObject Deserialize(const DriverTransport& transport,
                                  absl::Span<const uint8_t> data,
                                  absl::Span<const IpczDriverHandle> handles);

 private:
  Ref<Node> node_;
  IpczDriverHandle handle_ = IPCZ_INVALID_DRIVER_HANDLE;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_DRIVER_OBJECT_H_
