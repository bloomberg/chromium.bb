// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui.h"

#include <cairo.h>
#include <pango/pango.h>

#include <cmath>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/themes/theme_properties.h"  // nogncheck
#include "printing/buildflags/buildflags.h"          // nogncheck
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/cursor/cursor_theme_manager_observer.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/linux/fake_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/display/display.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_key_bindings_handler.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/input_method_context_impl_gtk.h"
#include "ui/gtk/native_theme_gtk.h"
#include "ui/gtk/nav_button_provider_gtk.h"
#include "ui/gtk/printing/print_dialog_gtk.h"
#include "ui/gtk/printing/printing_gtk_util.h"
#include "ui/gtk/select_file_dialog_impl.h"
#include "ui/gtk/settings_provider_gtk.h"
#include "ui/gtk/window_frame_provider_gtk.h"
#include "ui/native_theme/native_theme.h"
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/linux_ui/device_scale_factor_observer.h"
#include "ui/views/linux_ui/nav_button_provider.h"
#include "ui/views/linux_ui/window_button_order_observer.h"

#if defined(USE_GIO)
#include "ui/gtk/settings_provider_gsettings.h"
#endif

#if BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#define USE_WAYLAND
#endif
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_X11
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_context_linux.h"
#endif

#if defined(USE_WAYLAND)
#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"
#endif

#if defined(USE_X11)
#include "ui/gtk/x/gtk_ui_platform_x11.h"
#endif

namespace gtk {

namespace {

// Stores the GtkUi singleton instance
const GtkUi* g_gtk_ui = nullptr;

const double kDefaultDPI = 96;

class GtkButtonImageSource : public gfx::ImageSkiaSource {
 public:
  GtkButtonImageSource(bool focus,
                       views::Button::ButtonState button_state,
                       gfx::Size size)
      : focus_(focus), width_(size.width()), height_(size.height()) {
    switch (button_state) {
      case views::Button::ButtonState::STATE_NORMAL:
        state_ = ui::NativeTheme::kNormal;
        break;
      case views::Button::ButtonState::STATE_HOVERED:
        state_ = ui::NativeTheme::kHovered;
        break;
      case views::Button::ButtonState::STATE_PRESSED:
        state_ = ui::NativeTheme::kPressed;
        break;
      case views::Button::ButtonState::STATE_DISABLED:
        state_ = ui::NativeTheme::kDisabled;
        break;
      case views::Button::ButtonState::STATE_COUNT:
        NOTREACHED();
        state_ = ui::NativeTheme::kNormal;
        break;
    }
  }

  GtkButtonImageSource(const GtkButtonImageSource&) = delete;
  GtkButtonImageSource& operator=(const GtkButtonImageSource&) = delete;

  ~GtkButtonImageSource() override = default;

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    int width = width_ * scale;
    int height = height_ * scale;

    SkBitmap border;
    border.allocN32Pixels(width, height);
    border.eraseColor(0);

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(border.getAddr(0, 0)), CAIRO_FORMAT_ARGB32,
        width, height, width * 4);
    cairo_t* cr = cairo_create(surface);

    GtkCssContext context = GetStyleContextFromCss("GtkButton#button");
    GtkStateFlags state_flags = StateToStateFlags(state_);
    if (focus_) {
      state_flags =
          static_cast<GtkStateFlags>(state_flags | GTK_STATE_FLAG_FOCUSED);
    }
    gtk_style_context_set_state(context, state_flags);
    gtk_render_background(context, cr, 0, 0, width, height);
    gtk_render_frame(context, cr, 0, 0, width, height);
    if (focus_) {
      gfx::Rect focus_rect(width, height);

      if (!GtkCheckVersion(3, 14)) {
        gint focus_pad;
        GtkStyleContextGetStyle(context, "focus-padding", &focus_pad, nullptr);
        focus_rect.Inset(focus_pad, focus_pad);

        if (state_ == ui::NativeTheme::kPressed) {
          gint child_displacement_x, child_displacement_y;
          gboolean displace_focus;
          GtkStyleContextGetStyle(context, "child-displacement-x",
                                  &child_displacement_x, "child-displacement-y",
                                  &child_displacement_y, "displace-focus",
                                  &displace_focus, nullptr);
          if (displace_focus)
            focus_rect.Offset(child_displacement_x, child_displacement_y);
        }
      }

      if (!GtkCheckVersion(3, 20))
        focus_rect.Inset(GtkStyleContextGetBorder(context));

      gtk_render_focus(context, cr, focus_rect.x(), focus_rect.y(),
                       focus_rect.width(), focus_rect.height());
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return gfx::ImageSkiaRep(border, scale);
  }

