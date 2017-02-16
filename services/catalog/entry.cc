// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "services/catalog/public/cpp/manifest_parsing_util.h"
#include "services/catalog/store.h"

namespace catalog {
namespace {

#if defined(OS_WIN)
const char kServiceExecutableExtension[] = ".service.exe";
#else
const char kServiceExecutableExtension[] = ".service";
#endif

bool ReadStringSet(const base::ListValue& list_value,
                   std::set<std::string>* string_set) {
  DCHECK(string_set);
  for (const auto& value_value : list_value) {
    std::string value;
    if (!value_value->GetAsString(&value)) {
      LOG(ERROR) << "Entry::Deserialize: list member must be a string";
      return false;
    }
    string_set->insert(value);
  }
  return true;
}

bool ReadStringSetFromValue(const base::Value& value,
                            std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  if (!value.GetAsList(&list_value)) {
    LOG(ERROR) << "Entry::Deserialize: Value must be a list.";
    return false;
  }
  return ReadStringSet(*list_value, string_set);
}

bool BuildInterfaceProviderSpec(
    const base::DictionaryValue& value,
    service_manager::InterfaceProviderSpec* interface_provider_specs) {
  DCHECK(interface_provider_specs);
  const base::DictionaryValue* provides_value = nullptr;
  if (value.HasKey(Store::kInterfaceProviderSpecs_ProvidesKey) &&
      !value.GetDictionary(Store::kInterfaceProviderSpecs_ProvidesKey,
                           &provides_value)) {
    LOG(ERROR) << "Entry::Deserialize: "
               << Store::kInterfaceProviderSpecs_ProvidesKey
               << " must be a dictionary.";
    return false;
  }
  if (provides_value) {
    base::DictionaryValue::Iterator it(*provides_value);
    for(; !it.IsAtEnd(); it.Advance()) {
      service_manager::InterfaceSet interfaces;
      if (!ReadStringSetFromValue(it.value(), &interfaces)) {
        LOG(ERROR) << "Entry::Deserialize: Invalid interface list in provided "
                   << " capabilities dictionary";
        return false;
      }
      interface_provider_specs->provides[it.key()] = interfaces;
    }
  }

  const base::DictionaryValue* requires_value = nullptr;
  if (value.HasKey(Store::kInterfaceProviderSpecs_RequiresKey) &&
      !value.GetDictionary(Store::kInterfaceProviderSpecs_RequiresKey,
                           &requires_value)) {
    LOG(ERROR) << "Entry::Deserialize: "
               << Store::kInterfaceProviderSpecs_RequiresKey
               << " must be a dictionary.";
    return false;
  }
  if (requires_value) {
    base::DictionaryValue::Iterator it(*requires_value);
    for (; !it.IsAtEnd(); it.Advance()) {
      service_manager::CapabilitySet capabilities;
      const base::ListValue* entry_value = nullptr;
      if (!it.value().GetAsList(&entry_value)) {
        LOG(ERROR) << "Entry::Deserialize: "
                   << Store::kInterfaceProviderSpecs_RequiresKey
                   << " entry must be a list.";
        return false;
      }
      if (!ReadStringSet(*entry_value, &capabilities)) {
        LOG(ERROR) << "Entry::Deserialize: Invalid capabilities list in "
                   << "requires dictionary.";
        return false;
      }

      interface_provider_specs->requires[it.key()] = capabilities;
    }
  }
  return true;
}

}  // namespace

Entry::Entry() {}
Entry::Entry(const std::string& name)
    : name_(name),
      display_name_(name) {}
Entry::~Entry() {}

// static
std::unique_ptr<Entry> Entry::Deserialize(const base::Value& manifest_root) {
  const base::DictionaryValue* dictionary_value = nullptr;
  if (!manifest_root.GetAsDictionary(&dictionary_value))
    return nullptr;
  const base::DictionaryValue& value = *dictionary_value;

  auto entry = base::MakeUnique<Entry>();

  // Name.
  std::string name;
  if (!value.GetString(Store::kNameKey, &name)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kNameKey << " key";
    return nullptr;
  }
  if (name.empty()) {
    LOG(ERROR) << "Entry::Deserialize: empty service name.";
    return nullptr;
  }
  entry->set_name(name);

  // By default we assume a standalone service executable. The catalog may
  // override this layer based on configuration external to the service's own
  // manifest.
  base::FilePath module_path;
  base::PathService::Get(base::DIR_MODULE, &module_path);
  entry->set_path(module_path.AppendASCII(name + kServiceExecutableExtension));

  // Human-readable name.
  std::string display_name;
  if (!value.GetString(Store::kDisplayNameKey, &display_name)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kDisplayNameKey << " key";
    return nullptr;
  }
  entry->set_display_name(display_name);

  // InterfaceProvider specs.
  const base::DictionaryValue* interface_provider_specs = nullptr;
  if (!value.GetDictionary(Store::kInterfaceProviderSpecsKey,
                           &interface_provider_specs)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kInterfaceProviderSpecsKey << " key";
    return nullptr;
  }

  base::DictionaryValue::Iterator it(*interface_provider_specs);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* spec_value = nullptr;
    if (!interface_provider_specs->GetDictionary(it.key(), &spec_value)) {
      LOG(ERROR) << "Entry::Deserialize: value of InterfaceProvider map for "
                 << "key: " << it.key() << " not a dictionary.";
      return nullptr;
    }

    service_manager::InterfaceProviderSpec spec;
    if (!BuildInterfaceProviderSpec(*spec_value, &spec)) {
      LOG(ERROR) << "Entry::Deserialize: failed to build InterfaceProvider "
                 << "spec for key: " << it.key();
      return nullptr;
    }
    entry->AddInterfaceProviderSpec(it.key(), spec);
  }

