// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webpreferences.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNetworkStateNotifier.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/icu/public/common/unicode/uchar.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebNetworkStateNotifier;
using WebKit::WebRuntimeFeatures;
using WebKit::WebSettings;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

namespace webkit_glue {

// "Zyyy" is the ISO 15924 script code for undetermined script aka Common.
const char WebPreferences::kCommonScript[] = "Zyyy";

WebPreferences::WebPreferences()
    : default_font_size(16),
      default_fixed_font_size(13),
      minimum_font_size(0),
      minimum_logical_font_size(6),
      default_encoding("ISO-8859-1"),
      apply_default_device_scale_factor_in_compositor(false),
      apply_page_scale_factor_in_compositor(false),
      per_tile_painting_enabled(false),
      accelerated_animation_enabled(false),
      javascript_enabled(true),
      web_security_enabled(true),
      javascript_can_open_windows_automatically(true),
      loads_images_automatically(true),
      images_enabled(true),
      plugins_enabled(true),
      dom_paste_enabled(false),  // enables execCommand("paste")
      developer_extras_enabled(false),  // Requires extra work by embedder
      site_specific_quirks_enabled(false),
      shrinks_standalone_images_to_fit(true),
      uses_universal_detector(false),  // Disabled: page cycler regression
      text_areas_are_resizable(true),
      java_enabled(true),
      allow_scripts_to_close_windows(false),
      uses_page_cache(false),
      page_cache_supports_plugins(false),
      remote_fonts_enabled(true),
      javascript_can_access_clipboard(false),
      xss_auditor_enabled(true),
      dns_prefetching_enabled(true),
      local_storage_enabled(false),
      databases_enabled(false),
      application_cache_enabled(false),
      tabs_to_links(true),
      caret_browsing_enabled(false),
      hyperlink_auditing_enabled(true),
      is_online(true),
      user_style_sheet_enabled(false),
      author_and_user_styles_enabled(true),
      frame_flattening_enabled(false),
      allow_universal_access_from_file_urls(false),
      allow_file_access_from_file_urls(false),
      webaudio_enabled(false),
      experimental_webgl_enabled(false),
      flash_3d_enabled(true),
      flash_stage3d_enabled(false),
      gl_multisampling_enabled(true),
      privileged_webgl_extensions_enabled(false),
      webgl_errors_to_console_enabled(true),
      show_composited_layer_borders(false),
      show_composited_layer_tree(false),
      show_fps_counter(false),
      accelerated_compositing_for_overflow_scroll_enabled(false),
      accelerated_compositing_for_scrollable_frames_enabled(false),
      composited_scrolling_for_frames_enabled(false),
      show_paint_rects(false),
      render_vsync_enabled(true),
      asynchronous_spell_checking_enabled(true),
      unified_textchecker_enabled(false),
      accelerated_compositing_enabled(false),
      force_compositing_mode(false),
      fixed_position_compositing_enabled(false),
      accelerated_compositing_for_3d_transforms_enabled(false),
      accelerated_compositing_for_animation_enabled(false),
      accelerated_compositing_for_video_enabled(false),
      accelerated_2d_canvas_enabled(false),
      deferred_2d_canvas_enabled(false),
      antialiased_2d_canvas_disabled(false),
      accelerated_painting_enabled(false),
      accelerated_filters_enabled(false),
      gesture_tap_highlight_enabled(false),
      accelerated_compositing_for_plugins_enabled(false),
      memory_info_enabled(false),
      fullscreen_enabled(false),
      allow_displaying_insecure_content(true),
      allow_running_insecure_content(false),
      password_echo_enabled(false),
      should_print_backgrounds(false),
      enable_scroll_animator(false),
      visual_word_movement_enabled(false),
      css_sticky_position_enabled(false),
      css_shaders_enabled(false),
      css_variables_enabled(false),
      css_grid_layout_enabled(false),
      touch_enabled(false),
      device_supports_touch(false),
      device_supports_mouse(true),
      touch_adjustment_enabled(true),
      default_tile_width(256),
      default_tile_height(256),
      max_untiled_layer_width(512),
      max_untiled_layer_height(512),
      fixed_position_creates_stacking_context(false),
      sync_xhr_in_documents_enabled(true),
      deferred_image_decoding_enabled(false),
      should_respect_image_orientation(false),
      number_of_cpu_cores(1),
#if defined(OS_MACOSX)
      editing_behavior(EDITING_BEHAVIOR_MAC),
#elif defined(OS_WIN)
      editing_behavior(EDITING_BEHAVIOR_WIN),
#elif defined(OS_POSIX)
      editing_behavior(EDITING_BEHAVIOR_UNIX),
#else
      editing_behavior(EDITING_BEHAVIOR_MAC),
#endif
      supports_multiple_windows(true),
      cookie_enabled(true)
#if defined(OS_ANDROID)
      ,
      text_autosizing_enabled(true),
      font_scale_factor(1.0f),
      force_enable_zoom(false),
      user_gesture_required_for_media_playback(true)
#endif
{
  standard_font_family_map[kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  fixed_font_family_map[kCommonScript] =
      ASCIIToUTF16("Courier New");
  serif_font_family_map[kCommonScript] =
      ASCIIToUTF16("Times New Roman");
  sans_serif_font_family_map[kCommonScript] =
      ASCIIToUTF16("Arial");
  cursive_font_family_map[kCommonScript] =
      ASCIIToUTF16("Script");
  fantasy_font_family_map[kCommonScript] =
      ASCIIToUTF16("Impact");
  pictograph_font_family_map[kCommonScript] =
      ASCIIToUTF16("Times New Roman");
}

WebPreferences::~WebPreferences() {
}

namespace {

void setStandardFontFamilyWrapper(WebSettings* settings,
                                  const string16& font,
                                  UScriptCode script) {
  settings->setStandardFontFamily(font, script);
}

void setFixedFontFamilyWrapper(WebSettings* settings,
                               const string16& font,
                               UScriptCode script) {
  settings->setFixedFontFamily(font, script);
}

void setSerifFontFamilyWrapper(WebSettings* settings,
                               const string16& font,
                               UScriptCode script) {
  settings->setSerifFontFamily(font, script);
}

void setSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const string16& font,
                                   UScriptCode script) {
  settings->setSansSerifFontFamily(font, script);
}

void setCursiveFontFamilyWrapper(WebSettings* settings,
                                 const string16& font,
                                 UScriptCode script) {
  settings->setCursiveFontFamily(font, script);
}

void setFantasyFontFamilyWrapper(WebSettings* settings,
                                 const string16& font,
                                 UScriptCode script) {
  settings->setFantasyFontFamily(font, script);
}

void setPictographFontFamilyWrapper(WebSettings* settings,
                               const string16& font,
                               UScriptCode script) {
  settings->setPictographFontFamily(font, script);
}

typedef void (*SetFontFamilyWrapper)(
    WebKit::WebSettings*, const string16&, UScriptCode);

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
  case USCRIPT_HIRAGANA:
  case USCRIPT_KATAKANA:
  case USCRIPT_JAPANESE:
    return USCRIPT_KATAKANA_OR_HIRAGANA;
  case USCRIPT_KOREAN:
    return USCRIPT_HANGUL;
  default:
    return scriptCode;
  }
}

