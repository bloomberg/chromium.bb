// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

#include "build/build_config.h"

namespace service_manager {

Manifest::Manifest() = default;
Manifest::Manifest(const Manifest&) = default;
Manifest::Manifest(Manifest&&) = default;
Manifest::~Manifest() = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest& Manifest::operator=(Manifest&&) = default;

Manifest::Options::Options() = default;
Manifest::Options::Options(const Options&) = default;
Manifest::Options::Options(Options&&) = default;
Manifest::Options::~Options() = default;
Manifest::Options& Manifest::Options::operator=(Options&&) = default;
Manifest::Options& Manifest::Options::operator=(const Options&) = default;

Manifest::ExposedCapability::ExposedCapability() = default;
Manifest::ExposedCapability::ExposedCapability(const ExposedCapability&) =
    default;
Manifest::ExposedCapability::ExposedCapability(ExposedCapability&&) = default;
Manifest::ExposedCapability::ExposedCapability(
    const std::string& capability_name,
    std::set<const char*> interface_names)
    : capability_name(capability_name),
      interface_names(interface_names.begin(), interface_names.end()) {}
Manifest::ExposedCapability::~ExposedCapability() = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    const ExposedCapability&) = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    ExposedCapability&&) = default;

Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability() =
    default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    ExposedInterfaceFilterCapability&&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const std::string& filter_name,
    const std::string& capability_name,
    std::set<const char*> interface_names)
    : filter_name(filter_name),
      capability_name(capability_name),
      interface_names(interface_names.begin(), interface_names.end()) {}
Manifest::ExposedInterfaceFilterCapability::
    ~ExposedInterfaceFilterCapability() = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    ExposedInterfaceFilterCapability&&) = default;

// static
Manifest Manifest::FromValueDeprecated(std::unique_ptr<base::Value> value_ptr) {
  DCHECK(value_ptr);
  base::Value value(std::move(*value_ptr));

  Manifest manifest;
  if (!value.is_dict())
    return manifest;

  const base::Value* name_value = value.FindKey("name");
  if (name_value && name_value->is_string())
    manifest.service_name = name_value->GetString();

  const base::Value* display_name_value = value.FindKey("display_name");
  if (display_name_value && display_name_value->is_string())
    manifest.display_name.raw_string = display_name_value->GetString();

  const base::Value* sandbox_type_value = value.FindKey("sandbox_type");
  if (sandbox_type_value && sandbox_type_value->is_string()) {
    manifest.options.sandbox_type = sandbox_type_value->GetString();
  } else {
    // This is kind of weird, but the default when processing packaged service
    // manifests (which is all we care about now, in practice) is "utility". So
    // assume that if no "sandbox_type" key is present.
    manifest.options.sandbox_type = "utility";
  }

  const base::Value* options_value = value.FindKey("options");
  if (options_value && options_value->is_dict()) {
    const base::Value* can_connect_to_services_as_any_user_value =
        options_value->FindKey("can_connect_to_other_services_as_any_user");
    manifest.options.can_connect_to_instances_in_any_group =
        can_connect_to_services_as_any_user_value &&
        can_connect_to_services_as_any_user_value->is_bool() &&
        can_connect_to_services_as_any_user_value->GetBool();

    const base::Value*
        can_connect_to_other_services_with_any_instance_name_value =
            options_value->FindKey(
                "can_connect_to_other_services_with_any_instance_name");
    manifest.options.can_connect_to_instances_with_any_id =
        can_connect_to_other_services_with_any_instance_name_value &&
        can_connect_to_other_services_with_any_instance_name_value->is_bool() &&
        can_connect_to_other_services_with_any_instance_name_value->GetBool();

    const base::Value* can_create_other_service_instances_value =
        options_value->FindKey("can_create_other_service_instances");
    manifest.options.can_register_other_service_instances =
        can_create_other_service_instances_value &&
        can_create_other_service_instances_value->is_bool() &&
        can_create_other_service_instances_value->GetBool();

    const base::Value* instance_sharing_value =
        options_value->FindKey("instance_sharing");
    if (instance_sharing_value && instance_sharing_value->is_string()) {
      if (instance_sharing_value->GetString() == "singleton") {
        manifest.options.instance_sharing_policy =
            InstanceSharingPolicy::kSingleton;
      } else if (instance_sharing_value->GetString() ==
                     "shared_instance_across_users" ||
                 instance_sharing_value->GetString() ==
                     "shared_across_instance_groups") {
        manifest.options.instance_sharing_policy =
            InstanceSharingPolicy::kSharedAcrossGroups;
      }
    }
  }

  const base::Value* interface_provider_specs_value =
      value.FindKey("interface_provider_specs");
  if (interface_provider_specs_value &&
      interface_provider_specs_value->is_dict()) {
    for (const auto& spec : interface_provider_specs_value->DictItems()) {
      if (!spec.second.is_dict())
        continue;
      const std::string& spec_name = spec.first;
      const bool is_main_spec = (spec_name == "service_manager:connector");
      const base::Value* provides_value = spec.second.FindKey("provides");
      if (provides_value && provides_value->is_dict()) {
        for (const auto& capability : provides_value->DictItems()) {
          if (!capability.second.is_list())
            continue;
          const std::string& capability_name = capability.first;
          std::set<std::string> interface_names;
          for (const auto& interface_name : capability.second.GetList())
            interface_names.insert(interface_name.GetString());
          if (is_main_spec) {
            ExposedCapability capability;
            capability.capability_name = capability_name;
            capability.interface_names = std::move(interface_names);
            manifest.exposed_capabilities.emplace_back(std::move(capability));
          } else {
            ExposedInterfaceFilterCapability capability;
            capability.filter_name = spec_name;
            capability.capability_name = capability_name;
            capability.interface_names = std::move(interface_names);
            manifest.exposed_interface_filter_capabilities.emplace_back(
                std::move(capability));
          }
        }
      }

      const base::Value* requires_value = spec.second.FindKey("requires");
      if (requires_value && requires_value->is_dict()) {
        for (const auto& entry : requires_value->DictItems()) {
          if (!entry.second.is_list())
            continue;
          const std::string& from_service_name = entry.first;
          if (is_main_spec) {
            if (entry.second.GetList().empty()) {
              manifest.required_capabilities.push_back({from_service_name, ""});
            } else {
              for (const auto& capability_name : entry.second.GetList()) {
                manifest.required_capabilities.push_back(
                    {from_service_name, capability_name.GetString()});
              }
            }
          } else {
            for (const auto& capability_name : entry.second.GetList()) {
              manifest.required_interface_filter_capabilities.push_back(
                  {from_service_name, spec_name, capability_name.GetString()});
            }
          }
        }
      }
    }
  }

  const base::Value* required_values_value = value.FindKey("required_files");
  if (required_values_value && required_values_value->is_dict()) {
#if defined(OS_LINUX)
    constexpr const char kRequiredPlatform[] = "linux";
#elif defined(OS_ANDROID)
    constexpr const char kRequiredPlatform[] = "android";
#else
    constexpr const char kRequiredPlatform[] = "none";
#endif
    for (const auto& item : required_values_value->DictItems()) {
      const std::string& key = item.first;
      if (!item.second.is_list())
        continue;

      for (const auto& entry : item.second.GetList()) {
        if (!entry.is_dict())
          continue;
        const base::Value* path_value = entry.FindKey("path");
        const base::Value* platform_value = entry.FindKey("platform");
        if (platform_value && platform_value->is_string() && path_value &&
            path_value->is_string() &&
            platform_value->GetString() == kRequiredPlatform) {
          PreloadedFileInfo info;
          info.key = key;
          info.path = base::FilePath().AppendASCII(path_value->GetString());
          manifest.preloaded_files.emplace_back(std::move(info));
        }
      }
    }
  }

  constexpr const char kServicesKey[] = "services";
  base::Value* services_value = value.FindKey(kServicesKey);
  if (services_value && services_value->is_list()) {
    for (auto& manifest_value : services_value->GetList()) {
      manifest.packaged_services.emplace_back(FromValueDeprecated(
          std::make_unique<base::Value>(std::move(manifest_value))));
    }
  }

  return manifest;
}

