// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"

StubWebView::StubWebView(const std::string& id) : id_(id) {}

StubWebView::~StubWebView() {}

std::string StubWebView::GetId() {
  return id_;
}

bool StubWebView::WasCrashed() {
  return false;
}

Status StubWebView::ConnectIfNecessary() {
  return Status(kOk);
}

Status StubWebView::HandleReceivedEvents() {
  return Status(kOk);
}

Status StubWebView::GetUrl(std::string* url) {
  return Status(kOk);
}

Status StubWebView::Load(const std::string& url, const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Reload(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Freeze(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::Resume(const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::SendCommand(const std::string& cmd,
                                const base::DictionaryValue& params) {
  return Status(kOk);
}

Status StubWebView::SendCommandFromWebSocket(
    const std::string& cmd,
    const base::DictionaryValue& params,
    const int client_cmd_id) {
  return Status(kOk);
}

Status StubWebView::SendCommandAndGetResult(
        const std::string& cmd,
        const base::DictionaryValue& params,
        std::unique_ptr<base::Value>* value) {
  return Status(kOk);
}

Status StubWebView::TraverseHistory(int delta, const Timeout* timeout) {
  return Status(kOk);
}

Status StubWebView::EvaluateScript(const std::string& frame,
                                   const std::string& function,
                                   const bool awaitPromise,
                                   std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallFunction(const std::string& frame,
                                 const std::string& function,
                                 const base::ListValue& args,
                                 std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallAsyncFunction(const std::string& frame,
                                      const std::string& function,
                                      const base::ListValue& args,
                                      const base::TimeDelta& timeout,
                                      std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallUserAsyncFunction(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::CallUserSyncScript(const std::string& frame,
                                       const std::string& script,
                                       const base::ListValue& args,
                                       const base::TimeDelta& timeout,
                                       std::unique_ptr<base::Value>* result) {
  return Status(kOk);
}

Status StubWebView::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::ListValue& args,
                                       std::string* out_frame) {
  return Status(kOk);
}

Status StubWebView::DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                        const std::string& frame,
                                        bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEvent(const TouchEvent& event,
                                       bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEvents(const std::vector<TouchEvent>& events,
                                        bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchTouchEventWithMultiPoints(
    const std::vector<TouchEvent>& events,
    bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::DispatchKeyEvents(const std::vector<KeyEvent>& events,
                                      bool async_dispatch_events) {
  return Status(kOk);
}

Status StubWebView::GetCookies(std::unique_ptr<base::ListValue>* cookies,
                               const std::string& current_page_url) {
  return Status(kOk);
}

Status StubWebView::DeleteCookie(const std::string& name,
                                 const std::string& url,
                                 const std::string& domain,
                                 const std::string& path) {
  return Status(kOk);
}

Status StubWebView::AddCookie(const std::string& name,
                              const std::string& url,
                              const std::string& value,
                              const std::string& domain,
                              const std::string& path,
                              const std::string& sameSite,
                              bool secure,
                              bool httpOnly,
                              double expiry) {
  return Status(kOk);
}

Status StubWebView::WaitForPendingNavigations(const std::string& frame_id,
                                              const Timeout& timeout,
                                              bool stop_load_on_timeout) {
  return Status(kOk);
}

Status StubWebView::IsPendingNavigation(const std::string& frame_id,
                                        const Timeout* timeout,
                                        bool* is_pending) const {
  return Status(kOk);
}

JavaScriptDialogManager* StubWebView::GetJavaScriptDialogManager() {
  return NULL;
}

Status StubWebView::OverrideGeolocation(const Geoposition& geoposition) {
  return Status(kOk);
}

Status StubWebView::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  return Status(kOk);
}

Status StubWebView::OverrideDownloadDirectoryIfNeeded(
    const std::string& download_directory) {
  return Status(kOk);
}

Status StubWebView::CaptureScreenshot(
    std::string* screenshot,
    const base::DictionaryValue& params) {
  return Status(kOk);
}

Status StubWebView::SetFileInputFiles(const std::string& frame,
                                      const base::DictionaryValue& element,
                                      const std::vector<base::FilePath>& files,
                                      const bool append) {
  return Status(kOk);
}

Status StubWebView::TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) {
  return Status(kOk);
}

Status StubWebView::StartProfile() {
  return Status(kOk);
}

Status StubWebView::EndProfile(std::unique_ptr<base::Value>* profile_data) {
  return Status(kOk);
}

Status StubWebView::SynthesizeTapGesture(int x,
                                         int y,
                                         int tap_count,
                                         bool is_long_press) {
  return Status(kOk);
}

Status StubWebView::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  return Status(kOk);
}

bool StubWebView::IsNonBlocking() const {
  return false;
}

bool StubWebView::IsOOPIF(const std::string& frame_id) {
  return false;
}

FrameTracker* StubWebView::GetFrameTracker() const {
  return nullptr;
}

std::unique_ptr<base::Value> StubWebView::GetCastSinks() {
  return std::make_unique<base::Value>();
}

std::unique_ptr<base::Value> StubWebView::GetCastIssueMessage() {
  return std::make_unique<base::Value>();
}

void StubWebView::ClearNavigationState(const std::string& new_frame_id) {}
