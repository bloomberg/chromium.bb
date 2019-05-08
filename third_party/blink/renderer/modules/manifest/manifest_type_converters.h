// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_

#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
struct Manifest;
}

namespace mojo {

// TODO(crbug.com/704441): These converters are temporary to help the manifest
// migration until ManifestParser is changed to produce generated structs
// directly.

template <>
struct MODULES_EXPORT
    TypeConverter<blink::mojom::blink::ManifestPtr, const blink::Manifest*> {
  static blink::mojom::blink::ManifestPtr Convert(const blink::Manifest* input);
};

template <>
struct TypeConverter<blink::mojom::blink::ManifestImageResourcePtr,
                     const blink::Manifest::ImageResource*> {
  static blink::mojom::blink::ManifestImageResourcePtr Convert(
      const blink::Manifest::ImageResource* input);
};

template <>
struct TypeConverter<blink::mojom::blink::ManifestShareTargetPtr,
                     const blink::Manifest::ShareTarget*> {
  static blink::mojom::blink::ManifestShareTargetPtr Convert(
      const blink::Manifest::ShareTarget* input);
};

template <>
struct TypeConverter<blink::mojom::blink::ManifestShareTargetParamsPtr,
                     const blink::Manifest::ShareTargetParams*> {
  static blink::mojom::blink::ManifestShareTargetParamsPtr Convert(
      const blink::Manifest::ShareTargetParams* input);
};

template <>
struct TypeConverter<blink::mojom::blink::ManifestFileFilterPtr,
                     const blink::Manifest::FileFilter*> {
  static blink::mojom::blink::ManifestFileFilterPtr Convert(
      const blink::Manifest::FileFilter* input);
};

template <>
struct TypeConverter<blink::mojom::blink::ManifestRelatedApplicationPtr,
                     const blink::Manifest::RelatedApplication*> {
  static blink::mojom::blink::ManifestRelatedApplicationPtr Convert(
      const blink::Manifest::RelatedApplication* input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_TYPE_CONVERTERS_H_
