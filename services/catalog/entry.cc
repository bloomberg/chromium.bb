// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/files/file_path.h"
#include "services/catalog/store.h"
#include "services/service_manager/public/mojom/interface_provider_spec.mojom.h"

namespace catalog {

Entry::Entry() {}
Entry::Entry(const std::string& name)
    : name_(name),
      display_name_(name) {}
Entry::~Entry() {}

bool Entry::ProvidesCapability(const std::string& capability) const {
  auto it = interface_provider_specs_.find(
      service_manager::mojom::kServiceManager_ConnectorSpec);
  if (it == interface_provider_specs_.end())
    return false;

  const auto& connection_spec = it->second;
  return connection_spec.provides.find(capability) !=
      connection_spec.provides.end();
}

bool Entry::operator==(const Entry& other) const {
  return other.name_ == name_ && other.display_name_ == display_name_ &&
         other.sandbox_type_ == sandbox_type_ &&
         other.interface_provider_specs_ == interface_provider_specs_;
}

void Entry::AddOptions(ServiceOptions options) {
  options_ = std::move(options);
}

void Entry::AddInterfaceProviderSpec(
    const std::string& name,
    service_manager::InterfaceProviderSpec spec) {
  interface_provider_specs_[name] = std::move(spec);
}

void Entry::AddRequiredFilePath(const std::string& name, base::FilePath path) {
  required_file_paths_[name] = std::move(path);
}

}  // catalog

namespace mojo {

// static
catalog::mojom::EntryPtr
    TypeConverter<catalog::mojom::EntryPtr, catalog::Entry>::Convert(
        const catalog::Entry& input) {
  catalog::mojom::EntryPtr result(catalog::mojom::Entry::New());
  result->name = input.name();
  result->display_name = input.display_name();
  return result;
}

}  // namespace mojo
