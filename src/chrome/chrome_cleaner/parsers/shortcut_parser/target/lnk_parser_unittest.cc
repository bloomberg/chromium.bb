// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

#include <stdio.h>
#include <windows.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace internal {

class LnkParserTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                               &target_file_path_));
  }

  base::win::ScopedHandle CreateAndOpenShortcut(
      const base::win::ShortcutProperties& properties) {
    base::FilePath shortcut_path =
        temp_dir_.GetPath().AppendASCII("test_shortcut.lnk");
    if (!base::win::CreateOrUpdateShortcutLink(
            shortcut_path, properties,
            base::win::ShortcutOperation::SHORTCUT_CREATE_ALWAYS)) {
      LOG(ERROR) << "Could not create shortcut";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    base::File lnk_file(shortcut_path, base::File::Flags::FLAG_OPEN |
                                           base::File::Flags::FLAG_READ);
    base::win::ScopedHandle handle(lnk_file.TakePlatformFile());
    if (!handle.IsValid())
      LOG(ERROR) << "Error opening the lnk file";
    return handle;
  }

  void CheckParsedShortcut(ParsedLnkFile* parsed_shortcut,
                           base::FilePath target_path,
                           base::string16 arguments,
                           base::FilePath icon_location) {
    base::FilePath parsed_file_path(parsed_shortcut->target_path);
    ASSERT_TRUE(PathEqual(parsed_file_path, target_path));

    ASSERT_EQ(parsed_shortcut->command_line_arguments, arguments);

    base::FilePath parsed_icon_location(parsed_shortcut->icon_location);
    ASSERT_TRUE(PathEqual(parsed_icon_location, icon_location));
  }

  bool CreateFileWithUTF16Name(base::FilePath* file_path) {
    base::string16 UTF16_file_name = L"NormalFile\x0278";
    *file_path = temp_dir_.GetPath().Append(UTF16_file_name);
    return CreateFileInFolder(temp_dir_.GetPath(), UTF16_file_name);
  }

  bool LoadShortcutIntoMemory(base::File* shortcut,
                              std::vector<BYTE>* shortcut_contents) {
    int16_t file_size = shortcut->GetLength();
    if (file_size == -1) {
      LOG(ERROR) << "Cannot get lnk file size";
      return false;
    }

    shortcut_contents->resize(file_size);

    int64_t bytes_read = shortcut->Read(
        /*offset=*/0, reinterpret_cast<char*>(shortcut_contents->data()),
        file_size);

    if (bytes_read != file_size) {
      LOG(ERROR) << "Error reading lnk file, bytes read: " << bytes_read
                 << "file size: " << file_size;
      return false;
    }

    // Assert that the file was created with more than just the
    // header.
    if (file_size <= kHeaderSize) {
      LOG(ERROR) << "Not enough data on lnk file";
      return false;
    }

    return true;
  }

  base::win::ScopedHandle CreateAndOpenShortcutToNetworkAndLocalLocation(
      const base::win::ShortcutProperties& properties) {
    base::win::ScopedHandle local_lnk_handle =
        CreateAndOpenShortcut(properties);
    if (!local_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating lnk pointing to local file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory to modify the LinkInfoFlags.
    base::File local_lnk(local_lnk_handle.Take());
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&local_lnk, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    internal::LnkInfoPartialHeader* partial_header =
        internal::LocateAndParseLnkInfoPartialHeader(&shortcut_buffer, nullptr);

    if (!partial_header)
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);

    // The value 0x03 means that this link file points to a local file that
    // is also in a shared folder (local and network location paths).
    constexpr BYTE kLocalAndNetworkLnkFlag = 0x03;
    partial_header->flags = kLocalAndNetworkLnkFlag;

    // Write the lnk file back to disk.
    base::FilePath local_and_network_shortcut_path =
        temp_dir_.GetPath().AppendASCII("local_and_network_shortcut.lnk");
    base::File local_and_network_shortcut(local_and_network_shortcut_path,
                                          base::File::Flags::FLAG_CREATE |
                                              base::File::Flags::FLAG_WRITE |
                                              base::File::Flags::FLAG_READ);

    uint64_t written_bytes = local_and_network_shortcut.Write(
        /*offset=*/0, reinterpret_cast<const char*>(shortcut_buffer.data()),
        shortcut_buffer.size());

    if (written_bytes != shortcut_buffer.size()) {
      LOG(ERROR) << "Error writing modified shortcut";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    return base::win::ScopedHandle(
        local_and_network_shortcut.TakePlatformFile());
  }

  base::win::ScopedHandle CreateAndOpenCorruptedLnkFile(
      const base::win::ShortcutProperties& properties) {
    base::win::ScopedHandle good_lnk_handle = CreateAndOpenShortcut(properties);
    if (!good_lnk_handle.IsValid()) {
      LOG(ERROR) << "Error creating and opening good lnk file";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Load the shortcut into memory to corrupt it.
    base::File good_lnk_file(good_lnk_handle.Take());
    std::vector<BYTE> shortcut_buffer;
    if (!LoadShortcutIntoMemory(&good_lnk_file, &shortcut_buffer)) {
      LOG(ERROR) << "Error loading shortcut into memory";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Leave the header intact and then corrupt every second byte
    // on the file, this was an arbitrary choice.
    uint64_t current_byte = kHeaderSize + 0x02;

    while (current_byte < shortcut_buffer.size()) {
      shortcut_buffer[current_byte] = 0xff;
      current_byte += 2;
    }

    // Write the contents of back to a new file.
    base::FilePath corrupted_shortcut_path =
        temp_dir_.GetPath().AppendASCII("corrupted_shortcut.lnk");
    base::File bad_lnk_file(corrupted_shortcut_path,
                            base::File::Flags::FLAG_CREATE |
                                base::File::Flags::FLAG_WRITE |
                                base::File::Flags::FLAG_READ);

    uint64_t written_bytes = bad_lnk_file.Write(
        /*offset=*/0, reinterpret_cast<const char*>(shortcut_buffer.data()),
        shortcut_buffer.size());
    if (written_bytes != shortcut_buffer.size()) {
      LOG(ERROR) << "Error writing bad shortcut";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    // Rewind the file to the start.
    if (bad_lnk_file.Seek(base::File::Whence::FROM_BEGIN, /*offset=*/0) == -1) {
      LOG(ERROR) << "Could not rewind file to the begining";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }

    base::win::ScopedHandle bad_lnk_handle(bad_lnk_file.TakePlatformFile());
    if (!bad_lnk_handle.IsValid()) {
      LOG(ERROR) << "Cannot open bad lnk for writing";
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }
    return bad_lnk_handle;
  }

 protected:
  base::FilePath target_file_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(LnkParserTest, ParseLnkWithoutArgumentsTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;

  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, L"",
                      base::FilePath(L""));
}

TEST_F(LnkParserTest, ParseLnkWithArgumentsTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  const base::string16 kArguments = L"Seven";
  properties.set_arguments(kArguments.c_str());

  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, kArguments,
                      base::FilePath(L""));
}

TEST_F(LnkParserTest, ParseLnkWithExtraStringStructures) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  const base::string16 kArguments =
      L"lavidaesbella --arg1 --argmento2 --argumento3 /a --Seven";
  properties.set_arguments(kArguments.c_str());

  properties.set_working_dir(temp_dir_.GetPath());
  properties.set_app_id(L"id");

  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, kArguments,
                      base::FilePath(L""));
}

TEST_F(LnkParserTest, UTF16FileNameParseTest) {
  base::FilePath utf16_path;
  ASSERT_TRUE(CreateFileWithUTF16Name(&utf16_path));
  base::win::ShortcutProperties properties;
  properties.set_target(utf16_path);
  base::win::ScopedHandle lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, utf16_path, L"", base::FilePath(L""));
}

