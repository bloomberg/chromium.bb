// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_constants.h"

#include <limits>

namespace quic {

bool operator==(const QpackInstructionOpcode& a,
                const QpackInstructionOpcode& b) {
  return std::tie(a.value, a.mask) == std::tie(b.value, b.mask);
}

const QpackInstruction* InsertWithNameReferenceInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b10000000, 0b10000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kSbit, 0b01000000},
                            {QpackInstructionFieldType::kVarint, 6},
                            {QpackInstructionFieldType::kValue, 7}}};
  return instruction;
}

const QpackInstruction* InsertWithoutNameReferenceInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b01000000, 0b11000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kName, 5},
                            {QpackInstructionFieldType::kValue, 7}}};
  return instruction;
}

const QpackInstruction* DuplicateInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00000000, 0b11100000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 5}}};
  return instruction;
}

const QpackInstruction* DynamicTableSizeUpdateInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00100000, 0b11100000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 5}}};
  return instruction;
}

const QpackLanguage* QpackEncoderStreamLanguage() {
  static const QpackLanguage* const language = new QpackLanguage{
      InsertWithNameReferenceInstruction(),
      InsertWithoutNameReferenceInstruction(), DuplicateInstruction(),
      DynamicTableSizeUpdateInstruction()};
  return language;
}

const QpackInstruction* TableStateSynchronizeInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00000000, 0b11000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 6}}};
  return instruction;
}

const QpackInstruction* HeaderAcknowledgementInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b10000000, 0b10000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 7}}};
  return instruction;
}

const QpackInstruction* StreamCancellationInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b01000000, 0b11000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 6}}};
  return instruction;
}

const QpackLanguage* QpackDecoderStreamLanguage() {
  static const QpackLanguage* const language = new QpackLanguage{
      TableStateSynchronizeInstruction(), HeaderAcknowledgementInstruction(),
      StreamCancellationInstruction()};
  return language;
}

const QpackInstruction* QpackPrefixInstruction() {
  // This opcode matches every input.
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00000000, 0b00000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kVarint, 8},
                            {QpackInstructionFieldType::kSbit, 0b10000000},
                            {QpackInstructionFieldType::kVarint2, 7}}};
  return instruction;
}

const QpackLanguage* QpackPrefixLanguage() {
  static const QpackLanguage* const language =
      new QpackLanguage{QpackPrefixInstruction()};
  return language;
}

const QpackInstruction* QpackIndexedHeaderFieldInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b10000000, 0b10000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kSbit, 0b01000000},
                            {QpackInstructionFieldType::kVarint, 6}}};
  return instruction;
}

const QpackInstruction* QpackIndexedHeaderFieldPostBaseInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00010000, 0b11110000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode, {{QpackInstructionFieldType::kVarint, 4}}};
  return instruction;
}

const QpackInstruction* QpackLiteralHeaderFieldNameReferenceInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b01000000, 0b11000000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kSbit, 0b00010000},
                            {QpackInstructionFieldType::kVarint, 4},
                            {QpackInstructionFieldType::kValue, 7}}};
  return instruction;
}

const QpackInstruction* QpackLiteralHeaderFieldPostBaseInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00000000, 0b11110000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kVarint, 3},
                            {QpackInstructionFieldType::kValue, 7}}};
  return instruction;
}

const QpackInstruction* QpackLiteralHeaderFieldInstruction() {
  static const QpackInstructionOpcode* const opcode =
      new QpackInstructionOpcode{0b00100000, 0b11100000};
  static const QpackInstruction* const instruction =
      new QpackInstruction{*opcode,
                           {{QpackInstructionFieldType::kName, 3},
                            {QpackInstructionFieldType::kValue, 7}}};
  return instruction;
}

const QpackLanguage* QpackRequestStreamLanguage() {
  static const QpackLanguage* const language =
      new QpackLanguage{QpackIndexedHeaderFieldInstruction(),
                        QpackIndexedHeaderFieldPostBaseInstruction(),
                        QpackLiteralHeaderFieldNameReferenceInstruction(),
                        QpackLiteralHeaderFieldPostBaseInstruction(),
                        QpackLiteralHeaderFieldInstruction()};
  return language;
}

}  // namespace quic