 private:
  bool focus_;
  ui::NativeTheme::State state_;
  int width_;
  int height_;
};

class GtkButtonPainter : public views::Painter {
 public:
  GtkButtonPainter(bool focus, views::Button::ButtonState button_state)
      : focus_(focus), button_state_(button_state) {}

  GtkButtonPainter(const GtkButtonPainter&) = delete;
  GtkButtonPainter& operator=(const GtkButtonPainter&) = delete;

  ~GtkButtonPainter() override = default;

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::ImageSkia image(
        std::make_unique<GtkButtonImageSource>(focus_, button_state_, size), 1);
    canvas->DrawImageInt(image, 0, 0);
  }

 private:
  const bool focus_;
  const views::Button::ButtonState button_state_;
};

// Number of app indicators used (used as part of app-indicator id).
int indicators_count;

// The unknown content type.
const char kUnknownContentType[] = "application/octet-stream";

std::unique_ptr<SettingsProvider> CreateSettingsProvider(GtkUi* gtk_ui) {
  if (GtkCheckVersion(3, 14))
    return std::make_unique<SettingsProviderGtk>(gtk_ui);
#if defined(USE_GIO)
  return std::make_unique<SettingsProviderGSettings>(gtk_ui);
#else
  return nullptr;
#endif
}

