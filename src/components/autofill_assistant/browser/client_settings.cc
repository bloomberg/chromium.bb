// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_settings.h"

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

ClientSettings::ClientSettings() = default;

void ClientSettings::UpdateFromProto(const ClientSettingsProto& proto) {
  if (proto.has_periodic_script_check_interval_ms()) {
    periodic_script_check_interval = base::TimeDelta::FromMilliseconds(
        proto.periodic_script_check_interval_ms());
  }
  if (proto.has_periodic_element_check_interval_ms()) {
    periodic_element_check_interval = base::TimeDelta::FromMilliseconds(
        proto.periodic_element_check_interval_ms());
  }
  if (proto.has_periodic_script_check_count()) {
    periodic_script_check_count = proto.periodic_script_check_count();
  }
  if (proto.has_element_position_update_interval_ms()) {
    element_position_update_interval = base::TimeDelta::FromMilliseconds(
        proto.element_position_update_interval_ms());
  }
  if (proto.has_short_wait_for_element_deadline_ms()) {
    short_wait_for_element_deadline = base::TimeDelta::FromMilliseconds(
        proto.short_wait_for_element_deadline_ms());
  }
  if (proto.has_box_model_check_interval_ms()) {
    box_model_check_interval =
        base::TimeDelta::FromMilliseconds(proto.box_model_check_interval_ms());
  }
  if (proto.has_box_model_check_count()) {
    box_model_check_count = proto.box_model_check_count();
  }
  if (proto.has_document_ready_check_interval_ms()) {
    document_ready_check_interval = base::TimeDelta::FromMilliseconds(
        proto.document_ready_check_interval_ms());
  }
  if (proto.has_document_ready_check_count()) {
    document_ready_check_count = proto.document_ready_check_count();
  }
  if (proto.has_enable_graceful_shutdown()) {
    enable_graceful_shutdown = proto.enable_graceful_shutdown();
  }
  if (proto.has_graceful_shutdown_delay_ms()) {
    graceful_shutdown_delay =
        base::TimeDelta::FromMilliseconds(proto.graceful_shutdown_delay_ms());
  }
  if (proto.has_cancel_delay_ms()) {
    cancel_delay = base::TimeDelta::FromMilliseconds(proto.cancel_delay_ms());
  }
  if (proto.has_tap_count()) {
    tap_count = proto.tap_count();
  }
  if (proto.has_tap_tracking_duration_ms()) {
    tap_tracking_duration =
        base::TimeDelta::FromMilliseconds(proto.tap_tracking_duration_ms());
  }
  if (proto.has_tap_shutdown_delay_ms()) {
    tap_shutdown_delay =
        base::TimeDelta::FromMilliseconds(proto.tap_shutdown_delay_ms());
  }
}

}  // namespace autofill_assistant
