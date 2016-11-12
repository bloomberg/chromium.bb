// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_converter.h"

#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_property.h"

namespace aura {

namespace {

// Get the aura property key and name for a mus property transport |name|.
bool GetPropertyKeyForTransportName(const std::string& name,
                                    const void** key_out,
                                    const char** name_out) {
  if (name == ui::mojom::WindowManager::kAlwaysOnTop_Property) {
    *key_out = client::kAlwaysOnTopKey;
    *name_out = client::kAlwaysOnTopKey->name;
    return true;
  }
  return false;
}

}  // namespace

PropertyConverter::PropertyConverter() {}

PropertyConverter::~PropertyConverter() {}

bool PropertyConverter::ConvertPropertyForTransport(
    Window* window,
    const void* key,
    std::string* server_property_name,
    std::unique_ptr<std::vector<uint8_t>>* server_property_value) {
  *server_property_name = GetTransportNameForPropertyKey(key);
  if (!server_property_name->empty()) {
    // TODO(msw): Using the int64_t accessor is wasteful for bool, etc.
    const int64_t value = window->GetPropertyInternal(key, 0);
    *server_property_value = base::MakeUnique<std::vector<uint8_t>>(
        mojo::ConvertTo<std::vector<uint8_t>>(value));
    return true;
  }
  DVLOG(2) << "Unknown aura property key: " << key;
  return false;
}

std::string PropertyConverter::GetTransportNameForPropertyKey(const void* key) {
  if (key == client::kAlwaysOnTopKey)
    return ui::mojom::WindowManager::kAlwaysOnTop_Property;
  return std::string();
}

void PropertyConverter::SetPropertyFromTransportValue(
    Window* window,
    const std::string& server_property_name,
    const std::vector<uint8_t>* data) {
  const void* key = nullptr;
  const char* name = nullptr;
  if (GetPropertyKeyForTransportName(server_property_name, &key, &name)) {
    DCHECK(key);
    DCHECK(name);
    // Aura window only supports property types that fit in int64_t.
    CHECK_LE(8u, data->size()) << " Property type not supported: " << key;
    const int64_t value = mojo::ConvertTo<int64_t>(*data);
    // TODO(msw): Should aura::Window just store all properties by name?
    window->SetPropertyInternal(key, name, nullptr, value, 0);
  } else {
    DVLOG(2) << "Unknown mus property name: " << server_property_name;
  }
}

}  // namespace aura
