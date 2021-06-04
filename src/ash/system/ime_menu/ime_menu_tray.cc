// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_menu_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Used for testing.
const int kEmojiButtonId = 1;

// Insets for the title view (dp).
constexpr gfx::Insets kTitleViewPadding(0, 0, 0, 16);

// Returns the height range of ImeListView.
gfx::Range GetImeListViewRange() {
  const int max_items = 5;
  const int min_items = 1;
  const int tray_item_height = kTrayPopupItemMinHeight;
  return gfx::Range(tray_item_height * min_items, tray_item_height * max_items);
}

// Returns true if the current screen is login or lock screen.
bool IsInLoginOrLockScreen() {
  using session_manager::SessionState;
  SessionState state = Shell::Get()->session_controller()->GetSessionState();
  return state == SessionState::LOGIN_PRIMARY ||
         state == SessionState::LOCKED ||
         state == SessionState::LOGIN_SECONDARY;
}

// Returns true if the current input context type is password.
bool IsInPasswordInputContext() {
  return ui::IMEBridge::Get()->GetCurrentInputContext().type ==
         ui::TEXT_INPUT_TYPE_PASSWORD;
}

class ImeMenuLabel : public views::Label {
 public:
  METADATA_HEADER(ImeMenuLabel);

  ImeMenuLabel() {
    // Sometimes the label will be more than 2 characters, e.g. INTL and EXTD.
    // This border makes sure we only leave room for ~2 and the others are
    // truncated.
    SetBorder(views::CreateEmptyBorder(gfx::Insets(0, 6)));
  }
  ImeMenuLabel(const ImeMenuLabel&) = delete;
  ImeMenuLabel& operator=(const ImeMenuLabel&) = delete;
  ~ImeMenuLabel() override = default;

  // views:Label:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kTrayItemSize, kTrayItemSize);
  }
  int GetHeightForWidth(int width) const override { return kTrayItemSize; }
};

BEGIN_METADATA(ImeMenuLabel, views::Label)
END_METADATA

class ImeMenuImageView : public views::ImageView {
 public:
  METADATA_HEADER(ImeMenuImageView);

  ImeMenuImageView() { SetBorder(views::CreateEmptyBorder(gfx::Insets(0, 6))); }
  ImeMenuImageView(const ImeMenuImageView&) = delete;
  ImeMenuImageView& operator=(const ImeMenuImageView&) = delete;
  ~ImeMenuImageView() override = default;
};

BEGIN_METADATA(ImeMenuImageView, views::ImageView)
END_METADATA

// The view that contains IME menu title.
class ImeTitleView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(ImeTitleView);
  ImeTitleView() {
    auto* color_provider = AshColorProvider::Get();
    SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            0, 0, kMenuSeparatorWidth, 0,
            color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kSeparatorColor)),
        gfx::Insets(kMenuSeparatorVerticalPadding - kMenuSeparatorWidth, 0)));
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    SetInsideBorderInsets(kTitleViewPadding);
    SetMinimumCrossAxisSize(kTrayPopupItemMinHeight);

    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME)));
    title_label->SetBorder(
        views::CreateEmptyBorder(0, kMenuEdgeEffectivePadding, 1, 0));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    TrayPopupUtils::SetLabelFontList(title_label,
                                     TrayPopupUtils::FontStyle::kPodMenuHeader);
    SetFlexForView(title_label, 1);

    settings_button_ = AddChildView(std::make_unique<TopShortcutButton>(
        base::BindRepeating([]() {
          base::RecordAction(
              base::UserMetricsAction("StatusArea_IME_Detailed"));
          Shell::Get()->system_tray_model()->client()->ShowIMESettings();
        }),
        kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_IME_SETTINGS));
    settings_button_->SetEnabled(TrayPopupUtils::CanOpenWebUISettings());
  }
  ImeTitleView(const ImeTitleView&) = delete;
  ImeTitleView& operator=(const ImeTitleView&) = delete;

  ~ImeTitleView() override = default;

 private:
  TopShortcutButton* settings_button_ = nullptr;
};

