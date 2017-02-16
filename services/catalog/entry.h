// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_ENTRY_H_
#define SERVICES_CATALOG_ENTRY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"

namespace base {
class Value;
}

namespace catalog {

// Static information about a service package known to the Catalog.
class Entry {
 public:
  Entry();
  explicit Entry(const std::string& name);
  ~Entry();

  static std::unique_ptr<Entry> Deserialize(const base::Value& manifest_root);

  bool ProvidesCapability(const std::string& capability) const;

  bool operator==(const Entry& other) const;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  const base::FilePath& path() const { return path_; }
  void set_path(const base::FilePath& path) { path_ = path; }

  const std::string& display_name() const { return display_name_; }
  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }

  const Entry* parent() const { return parent_; }
  void set_parent(const Entry* parent) { parent_ = parent; }

  const std::vector<std::unique_ptr<Entry>>& children() const {
    return children_;
  }
  std::vector<std::unique_ptr<Entry>>& children() { return children_; }
  void set_children(std::vector<std::unique_ptr<Entry>>&& children) {
    children_ = std::move(children);
  }

  void AddInterfaceProviderSpec(
      const std::string& name,
      const service_manager::InterfaceProviderSpec& spec);
  const service_manager::InterfaceProviderSpecMap&
      interface_provider_specs() const {
    return interface_provider_specs_;
  }

  void AddRequiredFilePath(const std::string& name, const base::FilePath& path);
  const std::map<std::string, base::FilePath>& required_file_paths() const {
    return required_file_paths_;
  }

 private:
  std::string name_;
  base::FilePath path_;
  std::string display_name_;
  service_manager::InterfaceProviderSpecMap interface_provider_specs_;
  std::map<std::string, base::FilePath> required_file_paths_;
  const Entry* parent_ = nullptr;
  std::vector<std::unique_ptr<Entry>> children_;

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

}  // namespace catalog

namespace mojo {
template <>
struct TypeConverter<service_manager::mojom::ResolveResultPtr,
                     const catalog::Entry*> {
  static service_manager::mojom::ResolveResultPtr Convert(
      const catalog::Entry* input);
};

template<>
struct TypeConverter<catalog::mojom::EntryPtr, catalog::Entry> {
  static catalog::mojom::EntryPtr Convert(const catalog::Entry& input);
};

}  // namespace mojo

#endif  // SERVICES_CATALOG_ENTRY_H_
