// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_INTERFACES_TYPEMAPS_PUP_STRUCT_TRAITS_H_
#define CHROME_CHROME_CLEANER_INTERFACES_TYPEMAPS_PUP_STRUCT_TRAITS_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/interfaces/pup.mojom.h"
#include "chrome/chrome_cleaner/interfaces/string16_embedded_nulls.mojom.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::RegKeyPathDataView,
                    chrome_cleaner::RegKeyPath> {
  static HANDLE rootkey(const chrome_cleaner::RegKeyPath& reg_key_path);

  static base::string16 subkey(const chrome_cleaner::RegKeyPath& reg_key_path);

  static chrome_cleaner::mojom::Wow64Access wow64access(
      const chrome_cleaner::RegKeyPath& reg_key_path);

  static bool Read(chrome_cleaner::mojom::RegKeyPathDataView view,
                   chrome_cleaner::RegKeyPath* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::RegistryFootprintDataView,
                    chrome_cleaner::PUPData::RegistryFootprint> {
  static chrome_cleaner::RegKeyPath key_path(
      const chrome_cleaner::PUPData::RegistryFootprint& reg_footprint);

  static chrome_cleaner::String16EmbeddedNulls value_name(
      const chrome_cleaner::PUPData::RegistryFootprint& reg_footprint);

  static chrome_cleaner::String16EmbeddedNulls value_substring(
      const chrome_cleaner::PUPData::RegistryFootprint& reg_footprint);

  static uint32_t rule(
      const chrome_cleaner::PUPData::RegistryFootprint& reg_footprint);

  static bool Read(chrome_cleaner::mojom::RegistryFootprintDataView view,
                   chrome_cleaner::PUPData::RegistryFootprint* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                    chrome_cleaner::UwS::TraceLocation> {
  static int32_t value(const chrome_cleaner::UwS::TraceLocation& location);

  static bool Read(chrome_cleaner::mojom::TraceLocationDataView view,
                   chrome_cleaner::UwS::TraceLocation* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::FileInfoDataView,
                    chrome_cleaner::PUPData::FileInfo> {
  static const std::set<chrome_cleaner::UwS::TraceLocation>& found_in(
      const chrome_cleaner::PUPData::FileInfo& info);

  static bool Read(chrome_cleaner::mojom::FileInfoDataView view,
                   chrome_cleaner::PUPData::FileInfo* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::PUPDataView,
                    chrome_cleaner::PUPData::PUP> {
  // It's safe to return a reference, since the PUP object outlives the Mojo
  // struct object.
  static const chrome_cleaner::UnorderedFilePathSet& expanded_disk_footprints(
      const chrome_cleaner::PUPData::PUP& pup);

  // It's safe to return a reference, since the PUP object outlives the Mojo
  // struct object.
  static const std::vector<chrome_cleaner::PUPData::RegistryFootprint>&
  expanded_registry_footprints(const chrome_cleaner::PUPData::PUP& pup);

  // It's safe to return a reference, since the PUP object outlives the Mojo
  // struct object.
  static const std::vector<base::string16>& expanded_scheduled_tasks(
      const chrome_cleaner::PUPData::PUP& pup);

  static const chrome_cleaner::PUPData::PUP::FileInfoMap::MapType&
  disk_footprints_info(const chrome_cleaner::PUPData::PUP& pup);

  static bool Read(chrome_cleaner::mojom::PUPDataView view,
                   chrome_cleaner::PUPData::PUP* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_INTERFACES_TYPEMAPS_PUP_STRUCT_TRAITS_H_
