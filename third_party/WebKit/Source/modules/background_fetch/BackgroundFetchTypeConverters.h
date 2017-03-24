// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchTypeConverters_h
#define BackgroundFetchTypeConverters_h

#include "modules/background_fetch/BackgroundFetchOptions.h"
#include "modules/background_fetch/IconDefinition.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

namespace blink {
class BackgroundFetchRegistration;
}

namespace mojo {

template <>
struct TypeConverter<blink::BackgroundFetchRegistration*,
                     blink::mojom::blink::BackgroundFetchRegistrationPtr> {
  static blink::BackgroundFetchRegistration* Convert(
      const blink::mojom::blink::BackgroundFetchRegistrationPtr&);
};

template <>
struct TypeConverter<blink::mojom::blink::BackgroundFetchOptionsPtr,
                     blink::BackgroundFetchOptions> {
  static blink::mojom::blink::BackgroundFetchOptionsPtr Convert(
      const blink::BackgroundFetchOptions&);
};

template <>
struct TypeConverter<blink::IconDefinition,
                     blink::mojom::blink::IconDefinitionPtr> {
  static blink::IconDefinition Convert(
      const blink::mojom::blink::IconDefinitionPtr&);
};

template <>
struct TypeConverter<blink::mojom::blink::IconDefinitionPtr,
                     blink::IconDefinition> {
  static blink::mojom::blink::IconDefinitionPtr Convert(
      const blink::IconDefinition&);
};

}  // namespace mojo

#endif  // BackgroundFetchTypeConverters_h
