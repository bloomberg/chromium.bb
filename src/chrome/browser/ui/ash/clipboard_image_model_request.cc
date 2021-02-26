// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_image_model_request.h"

#include <memory>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

ClipboardImageModelRequest::Params::Params(const base::UnguessableToken& id,
                                           const std::string& html_markup,
                                           ImageModelCallback callback)
    : id(id), html_markup(html_markup), callback(std::move(callback)) {}

ClipboardImageModelRequest::Params::Params(Params&&) = default;

ClipboardImageModelRequest::Params&
ClipboardImageModelRequest::Params::operator=(Params&&) = default;

ClipboardImageModelRequest::Params::~Params() = default;

ClipboardImageModelRequest::ScopedClipboardModifier::ScopedClipboardModifier(
    const std::string& html_markup) {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* current_data = clipboard->GetClipboardData(&data_dst);

  // No need to replace the clipboard contents if the markup is the same.
  if (current_data && (html_markup == current_data->markup_data()))
    return;

  // Put |html_markup| on the clipboard temporarily so it can be pasted into
  // the WebContents. This is preferable to directly loading |html_markup_| in a
  // data URL because pasting the data into WebContents sanitizes the markup.
  // TODO(https://crbug.com/1144962): Sanitize copied HTML prior to storing it
  // in the clipboard buffer. Then |html_markup_| can be loaded from a data URL
  // and will not need to be pasted in this manner.
  auto new_data = std::make_unique<ui::ClipboardData>();
  new_data->set_markup_data(html_markup);

  scoped_clipboard_history_pause_ =
      ash::ClipboardHistoryController::Get()->CreateScopedPause();
  replaced_clipboard_data_ = clipboard->WriteClipboardData(std::move(new_data));
}

ClipboardImageModelRequest::ScopedClipboardModifier::
    ~ScopedClipboardModifier() {
  if (!replaced_clipboard_data_)
    return;

  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(replaced_clipboard_data_));
}

ClipboardImageModelRequest::ClipboardImageModelRequest(
    Profile* profile,
    base::RepeatingClosure on_request_finished_callback)
    : widget_(std::make_unique<views::Widget>()),
      web_view_(new views::WebView(profile)),
      on_request_finished_callback_(std::move(on_request_finished_callback)) {
  views::Widget::InitParams widget_params;
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.name = "ClipboardImageModelRequest";
  widget_->Init(std::move(widget_params));
  widget_->SetContentsView(web_view_);

  Observe(web_view_->GetWebContents());
  web_contents()->SetDelegate(this);
}

ClipboardImageModelRequest::~ClipboardImageModelRequest() = default;

void ClipboardImageModelRequest::Start(Params&& params) {
  DCHECK(!deliver_image_model_callback_);
  DCHECK(params.callback);
  DCHECK_EQ(base::UnguessableToken(), request_id_);

  request_id_ = std::move(params.id);
  html_markup_ = params.html_markup;
  deliver_image_model_callback_ = std::move(params.callback);

  timeout_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(10), this,
                       &ClipboardImageModelRequest::OnTimeout);

  // Begin the document with the proper charset, this should prevent strange
  // looking characters from showing up in the render in some cases.
  std::string html_document(
      "<!DOCTYPE html>"
      "<html>"
      " <head><meta charset=\"UTF-8\"></meta></head>"
      " <body contenteditable='true'> "
      "  <script>"
      // Focus the Contenteditable body to ensure WebContents::Paste() reaches
      // the body.
      "   document.body.focus();"
      "  </script>"
      " </body>"
      "</html");

  std::string encoded_html;
  base::Base64Encode(html_document, &encoded_html);
  constexpr char kDataURIPrefix[] = "data:text/html;base64,";
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(kDataURIPrefix + encoded_html)));
  widget_->ShowInactive();

  // Give some initial bounds to the NativeView to force painting in an inactive
  // shown widget.
  web_contents()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 1, 1));
}

