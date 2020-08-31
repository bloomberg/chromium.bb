// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_SOURCE_H_

#include <memory>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"

namespace blink {

class ExceptionState;
class XRInputSourceArray;
class XRSession;
class XRTransientInputHitTestResult;

class XRTransientInputHitTestSource : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRTransientInputHitTestSource(uint64_t id, XRSession* xr_session);

  uint64_t id() const;

  void cancel(ExceptionState& exception_state);

  void Update(
      const HashMap<uint32_t, Vector<device::mojom::blink::XRHitResultPtr>>&
          hit_test_results,
      XRInputSourceArray* input_source_array);

  HeapVector<Member<XRTransientInputHitTestResult>> Results();

  void Trace(Visitor*) override;

 private:
  HeapVector<Member<XRTransientInputHitTestResult>> current_frame_results_;

  const uint64_t id_;
  Member<XRSession> xr_session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_TRANSIENT_INPUT_HIT_TEST_SOURCE_H_
