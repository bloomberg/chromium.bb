// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_
#define COMPONENTS_VIZ_TEST_TEST_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"

namespace viz {

class TestLatestLocalSurfaceIdLookupDelegate
    : public LatestLocalSurfaceIdLookupDelegate {
 public:
  TestLatestLocalSurfaceIdLookupDelegate();
  ~TestLatestLocalSurfaceIdLookupDelegate() override;

  // LatestLocalSurfaceIdLookupDelegate:
  LocalSurfaceId GetSurfaceAtAggregation(
      const FrameSinkId& frame_sink_id) const override;

  void SetSurfaceIdMap(const SurfaceId& surface_id);

 private:
  base::flat_map<FrameSinkId, LocalSurfaceId> surface_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TestLatestLocalSurfaceIdLookupDelegate);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_LATEST_LOCAL_SURFACE_ID_LOOKUP_DELEGATE_H_
