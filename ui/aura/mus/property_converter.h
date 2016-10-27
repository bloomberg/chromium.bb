// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_PROPERTY_CONVERTER_H_
#define UI_AURA_MUS_PROPERTY_CONVERTER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ui/aura/aura_export.h"

namespace aura {

class Window;

// PropertyConverter is used to convert Window properties for transport to the
// mus window server and back. Any time a property changes from one side it is
// mapped to the other using this class. Not all Window properties need to map
// to server properties, and similarly not all transport properties need map to
// Window properties.
class PropertyConverter {
 public:
  virtual ~PropertyConverter() {}

  // Maps a property on the Window to a property pushed to the server. Return
  // true if the property should be sent to the server, false if the property
  // is only used locally.
  virtual bool ConvertPropertyForTransport(
      Window* window,
      const void* key,
      std::string* transport_name,
      std::unique_ptr<std::vector<uint8_t>>* transport_value) = 0;

  // Returns the transport name for a Window property.
  virtual std::string GetTransportNameForPropertyKey(const void* key) = 0;

  // Applies a value from the server to |window|. |transport_name| is the
  // name of the property and |transport_data| the value. |transport_data| may
  // be null.
  virtual void SetPropertyFromTransportValue(
      Window* window,
      const std::string& transport_name,
      const std::vector<uint8_t>* transport_data) = 0;
};

}  // namespace aura

#endif  // UI_AURA_MUS_PROPERTY_CONVERTER_H_
