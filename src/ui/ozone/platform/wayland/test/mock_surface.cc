// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void Attach(wl_client* client,
            wl_resource* resource,
            wl_resource* buffer_resource,
            int32_t x,
            int32_t y) {
  GetUserDataAs<MockSurface>(resource)->Attach(buffer_resource, x, y);
}

void SetOpaqueRegion(wl_client* client,
                     wl_resource* resource,
                     wl_resource* region) {
  GetUserDataAs<MockSurface>(resource)->SetOpaqueRegion(region);
}

void SetInputRegion(wl_client* client,
                    wl_resource* resource,
                    wl_resource* region) {
  GetUserDataAs<MockSurface>(resource)->SetInputRegion(region);
}

void Damage(wl_client* client,
            wl_resource* resource,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height) {
  GetUserDataAs<MockSurface>(resource)->Damage(x, y, width, height);
}

void Frame(struct wl_client* client,
           struct wl_resource* resource,
           uint32_t callback) {
  GetUserDataAs<MockSurface>(resource)->Frame(callback);
}

void Commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockSurface>(resource)->Commit();
}

void DamageBuffer(struct wl_client* client,
                  struct wl_resource* resource,
                  int32_t x,
                  int32_t y,
                  int32_t width,
                  int32_t height) {
  GetUserDataAs<MockSurface>(resource)->DamageBuffer(x, y, width, height);
}

}  // namespace

const struct wl_surface_interface kMockSurfaceImpl = {
    DestroyResource,  // destroy
    Attach,           // attach
    Damage,           // damage
    Frame,            // frame
    SetOpaqueRegion,  // set_opaque_region
    SetInputRegion,   // set_input_region
    Commit,           // commit
    nullptr,          // set_buffer_transform
    nullptr,          // set_buffer_scale
    DamageBuffer,     // damage_buffer
};

MockSurface::MockSurface(wl_resource* resource) : ServerObject(resource) {}

MockSurface::~MockSurface() {
  if (xdg_surface_ && xdg_surface_->resource())
    wl_resource_destroy(xdg_surface_->resource());
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!ResourceHasImplementation(resource, &wl_surface_interface,
                                 &kMockSurfaceImpl))
    return nullptr;
  return GetUserDataAs<MockSurface>(resource);
}

}  // namespace wl
