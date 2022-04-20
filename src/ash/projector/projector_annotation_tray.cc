// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_annotation_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Margins between the title view and the edges around it (dp).
constexpr int kPaddingBetweenBottomAndLastTrayItem = 4;

// Width of the bubble itself (dp).
constexpr int kBubbleWidth = 196;

// Insets for the views (dp).
constexpr auto kPenViewPadding = gfx::Insets::TLBR(4, 16, 0, 16);

// Spacing between buttons (dp).
constexpr int kButtonsPadding = 12;

// Size of menu rows.
constexpr int kMenuRowHeight = 48;

// Color selection buttons.
constexpr int kColorButtonColorViewSize = 16;
constexpr int kColorButtonViewRadius = 28;

constexpr SkColor kPenColors[] = {
    kProjectorMagentaPenColor, kProjectorBluePenColor, kProjectorYellowPenColor,
    kProjectorRedPenColor};

// TODO(b/201664243): Use AnnotatorToolType.
enum ProjectorTool { kToolNone, kToolPen };

ProjectorTool GetCurrentTool() {
  auto* controller = Shell::Get()->projector_controller();
  // ProjctorController may not be available yet as the ProjectorAnnotationTray
  // is created before it.
  if (!controller)
    return kToolNone;

  if (controller->IsAnnotatorEnabled())
    return kToolPen;
  return kToolNone;
}

const gfx::VectorIcon& GetIconForTool(ProjectorTool tool, SkColor color) {
  switch (tool) {
    case kToolNone:
      return kPaletteTrayIconProjectorIcon;
    case kToolPen:
      switch (color) {
        case kProjectorMagentaPenColor:
          return kPaletteTrayIconProjectorMagentaIcon;
        case kProjectorBluePenColor:
          return kPaletteTrayIconProjectorBlueIcon;
        case kProjectorRedPenColor:
          return kPaletteTrayIconProjectorRedIcon;
        case kProjectorYellowPenColor:
          return kPaletteTrayIconProjectorYellowIcon;
      }
  }

  NOTREACHED();
  return kPaletteTrayIconProjectorIcon;
}

}  // namespace

ProjectorAnnotationTray::ProjectorAnnotationTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      image_view_(
          tray_container()->AddChildView(std::make_unique<views::ImageView>())),
      pen_view_(nullptr) {
  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  // The default pen color upon creation is magenta.
  current_pen_color_ = kProjectorMagentaPenColor;
}

ProjectorAnnotationTray::~ProjectorAnnotationTray() = default;

bool ProjectorAnnotationTray::PerformAction(const ui::Event& event) {
  ToggleAnnotator();
  return true;
}

void ProjectorAnnotationTray::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED) {
    return;
  }
  if (event->IsRightMouseButton()) {
    ShowBubble();
  } else if (event->IsLeftMouseButton()) {
    ToggleAnnotator();
  }
}

void ProjectorAnnotationTray::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().type() == ui::ET_GESTURE_LONG_PRESS) {
    ShowBubble();
  } else if (event->details().type() == ui::ET_GESTURE_TAP) {
    ToggleAnnotator();
  }
}

void ProjectorAnnotationTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string ProjectorAnnotationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_TITLE);
}

void ProjectorAnnotationTray::HandleLocaleChange() {}

void ProjectorAnnotationTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void ProjectorAnnotationTray::CloseBubble() {
  pen_view_ = nullptr;
  bubble_.reset();

  shelf()->UpdateAutoHideState();
}

void ProjectorAnnotationTray::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(tray_container());

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetBubbleAnchor()->GetAnchorBoundsInScreen();
  init_params.anchor_rect.Inset(GetBubbleAnchorInsets());
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.translucent = true;
  init_params.has_shadow = false;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.reroute_event_handler = true;

  // Create and customize bubble view.
  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_margins(GetSecondaryBubbleInsets());
  bubble_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kPaddingBetweenBottomAndLastTrayItem, 0)));

  auto setup_layered_view = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  // Add drawing tools
  {
    auto* marker_view_container =
        bubble_view->AddChildView(std::make_unique<views::View>());

    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kPenViewPadding,
        kButtonsPadding);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    box_layout->set_minimum_cross_axis_size(kMenuRowHeight);
    marker_view_container->SetLayoutManager(std::move(box_layout));

    for (SkColor color : kPenColors) {
      auto* colorButton = marker_view_container->AddChildView(
          std::make_unique<ProjectorColorButton>(
              base::BindRepeating(&ProjectorAnnotationTray::OnPenColorPressed,
                                  base::Unretained(this), color),
              color, kColorButtonColorViewSize, kColorButtonViewRadius,
              l10n_util::GetStringUTF16(GetAccessibleNameForColor(color))));
      colorButton->SetToggled(current_pen_color_ == color);
    }
    setup_layered_view(marker_view_container);
  }

  // Show the bubble.
  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view);
  SetIsActive(true);
}

TrayBubbleView* ProjectorAnnotationTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* ProjectorAnnotationTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void ProjectorAnnotationTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateIcon();
}

void ProjectorAnnotationTray::HideAnnotationTray() {
  SetVisiblePreferred(false);
  UpdateIcon();
  // Reset pen color to default color.
  current_pen_color_ = kProjectorMagentaPenColor;
}

void ProjectorAnnotationTray::ToggleAnnotator() {
  if (GetCurrentTool() == kToolNone) {
    EnableAnnotatorTool();
  } else {
    DeactivateActiveTool();
  }
  if (bubble_) {
    CloseBubble();
  }
  UpdateIcon();
}

void ProjectorAnnotationTray::EnableAnnotatorTool() {
  auto* controller = Shell::Get()->projector_controller();
  DCHECK(controller);
  controller->OnMarkerPressed();
}

void ProjectorAnnotationTray::DeactivateActiveTool() {
  auto* controller = Shell::Get()->projector_controller();
  DCHECK(controller);
  controller->ResetTools();
}

void ProjectorAnnotationTray::UpdateIcon() {
  const ProjectorTool tool = GetCurrentTool();
  image_view_->SetImage(gfx::CreateVectorIcon(
      GetIconForTool(tool, current_pen_color_),
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  SetIsActive(tool != kToolNone);
}

void ProjectorAnnotationTray::OnPenColorPressed(SkColor color) {
  auto* projector_controller = ProjectorControllerImpl::Get();
  DCHECK(projector_controller);
  AnnotatorTool tool;
  tool.color = color;
  projector_controller->SetAnnotatorTool(tool);
  current_pen_color_ = color;
  CloseBubble();
  UpdateIcon();
}

int ProjectorAnnotationTray::GetAccessibleNameForColor(SkColor color) {
  switch (color) {
    case kProjectorRedPenColor:
      return IDS_RED_COLOR_BUTTON;
    case kProjectorBluePenColor:
      return IDS_BLUE_COLOR_BUTTON;
    case kProjectorYellowPenColor:
      return IDS_YELLOW_COLOR_BUTTON;
    case kProjectorMagentaPenColor:
      return IDS_MAGENTA_COLOR_BUTTON;
  }
  NOTREACHED();
  return IDS_RED_COLOR_BUTTON;
}

}  // namespace ash