Manifest& Manifest::Amend(Manifest other) {
  for (auto& other_capability : other.exposed_capabilities) {
    auto it = std::find_if(
        exposed_capabilities.begin(), exposed_capabilities.end(),
        [&other_capability](const ExposedCapability& capability) {
          return capability.capability_name == other_capability.capability_name;
        });
    if (it != exposed_capabilities.end()) {
      for (auto& name : other_capability.interface_names)
        it->interface_names.emplace(std::move(name));
    } else {
      exposed_capabilities.emplace_back(std::move(other_capability));
    }
  }

  for (auto& other_capability : other.exposed_interface_filter_capabilities) {
    auto it = std::find_if(
        exposed_interface_filter_capabilities.begin(),
        exposed_interface_filter_capabilities.end(),
        [&other_capability](
            const ExposedInterfaceFilterCapability& capability) {
          return capability.capability_name ==
                     other_capability.capability_name &&
                 capability.filter_name == other_capability.filter_name;
        });
    if (it != exposed_interface_filter_capabilities.end()) {
      for (auto& name : other_capability.interface_names)
        it->interface_names.emplace(std::move(name));
    } else {
      exposed_interface_filter_capabilities.emplace_back(
          std::move(other_capability));
    }
  }

  for (auto& capability : other.required_capabilities)
    required_capabilities.emplace_back(std::move(capability));
  for (auto& capability : other.required_interface_filter_capabilities)
    required_interface_filter_capabilities.emplace_back(std::move(capability));
  for (auto& manifest : other.packaged_services)
    packaged_services.emplace_back(std::move(manifest));
  for (auto& file_info : other.preloaded_files)
    preloaded_files.emplace_back(std::move(file_info));

  return *this;
}

}  // namespace service_manager