// Returns a gfx::FontRenderParams corresponding to GTK's configuration.
gfx::FontRenderParams GetGtkFontRenderParams() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = nullptr;
  gchar* rgba = nullptr;
  g_object_get(gtk_settings, "gtk-xft-antialias", &antialias, "gtk-xft-hinting",
               &hinting, "gtk-xft-hintstyle", &hint_style, "gtk-xft-rgba",
               &rgba, nullptr);

  gfx::FontRenderParams params;
  params.antialiasing = antialias != 0;

  if (hinting == 0 || !hint_style || strcmp(hint_style, "hintnone") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  } else if (strcmp(hint_style, "hintslight") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_SLIGHT;
  } else if (strcmp(hint_style, "hintmedium") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_MEDIUM;
  } else if (strcmp(hint_style, "hintfull") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_FULL;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-hintstyle \"" << hint_style << "\"";
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  }

  if (!rgba || strcmp(rgba, "none") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  } else if (strcmp(rgba, "rgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
  } else if (strcmp(rgba, "bgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
  } else if (strcmp(rgba, "vrgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
  } else if (strcmp(rgba, "vbgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-rgba \"" << rgba << "\"";
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }

  g_free(hint_style);
  g_free(rgba);

  return params;
}

views::LinuxUI::WindowFrameAction GetDefaultMiddleClickAction() {
  if (GtkCheckVersion(3, 14))
    return views::LinuxUI::WindowFrameAction::kNone;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      // Starting with KDE 4.4, windows' titlebars can be dragged with the
      // middle mouse button to create tab groups. We don't support that in
      // Chrome, but at least avoid lowering windows in response to middle
      // clicks to avoid surprising users who expect the KDE behavior.
      return views::LinuxUI::WindowFrameAction::kNone;
    default:
      return views::LinuxUI::WindowFrameAction::kLower;
  }
}

std::unique_ptr<GtkUiPlatform> CreateGtkUiPlatform(ui::LinuxUiBackend backend) {
  switch (backend) {
#if defined(USE_X11)
    case ui::LinuxUiBackend::kX11:
      return std::make_unique<GtkUiPlatformX11>();
#endif
#if defined(USE_WAYLAND)
    case ui::LinuxUiBackend::kWayland:
      return std::make_unique<GtkUiPlatformWayland>();
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

GtkUi::GtkUi() {
  using Action = views::LinuxUI::WindowFrameAction;
  using ActionSource = views::LinuxUI::WindowFrameActionSource;

  DCHECK(!g_gtk_ui);
  g_gtk_ui = this;

  CHECK(LoadGtk());

  auto* delegate = ui::LinuxUiDelegate::GetInstance();
  DCHECK(delegate);
  platform_ = CreateGtkUiPlatform(delegate->GetBackend());

  // Avoid GTK initializing atk-bridge, and let AuraLinux implementation
  // do it once it is ready.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("NO_AT_BRIDGE", "1");
  GtkInitFromCommandLine(*base::CommandLine::ForCurrentProcess());
  native_theme_ = NativeThemeGtk::instance();

  // This creates an extra thread that may race against GtkInitFromCommandLine,
  // so this must be done after to avoid the race condition.
  SelectFileDialogImpl::Initialize();

  window_frame_actions_ = {
      {ActionSource::kDoubleClick, Action::kToggleMaximize},
      {ActionSource::kMiddleClick, GetDefaultMiddleClickAction()},
      {ActionSource::kRightClick, Action::kMenu}};
}

GtkUi::~GtkUi() {
  DCHECK_EQ(g_gtk_ui, this);
  g_gtk_ui = nullptr;

  SelectFileDialogImpl::Shutdown();
}

// static
GtkUiPlatform* GtkUi::GetPlatform() {
  DCHECK(g_gtk_ui) << "GtkUi instance is not set.";
  return g_gtk_ui->platform_.get();
}

void GtkUi::Initialize() {
  // Linux ozone platforms may want to set LinuxInputMethodContextFactory
  // instance instead of using GtkUi context factory. This step is made upon
  // CreateInputMethod call. If the factory is not set, use the GtkUi context
  // factory.
  if (GetPlatform()->PreferGtkIme() ||
      !ui::OzonePlatform::GetInstance()->CreateInputMethod(
          nullptr, gfx::kNullAcceleratedWidget)) {
    if (!ui::LinuxInputMethodContextFactory::instance())
      ui::LinuxInputMethodContextFactory::SetInstance(this);
  }

  GtkSettings* settings = gtk_settings_get_default();
  g_signal_connect_after(settings, "notify::gtk-theme-name",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-icon-theme-name",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-application-prefer-dark-theme",
                         G_CALLBACK(OnThemeChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-cursor-theme-name",
                         G_CALLBACK(OnCursorThemeNameChangedThunk), this);
  g_signal_connect_after(settings, "notify::gtk-cursor-theme-size",
                         G_CALLBACK(OnCursorThemeSizeChangedThunk), this);

  // Listen for DPI changes.
  auto* dpi_callback = G_CALLBACK(OnDeviceScaleFactorMaybeChangedThunk);
  if (GtkCheckVersion(4)) {
    g_signal_connect_after(settings, "notify::gtk-xft-dpi", dpi_callback, this);
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    g_signal_connect_after(screen, "notify::resolution", dpi_callback, this);
  }

  // Listen for scale factor changes.  We would prefer to listen on
  // a GdkScreen, but there is no scale-factor property, so use an
  // unmapped window instead.
  g_signal_connect(GetDummyWindow(), "notify::scale-factor",
                   G_CALLBACK(OnDeviceScaleFactorMaybeChangedThunk), this);

  LoadGtkValues();

#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &PrintDialogGtk::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &GetPdfPaperSizeDeviceUnitsGtk);
#endif

  // We must build this after GTK gets initialized.
  settings_provider_ = CreateSettingsProvider(this);

  indicators_count = 0;

  platform_->OnInitialized(GetDummyWindow());
}

bool GtkUi::GetTint(int id, color_utils::HSL* tint) const {
  switch (id) {
    // Tints for which the cross-platform default is fine. Before adding new
    // values here, specifically verify they work well on Linux.
    case ThemeProperties::TINT_BACKGROUND_TAB:
    // TODO(estade): Return something useful for TINT_BUTTONS so that chrome://
    // page icons are colored appropriately.
    case ThemeProperties::TINT_BUTTONS:
      break;
    default:
      // Assume any tints not specifically verified on Linux aren't usable.
      // TODO(pkasting): Try to remove values from |colors_| that could just be
      // added to the group above instead.
      NOTREACHED();
  }
  return false;
}

bool GtkUi::GetColor(int id, SkColor* color, bool use_custom_frame) const {
  for (const ColorMap& color_map :
       {colors_,
        use_custom_frame ? custom_frame_colors_ : native_frame_colors_}) {
    auto it = color_map.find(id);
    if (it != color_map.end()) {
      *color = it->second;
      return true;
    }
  }

  return false;
}

bool GtkUi::GetDisplayProperty(int id, int* result) const {
  if (id == ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR) {
    *result = 0;
    return true;
  }
  return false;
}

SkColor GtkUi::GetFocusRingColor() const {
  return focus_ring_color_;
}

SkColor GtkUi::GetActiveSelectionBgColor() const {
  return active_selection_bg_color_;
}

SkColor GtkUi::GetActiveSelectionFgColor() const {
  return active_selection_fg_color_;
}

SkColor GtkUi::GetInactiveSelectionBgColor() const {
  return inactive_selection_bg_color_;
}

SkColor GtkUi::GetInactiveSelectionFgColor() const {
  return inactive_selection_fg_color_;
}

base::TimeDelta GtkUi::GetCursorBlinkInterval() const {
  // From http://library.gnome.org/devel/gtk/unstable/GtkSettings.html, this is
  // the default value for gtk-cursor-blink-time.
  static const gint kGtkDefaultCursorBlinkTime = 1200;

  // Dividing GTK's cursor blink cycle time (in milliseconds) by this value
  // yields an appropriate value for
  // blink::RendererPreferences::caret_blink_interval.
  static const double kGtkCursorBlinkCycleFactor = 2000.0;

  gint cursor_blink_time = kGtkDefaultCursorBlinkTime;
  gboolean cursor_blink = TRUE;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-blink-time",
               &cursor_blink_time, "gtk-cursor-blink", &cursor_blink, nullptr);
  return cursor_blink
             ? base::Seconds(cursor_blink_time / kGtkCursorBlinkCycleFactor)
             : base::TimeDelta();
}

ui::NativeTheme* GtkUi::GetNativeTheme(aura::Window* window) const {
  return (use_system_theme_callback_.is_null() ||
          use_system_theme_callback_.Run(window))
             ? native_theme_
             : ui::NativeTheme::GetInstanceForNativeUi();
}

void GtkUi::SetUseSystemThemeCallback(UseSystemThemeCallback callback) {
  use_system_theme_callback_ = std::move(callback);
}

bool GtkUi::GetDefaultUsesSystemTheme() const {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return true;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return false;
  }
  // Unless GetDesktopEnvironment() badly misbehaves, this should never happen.
  NOTREACHED();
  return false;
}

gfx::Image GtkUi::GetIconForContentType(const std::string& content_type,
                                        int size) const {
  // This call doesn't take a reference.
  GtkIconTheme* theme = GetDefaultIconTheme();

  std::string content_types[] = {content_type, kUnknownContentType};

  for (size_t i = 0; i < base::size(content_types); ++i) {
    auto icon = TakeGObject(g_content_type_get_icon(content_type.c_str()));
    SkBitmap bitmap;
    if (GtkCheckVersion(4)) {
      auto icon_paintable = Gtk4IconThemeLookupByGicon(
          theme, icon.get(), size, 1, GTK_TEXT_DIR_NONE,
          static_cast<GtkIconLookupFlags>(0));
      if (!icon_paintable)
        continue;

      auto* paintable = GlibCast<GdkPaintable>(icon_paintable.get(),
                                               gdk_paintable_get_type());
      auto* snapshot = gtk_snapshot_new();
      gdk_paintable_snapshot(paintable, snapshot, size, size);
      auto* node = gtk_snapshot_free_to_node(snapshot);
      GdkTexture* texture = GetTextureFromRenderNode(node);

      bitmap.allocN32Pixels(gdk_texture_get_width(texture),
                            gdk_texture_get_height(texture));
      gdk_texture_download(texture, static_cast<guchar*>(bitmap.getAddr(0, 0)),
                           bitmap.rowBytes());

      gsk_render_node_unref(node);
    } else {
      auto icon_info = Gtk3IconThemeLookupByGicon(
          theme, icon.get(), size,
          static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SIZE));
      if (!icon_info)
        continue;
      auto* surface =
          gtk_icon_info_load_surface(icon_info.get(), nullptr, nullptr);
      if (!surface)
        continue;
      DCHECK_EQ(cairo_surface_get_type(surface), CAIRO_SURFACE_TYPE_IMAGE);
      DCHECK_EQ(cairo_image_surface_get_format(surface), CAIRO_FORMAT_ARGB32);

      SkImageInfo image_info =
          SkImageInfo::Make(cairo_image_surface_get_width(surface),
                            cairo_image_surface_get_height(surface),
                            kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
      if (!bitmap.installPixels(
              image_info, cairo_image_surface_get_data(surface),
              image_info.minRowBytes(),
              [](void*, void* surface) {
                cairo_surface_destroy(
                    reinterpret_cast<cairo_surface_t*>(surface));
              },
              surface)) {
        continue;
      }
    }
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    image_skia.MakeThreadSafe();
    return gfx::Image(image_skia);
  }
  return gfx::Image();
}

std::unique_ptr<views::Border> GtkUi::CreateNativeBorder(
    views::LabelButton* owning_button,
    std::unique_ptr<views::LabelButtonBorder> border) {
  if (owning_button->GetNativeTheme() != native_theme_)
    return std::move(border);

  auto gtk_border = std::make_unique<views::LabelButtonAssetBorder>();

  gtk_border->set_insets(border->GetInsets());

  constexpr bool kFocus = true;

  static struct {
    bool focus;
    views::Button::ButtonState state;
  } const paintstates[] = {
      {!kFocus, views::Button::STATE_NORMAL},
      {!kFocus, views::Button::STATE_HOVERED},
      {!kFocus, views::Button::STATE_PRESSED},
      {!kFocus, views::Button::STATE_DISABLED},
      {kFocus, views::Button::STATE_NORMAL},
      {kFocus, views::Button::STATE_HOVERED},
      {kFocus, views::Button::STATE_PRESSED},
      {kFocus, views::Button::STATE_DISABLED},
  };

  for (const auto& paintstate : paintstates) {
    gtk_border->SetPainter(
        paintstate.focus, paintstate.state,
        border->PaintsButtonState(paintstate.focus, paintstate.state)
            ? std::make_unique<GtkButtonPainter>(paintstate.focus,
                                                 paintstate.state)
            : nullptr);
  }

  return std::move(gtk_border);
}

void GtkUi::AddWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  if (nav_buttons_set_)
    observer->OnWindowButtonOrderingChange(leading_buttons_, trailing_buttons_);

  window_button_order_observer_list_.AddObserver(observer);
}

void GtkUi::RemoveWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.RemoveObserver(observer);
}

void GtkUi::SetWindowButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;
  nav_buttons_set_ = true;

  for (views::WindowButtonOrderObserver& observer :
       window_button_order_observer_list_) {
    observer.OnWindowButtonOrderingChange(leading_buttons_, trailing_buttons_);
  }
}

