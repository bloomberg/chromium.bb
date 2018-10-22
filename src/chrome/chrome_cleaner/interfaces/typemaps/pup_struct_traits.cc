// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/interfaces/typemaps/pup_struct_traits.h"

#include "chrome/chrome_cleaner/interfaces/typemaps/string16_embedded_nulls_mojom_traits.h"
#include "chrome/chrome_cleaner/interfaces/typemaps/windows_handle_mojom_traits.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "components/chrome_cleaner/public/typemaps/chrome_prompt_struct_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

using chrome_cleaner::mojom::FileInfoDataView;
using chrome_cleaner::mojom::FilePathDataView;
using chrome_cleaner::mojom::PUPDataView;
using chrome_cleaner::mojom::TraceLocationDataView;
using chrome_cleaner::mojom::RegKeyPathDataView;
using chrome_cleaner::mojom::RegistryFootprintDataView;
using chrome_cleaner::FilePathSet;
using chrome_cleaner::PUPData;
using chrome_cleaner::RegKeyPath;
using chrome_cleaner::RegistryMatchRule;
using chrome_cleaner::String16EmbeddedNulls;
using chrome_cleaner::UnorderedFilePathSet;
using mojo_base::mojom::String16DataView;

namespace {

template <typename ValueDataView, typename Value>
bool ReadFromArrayDataView(ArrayDataView<ValueDataView>* array_view,
                           std::vector<Value>* out) {
  for (size_t i = 0; i < array_view->size(); ++i) {
    Value value;
    if (!array_view->Read(i, &value))
      return false;
    out->push_back(value);
  }
  return true;
}

bool ReadFilePathSetFromArrayDataView(
    ArrayDataView<FilePathDataView>* array_view,
    FilePathSet* out) {
  for (size_t i = 0; i < array_view->size(); ++i) {
    base::FilePath file_path;
    if (!array_view->Read(i, &file_path))
      return false;
    out->Insert(file_path);
  }
  return true;
}

}  // namespace

// static
HANDLE
StructTraits<RegKeyPathDataView, RegKeyPath>::rootkey(
    const RegKeyPath& reg_key_path) {
  return reg_key_path.rootkey();
}

// static
base::string16 StructTraits<RegKeyPathDataView, RegKeyPath>::subkey(
    const RegKeyPath& reg_key_path) {
  return reg_key_path.subkey();
}

// static
chrome_cleaner::mojom::Wow64Access
StructTraits<RegKeyPathDataView, RegKeyPath>::wow64access(
    const RegKeyPath& reg_key_path) {
  return static_cast<chrome_cleaner::mojom::Wow64Access>(
      reg_key_path.wow64access());
}

// static
bool StructTraits<RegKeyPathDataView, RegKeyPath>::Read(RegKeyPathDataView view,
                                                        RegKeyPath* out) {
  HANDLE rootkey;
  if (!view.ReadRootkey(&rootkey))
    return false;
  base::string16 subkey;
  if (!view.ReadSubkey(&subkey))
    return false;

  const REGSAM wow64access = static_cast<REGSAM>(view.wow64access());
  if (wow64access != KEY_WOW64_32KEY && wow64access != KEY_WOW64_64KEY &&
      wow64access != 0) {
    return false;
  }
  *out = RegKeyPath(HKEY_LOCAL_MACHINE, subkey, wow64access);
  return true;
}

// static
RegKeyPath
StructTraits<RegistryFootprintDataView, PUPData::RegistryFootprint>::key_path(
    const PUPData::RegistryFootprint& reg_footprint) {
  return reg_footprint.key_path;
}

// static
String16EmbeddedNulls
StructTraits<RegistryFootprintDataView, PUPData::RegistryFootprint>::value_name(
    const PUPData::RegistryFootprint& reg_footprint) {
  return String16EmbeddedNulls(reg_footprint.value_name);
}

// static
String16EmbeddedNulls
StructTraits<RegistryFootprintDataView, PUPData::RegistryFootprint>::
    value_substring(const PUPData::RegistryFootprint& reg_footprint) {
  return String16EmbeddedNulls(reg_footprint.value_substring);
}

// static
uint32_t
StructTraits<RegistryFootprintDataView, PUPData::RegistryFootprint>::rule(
    const PUPData::RegistryFootprint& reg_footprint) {
  return static_cast<uint32_t>(reg_footprint.rule);
}

