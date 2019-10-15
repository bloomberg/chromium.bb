// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_SOURCE_H_

#include <memory>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRHitTestOptions;
class XRHitTestResult;

class XRHitTestSource : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRHitTestSource(uint32_t id, XRHitTestOptions* options);

  uint32_t id() const;

  XRHitTestOptions* hitTestOptions() const;

  // Returns a vector of XRHitTestResults that were obtained during last frame
  // update. This method is not exposed to JavaScript.
  HeapVector<Member<XRHitTestResult>> Results();

  void Update(const WTF::Vector<device::mojom::blink::XRHitResultPtr>&
                  hit_test_results);

  void Trace(blink::Visitor*) override;

 private:
  const uint32_t id_;

  Member<XRHitTestOptions> options_;

  Vector<std::unique_ptr<TransformationMatrix>> last_frame_results_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_TEST_SOURCE_H_
