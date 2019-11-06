// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/chip.h"

namespace autofill_assistant {

Chip::Chip() = default;
Chip::~Chip() = default;
Chip::Chip(Chip&&) = default;
Chip& Chip::operator=(Chip&&) = default;

Chip::Chip(const ChipProto& chip_proto)
    : type(chip_proto.type()),
      icon(chip_proto.icon()),
      text(chip_proto.text()),
      sticky(chip_proto.sticky()) {}

void SetDefaultChipType(std::vector<Chip>* chips) {
  ChipType default_type = SUGGESTION;
  for (const Chip& chip : *chips) {
    if (chip.type != UNKNOWN_CHIP_TYPE && chip.type != SUGGESTION) {
      // If there's an action chip, assume chips with unknown type are also
      // actions.
      default_type = NORMAL_ACTION;
      break;
    }
  }
  for (Chip& chip : *chips) {
    if (chip.type == UNKNOWN_CHIP_TYPE) {
      chip.type = default_type;
    }
  }
}

}  // namespace autofill_assistant
