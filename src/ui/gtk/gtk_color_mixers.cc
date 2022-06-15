// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_color_mixers.h"

#include "base/strings/strcat.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

void AddGtkNativeColorMixer(ui::ColorProvider* provider,
                            const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::ColorProviderManager::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();

  const std::string header_selector =
      key.frame_type == ui::ColorProviderManager::FrameType::kChromium
          ? "#headerbar.header-bar.titlebar"
          : "GtkMenuBar#menubar";
  const std::string header_selector_inactive = header_selector + ":backdrop";
  const auto tooltip_context = AppendCssNodeToStyleContext(
      {}, GtkCheckVersion(3, 20) ? "#tooltip.background"
                                 : "GtkWindow#window.background.tooltip");

  const SkColor primary_bg = GetBgColor("");
  const SkColor button_bg_disabled =
      GetBgColor("GtkButton#button.text-button:disabled");
  const SkColor button_border = GetBorderColor("GtkButton#button");
  const SkColor frame_color =
      SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
  const SkColor frame_color_inactive =
      SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);
  const SkColor label_fg = GetFgColor("GtkLabel#label");
  const SkColor label_fg_disabled = GetFgColor("GtkLabel#label:disabled");
  const SkColor entry_border = GetBorderColor("GtkEntry#entry");
  const SkColor toolbar_color =
      color_utils::GetResultingPaintColor(primary_bg, frame_color);
  const SkColor accent = GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label");

  // Core colors
  mixer[ui::kColorAccent] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorAlertHighSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleRed300, gfx::kGoogleRed600)};
  mixer[ui::kColorAlertLowSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen700)};
  mixer[ui::kColorAlertMediumSeverity] = {
      SelectBasedOnDarkInput(ui::kColorPrimaryBackground, gfx::kGoogleYellow300,
                             gfx::kGoogleYellow700)};
  mixer[ui::kColorDisabledForeground] = {label_fg_disabled};
  mixer[ui::kColorItemHighlight] = {GetBorderColor("GtkEntry#entry:focus")};
  mixer[ui::kColorItemSelectionBackground] = {ui::kColorAccent};
  mixer[ui::kColorMenuSelectionBackground] = {GetBgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover"}))};
  mixer[ui::kColorMidground] = {
      GetSeparatorColor("GtkSeparator#separator.horizontal")};
  mixer[ui::kColorPrimaryBackground] = {primary_bg};
  mixer[ui::kColorPrimaryForeground] = {label_fg};
  mixer[ui::kColorSecondaryForeground] = {label_fg_disabled};
  mixer[ui::kColorTextSelectionBackground] = {
      GetSelectionBgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                                 : "GtkLabel#label:selected")};
  mixer[ui::kColorTextSelectionForeground] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                        : "GtkLabel#label:selected")};

  // UI element colors
  mixer[ui::kColorAvatarHeaderArt] =
      AlphaBlend(ui::kColorPrimaryForeground, ui::kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha300);
  mixer[ui::kColorAvatarIconGuest] =
      DeriveDefaultIconColor(ui::kColorPrimaryForeground);
  mixer[ui::kColorButtonBackground] = {GetBgColor("GtkButton#button")};
  mixer[ui::kColorButtonBackgroundProminentDisabled] = {button_bg_disabled};
  mixer[ui::kColorButtonBorder] = {button_border};
  mixer[ui::kColorButtonBorderDisabled] = {button_bg_disabled};
  mixer[ui::kColorButtonForeground] = {
      GetFgColor("GtkButton#button.text-button GtkLabel#label")};
  mixer[ui::kColorButtonForegroundChecked] = {ui::kColorAccent};
  mixer[ui::kColorButtonForegroundDisabled] = {
      GetFgColor("GtkButton#button.text-button:disabled GtkLabel#label")};
  mixer[ui::kColorButtonForegroundProminent] = {accent};
  mixer[ui::kColorButtonForegroundUnchecked] = {ui::kColorButtonForeground};
  mixer[ui::kColorDialogForeground] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorDropdownBackground] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownBackgroundSelected] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownForeground] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownForegroundSelected] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  mixer[ui::kColorFrameActive] = {frame_color};
  mixer[ui::kColorFrameInactive] = {frame_color_inactive};
  mixer[ui::kColorFocusableBorderUnfocused] = {entry_border};
  mixer[ui::kColorHelpIconActive] = {
      GetFgColor("GtkButton#button.image-button:hover")};
  mixer[ui::kColorIcon] = {
      GetFgColor("GtkButton#button.flat.scale GtkImage#image")};
  mixer[ui::kColorHelpIconInactive] = {
      GetFgColor("GtkButton#button.image-button")};
  if (GtkCheckVersion(3, 12)) {
    mixer[ui::kColorLinkForeground] = {GetFgColor("GtkLabel#label.link:link")};
    mixer[ui::kColorLinkForegroundDisabled] = {
        GetFgColor("GtkLabel#label.link:link:disabled")};
    mixer[ui::kColorLinkForegroundPressed] = {
        GetFgColor("GtkLabel#label.link:link:hover:active")};
  } else {
    auto link_context = GetStyleContextFromCss("GtkLabel#label.view");
    GdkColor* gdk_color = nullptr;
    GtkStyleContextGetStyle(link_context, "link-color", &gdk_color, nullptr);
    SkColor color = SkColorSetRGB(0x00, 0x00, 0xEE);  // From gtklinkbutton.c.
    if (gdk_color) {
      color = SkColorSetRGB(gdk_color->red >> 8, gdk_color->green >> 8,
                            gdk_color->blue >> 8);
      // gdk_color_free() was deprecated in Gtk3.14.  This code path is only
      // taken on versions earlier than Gtk3.12, but the compiler doesn't know
      // that, so silence the deprecation warnings.
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_color_free(gdk_color);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
    mixer[ui::kColorLinkForeground] = {color};
    mixer[ui::kColorLinkForegroundDisabled] = {color};
    mixer[ui::kColorLinkForegroundPressed] = {color};
  }
  mixer[ui::kColorMenuBackground] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuBorder] = {GetBorderColor(GtkCssMenu())};
  mixer[ui::kColorMenuDropmarker] = {ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuIcon] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #radio" : ".radio"}))};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {ui::kColorMenuBackground};
  mixer[ui::kColorMenuItemForeground] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {
      ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundDisabled] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":disabled GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundSecondary] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #accelerator"
                                           : " GtkLabel#label.accelerator"}))};
  mixer[ui::kColorMenuItemForegroundSelected] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":hover GtkLabel#label"}))};
  mixer[ui::kColorMenuSeparator] = {GetSeparatorColor(
      GtkCheckVersion(3, 20)
          ? base::StrCat({GtkCssMenu(), " GtkSeparator#separator.horizontal"})
          : base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ".separator"}))};
  mixer[ui::kColorNotificationInputForeground] = {accent};
  mixer[ui::kColorOverlayScrollbarFill] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider")};
  mixer[ui::kColorOverlayScrollbarFillHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider:hover")};
  mixer[ui::kColorOverlayScrollbarStroke] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough")};
  mixer[ui::kColorOverlayScrollbarStrokeHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough:hover")};
  mixer[ui::kColorSliderThumb] = {GetBgColor("GtkScale#scale #highlight")};
  mixer[ui::kColorSliderThumbMinimal] = {
      GetBgColor("GtkScale#scale:disabled #highlight")};
  mixer[ui::kColorSliderTrack] = {GetBgColor("GtkScale#scale #trough")};
  mixer[ui::kColorSliderTrackMinimal] = {
      GetBgColor("GtkScale#scale:disabled #trough")};
  mixer[ui::kColorSyncInfoBackground] = {GetBgColor("#statusbar")};
  mixer[ui::kColorTabBackgroundHighlighted] = {
      GetBgColor("GtkNotebook#notebook #tab:checked")};
  mixer[ui::kColorTabBackgroundHighlightedFocused] = {
      GetBgColor("GtkNotebook#notebook:focus #tab:checked")};
  mixer[ui::kColorTabContentSeparator] = {GetBorderColor(
      GtkCheckVersion(3, 20) ? "GtkFrame#frame #border" : "GtkFrame#frame")};
  mixer[ui::kColorTabForegroundSelected] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorTableBackground] = {ui::kColorTreeBackground};
  mixer[ui::kColorTableBackgroundAlternate] = {ui::kColorTreeBackground};
  mixer[ui::kColorTableBackgroundSelectedUnfocused] = {
      ui::kColorTreeNodeBackgroundSelectedUnfocused};
  mixer[ui::kColorTableForeground] = {ui::kColorTreeNodeForeground};
  mixer[ui::kColorTableForegroundSelectedFocused] = {
      ui::kColorTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorTableForegroundSelectedUnfocused] = {
      ui::kColorTreeNodeForegroundSelectedUnfocused};
  mixer[ui::kColorTableGroupingIndicator] = {ui::kColorTableForeground};
  mixer[ui::kColorTableHeaderBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorTableHeaderForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel#label")};
  mixer[ui::kColorTableHeaderSeparator] = {
      GetBorderColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorTextfieldBackground] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view"
                                        : "GtkTextView.view")};
  mixer[ui::kColorTextfieldBackgroundDisabled] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled"
                                        : "GtkTextView.view:disabled")};
  mixer[ui::kColorTextfieldForeground] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text"
                                        : "GtkTextView.view")};
  mixer[ui::kColorTextfieldForegroundDisabled] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled #text"
                             : "GtkTextView.view:disabled")};
  mixer[ui::kColorTextfieldForegroundPlaceholder] = {GtkCheckVersion(4)};
  mixer[ui::kColorTextfieldSelectionBackground] = {GetSelectionBgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorTextfieldSelectionForeground] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorThrobber] = {GetFgColor("GtkSpinner#spinner")};
  mixer[ui::kColorThrobberPreconnect] = {
      GetFgColor("GtkSpinner#spinner:disabled")};
  mixer[ui::kColorToggleButtonTrackOff] = {
      GetBgColor("GtkButton#button.text-button.toggle")};
  mixer[ui::kColorToggleButtonTrackOn] = {
      GetBgColor("GtkButton#button.text-button.toggle:checked")};
  mixer[ui::kColorTooltipBackground] = {
      GetBgColorFromStyleContext(tooltip_context)};
  mixer[ui::kColorTooltipForeground] = {GtkStyleContextGetColor(
      AppendCssNodeToStyleContext(tooltip_context, "GtkLabel#label"))};
  mixer[ui::kColorTreeBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell")};
  mixer[ui::kColorTreeNodeForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
                 "GtkLabel#label")};
  mixer[ui::kColorTreeNodeForegroundSelectedFocused] = {accent};
  mixer[ui::kColorTreeNodeBackgroundSelectedUnfocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected")};
  mixer[ui::kColorTreeNodeForegroundSelectedUnfocused] = {
      GetFgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected GtkLabel#label")};

  // Platform-specific UI elements
  mixer[ui::kColorNativeHeaderButtonBorderActive] = {
      GetBorderColor(header_selector + " GtkButton#button")};
  mixer[ui::kColorNativeHeaderButtonBorderInactive] = {
      GetBorderColor(header_selector + ":backdrop GtkButton#button")};
  mixer[ui::kColorNativeHeaderSeparatorBorderActive] = {GetBorderColor(
      header_selector + " GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeHeaderSeparatorBorderInactive] = {
      GetBorderColor(header_selector +
                     ":backdrop GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameActive] = {
      GetFgColor(header_selector + " GtkLabel#label.title")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameInactive] = {
      GetFgColor(header_selector_inactive + " GtkLabel#label.title")};
  mixer[ui::kColorNativeToolbarBackground] = {toolbar_color};
  mixer[ui::kColorNativeTextfieldBorderUnfocused] = {entry_border};
  mixer[ui::kColorNativeButtonBorder] = {button_border};
  mixer[ui::kColorNativeLabelForeground] = {label_fg};
}

}  // namespace gtk
