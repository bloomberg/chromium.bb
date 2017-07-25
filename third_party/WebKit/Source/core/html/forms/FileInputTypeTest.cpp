// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/FileInputType.h"

#include "core/HTMLNames.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/Document.h"
#include "core/fileapi/FileList.h"
#include "core/html/HTMLInputElement.h"
#include "core/page/DragData.h"
#include "platform/wtf/DateMath.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FileInputTypeTest, createFileList) {
  Vector<FileChooserFileInfo> files;

  // Native file.
  files.push_back(
      FileChooserFileInfo("/native/path/native-file", "display-name"));

  // Non-native file.
  KURL url(ParsedURLStringTag(),
           "filesystem:http://example.com/isolated/hash/non-native-file");
  FileMetadata metadata;
  metadata.length = 64;
  metadata.modification_time = 1.0 * kMsPerDay + 3;
  files.push_back(FileChooserFileInfo(url, metadata));

  FileList* list = FileInputType::CreateFileList(files, false);
  ASSERT_TRUE(list);
  ASSERT_EQ(2u, list->length());

  EXPECT_EQ("/native/path/native-file", list->item(0)->GetPath());
  EXPECT_EQ("display-name", list->item(0)->name());
  EXPECT_TRUE(list->item(0)->FileSystemURL().IsEmpty());

  EXPECT_TRUE(list->item(1)->GetPath().IsEmpty());
  EXPECT_EQ("non-native-file", list->item(1)->name());
  EXPECT_EQ(url, list->item(1)->FileSystemURL());
  EXPECT_EQ(64u, list->item(1)->size());
  EXPECT_EQ(1.0 * kMsPerDay + 3, list->item(1)->lastModified());
}

TEST(FileInputTypeTest, ignoreDroppedNonNativeFiles) {
  Document* document = Document::CreateForTest();
  HTMLInputElement* input = HTMLInputElement::Create(*document, false);
  InputType* file_input = FileInputType::Create(*input);

  DataObject* native_file_raw_drag_data = DataObject::Create();
  const DragData native_file_drag_data(native_file_raw_drag_data, IntPoint(),
                                       IntPoint(), kDragOperationCopy);
  native_file_drag_data.PlatformData()->Add(File::Create("/native/path"));
  native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&native_file_drag_data);
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  DataObject* non_native_file_raw_drag_data = DataObject::Create();
  const DragData non_native_file_drag_data(non_native_file_raw_drag_data,
                                           IntPoint(), IntPoint(),
                                           kDragOperationCopy);
  FileMetadata metadata;
  metadata.length = 1234;
  const KURL url(ParsedURLStringTag(),
                 "filesystem:http://example.com/isolated/hash/non-native-file");
  non_native_file_drag_data.PlatformData()->Add(
      File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible));
  non_native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&non_native_file_drag_data);
  // Dropping non-native files should not change the existing files.
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());
}

TEST(FileInputTypeTest, setFilesFromPaths) {
  Document* document = Document::CreateForTest();
  HTMLInputElement* input = HTMLInputElement::Create(*document, false);
  InputType* file_input = FileInputType::Create(*input);
  Vector<String> paths;
  paths.push_back("/native/path");
  paths.push_back("/native/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  // Try to upload multiple files without multipleAttr
  paths.clear();
  paths.push_back("/native/path1");
  paths.push_back("/native/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path1"), file_input->Files()->item(0)->GetPath());

  // Try to upload multiple files with multipleAttr
  input->SetBooleanAttribute(HTMLNames::multipleAttr, true);
  paths.clear();
  paths.push_back("/native/real/path1");
  paths.push_back("/native/real/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(2u, file_input->Files()->length());
  EXPECT_EQ(String("/native/real/path1"),
            file_input->Files()->item(0)->GetPath());
  EXPECT_EQ(String("/native/real/path2"),
            file_input->Files()->item(1)->GetPath());
}

}  // namespace blink
