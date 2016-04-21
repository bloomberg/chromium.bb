// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/values.h"
#include "mojo/util/filename_util.h"
#include "services/catalog/store.h"
#include "services/shell/public/cpp/names.h"
#include "url/gurl.h"

namespace catalog {
namespace {

shell::CapabilitySpec BuildCapabilitiesV0(const base::DictionaryValue& value) {
  shell::CapabilitySpec capabilities;
  base::DictionaryValue::Iterator it(value);
  for (; !it.IsAtEnd(); it.Advance()) {
    const base::ListValue* values = nullptr;
    CHECK(it.value().GetAsList(&values));
    shell::CapabilityRequest spec;
    for (auto i = values->begin(); i != values->end(); ++i) {
      shell::Interface interface_name;
      const base::Value* v = *i;
      CHECK(v->GetAsString(&interface_name));
      spec.interfaces.insert(interface_name);
    }
    capabilities.required[it.key()] = spec;
  }
  return capabilities;
}

void ReadStringSet(const base::ListValue& list_value,
                   std::set<std::string>* string_set) {
  DCHECK(string_set);
  for (auto i = list_value.begin(); i != list_value.end(); ++i) {
    std::string value;
    const base::Value* value_value = *i;
    CHECK(value_value->GetAsString(&value));
    string_set->insert(value);
  }
}

void ReadStringSetFromValue(const base::Value& value,
                            std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  CHECK(value.GetAsList(&list_value));
  ReadStringSet(*list_value, string_set);
}

void ReadStringSetFromDictionary(const base::DictionaryValue& dictionary,
                                 const std::string& key,
                                 std::set<std::string>* string_set) {
  const base::ListValue* list_value = nullptr;
  if (dictionary.HasKey(key))
    CHECK(dictionary.GetList(key, &list_value));
  if (list_value)
    ReadStringSet(*list_value, string_set);
}

shell::CapabilitySpec BuildCapabilitiesV1(const base::DictionaryValue& value) {
  shell::CapabilitySpec capabilities;

  const base::DictionaryValue* provided_value = nullptr;
  if (value.HasKey(Store::kCapabilities_ProvidedKey)) {
    CHECK(value.GetDictionary(Store::kCapabilities_ProvidedKey,
                              &provided_value));
  }
  if (provided_value) {
    shell::CapabilityRequest provided;
    base::DictionaryValue::Iterator it(*provided_value);
    for(; !it.IsAtEnd(); it.Advance()) {
      shell::Interfaces interfaces;
      ReadStringSetFromValue(it.value(), &interfaces);
      capabilities.provided[it.key()] = interfaces;
    }
  }

  const base::DictionaryValue* required_value = nullptr;
  if (value.HasKey(Store::kCapabilities_RequiredKey)) {
    CHECK(value.GetDictionary(Store::kCapabilities_RequiredKey,
                              &required_value));
  }
  if (required_value) {
    base::DictionaryValue::Iterator it(*required_value);
    for (; !it.IsAtEnd(); it.Advance()) {
      shell::CapabilityRequest spec;
      const base::DictionaryValue* entry_value = nullptr;
      CHECK(it.value().GetAsDictionary(&entry_value));
      ReadStringSetFromDictionary(
          *entry_value, Store::kCapabilities_ClassesKey, &spec.classes);
      ReadStringSetFromDictionary(
          *entry_value, Store::kCapabilities_InterfacesKey, &spec.interfaces);
      capabilities.required[it.key()] = spec;
    }
  }
  return capabilities;
}

}  // namespace

Entry::Entry() {}
Entry::Entry(const std::string& name)
    : name_(name), qualifier_(shell::GetNamePath(name)), display_name_(name) {}
Entry::Entry(const Entry& other) = default;
Entry::~Entry() {}

std::unique_ptr<base::DictionaryValue> Entry::Serialize() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetInteger(Store::kManifestVersionKey, 1);
  value->SetString(Store::kNameKey, name_);
  value->SetString(Store::kDisplayNameKey, display_name_);
  value->SetString(Store::kQualifierKey, qualifier_);
  std::unique_ptr<base::DictionaryValue> spec(new base::DictionaryValue);

