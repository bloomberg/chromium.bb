// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SMBFS_FILE_PATH_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_SMBFS_FILE_PATH_MOJOM_TRAITS_H_

#include <string>

#include "base/files/file_path.h"
#include "chromeos/components/smbfs/mojom/file_path.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<smbfs::mojom::FilePathDataView, base::FilePath> {
  static std::string path(const base::FilePath& path);

  static bool Read(smbfs::mojom::FilePathDataView data, base::FilePath* out);
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_SMBFS_FILE_PATH_MOJOM_TRAITS_H_