void GtkUi::SetWindowFrameAction(WindowFrameActionSource source,
                                 WindowFrameAction action) {
  window_frame_actions_[source] = action;
}

std::unique_ptr<ui::LinuxInputMethodContext> GtkUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  return std::make_unique<InputMethodContextImplGtk>(delegate, is_simple);
}

gfx::FontRenderParams GtkUi::GetDefaultFontRenderParams() const {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
}

void GtkUi::GetDefaultFontDescription(std::string* family_out,
                                      int* size_pixels_out,
                                      int* style_out,
                                      gfx::Font::Weight* weight_out,
                                      gfx::FontRenderParams* params_out) const {
  *family_out = default_font_family_;
  *size_pixels_out = default_font_size_pixels_;
  *style_out = default_font_style_;
  *weight_out = default_font_weight_;
  *params_out = default_font_render_params_;
}

ui::SelectFileDialog* GtkUi::CreateSelectFileDialog(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  return SelectFileDialogImpl::Create(listener, std::move(policy));
}

views::LinuxUI::WindowFrameAction GtkUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  return window_frame_actions_[source];
}

void GtkUi::NotifyWindowManagerStartupComplete() {
  // TODO(port) Implement this using _NET_STARTUP_INFO_BEGIN/_NET_STARTUP_INFO
  // from http://standards.freedesktop.org/startup-notification-spec/ instead.
  gdk_display_notify_startup_complete(gdk_display_get_default(), nullptr);
}

