// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/type_converters.h"

#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

namespace mojo {

base::Optional<blink::XRPlane::Orientation>
TypeConverter<base::Optional<blink::XRPlane::Orientation>,
              device::mojom::blink::XRPlaneOrientation>::
    Convert(const device::mojom::blink::XRPlaneOrientation& orientation) {
  switch (orientation) {
    case device::mojom::blink::XRPlaneOrientation::UNKNOWN:
      return base::nullopt;
    case device::mojom::blink::XRPlaneOrientation::HORIZONTAL:
      return blink::XRPlane::Orientation::kHorizontal;
    case device::mojom::blink::XRPlaneOrientation::VERTICAL:
      return blink::XRPlane::Orientation::kVertical;
  }
}

blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>
TypeConverter<blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>,
              Vector<device::mojom::blink::XRPlanePointDataPtr>>::
    Convert(const Vector<device::mojom::blink::XRPlanePointDataPtr>& vertices) {
  blink::HeapVector<blink::Member<blink::DOMPointReadOnly>> result;

  for (const auto& vertex_data : vertices) {
    result.push_back(blink::DOMPointReadOnly::Create(vertex_data->x, 0.0,
                                                     vertex_data->z, 1.0));
  }

  return result;
}

}  // namespace mojo