BEGIN_METADATA(ImeTitleView, views::BoxLayoutView)
END_METADATA

// The view that contains buttons shown on the bottom of IME menu.
class ImeButtonsView : public views::View {
 public:
  METADATA_HEADER(ImeButtonsView);
  ImeButtonsView(ImeMenuTray* ime_menu_tray,
                 bool show_emoji,
                 bool show_handwriting,
                 bool show_voice)
      : ime_menu_tray_(ime_menu_tray) {
    DCHECK(ime_menu_tray_);

    Init(show_emoji, show_handwriting, show_voice);
  }
  ImeButtonsView(const ImeButtonsView&) = delete;
  ImeButtonsView& operator=(const ImeButtonsView&) = delete;

  ~ImeButtonsView() override = default;

  void KeysetButtonPressed(chromeos::input_method::ImeKeyset keyset) {
    // TODO(dcheng): When https://crbug.com/742517 is fixed, Mojo will generate
    // a constant for the number of values in the enum. For now, we just define
    // it here and keep it in sync with the enum.
    const int kImeKeysetUmaBoundary = 4;
    UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeMenu.EmojiHandwritingVoiceButton",
                              keyset, kImeKeysetUmaBoundary);
    // The |keyset| will be used for drawing input view keyset in IME
    // extensions. ImeMenuTray::ShowKeyboardWithKeyset() will deal with
    // the |keyset| string to generate the right input view url.
    ime_menu_tray_->ShowKeyboardWithKeyset(keyset);
  }

 private:
  void Init(bool show_emoji, bool show_handwriting, bool show_voice) {
    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal);
    box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
    SetLayoutManager(std::move(box_layout));
    SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            kMenuSeparatorWidth, 0, 0, 0,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kSeparatorColor)),
        gfx::Insets(kMenuSeparatorVerticalPadding - kMenuSeparatorWidth,
                    kMenuExtraMarginFromLeftEdge)));

    if (show_emoji) {
      emoji_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              chromeos::input_method::ImeKeyset::kEmoji),
          kImeMenuEmoticonIcon, IDS_ASH_STATUS_TRAY_IME_EMOJI);
      emoji_button_->SetID(kEmojiButtonId);
      AddChildView(emoji_button_);
    }

    if (show_handwriting) {
      handwriting_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              chromeos::input_method::ImeKeyset::kHandwriting),
          kImeMenuWriteIcon, IDS_ASH_STATUS_TRAY_IME_HANDWRITING);
      AddChildView(handwriting_button_);
    }

    if (show_voice) {
      voice_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              chromeos::input_method::ImeKeyset::kVoice),
          kImeMenuMicrophoneIcon, IDS_ASH_STATUS_TRAY_IME_VOICE);
      AddChildView(voice_button_);
    }
  }

  ImeMenuTray* ime_menu_tray_;
  SystemMenuButton* emoji_button_;
  SystemMenuButton* handwriting_button_;
  SystemMenuButton* voice_button_;
};

BEGIN_METADATA(ImeButtonsView, views::View)
END_METADATA

// A list of available IMEs shown in the opt-in IME menu, which has a different
// height depending on the number of IMEs in the list.
class ImeMenuListView : public ImeListView {
 public:
  METADATA_HEADER(ImeMenuListView);

  ImeMenuListView() : ImeMenuListView(std::make_unique<Delegate>()) {}
  ImeMenuListView(const ImeMenuListView&) = delete;
  ImeMenuListView& operator=(const ImeMenuListView&) = delete;

  ~ImeMenuListView() override = default;

 private:
  class Delegate : public DetailedViewDelegate {
   public:
    Delegate() : DetailedViewDelegate(nullptr /* tray_controller */) {}

    // DetailedViewDelegate:
    void TransitionToMainView(bool restore_focus) override {}
    void CloseBubble() override {}

