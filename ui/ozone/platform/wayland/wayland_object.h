// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_

#include <wayland-client.h>
#include <xdg-shell-unstable-v5-client-protocol.h>

#include "base/memory/scoped_ptr.h"

namespace wl {

template <typename T>
struct ObjectTraits;

template <>
struct ObjectTraits<wl_buffer> {
  static constexpr const wl_interface* interface = &wl_buffer_interface;
  static constexpr void (*deleter)(wl_buffer*) = &wl_buffer_destroy;
};

template <>
struct ObjectTraits<wl_compositor> {
  static constexpr const wl_interface* interface = &wl_compositor_interface;
  static constexpr void (*deleter)(wl_compositor*) = &wl_compositor_destroy;
};

template <>
struct ObjectTraits<wl_display> {
  static constexpr const wl_interface* interface = &wl_display_interface;
  static constexpr void (*deleter)(wl_display*) = &wl_display_disconnect;
};

template <>
struct ObjectTraits<wl_registry> {
  static constexpr const wl_interface* interface = &wl_registry_interface;
  static constexpr void (*deleter)(wl_registry*) = &wl_registry_destroy;
};

template <>
struct ObjectTraits<wl_shm> {
  static constexpr const wl_interface* interface = &wl_shm_interface;
  static constexpr void (*deleter)(wl_shm*) = &wl_shm_destroy;
};

template <>
struct ObjectTraits<wl_shm_pool> {
  static constexpr const wl_interface* interface = &wl_shm_pool_interface;
  static constexpr void (*deleter)(wl_shm_pool*) = &wl_shm_pool_destroy;
};

template <>
struct ObjectTraits<wl_surface> {
  static constexpr const wl_interface* interface = &wl_surface_interface;
  static constexpr void (*deleter)(wl_surface*) = &wl_surface_destroy;
};

template <>
struct ObjectTraits<xdg_shell> {
  static constexpr const wl_interface* interface = &xdg_shell_interface;
  static constexpr void (*deleter)(xdg_shell*) = &xdg_shell_destroy;
};

template <>
struct ObjectTraits<xdg_surface> {
  static constexpr const wl_interface* interface = &xdg_surface_interface;
  static constexpr void (*deleter)(xdg_surface*) = &xdg_surface_destroy;
};

struct Deleter {
  template <typename T>
  void operator()(T* obj) {
    ObjectTraits<T>::deleter(obj);
  }
};

template <typename T>
class Object : public scoped_ptr<T, Deleter> {
 public:
  Object() {}
  explicit Object(T* obj) : scoped_ptr<T, Deleter>(obj) {}

  uint32_t id() {
    return wl_proxy_get_id(
        reinterpret_cast<wl_proxy*>(scoped_ptr<T, Deleter>::get()));
  }
};

template <typename T>
wl::Object<T> Bind(wl_registry* registry, uint32_t name, uint32_t version) {
  return wl::Object<T>(static_cast<T*>(
      wl_registry_bind(registry, name, ObjectTraits<T>::interface, version)));
}

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_
