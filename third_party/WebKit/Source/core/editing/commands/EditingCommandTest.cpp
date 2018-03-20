// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/Editor.h"
#include "core/editing/commands/EditorCommand.h"
#include "core/editing/commands/EditorCommandNames.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/frame/LocalFrame.h"
#include "platform/wtf/StringExtras.h"
#include "public/platform/WebEditingCommandType.h"

namespace blink {

namespace {

struct CommandNameEntry {
  const char* name;
  WebEditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) {#name, WebEditingCommandType::k##name},
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Test all commands except WebEditingCommandType::Invalid.
static_assert(
    arraysize(kCommandNameEntries) + 1 ==
        static_cast<size_t>(WebEditingCommandType::kNumberOfCommandTypes),
    "must test all valid WebEditingCommandType");

}  // anonymous namespace

class EditingCommandTest : public EditingTestBase {};

TEST_F(EditingCommandTest, EditorCommandOrder) {
  for (size_t i = 1; i < arraysize(kCommandNameEntries); ++i) {
    EXPECT_GT(0, strcasecmp(kCommandNameEntries[i - 1].name,
                            kCommandNameEntries[i].name))
        << "EDITOR_COMMAND_MAP must be case-folding ordered. Incorrect index:"
        << i;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromString) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand command = dummy_editor.CreateCommand(entry.name);
    EXPECT_EQ(static_cast<int>(entry.type), command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromStringCaseFolding) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand lower_name_command =
        dummy_editor.CreateCommand(String(entry.name).DeprecatedLower());
    EXPECT_EQ(static_cast<int>(entry.type), lower_name_command.IdForHistogram())
        << entry.name;
    const EditorCommand upper_name_command =
        dummy_editor.CreateCommand(String(entry.name).UpperASCII());
    EXPECT_EQ(static_cast<int>(entry.type), upper_name_command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromInvalidString) {
  const String kInvalidCommandName[] = {
      "", "iNvAlId", "12345",
  };
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& command_name : kInvalidCommandName) {
    const EditorCommand command = dummy_editor.CreateCommand(command_name);
    EXPECT_EQ(0, command.IdForHistogram());
  }
}

}  // namespace blink
