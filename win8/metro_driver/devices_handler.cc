// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/devices_handler.h"

#include "base/logging.h"

namespace metro_driver {

DevicesHandler::DevicesHandler() {
}

DevicesHandler::~DevicesHandler() {
}

HRESULT DevicesHandler::Initialize(winui::Core::ICoreWindow* window) {
  HRESULT hr = print_handler_.Initialize(window);
  return hr;
}

PointerEventHandler::PointerEventHandler() {
}

PointerEventHandler::~PointerEventHandler() {
}

HRESULT PointerEventHandler::Init(winui::Core::IPointerEventArgs* args) {
  mswr::ComPtr<winui::Input::IPointerPoint> pointer_point;
  HRESULT hr = args->get_CurrentPoint(&pointer_point);
  if (FAILED(hr))
    return hr;

  mswr::ComPtr<windevs::Input::IPointerDevice> pointer_device;
  hr = pointer_point->get_PointerDevice(&pointer_device);
  if (FAILED(hr))
    return hr;

  windevs::Input::PointerDeviceType device_type;
  hr = pointer_device->get_PointerDeviceType(&device_type);
  if (FAILED(hr))
    return hr;

  is_mouse_ =  (device_type == windevs::Input::PointerDeviceType_Mouse);
  winfoundtn::Point point;
  hr = pointer_point->get_Position(&point);
  if (FAILED(hr))
    return hr;

  x_ = point.X;
  y_ = point.Y;
  return S_OK;
}

}  // namespace metro_driver