void GtkUi::AddDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.AddObserver(observer);
}

void GtkUi::RemoveDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.RemoveObserver(observer);
}

bool GtkUi::PreferDarkTheme() const {
  gboolean dark = false;
  g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               &dark, nullptr);
  return dark;
}

bool GtkUi::AnimationsEnabled() const {
  gboolean animations_enabled = false;
  g_object_get(gtk_settings_get_default(), "gtk-enable-animations",
               &animations_enabled, nullptr);
  return animations_enabled;
}

std::unique_ptr<views::NavButtonProvider> GtkUi::CreateNavButtonProvider() {
  if (GtkCheckVersion(3, 14))
    return std::make_unique<gtk::NavButtonProviderGtk>();
  return nullptr;
}

views::WindowFrameProvider* GtkUi::GetWindowFrameProvider(bool solid_frame) {
  if (!GtkCheckVersion(3, 14))
    return nullptr;
  auto& provider =
      solid_frame ? solid_frame_provider_ : transparent_frame_provider_;
  if (!provider)
    provider = std::make_unique<gtk::WindowFrameProviderGtk>(solid_frame);
  return provider.get();
}

// Mapping from GDK dead keys to corresponding printable character.
static struct {
  guint gdk_key;
  guint16 unicode;
} kDeadKeyMapping[] = {
    {GDK_KEY_dead_grave, 0x0060},      {GDK_KEY_dead_acute, 0x0027},
    {GDK_KEY_dead_circumflex, 0x005e}, {GDK_KEY_dead_tilde, 0x007e},
    {GDK_KEY_dead_diaeresis, 0x00a8},
};