TEST_F(LnkParserTest, InvalidHandleTest) {
  base::win::ScopedHandle invalid_handle(INVALID_HANDLE_VALUE);
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(invalid_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, NoShortcutFileTest) {
  base::File no_shortcut(target_file_path_, base::File::Flags::FLAG_OPEN |
                                                base::File::Flags::FLAG_READ);
  base::win::ScopedHandle handle(no_shortcut.TakePlatformFile());

  ASSERT_TRUE(handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, CorruptedShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  base::win::ScopedHandle bad_lnk_handle =
      CreateAndOpenCorruptedLnkFile(properties);

  ASSERT_TRUE(bad_lnk_handle.IsValid());
  ParsedLnkFile unused_parsed_shortcut;
  ASSERT_NE(ParseLnk(std::move(bad_lnk_handle), &unused_parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
}

TEST_F(LnkParserTest, LocalAndNetworkShortcutTest) {
  base::win::ShortcutProperties properties;
  properties.set_target(target_file_path_);
  properties.set_working_dir(temp_dir_.GetPath());

  base::win::ScopedHandle local_and_network_lnk_handle =
      CreateAndOpenShortcutToNetworkAndLocalLocation(properties);

  ASSERT_TRUE(local_and_network_lnk_handle.IsValid());
  ParsedLnkFile parsed_shortcut;
  ASSERT_EQ(ParseLnk(std::move(local_and_network_lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, target_file_path_, L"",
                      base::FilePath(L""));
}

TEST_F(LnkParserTest, ParseIconLocationTest) {
  base::FilePath txt_file_path = temp_dir_.GetPath().Append(L"file.txt");
  base::File txt_file(txt_file_path, base::File::Flags::FLAG_CREATE |
                                         base::File::Flags::FLAG_READ);
  ASSERT_TRUE(txt_file.IsValid());

  base::win::ShortcutProperties properties;
  properties.set_target(txt_file_path);
  properties.set_icon(txt_file_path, /*icon_index=*/0);

  base::win::ScopedHandle txt_lnk_handle = CreateAndOpenShortcut(properties);
  ASSERT_TRUE(txt_lnk_handle.IsValid());

  ParsedLnkFile parsed_shortcut;
  EXPECT_EQ(ParseLnk(std::move(txt_lnk_handle), &parsed_shortcut),
            mojom::LnkParsingResult::SUCCESS);
  CheckParsedShortcut(&parsed_shortcut, txt_file_path, L"", txt_file_path);
}

}  // namespace internal

}  // namespace chrome_cleaner