    gfx::Insets GetInsetsForDetailedView() const override {
      return gfx::Insets();
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  explicit ImeMenuListView(std::unique_ptr<Delegate> delegate)
      : ImeListView(delegate.get()) {
    set_should_focus_ime_after_selection_with_keyboard(true);
    delegate_ = std::move(delegate);
  }

  // ImeListView:
  void Layout() override {
    gfx::Range height_range = GetImeListViewRange();
    scroller()->ClipHeightTo(height_range.start(), height_range.end());
    ImeListView::Layout();
  }

  std::unique_ptr<Delegate> delegate_;
};

BEGIN_METADATA(ImeMenuListView, ImeListView)
END_METADATA

}  // namespace

ImeMenuTray::ImeMenuTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      ime_controller_(Shell::Get()->ime_controller()),
      label_(nullptr),
      image_view_(nullptr),
      keyboard_suppressed_(false),
      show_bubble_after_keyboard_hidden_(false),
      is_emoji_enabled_(false),
      is_handwriting_enabled_(false),
      is_voice_enabled_(false) {
  DCHECK(ime_controller_);
  CreateLabel();
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->AddIMEObserver(this);
  tray_notifier->AddVirtualKeyboardObserver(this);

  // Show the tray even if virtual keyboard is shown. (Other tray buttons will
  // be hidden).
  set_show_with_virtual_keyboard(true);
}

ImeMenuTray::~ImeMenuTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->RemoveIMEObserver(this);
  tray_notifier->RemoveVirtualKeyboardObserver(this);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->HasObserver(this))
    keyboard_controller->RemoveObserver(this);
}

void ImeMenuTray::ShowImeMenuBubbleInternal() {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetBubbleAnchor()->GetAnchorBoundsInScreen();
  init_params.anchor_rect.Inset(GetBubbleAnchorInsets());
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.reroute_event_handler = true;

  auto setup_layered_view = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  };

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_margins(GetSecondaryBubbleInsets());

  // Add a title item with a separator on the top of the IME menu.
  setup_layered_view(
      bubble_view->AddChildView(std::make_unique<ImeTitleView>()));

  // Adds IME list to the bubble.
  ime_list_view_ =
      bubble_view->AddChildView(std::make_unique<ImeMenuListView>());
  ime_list_view_->Init(ShouldShowKeyboardToggle(),
                       ImeListView::SHOW_SINGLE_IME);
  setup_layered_view(ime_list_view_);

  if (ShouldShowBottomButtons()) {
    setup_layered_view(
        bubble_view->AddChildView(std::make_unique<ImeButtonsView>(
            this, is_emoji_enabled_, is_handwriting_enabled_,
            is_voice_enabled_)));
  }

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view);
  SetIsActive(true);
}

void ImeMenuTray::ShowKeyboardWithKeyset(
    chromeos::input_method::ImeKeyset keyset) {
  CloseBubble();

  Shell::Get()
      ->keyboard_controller()
      ->virtual_keyboard_controller()
      ->ForceShowKeyboardWithKeyset(keyset);
}

bool ImeMenuTray::ShouldShowBottomButtons() {
  // Emoji, handwriting and voice input is not supported for these cases:
  // 1) third party IME extensions.
  // 2) login/lock screen.
  // 3) password input client.

  bool should_show_buttom_buttoms =
      ime_controller_->is_extra_input_options_enabled() &&
      !ime_controller_->current_ime().third_party && !IsInLoginOrLockScreen() &&
      !IsInPasswordInputContext();

  if (!should_show_buttom_buttoms) {
    is_emoji_enabled_ = is_handwriting_enabled_ = is_voice_enabled_ = false;
    return false;
  }

  is_emoji_enabled_ = ime_controller_->is_emoji_enabled();
  is_handwriting_enabled_ = ime_controller_->is_handwriting_enabled();
  is_voice_enabled_ = ime_controller_->is_voice_enabled();

  return is_emoji_enabled_ || is_handwriting_enabled_ || is_voice_enabled_;
}

bool ImeMenuTray::ShouldShowKeyboardToggle() const {
  return keyboard_suppressed_ && !Shell::Get()
                                      ->accessibility_controller()
                                      ->virtual_keyboard()
                                      .enabled();
}

void ImeMenuTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayLabel();
}

std::u16string ImeMenuTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::HandleLocaleChange() {
  if (image_view_) {
    image_view_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  }

  if (label_)
    label_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
}

void ImeMenuTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void ImeMenuTray::ClickedOutsideBubble() {
  CloseBubble();
}

bool ImeMenuTray::PerformAction(const ui::Event& event) {
  if (event.IsMouseEvent() || event.IsGestureEvent()) {
    UserMetricsRecorder::RecordUserClickOnTray(
        LoginMetricsRecorder::TrayClickTarget::kImeTray);
  }
  return TrayBackgroundView::PerformAction(event);
}

void ImeMenuTray::CloseBubble() {
  bubble_.reset();
  ime_list_view_ = nullptr;
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void ImeMenuTray::ShowBubble() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    show_bubble_after_keyboard_hidden_ = true;
    keyboard_controller->AddObserver(this);
    keyboard_controller->HideKeyboardExplicitlyBySystem();
  } else {
    base::RecordAction(base::UserMetricsAction("Tray_ImeMenu_Opened"));
    ShowImeMenuBubbleInternal();
  }
}

TrayBubbleView* ImeMenuTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* ImeMenuTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void ImeMenuTray::OnIMERefresh() {
  UpdateTrayLabel();
  if (bubble_ && ime_list_view_) {
    ime_list_view_->Update(
        ime_controller_->current_ime().id, ime_controller_->available_imes(),
        ime_controller_->current_ime_menu_items(), ShouldShowKeyboardToggle(),
        ImeListView::SHOW_SINGLE_IME);
  }
}

void ImeMenuTray::OnIMEMenuActivationChanged(bool is_activated) {
  SetVisiblePreferred(is_activated);
  if (is_activated)
    UpdateTrayLabel();
  else
    CloseBubble();
}

std::u16string ImeMenuTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

bool ImeMenuTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void ImeMenuTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void ImeMenuTray::OnKeyboardHidden(bool is_temporary_hide) {
  if (show_bubble_after_keyboard_hidden_) {
    show_bubble_after_keyboard_hidden_ = false;
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    keyboard_controller->RemoveObserver(this);

    ShowImeMenuBubbleInternal();
    return;
  }
}

void ImeMenuTray::OnKeyboardSuppressionChanged(bool suppressed) {
  if (suppressed != keyboard_suppressed_ && bubble_)
    CloseBubble();
  keyboard_suppressed_ = suppressed;
}

void ImeMenuTray::UpdateTrayLabel() {
  const ImeInfo& current_ime = ime_controller_->current_ime();

  // For ARC IMEs, we use the globe icon instead of the short name of the active
  // IME.
  if (chromeos::extension_ime_util::IsArcIME(current_ime.id)) {
    CreateImageView();
    image_view_->SetImage(gfx::CreateVectorIcon(
        kShelfGlobeIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
    return;
  }

  // Updates the tray label based on the current input method.
  CreateLabel();

  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary));

  if (current_ime.third_party)
    label_->SetText(current_ime.short_name + u"*");
  else
    label_->SetText(current_ime.short_name);
}

void ImeMenuTray::CreateLabel() {
  // Do nothing if label_ is already created.
  if (label_)
    return;
  // Remove image_view_ at first if it's created.
  if (image_view_) {
    tray_container()->RemoveChildView(image_view_);
    image_view_ = nullptr;
  }
  label_ = new ImeMenuLabel();
  SetupLabelForTray(label_);
  label_->SetElideBehavior(gfx::TRUNCATE);
  label_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  tray_container()->AddChildView(label_);
}

void ImeMenuTray::CreateImageView() {
  // Do nothing if image_view_ is already created.
  if (image_view_)
    return;
  // Remove label_ at first if it's created.
  if (label_) {
    tray_container()->RemoveChildView(label_);
    label_ = nullptr;
  }
  image_view_ = new ImeMenuImageView();
  image_view_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  tray_container()->AddChildView(image_view_);
}

BEGIN_METADATA(ImeMenuTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
