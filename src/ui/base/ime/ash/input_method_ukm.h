// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_

#include "ash/services/ime/public/mojom/input_method_host.mojom-shared.h"
#include "base/component_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/text_input_type.h"

namespace ui {

// Records an event in UKM, under the InputMethod.NonCompliantApi metric.
// Ignores invalid sources.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
void RecordUkmNonCompliantApi(
    ukm::SourceId source,
    chromeos::ime::mojom::InputMethodApiOperation operation);

// Records an event in UKM, under the InputMethod.Assistive.Match metric.
// Ignores invalid sources.
// `type` is a value in the chromeos.AssistiveType enum.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
void RecordUkmAssistiveMatch(ukm::SourceId source, int64_t type);

}  // namespace ui

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_