  // Required files.
  base::Optional<RequiredFileMap> required_files =
      catalog::RetrieveRequiredFiles(value);
  DCHECK(required_files);
  for (const auto& iter : *required_files) {
    entry->AddRequiredFilePath(iter.first, iter.second);
  }

  if (value.HasKey(Store::kServicesKey)) {
    const base::ListValue* services = nullptr;
    value.GetList(Store::kServicesKey, &services);
    for (size_t i = 0; i < services->GetSize(); ++i) {
      const base::DictionaryValue* service = nullptr;
      services->GetDictionary(i, &service);
      std::unique_ptr<Entry> child = Entry::Deserialize(*service);
      if (child) {
        child->set_parent(entry.get());
        entry->children().emplace_back(std::move(child));
      }
    }
  }

  return entry;
}

bool Entry::ProvidesCapability(const std::string& capability) const {
  auto it = interface_provider_specs_.find(
      service_manager::mojom::kServiceManager_ConnectorSpec);
  if (it == interface_provider_specs_.end())
    return false;

  auto connection_spec = it->second;
  return connection_spec.provides.find(capability) !=
      connection_spec.provides.end();
}

bool Entry::operator==(const Entry& other) const {
  return other.name_ == name_ &&
         other.display_name_ == display_name_ &&
         other.interface_provider_specs_ == interface_provider_specs_;
}

void Entry::AddInterfaceProviderSpec(
    const std::string& name,
    const service_manager::InterfaceProviderSpec& spec) {
  interface_provider_specs_[name] = spec;
}

void Entry::AddRequiredFilePath(const std::string& name,
                                const base::FilePath& path) {
  required_file_paths_[name] = path;
}

}  // catalog

namespace mojo {

// static
service_manager::mojom::ResolveResultPtr
TypeConverter<service_manager::mojom::ResolveResultPtr, const catalog::Entry*>
    ::Convert(const catalog::Entry* input) {
  service_manager::mojom::ResolveResultPtr result;
  if (input) {
    result = service_manager::mojom::ResolveResult::New();
    result->name = input->name();
    result->interface_provider_specs = input->interface_provider_specs();
    result->package_path = input->path();
  }
  return result;
}

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
