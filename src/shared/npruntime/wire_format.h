// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_WIRE_FORMAT_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_WIRE_FORMAT_H_

#include <map>
#include <new>

namespace nacl {

// NPAPI uses a number of pointer types that need to be passed between
// browser and the NaCl module.  As pointers are not valid across multiple
// address spaces, we translate these types to a neutral format, namely
// WireType.  As the translation provided by this module is expected to
// be used only for long-lived objects, there is no attempt made to
// reuse values.  -1 is reserved as an invalid return value.
//
// Usage of these classes is expected to be from NPAPI methods running on the
// foreground thread only, so it is explicitly not thread safe.

// To be as efficient as possible we try to avoid unnecessary lookups, so
// in many cases we simply cast the native pointer to and from WireType.
// This sort of translation is maintained by NPWireTrivial.
template <typename NativeType, typename WireType> class NPWireTrivial {
 public:
  WireType ToWire(NativeType native) {
    // NULL values are not entered into the table.
    if (NULL == native) {
      return -1;
    }
    return reinterpret_cast<WireType>(native);
  }

  NativeType ToNative(WireType wire) {
    // Error values return NULL.
    if (-1 == wire) {
      return NULL;
    }
    return reinterpret_cast<NativeType>(wire);
  }
};

// The browser side of the connection typically owns such structures as the
// NPP or an NPIdentifier.  So when it chooses to pass a pointer to one of
// these objects, and that passing cannot be done by a simple cast to WireType,
// we generate a unique WireType value and use this value to pass.
template <typename NativeType, typename WireType> class NPWireOwner {
 public:
  NPWireOwner() {
    to_wire_map_ = new(std::nothrow) std::map<NativeType, WireType>;
    to_native_map_ = new(std::nothrow) std::map<WireType, NativeType>;
  }

  ~NPWireOwner() {
    delete to_wire_map_;
    delete to_native_map_;
  }

  bool IsProperlyInitialized() {
    return (NULL != to_wire_map_ && NULL != to_native_map_);
  }

  WireType ToWire(NativeType native) {
    if (!IsProperlyInitialized()) {
      return -1;
    }
    // NULL values are not entered into the table.
    if (NULL == native) {
      return -1;
    }
    static WireType next_wire_value = 0;
    if (to_wire_map_->end() == to_wire_map_->find(native)) {
      if (0 > next_wire_value) {
        // We have wrapped around and consumed all the values.
        return -1;
      }
      SetMapping(native, next_wire_value);
      next_wire_value++;
    }
    return (*to_wire_map_)[native];
  }

  NativeType ToNative(WireType wire) {
    if (!IsProperlyInitialized()) {
      return NULL;
    }
    // Error values return NULL.
    if (-1 == wire) {
      return NULL;
    }
    if (to_native_map_->end() == to_native_map_->find(wire)) {
      // No mapping was created for this value.
      return NULL;
    }
    return (*to_native_map_)[wire];
  }

 private:
  void SetMapping(NativeType native, WireType wire) {
    (*to_native_map_)[wire] = native;
    (*to_wire_map_)[native] = wire;
  }

  std::map<NativeType, WireType>* to_wire_map_;
  std::map<WireType, NativeType>* to_native_map_;
};

// The browser typically passes the values into the NaCl module, and they
// are often used as opaque values, not requiring a translation to or from
// wire format.  If there needs to be a local copy pointer value in the
// NaCl module, the NaCl module must explicitly set the mapping by a call
// to SetLocalCopy.
template <typename NativeType, typename WireType> class NPWireLocalCopy {
 public:
  NPWireLocalCopy() {
    to_wire_map_ = new(std::nothrow) std::map<NativeType, WireType>;
    to_native_map_ = new(std::nothrow) std::map<WireType, NativeType>;
  }

  ~NPWireLocalCopy() {
    delete to_wire_map_;
    delete to_native_map_;
  }

  bool IsProperlyInitialized() {
    return (NULL != to_wire_map_ && NULL != to_native_map_);
  }

  WireType ToWire(NativeType native) {
    if (!IsProperlyInitialized()) {
      return -1;
    }
    // NULL values are not entered into the table.
    if (NULL == native) {
      return -1;
    }
    if (to_wire_map_->end() == to_wire_map_->find(native)) {
      return -1;
    }
    return (*to_wire_map_)[native];
  }

  NativeType ToNative(WireType wire) {
    if (!IsProperlyInitialized()) {
      return NULL;
    }
    // Error values return NULL.
    if (-1 == wire) {
      return NULL;
    }
    if (to_native_map_->end() == to_native_map_->find(wire)) {
      // No mapping was created for this value.
      return NULL;
    }
    return (*to_native_map_)[wire];
  }

  void SetLocalCopy(WireType wire, NativeType native) {
    (*to_native_map_)[wire] = native;
    (*to_wire_map_)[native] = wire;
  }

 private:
  std::map<NativeType, WireType>* to_wire_map_;
  std::map<WireType, NativeType>* to_native_map_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_WIRE_FORMAT_H_
