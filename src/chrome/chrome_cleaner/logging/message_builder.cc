// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/message_builder.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace chrome_cleaner {

MessageBuilder::MessageItem::MessageItem(base::StringPiece16 value)
    : value_(value.as_string()) {}

MessageBuilder::MessageItem::MessageItem(base::StringPiece value)
    : value_(base::UTF8ToUTF16(value.as_string())) {}

MessageBuilder::MessageItem::MessageItem(int value)
    : value_(base::NumberToString16(value)) {}

MessageBuilder::ScopedIndent::ScopedIndent(MessageBuilder* builder)
    : builder_(builder) {
  builder_->IncreaseIdentationLevel();
}

MessageBuilder::ScopedIndent::ScopedIndent(
    MessageBuilder::ScopedIndent&& other) {
  operator=(std::move(other));  // Should call the move assignment operator.
}

MessageBuilder::ScopedIndent& MessageBuilder::ScopedIndent::operator=(
    MessageBuilder::ScopedIndent&& other) {
  std::swap(builder_, other.builder_);
  return *this;
}

MessageBuilder::ScopedIndent::~ScopedIndent() {
  builder_->DecreaseIdentationLevel();
}

void MessageBuilder::IncreaseIdentationLevel() {
  ++indentation_level_;
  if (!content_.empty() && content_.back() != L'\n')
    NewLine();
}

void MessageBuilder::DecreaseIdentationLevel() {
  DCHECK_GT(indentation_level_, 0);
  --indentation_level_;
  if (!content_.empty() && content_.back() != L'\n')
    NewLine();
}

MessageBuilder& MessageBuilder::NewLine() {
  content_ += L"\n";
  return *this;
}

void MessageBuilder::AddInternal(std::initializer_list<MessageItem> values) {
  for (auto value : values)
    content_ += value.value();
}

void MessageBuilder::IndentIfNewLine() {
  if (content_.empty() || content_.back() != L'\n')
    return;
  for (int i = 0; i < indentation_level_; ++i)
    content_ += L"\t";
}

MessageBuilder& MessageBuilder::AddHeaderLine(base::StringPiece16 title) {
  Add(title, L":").NewLine();
  return *this;
}

MessageBuilder::ScopedIndent MessageBuilder::Indent() {
  return MessageBuilder::ScopedIndent(this);
}

}  // namespace chrome_cleaner