void ApplyFontsFromMap(const WebPreferences::ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (WebPreferences::ScriptFontFamilyMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    int32 script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

}  // namespace

void WebPreferences::Apply(WebView* web_view) const {
  WebSettings* settings = web_view->settings();
  ApplyFontsFromMap(standard_font_family_map, setStandardFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(fixed_font_family_map, setFixedFontFamilyWrapper, settings);
  ApplyFontsFromMap(serif_font_family_map, setSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(sans_serif_font_family_map, setSansSerifFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(cursive_font_family_map, setCursiveFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(fantasy_font_family_map, setFantasyFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(pictograph_font_family_map, setPictographFontFamilyWrapper,
                    settings);
  settings->setDefaultFontSize(default_font_size);
  settings->setDefaultFixedFontSize(default_fixed_font_size);
  settings->setMinimumFontSize(minimum_font_size);
  settings->setMinimumLogicalFontSize(minimum_logical_font_size);
  settings->setDefaultTextEncodingName(ASCIIToUTF16(default_encoding));
  settings->setApplyDefaultDeviceScaleFactorInCompositor(
      apply_default_device_scale_factor_in_compositor);
  settings->setApplyPageScaleFactorInCompositor(
      apply_page_scale_factor_in_compositor);
  settings->setPerTilePaintingEnabled(per_tile_painting_enabled);
  settings->setAcceleratedAnimationEnabled(accelerated_animation_enabled);
  settings->setJavaScriptEnabled(javascript_enabled);
  settings->setWebSecurityEnabled(web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
      javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(loads_images_automatically);
  settings->setImagesEnabled(images_enabled);
  settings->setPluginsEnabled(plugins_enabled);
  settings->setDOMPasteAllowed(dom_paste_enabled);
  settings->setDeveloperExtrasEnabled(developer_extras_enabled);
  settings->setNeedsSiteSpecificQuirks(site_specific_quirks_enabled);
  settings->setShrinksStandaloneImagesToFit(shrinks_standalone_images_to_fit);
  settings->setUsesEncodingDetector(uses_universal_detector);
  settings->setTextAreasAreResizable(text_areas_are_resizable);
  settings->setAllowScriptsToCloseWindows(allow_scripts_to_close_windows);
  if (user_style_sheet_enabled)
    settings->setUserStyleSheetLocation(user_style_sheet_location);
  else
    settings->setUserStyleSheetLocation(WebURL());
  settings->setAuthorAndUserStylesEnabled(author_and_user_styles_enabled);
  settings->setUsesPageCache(uses_page_cache);
  settings->setPageCacheSupportsPlugins(page_cache_supports_plugins);
  settings->setDownloadableBinaryFontsEnabled(remote_fonts_enabled);
  settings->setJavaScriptCanAccessClipboard(javascript_can_access_clipboard);
  settings->setXSSAuditorEnabled(xss_auditor_enabled);
  settings->setDNSPrefetchingEnabled(dns_prefetching_enabled);
  settings->setLocalStorageEnabled(local_storage_enabled);
  settings->setSyncXHRInDocumentsEnabled(sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::enableDatabase(databases_enabled);
  settings->setOfflineWebApplicationCacheEnabled(application_cache_enabled);
  settings->setCaretBrowsingEnabled(caret_browsing_enabled);
  settings->setHyperlinkAuditingEnabled(hyperlink_auditing_enabled);
  settings->setCookieEnabled(cookie_enabled);

  // This setting affects the behavior of links in an editable region:
  // clicking the link should select it rather than navigate to it.
  // Safari uses the same default. It is unlikley an embedder would want to
  // change this, since it would break existing rich text editors.
  settings->setEditableLinkBehaviorNeverLive();

  settings->setFrameFlatteningEnabled(frame_flattening_enabled);

  settings->setFontRenderingModeNormal();
  settings->setJavaEnabled(java_enabled);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->setAllowUniversalAccessFromFileURLs(
      allow_universal_access_from_file_urls);
  settings->setAllowFileAccessFromFileURLs(allow_file_access_from_file_urls);

  // We prevent WebKit from checking if it needs to add a "text direction"
  // submenu to a context menu. it is not only because we don't need the result
  // but also because it cause a possible crash in Editor::hasBidiSelection().
  settings->setTextDirectionSubmenuInclusionBehaviorNeverIncluded();

  // Enable the web audio API if requested on the command line.
  settings->setWebAudioEnabled(webaudio_enabled);

  // Enable experimental WebGL support if requested on command line
  // and support is compiled in.
  settings->setExperimentalWebGLEnabled(experimental_webgl_enabled);

  // Disable GL multisampling if requested on command line.
  settings->setOpenGLMultisamplingEnabled(gl_multisampling_enabled);

  // Enable privileged WebGL extensions for Chrome extensions or if requested
  // on command line.
  settings->setPrivilegedWebGLExtensionsEnabled(
      privileged_webgl_extensions_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->setWebGLErrorsToConsoleEnabled(webgl_errors_to_console_enabled);

  // Display colored borders around composited render layers if requested
  // on command line.
  settings->setShowDebugBorders(show_composited_layer_borders);

  // Display an FPS indicator if requested on the command line.
  settings->setShowFPSCounter(show_fps_counter);

  // Enables accelerated compositing for overflow scroll.
  settings->setAcceleratedCompositingForOverflowScrollEnabled(
      accelerated_compositing_for_overflow_scroll_enabled);

  // Enables accelerated compositing for scrollable frames if requested on
  // command line.
  settings->setAcceleratedCompositingForScrollableFramesEnabled(
      accelerated_compositing_for_scrollable_frames_enabled);

  // Enables composited scrolling for frames if requested on command line.
  settings->setCompositedScrollingForFramesEnabled(
      composited_scrolling_for_frames_enabled);

  // Display the current compositor tree as overlay if requested on
  // the command line
  settings->setShowPlatformLayerTree(show_composited_layer_tree);

  // Display visualization of what has changed on the screen using an
  // overlay of rects, if requested on the command line.
  settings->setShowPaintRects(show_paint_rects);

  // Set whether to throttle framerate to Vsync.
  settings->setRenderVSyncEnabled(render_vsync_enabled);

  // Enable gpu-accelerated compositing if requested on the command line.
  settings->setAcceleratedCompositingEnabled(accelerated_compositing_enabled);

  // Enable compositing for fixed position elements if requested
  // on the command line.
  settings->setAcceleratedCompositingForFixedPositionEnabled(
      fixed_position_compositing_enabled);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  settings->setAccelerated2dCanvasEnabled(accelerated_2d_canvas_enabled);

  // Enable deferred 2d canvas if requested on the command line.
  settings->setDeferred2dCanvasEnabled(deferred_2d_canvas_enabled);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->setAntialiased2dCanvasEnabled(!antialiased_2d_canvas_disabled);

  // Enable gpu-accelerated painting if requested on the command line.
  settings->setAcceleratedPaintingEnabled(accelerated_painting_enabled);

  // Enable gpu-accelerated filters if requested on the command line.
  settings->setAcceleratedFiltersEnabled(accelerated_filters_enabled);

  // Enable gesture tap highlight if requested on the command line.
  settings->setGestureTapHighlightEnabled(gesture_tap_highlight_enabled);

  // Enabling accelerated layers from the command line enabled accelerated
  // 3D CSS, Video, and Animations.
  settings->setAcceleratedCompositingFor3DTransformsEnabled(
      accelerated_compositing_for_3d_transforms_enabled);
  settings->setAcceleratedCompositingForVideoEnabled(
      accelerated_compositing_for_video_enabled);
  settings->setAcceleratedCompositingForAnimationEnabled(
      accelerated_compositing_for_animation_enabled);

  // Enabling accelerated plugins if specified from the command line.
  settings->setAcceleratedCompositingForPluginsEnabled(
      accelerated_compositing_for_plugins_enabled);

  // WebGL and accelerated 2D canvas are always gpu composited.
  settings->setAcceleratedCompositingForCanvasEnabled(
      experimental_webgl_enabled || accelerated_2d_canvas_enabled);

  // Enable memory info reporting to page if requested on the command line.
  settings->setMemoryInfoEnabled(memory_info_enabled);

  settings->setAsynchronousSpellCheckingEnabled(
      asynchronous_spell_checking_enabled);
  settings->setUnifiedTextCheckerEnabled(unified_textchecker_enabled);

  for (WebInspectorPreferences::const_iterator it = inspector_settings.begin();
       it != inspector_settings.end(); ++it)
    web_view->setInspectorSetting(WebString::fromUTF8(it->first),
                                  WebString::fromUTF8(it->second));

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->setTabsToLinks(tabs_to_links);

  settings->setInteractiveFormValidationEnabled(true);

  settings->setFullScreenEnabled(fullscreen_enabled);
  settings->setAllowDisplayOfInsecureContent(allow_displaying_insecure_content);
  settings->setAllowRunningOfInsecureContent(allow_running_insecure_content);
  settings->setPasswordEchoEnabled(password_echo_enabled);
  settings->setShouldPrintBackgrounds(should_print_backgrounds);
  settings->setEnableScrollAnimator(enable_scroll_animator);
  settings->setVisualWordMovementEnabled(visual_word_movement_enabled);

  settings->setCSSStickyPositionEnabled(css_sticky_position_enabled);
  settings->setExperimentalCSSCustomFilterEnabled(css_shaders_enabled);
  settings->setExperimentalCSSVariablesEnabled(css_variables_enabled);
  settings->setExperimentalCSSGridLayoutEnabled(css_grid_layout_enabled);

  WebRuntimeFeatures::enableTouch(touch_enabled);
  settings->setDeviceSupportsTouch(device_supports_touch);
  settings->setDeviceSupportsMouse(device_supports_mouse);
  settings->setEnableTouchAdjustment(touch_adjustment_enabled);

  settings->setDefaultTileSize(
      WebSize(default_tile_width, default_tile_height));
  settings->setMaxUntiledLayerSize(
      WebSize(max_untiled_layer_width, max_untiled_layer_height));

  settings->setFixedPositionCreatesStackingContext(
      fixed_position_creates_stacking_context);

  settings->setDeferredImageDecodingEnabled(deferred_image_decoding_enabled);
  settings->setShouldRespectImageOrientation(should_respect_image_orientation);

  settings->setEditingBehavior(
      static_cast<WebSettings::EditingBehavior>(editing_behavior));

  settings->setSupportsMultipleWindows(supports_multiple_windows);

#if defined(OS_ANDROID)
  settings->setAllowCustomScrollbarInMainFrame(false);
  settings->setTextAutosizingEnabled(text_autosizing_enabled);
  settings->setTextAutosizingFontScaleFactor(font_scale_factor);
  web_view->setIgnoreViewportTagMaximumScale(force_enable_zoom);
  settings->setAutoZoomFocusedNodeToLegibleScale(true);
  settings->setDoubleTapToZoomEnabled(true);
  settings->setMediaPlaybackRequiresUserGesture(
      user_gesture_required_for_media_playback);
#endif

  WebNetworkStateNotifier::setOnLine(is_online);
}

#define COMPILE_ASSERT_MATCHING_ENUMS(webkit_glue_name, webkit_name)         \
    COMPILE_ASSERT(                                                          \
        static_cast<int>(webkit_glue_name) == static_cast<int>(webkit_name), \
        mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    WebPreferences::EDITING_BEHAVIOR_MAC, WebSettings::EditingBehaviorMac);
COMPILE_ASSERT_MATCHING_ENUMS(
    WebPreferences::EDITING_BEHAVIOR_WIN, WebSettings::EditingBehaviorWin);
COMPILE_ASSERT_MATCHING_ENUMS(
    WebPreferences::EDITING_BEHAVIOR_UNIX, WebSettings::EditingBehaviorUnix);

}  // namespace webkit_glue