base::flat_map<std::string, std::string> GtkUi::GetKeyboardLayoutMap() {
  GdkDisplay* display = gdk_display_get_default();
  GdkKeymap* keymap = nullptr;
  if (!GtkCheckVersion(4)) {
    keymap = gdk_keymap_get_for_display(display);
    if (!keymap)
      return {};
  }

  auto layouts = std::make_unique<ui::DomKeyboardLayoutManager>();
  auto map = base::flat_map<std::string, std::string>();

  for (unsigned int i_domcode = 0;
       i_domcode < ui::kWritingSystemKeyDomCodeEntries; ++i_domcode) {
    ui::DomCode domcode = ui::writing_system_key_domcodes[i_domcode];
    guint16 keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(domcode);
    GdkKeymapKey* keys = nullptr;
    guint* keyvals = nullptr;
    gint n_entries = 0;

    // The order of the layouts is based on the system default ordering in
    // Keyboard Settings. The currently active layout does not affect this
    // order.
    const bool success =
        GtkCheckVersion(4) ? gdk_display_map_keycode(display, keycode, &keys,
                                                     &keyvals, &n_entries)
                           : gdk_keymap_get_entries_for_keycode(
                                 keymap, keycode, &keys, &keyvals, &n_entries);
    if (success) {
      for (gint i = 0; i < n_entries; ++i) {
        // There are 4 entries per layout group, one each for shift level 0..3.
        // We only care about the unshifted values (level = 0).
        if (keys[i].level == 0) {
          uint16_t unicode = gdk_keyval_to_unicode(keyvals[i]);
          if (unicode == 0) {
            for (const auto& i_dead : kDeadKeyMapping) {
              if (keyvals[i] == i_dead.gdk_key)
                unicode = i_dead.unicode;
            }
          }
          if (unicode != 0)
            layouts->GetLayout(keys[i].group)->AddKeyMapping(domcode, unicode);
        }
      }
    }
    g_free(keys);
    g_free(keyvals);
  }
  return layouts->GetFirstAsciiCapableLayout()->GetMap();
}

std::string GtkUi::GetCursorThemeName() {
  gchar* theme = nullptr;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-name", &theme,
               nullptr);
  std::string theme_string;
  if (theme) {
    theme_string = theme;
    g_free(theme);
  }
  return theme_string;
}

int GtkUi::GetCursorThemeSize() {
  gint size = 0;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-theme-size", &size,
               nullptr);
  return size;
}

bool GtkUi::MatchEvent(const ui::Event& event,
                       std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // GTK4 dropped custom key bindings.
  if (GtkCheckVersion(4))
    return false;

  // TODO(crbug.com/963419): Use delegate's |GetGdkKeymap| here to
  // determine if GtkUi's key binding handling implementation is used or not.
  // Ozone/Wayland was unintentionally using GtkUi for keybinding handling, so
  // early out here, for now, until a proper solution for ozone is implemented.
  if (!platform_->GetGdkKeymap())
    return false;

  // Ensure that we have a keyboard handler.
  if (!key_bindings_handler_)
    key_bindings_handler_ = std::make_unique<GtkKeyBindingsHandler>();

  return key_bindings_handler_->MatchEvent(event, commands);
}

void GtkUi::OnThemeChanged(GtkSettings* settings, GtkParamSpec* param) {
  colors_.clear();
  custom_frame_colors_.clear();
  native_frame_colors_.clear();
  native_theme_->OnThemeChanged(settings, param);
  LoadGtkValues();
  native_theme_->NotifyOnNativeThemeUpdated();
}

void GtkUi::OnCursorThemeNameChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  std::string cursor_theme_name = GetCursorThemeName();
  if (cursor_theme_name.empty())
    return;
  for (auto& observer : cursor_theme_observers())
    observer.OnCursorThemeNameChanged(cursor_theme_name);
}

void GtkUi::OnCursorThemeSizeChanged(GtkSettings* settings,
                                     GtkParamSpec* param) {
  int cursor_theme_size = GetCursorThemeSize();
  if (!cursor_theme_size)
    return;
  for (auto& observer : cursor_theme_observers())
    observer.OnCursorThemeSizeChanged(cursor_theme_size);
}

void GtkUi::OnDeviceScaleFactorMaybeChanged(void*, GParamSpec*) {
  UpdateDeviceScaleFactor();
}

void GtkUi::LoadGtkValues() {
  // TODO(thomasanderson): GtkThemeService had a comment here about having to
  // muck with the raw Prefs object to remove prefs::kCurrentThemeImages or else
  // we'd regress startup time. Figure out how to do that when we can't access
  // the prefs system from here.
  UpdateDeviceScaleFactor();
  UpdateColors();
}