void ClipboardImageModelRequest::Stop() {
  scoped_clipboard_modifier_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_.Stop();
  widget_->Hide();
  deliver_image_model_callback_.Reset();
  request_id_ = base::UnguessableToken();
  did_stop_loading_ = false;

  on_request_finished_callback_.Run();
}

ClipboardImageModelRequest::Params
ClipboardImageModelRequest::StopAndGetParams() {
  DCHECK(IsRunningRequest());
  Params params(request_id_, html_markup_,
                std::move(deliver_image_model_callback_));
  Stop();
  return params;
}

bool ClipboardImageModelRequest::IsModifyingClipboard() const {
  return scoped_clipboard_modifier_.has_value();
}

bool ClipboardImageModelRequest::IsRunningRequest(
    base::Optional<base::UnguessableToken> request_id) const {
  return request_id.has_value() ? *request_id == request_id_
                                : !request_id_.is_empty();
}

void ClipboardImageModelRequest::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  web_contents->GetNativeView()->SetBounds(gfx::Rect(gfx::Point(), new_size));

  // `ResizeDueToAutoResize()` can be called before and/or after
  // DidStopLoading(). If `DidStopLoading()` has not been called, wait for the
  // next resize before copying the surface.
  if (!web_contents->IsLoading())
    PostCopySurfaceTask();
}

void ClipboardImageModelRequest::DidStopLoading() {
  // `DidStopLoading()` can be called multiple times after a paste. We are only
  // interested in the initial load of the data URL.
  if (did_stop_loading_)
    return;

  did_stop_loading_ = true;

  // Modify the clipboard so `html_markup_` can be pasted into the WebContents.
  scoped_clipboard_modifier_.emplace(html_markup_);

  web_contents()->GetRenderViewHost()->GetWidget()->InsertVisualStateCallback(
      base::BindOnce(&ClipboardImageModelRequest::OnVisualStateChangeFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(https://crbug.com/1149556): Clipboard Contents could be overwritten
  // prior to the `WebContents::Paste()` completing.
  web_contents()->Paste();
}

void ClipboardImageModelRequest::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if (!web_contents()->GetRenderWidgetHostView())
    return;

  web_contents()->GetRenderWidgetHostView()->EnableAutoResize(
      gfx::Size(1, 1), gfx::Size(INT_MAX, INT_MAX));
}

void ClipboardImageModelRequest::OnVisualStateChangeFinished(bool done) {
  if (!done)
    return;

  scoped_clipboard_modifier_.reset();
  PostCopySurfaceTask();
}

void ClipboardImageModelRequest::PostCopySurfaceTask() {
  if (!deliver_image_model_callback_)
    return;

  // Debounce calls to `CopySurface()`. `DidStopLoading()` and
  // `ResizeDueToAutoResize()` can be called multiple times in the same task
  // sequence. Wait for the final update before copying the surface.
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardImageModelRequest::CopySurface,
                     copy_surface_weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(250));
}

void ClipboardImageModelRequest::CopySurface() {
  content::RenderWidgetHostView* source_view =
      web_contents()->GetRenderViewHost()->GetWidget()->GetView();
  if (source_view->GetViewBounds().size().IsEmpty()) {
    Stop();
    return;
  }

  // There is no guarantee CopyFromSurface will call OnCopyComplete. If this
  // takes too long, this will be cleaned up by |timeout_timer_|.
  source_view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&ClipboardImageModelRequest::OnCopyComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardImageModelRequest::OnCopyComplete(const SkBitmap& bitmap) {
  if (!deliver_image_model_callback_) {
    Stop();
    return;
  }

  std::move(deliver_image_model_callback_)
      .Run(ui::ImageModel::FromImageSkia(
          gfx::ImageSkia::CreateFrom1xBitmap(bitmap)));
  Stop();
}

void ClipboardImageModelRequest::OnTimeout() {
  DCHECK(deliver_image_model_callback_);
  Stop();
}
