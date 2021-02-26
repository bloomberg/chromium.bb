// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/file_input_type.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/mock_file_chooser.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

class WebKitDirectoryChromeClient : public EmptyChromeClient {
 public:
  void RegisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED() << "RegisterPopupOpeningObserver should not be called.";
  }
  void UnregisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED() << "UnregisterPopupOpeningObserver should not be called.";
  }
};

}  // namespace

TEST(FileInputTypeTest, createFileList) {
  FileChooserFileInfoList files;

  // Native file.
  files.push_back(CreateFileChooserFileInfoNative("/native/path/native-file",
                                                  "display-name"));

  // Non-native file.
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  files.push_back(CreateFileChooserFileInfoFileSystem(
      url, base::Time::FromJsTime(1.0 * kMsPerDay + 3), 64));

  FileList* list = FileInputType::CreateFileList(files, base::FilePath());
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
  auto* document = Document::CreateForTest();
  auto* input =
      MakeGarbageCollected<HTMLInputElement>(*document, CreateElementFlags());
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);

  DataObject* native_file_raw_drag_data = DataObject::Create();
  const DragData native_file_drag_data(native_file_raw_drag_data, FloatPoint(),
                                       FloatPoint(), kDragOperationCopy);
  native_file_drag_data.PlatformData()->Add(
      MakeGarbageCollected<File>("/native/path"));
  native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&native_file_drag_data);
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  DataObject* non_native_file_raw_drag_data = DataObject::Create();
  const DragData non_native_file_drag_data(non_native_file_raw_drag_data,
                                           FloatPoint(), FloatPoint(),
                                           kDragOperationCopy);
  FileMetadata metadata;
  metadata.length = 1234;
  const KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
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
  auto* document = Document::CreateForTest();
  auto* input =
      MakeGarbageCollected<HTMLInputElement>(*document, CreateElementFlags());
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
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
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);
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

TEST(FileInputTypeTest, DropTouchesNoPopupOpeningObserver) {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  auto* chrome_client = MakeGarbageCollected<WebKitDirectoryChromeClient>();
  page_clients.chrome_client = chrome_client;
  auto page_holder =
      std::make_unique<DummyPageHolder>(IntSize(), &page_clients);
  Document& doc = page_holder->GetDocument();

  doc.body()->setInnerHTML("<input type=file webkitdirectory>");
  auto& input = *To<HTMLInputElement>(doc.body()->firstChild());

  base::RunLoop run_loop;
  MockFileChooser chooser(doc.GetFrame()->GetBrowserInterfaceBroker(),
                          run_loop.QuitClosure());
  DragData drag_data(DataObject::Create(), FloatPoint(), FloatPoint(),
                     kDragOperationCopy);
  drag_data.PlatformData()->Add(MakeGarbageCollected<File>("/foo/bar"));
  input.ReceiveDroppedFiles(&drag_data);
  run_loop.Run();

  chooser.ResponseOnOpenFileChooser(FileChooserFileInfoList());

  // The test passes if WebKitDirectoryChromeClient::
  // UnregisterPopupOpeningObserver() was not called.
}

TEST(FileInputTypeTest, BeforePseudoCrash) {
  std::unique_ptr<DummyPageHolder> page_holder =
      std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& doc = page_holder->GetDocument();
  doc.documentElement()->setInnerHTML(R"HTML(
<style>
.c6 {
  zoom: 0.01;
}

.c6::first-letter {
  position: fixed;
  border-style: groove;
}

.c6::before {
  content: 'c6';
}

.c7 {
  zoom: 0.1;
}

.c7::first-letter {
  position: fixed;
  border-style: groove;
}

.c7::before {
  content: 'c7';
}

</style>
<input type=file class=c6>
<input type=file class=c7>
)HTML");
  doc.View()->UpdateAllLifecyclePhasesForTest();
  // The test passes if no CHECK failures and no null pointer dereferences.
}

TEST(FileInputTypeTest, ChangeTypeDuringOpeningFileChooser) {
  // We use WebViewHelper instead of DummyPageHolder, in order to use
  // ChromeClientImpl.
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  LocalFrame* frame = helper.LocalMainFrame()->GetFrame();

  Document& doc = *frame->GetDocument();
  doc.body()->setInnerHTML("<input type=file>");
  auto& input = *To<HTMLInputElement>(doc.body()->firstChild());

  base::RunLoop run_loop;
  MockFileChooser chooser(frame->GetBrowserInterfaceBroker(),
                          run_loop.QuitClosure());

  // Calls MockFileChooser::OpenFileChooser().
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kInteraction);
  input.click();
  run_loop.Run();

  input.setType(input_type_names::kColor);

  FileChooserFileInfoList list;
  list.push_back(CreateFileChooserFileInfoNative("/path/to/file.txt", ""));
  chooser.ResponseOnOpenFileChooser(std::move(list));

  // Receiving a FileChooser response should not alter a shadow tree
  // for another type.
  EXPECT_TRUE(IsA<HTMLElement>(
      input.UserAgentShadowRoot()->firstChild()->firstChild()));
}

}  // namespace blink