void GtkUi::UpdateColors() {
  // TODO(tluk): The below code sets various ThemeProvider colors for GTK. Some
  // of these definitions leverage colors that were previously defined by
  // NativeThemeGtk and are now defined as GTK ColorMixers. These ThemeProvider
  // color definitions should be added as recipes to a browser ColorMixer once
  // the Color Pipeline project begins rollout into c/b/ui. In the meantime
  // use the ColorProvider instance from the ColorProviderManager corresponding
  // to the theme bits associated with the NativeThemeGtk instance to ensure
  // we do not regress existing behavior during the transition.
  const auto color_scheme = native_theme_->GetDefaultSystemColorScheme();
  const auto* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(
          {(color_scheme == ui::NativeTheme::ColorScheme::kDark)
               ? ui::ColorProviderManager::ColorMode::kDark
               : ui::ColorProviderManager::ColorMode::kLight,
           (color_scheme == ui::NativeTheme::ColorScheme::kPlatformHighContrast)
               ? ui::ColorProviderManager::ContrastMode::kHigh
               : ui::ColorProviderManager::ContrastMode::kNormal,
           ui::ColorProviderManager::SystemTheme::kCustom, nullptr});

  SkColor location_bar_border = GetBorderColor("GtkEntry#entry");
  if (SkColorGetA(location_bar_border))
    colors_[ThemeProperties::COLOR_LOCATION_BAR_BORDER] = location_bar_border;

  inactive_selection_bg_color_ = GetSelectionBgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:backdrop "
                               "#text:backdrop #selection:backdrop"
                             : "GtkTextView.view:selected:backdrop");
  inactive_selection_fg_color_ =
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:backdrop "
                                          "#text:backdrop #selection:backdrop"
                                        : "GtkTextView.view:selected:backdrop");

  SkColor tab_border = GetBorderColor("GtkButton#button");
  // Separates the toolbar from the bookmark bar or butter bars.
  colors_[ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR] = tab_border;
  // Separates entries in the downloads bar.
  colors_[ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR] = tab_border;

  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] =
      color_provider->GetColor(ui::kColorTextfieldBackground);
  colors_[ThemeProperties::COLOR_NTP_TEXT] =
      color_provider->GetColor(ui::kColorTextfieldForeground);
  colors_[ThemeProperties::COLOR_NTP_HEADER] =
      GetBorderColor("GtkButton#button");

  SkColor tab_text_color = GetFgColor("GtkLabel#label");
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED] = tab_text_color;
  colors_[ThemeProperties::COLOR_TOOLBAR_TEXT] = tab_text_color;

  colors_[ThemeProperties::COLOR_NTP_LINK] =
      color_provider->GetColor(ui::kColorTextfieldSelectionBackground);

  // Generate the colors that we pass to Blink.
  focus_ring_color_ =
      color_provider->GetColor(ui::kColorFocusableBorderFocused);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      color_provider->GetColor(ui::kColorTextfieldSelectionBackground);
  active_selection_fg_color_ =
      color_provider->GetColor(ui::kColorTextfieldSelectionForeground);

  colors_[ThemeProperties::COLOR_TAB_THROBBER_SPINNING] =
      color_provider->GetColor(ui::kColorThrobber);
  colors_[ThemeProperties::COLOR_TAB_THROBBER_WAITING] =
      color_provider->GetColor(ui::kColorThrobberPreconnect);

  // Generate colors that depend on whether or not a custom window frame is
  // used.  These colors belong in |color_map| below, not |colors_|.
  for (bool custom_frame : {false, true}) {
    ColorMap& color_map =
        custom_frame ? custom_frame_colors_ : native_frame_colors_;
    const std::string header_selector =
        custom_frame ? "#headerbar.header-bar.titlebar" : "GtkMenuBar#menubar";
    const std::string header_selector_inactive = header_selector + ":backdrop";
    const SkColor frame_color =
        SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
    const SkColor frame_color_inactive =
        SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);

    color_map[ThemeProperties::COLOR_FRAME_ACTIVE] = frame_color;
    color_map[ThemeProperties::COLOR_FRAME_INACTIVE] = frame_color_inactive;
    color_map[ThemeProperties::COLOR_FRAME_ACTIVE_INCOGNITO] = frame_color;
    color_map[ThemeProperties::COLOR_FRAME_INACTIVE_INCOGNITO] =
        frame_color_inactive;

    // Compose the window color on the frame color to ensure the resulting tab
    // color is opaque.
    SkColor tab_color =
        color_utils::GetResultingPaintColor(GetBgColor(""), frame_color);

    color_map[ThemeProperties::COLOR_TOOLBAR] = tab_color;
    color_map[ThemeProperties::COLOR_DOWNLOAD_SHELF] = tab_color;
    color_map[ThemeProperties::COLOR_INFOBAR] = tab_color;
    color_map[ThemeProperties::COLOR_STATUS_BUBBLE] = tab_color;
    color_map[ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE] =
        tab_color;
    color_map[ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE] =
        tab_color;

    const SkColor background_tab_text_color =
        GetFgColor(header_selector + " GtkLabel#label.title");
    const SkColor background_tab_text_color_inactive =
        GetFgColor(header_selector_inactive + " GtkLabel#label.title");

    color_map[ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE] =
        background_tab_text_color;
    color_map[ThemeProperties::
                  COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE_INCOGNITO] =
        background_tab_text_color;
    color_map[ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE] =
        background_tab_text_color_inactive;
    color_map[ThemeProperties::
                  COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE_INCOGNITO] =
        background_tab_text_color_inactive;

    color_map[ThemeProperties::COLOR_OMNIBOX_TEXT] =
        color_provider->GetColor(ui::kColorTextfieldForeground);
    color_map[ThemeProperties::COLOR_OMNIBOX_BACKGROUND] =
        color_provider->GetColor(ui::kColorTextfieldBackground);

    // These colors represent the border drawn around tabs and between
    // the tabstrip and toolbar.
    SkColor toolbar_top_separator = GetBorderColor(
        header_selector + " GtkSeparator#separator.vertical.titlebutton");
    SkColor toolbar_top_separator_inactive =
        GetBorderColor(header_selector +
                       ":backdrop GtkSeparator#separator.vertical.titlebutton");

    auto toolbar_top_separator_has_good_contrast = [&]() {
      // This constant is copied from chrome/browser/themes/theme_service.cc.
      const float kMinContrastRatio = 2.f;

      SkColor active = color_utils::GetResultingPaintColor(
          toolbar_top_separator, frame_color);
      SkColor inactive = color_utils::GetResultingPaintColor(
          toolbar_top_separator_inactive, frame_color_inactive);
      return color_utils::GetContrastRatio(frame_color, active) >=
                 kMinContrastRatio &&
             color_utils::GetContrastRatio(frame_color_inactive, inactive) >=
                 kMinContrastRatio;
    };

    if (!toolbar_top_separator_has_good_contrast()) {
      toolbar_top_separator =
          GetBorderColor(header_selector + " GtkButton#button");
      toolbar_top_separator_inactive =
          GetBorderColor(header_selector + ":backdrop GtkButton#button");
    }

    // If we can't get a contrasting stroke from the theme, have ThemeService
    // provide a stroke color for us.
    if (toolbar_top_separator_has_good_contrast()) {
      color_map[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR] =
          toolbar_top_separator;
      color_map[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE] =
          toolbar_top_separator_inactive;
    }
  }
}

