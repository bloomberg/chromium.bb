// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/ui/media_router/internal/vector_icons/vector_icons.h"
#endif

namespace media_router {

namespace {

gfx::ImageSkia CreateSinkIcon(SinkIconType icon_type, bool enabled = true) {
  const gfx::VectorIcon* vector_icon;
  switch (icon_type) {
    case SinkIconType::CAST_AUDIO_GROUP:
      vector_icon = &kSpeakerGroupIcon;
      break;
    case SinkIconType::CAST_AUDIO:
      vector_icon = &kSpeakerIcon;
      break;
    case SinkIconType::EDUCATION:
      vector_icon = &kCastForEducationIcon;
      break;
    case SinkIconType::WIRED_DISPLAY:
      vector_icon = &kInputIcon;
      break;
// Use proprietary icons only in Chrome builds. The default TV icon is used
// instead for these sink types in Chromium builds.
#if defined(GOOGLE_CHROME_BUILD)
    case SinkIconType::MEETING:
      vector_icon = &vector_icons::kMeetIcon;
      break;
    case SinkIconType::HANGOUT:
      vector_icon = &vector_icons::kHangoutIcon;
      break;
#endif  // defined(GOOGLE_CHROME_BUILD)
    case SinkIconType::CAST:
    case SinkIconType::GENERIC:
    default:
      vector_icon = &kTvIcon;
      break;
  }
  SkColor icon_color = enabled ? gfx::kChromeIconGrey : gfx::kGoogleGrey500;
  return gfx::CreateVectorIcon(
      *vector_icon, CastDialogSinkButton::kPrimaryIconSize, icon_color);
}

gfx::ImageSkia CreateDisabledSinkIcon(SinkIconType icon_type) {
  return CreateSinkIcon(icon_type, false);
}

std::unique_ptr<views::View> CreateThrobber() {
  views::Throbber* throbber = new views::Throbber();
  auto throbber_container = std::make_unique<views::View>();
  throbber_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  constexpr int kBorderWidth = 4;
  throbber_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kBorderWidth)));
  throbber->Start();
  throbber_container->AddChildView(throbber);
  return throbber_container;
}

std::unique_ptr<views::View> CreatePrimaryIconForSink(const UIMediaSink& sink) {
  if (sink.issue) {
    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(CreateVectorIcon(::vector_icons::kInfoOutlineIcon,
                                         CastDialogSinkButton::kPrimaryIconSize,
                                         gfx::kChromeIconGrey));
    return icon_view;
  } else if (sink.state == UIMediaSinkState::CONNECTED ||
             sink.state == UIMediaSinkState::DISCONNECTING) {
    auto icon_view = std::make_unique<views::ImageView>();
    // TODO(takumif): Replace the vector icon to match the mocks.
    icon_view->SetImage(CreateVectorIcon(kNavigateStopIcon,
                                         CastDialogSinkButton::kPrimaryIconSize,
                                         gfx::kGoogleBlue500));
    return icon_view;
  } else if (sink.state == UIMediaSinkState::CONNECTING) {
    return CreateThrobber();
  }
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(CreateSinkIcon(sink.icon_type));
  return icon_view;
}

base::string16 GetStatusTextForSink(const UIMediaSink& sink) {
  if (sink.issue)
    return base::UTF8ToUTF16(sink.issue->info().title);
  if (!sink.status_text.empty())
    return sink.status_text;
  switch (sink.state) {
    case UIMediaSinkState::AVAILABLE:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE);
    case UIMediaSinkState::CONNECTING:
      return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING);
    default:
      return base::string16();
  }
}

}  // namespace

// static
int CastDialogSinkButton::kPrimaryIconSize = 24;
int CastDialogSinkButton::kSecondaryIconSize = 16;

CastDialogSinkButton::CastDialogSinkButton(
    views::ButtonListener* button_listener,
    const UIMediaSink& sink)
    : HoverButton(button_listener,
                  CreatePrimaryIconForSink(sink),
                  sink.friendly_name,
                  GetStatusTextForSink(sink),
                  /** secondary_icon_view */ nullptr),
      sink_(sink) {}

CastDialogSinkButton::~CastDialogSinkButton() = default;

bool CastDialogSinkButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return true;
  return HoverButton::OnMousePressed(event);
}

void CastDialogSinkButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return;
  HoverButton::OnMouseReleased(event);
}

void CastDialogSinkButton::OnEnabledChanged() {
  HoverButton::OnEnabledChanged();
  SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor);
  if (enabled()) {
    SetTitleTextStyle(views::style::STYLE_PRIMARY, background_color);
    if (sink_.state == UIMediaSinkState::AVAILABLE) {
      static_cast<views::ImageView*>(icon_view())
          ->SetImage(CreateSinkIcon(sink_.icon_type));
    }
  } else {
    SetTitleTextStyle(views::style::STYLE_DISABLED, background_color);
    if (sink_.state == UIMediaSinkState::AVAILABLE) {
      static_cast<views::ImageView*>(icon_view())
          ->SetImage(CreateDisabledSinkIcon(sink_.icon_type));
    }
  }
  title()->Layout();
}

}  // namespace media_router