  std::unique_ptr<base::DictionaryValue> provided(new base::DictionaryValue);
  for (const auto& i : capabilities_.provided) {
    std::unique_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& interface_name : i.second)
      interfaces->AppendString(interface_name);
    provided->Set(i.first, std::move(interfaces));
  }
  spec->Set(Store::kCapabilities_ProvidedKey, std::move(provided));

  std::unique_ptr<base::DictionaryValue> required(new base::DictionaryValue);
  for (const auto& i : capabilities_.required) {
    std::unique_ptr<base::DictionaryValue> request(new base::DictionaryValue);
    std::unique_ptr<base::ListValue> classes(new base::ListValue);
    for (const auto& class_name : i.second.classes)
      classes->AppendString(class_name);
    request->Set(Store::kCapabilities_ClassesKey, std::move(classes));
    std::unique_ptr<base::ListValue> interfaces(new base::ListValue);
    for (const auto& interface_name : i.second.interfaces)
      interfaces->AppendString(interface_name);
    request->Set(Store::kCapabilities_InterfacesKey, std::move(interfaces));
    required->Set(i.first, std::move(request));
  }
  spec->Set(Store::kCapabilities_RequiredKey, std::move(required));

  value->Set(Store::kCapabilitiesKey, std::move(spec));
  return value;
}

// static
std::unique_ptr<Entry> Entry::Deserialize(const base::DictionaryValue& value) {
  std::unique_ptr<Entry> entry(new Entry);
  int manifest_version = 0;
  if (value.HasKey(Store::kManifestVersionKey))
    CHECK(value.GetInteger(Store::kManifestVersionKey, &manifest_version));
  std::string name_string;
  if (!value.GetString(Store::kNameKey, &name_string)) {
    LOG(ERROR) << "Entry::Deserialize: dictionary has no name key";
    return nullptr;
  }
  if (!shell::IsValidName(name_string)) {
    LOG(WARNING) << "Entry::Deserialize: " << name_string << " is not a valid "
                 << "Mojo name";
    return nullptr;
  }
  entry->set_name(name_string);
  if (value.HasKey(Store::kQualifierKey)) {
    std::string qualifier;
    CHECK(value.GetString(Store::kQualifierKey, &qualifier));
    entry->set_qualifier(qualifier);
  } else {
    entry->set_qualifier(shell::GetNamePath(name_string));
  }
  std::string display_name;
  if (!value.GetString(Store::kDisplayNameKey, &display_name)) {
    LOG(WARNING) << "Entry::Deserialize: dictionary has no display_name key";
    return nullptr;
  }
  entry->set_display_name(display_name);
  const base::DictionaryValue* capabilities = nullptr;
  if (!value.GetDictionary(Store::kCapabilitiesKey, &capabilities)) {
    LOG(WARNING) << "Entry::Description: dictionary has no capabilities key";
    return nullptr;
  }
  if (manifest_version == 0)
    entry->set_capabilities(BuildCapabilitiesV0(*capabilities));
  else
    entry->set_capabilities(BuildCapabilitiesV1(*capabilities));

  if (value.HasKey(Store::kApplicationsKey)) {
    const base::ListValue* applications = nullptr;
    value.GetList(Store::kApplicationsKey, &applications);
    for (size_t i = 0; i < applications->GetSize(); ++i) {
      const base::DictionaryValue* application = nullptr;
      applications->GetDictionary(i, &application);
      std::unique_ptr<Entry> child = Entry::Deserialize(*application);
      if (child) {
        child->set_package(entry.get());
        // Caller must assume ownership of these items.
        entry->applications_.insert(child.release());
      }
    }
  }

  return entry;
}

bool Entry::ProvidesClass(const std::string& clazz) const {
  return capabilities_.provided.find(clazz) != capabilities_.provided.end();
}

bool Entry::operator==(const Entry& other) const {
  return other.name_ == name_ && other.qualifier_ == qualifier_ &&
         other.display_name_ == display_name_ &&
         other.capabilities_ == capabilities_;
}

bool Entry::operator<(const Entry& other) const {
  return std::tie(name_, qualifier_, display_name_, capabilities_) <
         std::tie(other.name_, other.qualifier_, other.display_name_,
                  other.capabilities_);
}

}  // catalog

namespace mojo {

// static
shell::mojom::ResolveResultPtr
    TypeConverter<shell::mojom::ResolveResultPtr, catalog::Entry>::Convert(
        const catalog::Entry& input) {
  shell::mojom::ResolveResultPtr result(shell::mojom::ResolveResult::New());
  result->name = input.name();
  const catalog::Entry& package = input.package() ? *input.package() : input;
  result->resolved_name = package.name();
  result->qualifier = input.qualifier();
  result->capabilities =
      shell::mojom::CapabilitySpec::From(input.capabilities());
  result->package_url = mojo::util::FilePathToFileURL(package.path()).spec();
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