void GtkUi::UpdateDefaultFont() {
  gfx::SetFontRenderParamsDeviceScaleFactor(device_scale_factor_);

  auto fake_label = TakeGObject(gtk_label_new(nullptr));
  PangoContext* pc = gtk_widget_get_pango_context(fake_label);
  const PangoFontDescription* desc = pango_context_get_font_description(pc);

  // Use gfx::FontRenderParams to select a family and determine the rendering
  // settings.
  gfx::FontRenderParamsQuery query;
  query.families =
      base::SplitString(pango_font_description_get_family(desc), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (pango_font_description_get_size_is_absolute(desc)) {
    // If the size is absolute, it's specified in Pango units. There are
    // PANGO_SCALE Pango units in a device unit (pixel).
    const int size_pixels = pango_font_description_get_size(desc) / PANGO_SCALE;
    default_font_size_pixels_ = size_pixels;
    query.pixel_size = size_pixels;
  } else {
    // Non-absolute sizes are in points (again scaled by PANGO_SIZE).
    // Round the value when converting to pixels to match GTK's logic.
    const double size_points = pango_font_description_get_size(desc) /
                               static_cast<double>(PANGO_SCALE);
    default_font_size_pixels_ =
        static_cast<int>(kDefaultDPI / 72.0 * size_points + 0.5);
    query.point_size = static_cast<int>(size_points);
  }

  query.style = gfx::Font::NORMAL;
  query.weight =
      static_cast<gfx::Font::Weight>(pango_font_description_get_weight(desc));
  // TODO(davemoore): What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC)
    query.style |= gfx::Font::ITALIC;

  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family_);
  default_font_style_ = query.style;
}

float GtkUi::GetRawDeviceScaleFactor() {
  if (display::Display::HasForceDeviceScaleFactor())
    return display::Display::GetForcedDeviceScaleFactor();

  float scale = gtk_widget_get_scale_factor(GetDummyWindow());
  DCHECK_GT(scale, 0.0);

  double resolution = 0;
  if (GtkCheckVersion(4)) {
    auto* settings = gtk_settings_get_default();
    int dpi = 0;
    g_object_get(settings, "gtk-xft-dpi", &dpi, nullptr);
    resolution = dpi / 1024.0;
  } else {
    GdkScreen* screen = gdk_screen_get_default();
    resolution = gdk_screen_get_resolution(screen);
  }
  if (resolution > 0)
    scale *= resolution / kDefaultDPI;

  // Round to the nearest 64th so that UI can losslessly multiply and divide
  // the scale factor.
  scale = roundf(scale * 64) / 64;

  return scale;
}

void GtkUi::UpdateDeviceScaleFactor() {
  float old_device_scale_factor = device_scale_factor_;
  device_scale_factor_ = GetRawDeviceScaleFactor();
  if (device_scale_factor_ != old_device_scale_factor) {
    for (views::DeviceScaleFactorObserver& observer :
         device_scale_factor_observer_list_) {
      observer.OnDeviceScaleFactorChanged();
    }
  }
  UpdateDefaultFont();
}

float GtkUi::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

}  // namespace gtk
