// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

namespace ui {

namespace {

void ForwardGetTouchDeviceStatusReply(
    scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
    const GetTouchDeviceStatusReply& reply,
    scoped_ptr<std::string> status) {
  // Thread hop back to UI for reply.
  reply_runner->PostTask(FROM_HERE, base::Bind(reply, base::Passed(&status)));
}

}  // namespace

InputDeviceFactoryEvdevProxy::InputDeviceFactoryEvdevProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<InputDeviceFactoryEvdev> input_device_factory)
    : task_runner_(task_runner), input_device_factory_(input_device_factory) {
}

InputDeviceFactoryEvdevProxy::~InputDeviceFactoryEvdevProxy() {
}

void InputDeviceFactoryEvdevProxy::AddInputDevice(int id,
                                                  const base::FilePath& path) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InputDeviceFactoryEvdev::AddInputDevice,
                                    input_device_factory_, id, path));
}

void InputDeviceFactoryEvdevProxy::RemoveInputDevice(
    const base::FilePath& path) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InputDeviceFactoryEvdev::RemoveInputDevice,
                                    input_device_factory_, path));
}

void InputDeviceFactoryEvdevProxy::DisableInternalTouchpad() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::DisableInternalTouchpad,
                            input_device_factory_));
}

void InputDeviceFactoryEvdevProxy::EnableInternalTouchpad() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::EnableInternalTouchpad,
                            input_device_factory_));
}

void InputDeviceFactoryEvdevProxy::DisableInternalKeyboardExceptKeys(
    scoped_ptr<std::set<DomCode>> excepted_keys) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InputDeviceFactoryEvdev::DisableInternalKeyboardExceptKeys,
                 input_device_factory_, base::Passed(&excepted_keys)));
}

void InputDeviceFactoryEvdevProxy::EnableInternalKeyboard() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::EnableInternalKeyboard,
                            input_device_factory_));
}

void InputDeviceFactoryEvdevProxy::SetTouchpadSensitivity(int value) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::SetTouchpadSensitivity,
                            input_device_factory_, value));
}

void InputDeviceFactoryEvdevProxy::SetTapToClick(bool enabled) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InputDeviceFactoryEvdev::SetTapToClick,
                                    input_device_factory_, enabled));
}

void InputDeviceFactoryEvdevProxy::SetThreeFingerClick(bool enabled) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::SetThreeFingerClick,
                            input_device_factory_, enabled));
}

void InputDeviceFactoryEvdevProxy::SetTapDragging(bool enabled) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InputDeviceFactoryEvdev::SetTapDragging,
                                    input_device_factory_, enabled));
}

void InputDeviceFactoryEvdevProxy::SetNaturalScroll(bool enabled) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InputDeviceFactoryEvdev::SetNaturalScroll,
                                    input_device_factory_, enabled));
}

void InputDeviceFactoryEvdevProxy::SetMouseSensitivity(int value) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::SetMouseSensitivity,
                            input_device_factory_, value));
}

void InputDeviceFactoryEvdevProxy::SetTapToClickPaused(bool state) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputDeviceFactoryEvdev::SetTapToClickPaused,
                            input_device_factory_, state));
}

void InputDeviceFactoryEvdevProxy::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InputDeviceFactoryEvdev::GetTouchDeviceStatus,
                 input_device_factory_,
                 base::Bind(&ForwardGetTouchDeviceStatusReply,
                            base::ThreadTaskRunnerHandle::Get(), reply)));
}

}  // namespace ui
