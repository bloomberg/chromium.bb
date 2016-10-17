// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/values.h"
#include "services/catalog/store.h"
#include "services/service_manager/public/cpp/names.h"

namespace catalog {
namespace {

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

bool BuildInterfaceProviderSpecs(
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
      qualifier_(service_manager::GetNamePath(name)),
      display_name_(name) {}
Entry::~Entry() {}

std::unique_ptr<base::DictionaryValue> Entry::Serialize() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetInteger(Store::kManifestVersionKey, 1);
  value->SetString(Store::kNameKey, name_);
  value->SetString(Store::kDisplayNameKey, display_name_);
  value->SetString(Store::kQualifierKey, qualifier_);
  std::unique_ptr<base::DictionaryValue> spec(new base::DictionaryValue);

  std::unique_ptr<base::DictionaryValue> provides(new base::DictionaryValue);
  for (const auto& i : connection_spec_.provides) {
    std::unique_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& interface_name : i.second)
      interfaces->AppendString(interface_name);
    provides->Set(i.first, std::move(interfaces));
  }
  spec->Set(Store::kInterfaceProviderSpecs_ProvidesKey, std::move(provides));

  std::unique_ptr<base::DictionaryValue> requires(new base::DictionaryValue);
  for (const auto& i : connection_spec_.requires) {
    std::unique_ptr<base::ListValue> capabilities(new base::ListValue);
    for (const auto& class_name : i.second)
      capabilities->AppendString(class_name);
    requires->Set(i.first, std::move(capabilities));
  }
  spec->Set(Store::kInterfaceProviderSpecs_RequiresKey, std::move(requires));

  value->Set(Store::kInterfaceProviderSpecsKey, std::move(spec));
  return value;
}

// static
std::unique_ptr<Entry> Entry::Deserialize(const base::DictionaryValue& value) {
  std::unique_ptr<Entry> entry(new Entry);

  // Manifest version.
  int manifest_version = 0;
  if (!value.GetInteger(Store::kManifestVersionKey, &manifest_version)) {
    LOG(ERROR) << "Entry::Deserialize: " << Store::kManifestVersionKey
               << " must be an integer.";
    return nullptr;
  }
  if (manifest_version != 1) {
    LOG(ERROR) << "Entry::Deserialize: Unsupported value of "
               << Store::kManifestVersionKey << ":" << manifest_version;
    return nullptr;
  }

  // Name.
  std::string name_string;
  if (!value.GetString(Store::kNameKey, &name_string)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no "
               << Store::kNameKey << " key";
    return nullptr;
  }
  if (!service_manager::IsValidName(name_string)) {
    LOG(ERROR) << "Entry::Deserialize: " << name_string << " is not a valid "
               << "Mojo name";
    return nullptr;
  }
  entry->set_name(name_string);

  // Process group.
  if (value.HasKey(Store::kQualifierKey)) {
    std::string qualifier;
    if (!value.GetString(Store::kQualifierKey, &qualifier)) {
      LOG(ERROR) << "Entry::Deserialize: " << Store::kQualifierKey << " must "
                 << "be a string.";
      return nullptr;
    }
    entry->set_qualifier(qualifier);
  } else {
    entry->set_qualifier(service_manager::GetNamePath(name_string));
  }

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

  service_manager::InterfaceProviderSpec spec;
  if (!BuildInterfaceProviderSpecs(*interface_provider_specs, &spec)) {
    LOG(ERROR) << "Entry::Deserialize: failed to build InterfaceProvider spec "
               << "for " << entry->name();
    return nullptr;
  }
  entry->set_connection_spec(spec);

  if (value.HasKey(Store::kServicesKey)) {
    const base::ListValue* services = nullptr;
    value.GetList(Store::kServicesKey, &services);
    for (size_t i = 0; i < services->GetSize(); ++i) {
      const base::DictionaryValue* service = nullptr;
      services->GetDictionary(i, &service);
      std::unique_ptr<Entry> child = Entry::Deserialize(*service);
      if (child) {
        child->set_package(entry.get());
        // Caller must assume ownership of these items.
        entry->children_.emplace_back(std::move(child));
      }
    }
  }

  return entry;
}

bool Entry::ProvidesClass(const std::string& clazz) const {
  return connection_spec_.provides.find(clazz) !=
      connection_spec_.provides.end();
}

bool Entry::operator==(const Entry& other) const {
  return other.name_ == name_ && other.qualifier_ == qualifier_ &&
         other.display_name_ == display_name_ &&
         other.connection_spec_ == connection_spec_;
}

bool Entry::operator<(const Entry& other) const {
  return std::tie(name_, qualifier_, display_name_, connection_spec_) <
         std::tie(other.name_, other.qualifier_, other.display_name_,
                  other.connection_spec_);
}

}  // catalog

namespace mojo {

// static
service_manager::mojom::ResolveResultPtr
TypeConverter<service_manager::mojom::ResolveResultPtr,
              catalog::Entry>::Convert(const catalog::Entry& input) {
  service_manager::mojom::ResolveResultPtr result(
      service_manager::mojom::ResolveResult::New());
  result->name = input.name();
  const catalog::Entry& package = input.package() ? *input.package() : input;
  result->resolved_name = package.name();
  result->qualifier = input.qualifier();
  result->connection_spec = input.connection_spec();
  result->package_path = package.path();
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
