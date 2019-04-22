// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webshare/navigator_share.h"

#include <memory>
#include <utility>
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_property_bag.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webshare/share_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using mojom::blink::SharedFile;
using mojom::blink::SharedFilePtr;
using mojom::blink::ShareService;

// A mock ShareService used to intercept calls to the mojo methods.
class MockShareService : public ShareService {
 public:
  MockShareService() : binding_(this) {}
  ~MockShareService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(mojom::blink::ShareServiceRequest(std::move(handle)));
  }

  const WTF::String& title() const { return title_; }
  const WTF::String& text() const { return text_; }
  const KURL& url() const { return url_; }
  const WTF::Vector<SharedFilePtr>& files() const { return files_; }

 private:
  void Share(const WTF::String& title,
             const WTF::String& text,
             const KURL& url,
             WTF::Vector<SharedFilePtr> files,
             ShareCallback callback) override {
    title_ = title;
    text_ = text;
    url_ = url;

    files_.clear();
    files_.ReserveInitialCapacity(files.size());
    for (const auto& entry : files) {
      files_.push_back(entry->Clone());
    }

    std::move(callback).Run(mojom::ShareError::OK);
  }

  mojo::Binding<ShareService> binding_;
  WTF::String title_;
  WTF::String text_;
  KURL url_;
  WTF::Vector<SharedFilePtr> files_;
};

class NavigatorShareTest : public testing::Test {
 public:
  NavigatorShareTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  Document& GetDocument() { return holder_->GetDocument(); }

  LocalFrame& GetFrame() { return holder_->GetFrame(); }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  void Share(const ShareData& share_data) {
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::NotifyUserActivation(&GetFrame(),
                                         UserGestureToken::kNewGesture);
    Navigator* navigator = GetFrame().DomWindow()->navigator();
    ScriptPromise promise =
        NavigatorShare::share(GetScriptState(), *navigator, &share_data);
    test::RunPendingTasks();
    EXPECT_EQ(v8::Promise::kFulfilled,
              promise.V8Value().As<v8::Promise>()->State());
  }

  const MockShareService& mock_share_service() const {
    return mock_share_service_;
  }

 protected:
  void SetUp() override {
    GetDocument().SetSecurityOrigin(
        SecurityOrigin::Create(KURL("https://example.com")));

    service_manager::InterfaceProvider::TestApi test_api(
        &GetFrame().GetInterfaceProvider());
    test_api.SetBinderForName(
        ShareService::Name_,
        WTF::BindRepeating(&MockShareService::Bind,
                           WTF::Unretained(&mock_share_service_)));
  }

 public:
  MockShareService mock_share_service_;

  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(NavigatorShareTest, ShareText) {
  const String title = "Subject";
  const String message = "Body";
  const String url = "https://example.com/path?query#fragment";

  ShareData share_data;
  share_data.setTitle(title);
  share_data.setText(message);
  share_data.setURL(url);
  Share(share_data);

  EXPECT_EQ(mock_share_service().title(), title);
  EXPECT_EQ(mock_share_service().text(), message);
  EXPECT_EQ(mock_share_service().url(), KURL(url));
  EXPECT_EQ(mock_share_service().files().size(), 0U);
}

TEST_F(NavigatorShareTest, ShareFile) {
  const String file_name = "chart.svg";
  const String content_type = "image/svg+xml";
  const String file_contents = "<svg></svg>";

  HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString> blob_parts;
  blob_parts.push_back(ArrayBufferOrArrayBufferViewOrBlobOrUSVString());
  blob_parts.back().SetUSVString(file_contents);

  FilePropertyBag file_property_bag;
  file_property_bag.setType(content_type);

  HeapVector<Member<File>> files;
  files.push_back(File::Create(ExecutionContext::From(GetScriptState()),
                               blob_parts, file_name, &file_property_bag));

  ShareData share_data;
  share_data.setFiles(files);
  Share(share_data);

  EXPECT_EQ(mock_share_service().files().size(), 1U);
  EXPECT_EQ(mock_share_service().files()[0]->name, file_name);
  EXPECT_EQ(mock_share_service().files()[0]->blob->GetType(), content_type);
  EXPECT_EQ(mock_share_service().files()[0]->blob->size(),
            file_contents.length());
}

}  // namespace blink
