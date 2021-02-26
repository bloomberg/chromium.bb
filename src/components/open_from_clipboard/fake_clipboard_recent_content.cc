// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/fake_clipboard_recent_content.h"

FakeClipboardRecentContent::FakeClipboardRecentContent()
    : content_age_(base::TimeDelta::Max()), suppress_content_(false) {}

FakeClipboardRecentContent::~FakeClipboardRecentContent() {}

base::Optional<GURL> FakeClipboardRecentContent::GetRecentURLFromClipboard() {
  if (suppress_content_)
    return base::nullopt;

  return clipboard_url_content_;
}

base::Optional<base::string16>
FakeClipboardRecentContent::GetRecentTextFromClipboard() {
  if (suppress_content_)
    return base::nullopt;

  return clipboard_text_content_;
}

void FakeClipboardRecentContent::GetRecentImageFromClipboard(
    GetRecentImageCallback callback) {
  if (suppress_content_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::move(callback).Run(clipboard_image_content_);
}

bool FakeClipboardRecentContent::HasRecentImageFromClipboard() {
  if (suppress_content_)
    return false;
  return clipboard_image_content_.has_value();
}

void FakeClipboardRecentContent::HasRecentContentFromClipboard(
    std::set<ClipboardContentType> types,
    HasDataCallback callback) {
  std::set<ClipboardContentType> matching_types;
  for (ClipboardContentType type : types) {
    switch (type) {
      case ClipboardContentType::URL:
        if (GetRecentURLFromClipboard()) {
          matching_types.insert(ClipboardContentType::URL);
        }
        break;
      case ClipboardContentType::Text:
        if (GetRecentTextFromClipboard()) {
          matching_types.insert(ClipboardContentType::Text);
        }
        break;
      case ClipboardContentType::Image:
        if (HasRecentImageFromClipboard()) {
          matching_types.insert(ClipboardContentType::Image);
        }
        break;
    }
  }
  std::move(callback).Run(matching_types);
}

void FakeClipboardRecentContent::GetRecentURLFromClipboard(
    GetRecentURLCallback callback) {
  std::move(callback).Run(GetRecentURLFromClipboard());
}

void FakeClipboardRecentContent::GetRecentTextFromClipboard(
    GetRecentTextCallback callback) {
  std::move(callback).Run(GetRecentTextFromClipboard());
}

base::TimeDelta FakeClipboardRecentContent::GetClipboardContentAge() const {
  return content_age_;
}

void FakeClipboardRecentContent::SuppressClipboardContent() {
  suppress_content_ = true;
}

void FakeClipboardRecentContent::ClearClipboardContent() {
  clipboard_url_content_ = base::nullopt;
  clipboard_text_content_ = base::nullopt;
  clipboard_image_content_ = base::nullopt;
  content_age_ = base::TimeDelta::Max();
  suppress_content_ = false;
}

void FakeClipboardRecentContent::SetClipboardURL(const GURL& url,
                                                 base::TimeDelta content_age) {
  DCHECK(url.is_valid());
  clipboard_url_content_ = url;
  clipboard_text_content_ = base::nullopt;
  clipboard_image_content_ = base::nullopt;
  content_age_ = content_age;
  suppress_content_ = false;
}

void FakeClipboardRecentContent::SetClipboardText(const base::string16& text,
                                                  base::TimeDelta content_age) {
  clipboard_url_content_ = base::nullopt;
  clipboard_text_content_ = text;
  clipboard_image_content_ = base::nullopt;
  content_age_ = content_age;
  suppress_content_ = false;
}

void FakeClipboardRecentContent::SetClipboardImage(
    const gfx::Image& image,
    base::TimeDelta content_age) {
  clipboard_url_content_ = base::nullopt;
  clipboard_text_content_ = base::nullopt;
  clipboard_image_content_ = image;
  content_age_ = content_age;
  suppress_content_ = false;
}