// static
bool StructTraits<RegistryFootprintDataView, PUPData::RegistryFootprint>::Read(
    RegistryFootprintDataView view,
    PUPData::RegistryFootprint* out) {
  if (!view.ReadKeyPath(&out->key_path))
    return false;

  String16EmbeddedNulls value_name;
  if (!view.ReadValueName(&value_name))
    return false;
  out->value_name = value_name.CastAsStringPiece16().as_string();

  String16EmbeddedNulls value_substring;
  if (!view.ReadValueSubstring(&value_substring))
    return false;
  out->value_substring = value_substring.CastAsStringPiece16().as_string();

  if (!chrome_cleaner::RegistryMatchRule_IsValid(view.rule()))
    return false;
  out->rule = static_cast<RegistryMatchRule>(view.rule());

  return true;
}

// static
int32_t StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                     chrome_cleaner::UwS::TraceLocation>::
    value(const chrome_cleaner::UwS::TraceLocation& location) {
  return static_cast<int32_t>(location);
}

// static
bool StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                  chrome_cleaner::UwS::TraceLocation>::
    Read(chrome_cleaner::mojom::TraceLocationDataView view,
         chrome_cleaner::UwS::TraceLocation* out) {
  if (!chrome_cleaner::UwS::TraceLocation_IsValid(view.value()))
    return false;
  *out = static_cast<chrome_cleaner::UwS::TraceLocation>(view.value());
  return true;
}

// static
const std::set<chrome_cleaner::UwS::TraceLocation>&
StructTraits<FileInfoDataView, PUPData::FileInfo>::found_in(
    const PUPData::FileInfo& info) {
  return info.found_in;
}

// static
bool StructTraits<FileInfoDataView, PUPData::FileInfo>::Read(
    FileInfoDataView view,
    PUPData::FileInfo* out) {
  ArrayDataView<TraceLocationDataView> found_in_view;
  view.GetFoundInDataView(&found_in_view);
  for (size_t i = 0; i < found_in_view.size(); ++i) {
    chrome_cleaner::UwS::TraceLocation location;
    if (!found_in_view.Read(i, &location))
      return false;
    out->found_in.insert(location);
  }
  return true;
}

// static
const UnorderedFilePathSet&
StructTraits<PUPDataView, PUPData::PUP>::expanded_disk_footprints(
    const PUPData::PUP& pup) {
  return pup.expanded_disk_footprints.file_paths();
}

// static
const std::vector<PUPData::RegistryFootprint>&
StructTraits<PUPDataView, PUPData::PUP>::expanded_registry_footprints(
    const PUPData::PUP& pup) {
  return pup.expanded_registry_footprints;
}

// static
const std::vector<base::string16>&
StructTraits<PUPDataView, PUPData::PUP>::expanded_scheduled_tasks(
    const PUPData::PUP& pup) {
  return pup.expanded_scheduled_tasks;
}

// static
const PUPData::PUP::FileInfoMap::MapType&
StructTraits<PUPDataView, PUPData::PUP>::disk_footprints_info(
    const chrome_cleaner::PUPData::PUP& pup) {
  return pup.disk_footprints_info.map();
}

// static
bool StructTraits<PUPDataView, PUPData::PUP>::Read(PUPDataView view,
                                                   PUPData::PUP* out) {
  ArrayDataView<FilePathDataView> disk_view;
  view.GetExpandedDiskFootprintsDataView(&disk_view);
  if (!ReadFilePathSetFromArrayDataView(&disk_view,
                                        &out->expanded_disk_footprints)) {
    return false;
  }

  ArrayDataView<RegistryFootprintDataView> reg_footprints_view;
  view.GetExpandedRegistryFootprintsDataView(&reg_footprints_view);
  if (!ReadFromArrayDataView(&reg_footprints_view,
                             &out->expanded_registry_footprints)) {
    return false;
  }

  ArrayDataView<String16DataView> tasks_view;
  view.GetExpandedScheduledTasksDataView(&tasks_view);
  if (!ReadFromArrayDataView(&tasks_view, &out->expanded_scheduled_tasks)) {
    return false;
  }

  MapDataView<FilePathDataView, FileInfoDataView> disk_footprints_info_view;
  view.GetDiskFootprintsInfoDataView(&disk_footprints_info_view);
  for (size_t i = 0; i < disk_footprints_info_view.size(); ++i) {
    base::FilePath file_path;
    PUPData::FileInfo file_info;
    if (!disk_footprints_info_view.keys().Read(i, &file_path) ||
        !disk_footprints_info_view.values().Read(i, &file_info))
      return false;
    out->disk_footprints_info.Insert(file_path, file_info);
  }

  return true;
}

}  // namespace mojo
