// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/navigable_contents.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/content/public/cpp/navigable_contents_view.h"

namespace content {

NavigableContents::NavigableContents(mojom::NavigableContentsFactory* factory)
    : NavigableContents(factory, mojom::NavigableContentsParams::New()) {}

NavigableContents::NavigableContents(mojom::NavigableContentsFactory* factory,
                                     mojom::NavigableContentsParamsPtr params)
    : client_binding_(this), content_ax_tree_id_(ui::AXTreeIDUnknown()) {
  mojom::NavigableContentsClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  factory->CreateContents(std::move(params), mojo::MakeRequest(&contents_),
                          std::move(client));
}

NavigableContents::~NavigableContents() = default;

void NavigableContents::AddObserver(NavigableContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void NavigableContents::RemoveObserver(NavigableContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

NavigableContentsView* NavigableContents::GetView() {
  if (!view_) {
    view_ = base::WrapUnique(new NavigableContentsView(this));
    contents_->CreateView(
        ShouldUseWindowService(),
        base::BindOnce(&NavigableContents::OnEmbedTokenReceived,
                       base::Unretained(this)));
  }
  return view_.get();
}

void NavigableContents::Navigate(const GURL& url) {
  NavigateWithParams(url, mojom::NavigateParams::New());
}

void NavigableContents::NavigateWithParams(const GURL& url,
                                           mojom::NavigateParamsPtr params) {
  contents_->Navigate(url, std::move(params));
}

void NavigableContents::GoBack(
    content::mojom::NavigableContents::GoBackCallback callback) {
  contents_->GoBack(std::move(callback));
}

void NavigableContents::Focus() {
  contents_->Focus();
}

void NavigableContents::FocusThroughTabTraversal(bool reverse) {
  contents_->FocusThroughTabTraversal(reverse);
}

void NavigableContents::ForceUseWindowService() {
  // This should only be called before |view_| is created.
  DCHECK(!view_);

  force_use_window_service_ = true;
}

bool NavigableContents::ShouldUseWindowService() const {
  return !NavigableContentsView::IsClientRunningInServiceProcess() ||
         force_use_window_service_;
}

void NavigableContents::ClearViewFocus() {
  if (view_)
    view_->ClearNativeFocus();
}

void NavigableContents::DidFinishNavigation(
    const GURL& url,
    bool is_main_frame,
    bool is_error_page,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  for (auto& observer : observers_) {
    observer.DidFinishNavigation(url, is_main_frame, is_error_page,
                                 response_headers.get());
  }
}

void NavigableContents::DidStopLoading() {
  for (auto& observer : observers_)
    observer.DidStopLoading();
}

void NavigableContents::DidAutoResizeView(const gfx::Size& new_size) {
  for (auto& observer : observers_)
    observer.DidAutoResizeView(new_size);
}

void NavigableContents::DidSuppressNavigation(const GURL& url,
                                              WindowOpenDisposition disposition,
                                              bool from_user_gesture) {
  for (auto& observer : observers_)
    observer.DidSuppressNavigation(url, disposition, from_user_gesture);
}

void NavigableContents::UpdateContentAXTree(const ui::AXTreeID& id) {
  content_ax_tree_id_ = id;
  if (view_)
    view_->NotifyAccessibilityTreeChange();
}

void NavigableContents::OnEmbedTokenReceived(
    const base::UnguessableToken& token) {
  DCHECK(view_);
  view_->EmbedUsingToken(token);
}

}  // namespace content
