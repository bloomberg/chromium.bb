// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webpreferences.h"

#include <unicode/uchar.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNetworkStateNotifier.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebNetworkStateNotifier;
using WebKit::WebRuntimeFeatures;
using WebKit::WebSettings;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

WebPreferences::WebPreferences()
    : standard_font_family(ASCIIToUTF16("Times New Roman")),
      fixed_font_family(ASCIIToUTF16("Courier New")),
      serif_font_family(ASCIIToUTF16("Times New Roman")),
      sans_serif_font_family(ASCIIToUTF16("Arial")),
      cursive_font_family(ASCIIToUTF16("Script")),
      fantasy_font_family(),  // Not sure what to use on Windows.
      default_font_size(16),
      default_fixed_font_size(13),
      minimum_font_size(0),
      minimum_logical_font_size(6),
      default_encoding("ISO-8859-1"),
      javascript_enabled(true),
      web_security_enabled(true),
      javascript_can_open_windows_automatically(true),
      loads_images_automatically(true),
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
      remote_fonts_enabled(true),
      javascript_can_access_clipboard(false),
      xss_auditor_enabled(false),
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
      gl_multisampling_enabled(true),
      privileged_webgl_extensions_enabled(false),
      show_composited_layer_borders(false),
      show_composited_layer_tree(false),
      show_fps_counter(false),
      asynchronous_spell_checking_enabled(true),
      accelerated_compositing_enabled(false),
      threaded_compositing_enabled(false),
      force_compositing_mode(false),
      allow_webui_compositing(false),
      composite_to_texture_enabled(false),
      fixed_position_compositing_enabled(false),
      accelerated_layers_enabled(false),
      accelerated_video_enabled(false),
      accelerated_2d_canvas_enabled(false),
      accelerated_drawing_enabled(false),
      accelerated_plugins_enabled(false),
      memory_info_enabled(false),
      interactive_form_validation_enabled(true),
      fullscreen_enabled(false),
      allow_displaying_insecure_content(true),
      allow_running_insecure_content(false),
      should_print_backgrounds(false),
      enable_scroll_animator(false),
      hixie76_websocket_protocol_enabled(false),
      visual_word_movement_enabled(false) {
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

typedef void (*SetFontFamilyWrapper)(
    WebKit::WebSettings*, const string16&, UScriptCode);

void ApplyFontsFromMap(const WebPreferences::ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (WebPreferences::ScriptFontFamilyMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    int32 script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT)
      (*setter)(settings, it->second, (UScriptCode) script);
  }
}

}  // namespace

void WebPreferences::Apply(WebView* web_view) const {
  WebSettings* settings = web_view->settings();
  settings->setStandardFontFamily(standard_font_family);
  settings->setFixedFontFamily(fixed_font_family);
  settings->setSerifFontFamily(serif_font_family);
  settings->setSansSerifFontFamily(sans_serif_font_family);
  settings->setCursiveFontFamily(cursive_font_family);
  settings->setFantasyFontFamily(fantasy_font_family);
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
  settings->setDefaultFontSize(default_font_size);
  settings->setDefaultFixedFontSize(default_fixed_font_size);
  settings->setMinimumFontSize(minimum_font_size);
  settings->setMinimumLogicalFontSize(minimum_logical_font_size);
  settings->setDefaultTextEncodingName(ASCIIToUTF16(default_encoding));
  settings->setJavaScriptEnabled(javascript_enabled);
  settings->setWebSecurityEnabled(web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
      javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(loads_images_automatically);
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
  settings->setDownloadableBinaryFontsEnabled(remote_fonts_enabled);
  settings->setJavaScriptCanAccessClipboard(javascript_can_access_clipboard);
  settings->setXSSAuditorEnabled(xss_auditor_enabled);
  settings->setDNSPrefetchingEnabled(dns_prefetching_enabled);
  settings->setLocalStorageEnabled(local_storage_enabled);
  WebRuntimeFeatures::enableDatabase(
      WebRuntimeFeatures::isDatabaseEnabled() || databases_enabled);
  settings->setOfflineWebApplicationCacheEnabled(application_cache_enabled);
  settings->setCaretBrowsingEnabled(caret_browsing_enabled);
  settings->setHyperlinkAuditingEnabled(hyperlink_auditing_enabled);

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

  // Display colored borders around composited render layers if requested
  // on command line.
  settings->setShowDebugBorders(show_composited_layer_borders);

  // Display an FPS indicator if requested on the command line.
  settings->setShowFPSCounter(show_fps_counter);

  // Display the current compositor tree as overlay if requested on
  // the command line
  settings->setShowPlatformLayerTree(show_composited_layer_tree);

  // Enable gpu-accelerated compositing if requested on the command line.
  settings->setAcceleratedCompositingEnabled(accelerated_compositing_enabled);

#ifndef WEBCOMPOSITOR_HAS_INITIALIZE
  settings->setUseThreadedCompositor(threaded_compositing_enabled);
#endif

  // Always enter compositing if requested on the command line.
  settings->setForceCompositingMode(force_compositing_mode);

  // Enable composite to offscreen texture if requested on the command line.
  settings->setCompositeToTextureEnabled(composite_to_texture_enabled);

  // Enable compositing for fixed position elements if requested
  // on the command line.
  settings->setAcceleratedCompositingForFixedPositionEnabled(
      fixed_position_compositing_enabled);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  settings->setAccelerated2dCanvasEnabled(accelerated_2d_canvas_enabled);

  // Enable gpu-accelerated drawing if requested on the command line.
  settings->setAcceleratedDrawingEnabled(accelerated_drawing_enabled);

  // Enabling accelerated layers from the command line enabled accelerated
  // 3D CSS, Video, and Animations.
  settings->setAcceleratedCompositingFor3DTransformsEnabled(
      accelerated_layers_enabled);
  settings->setAcceleratedCompositingForVideoEnabled(
      accelerated_video_enabled);
  settings->setAcceleratedCompositingForAnimationEnabled(
      accelerated_layers_enabled);

  // Enabling accelerated plugins if specified from the command line.
  settings->setAcceleratedCompositingForPluginsEnabled(
      accelerated_plugins_enabled);

  // WebGL and accelerated 2D canvas are always gpu composited.
  settings->setAcceleratedCompositingForCanvasEnabled(
      experimental_webgl_enabled || accelerated_2d_canvas_enabled);

  // Enable memory info reporting to page if requested on the command line.
  settings->setMemoryInfoEnabled(memory_info_enabled);

  settings->setAsynchronousSpellCheckingEnabled(
      asynchronous_spell_checking_enabled);

  for (WebInspectorPreferences::const_iterator it = inspector_settings.begin();
       it != inspector_settings.end(); ++it)
    web_view->setInspectorSetting(WebString::fromUTF8(it->first),
                                  WebString::fromUTF8(it->second));

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->setTabsToLinks(tabs_to_links);

  settings->setInteractiveFormValidationEnabled(
      interactive_form_validation_enabled);

  settings->setFullScreenEnabled(fullscreen_enabled);
  settings->setAllowDisplayOfInsecureContent(allow_displaying_insecure_content);
  settings->setAllowRunningOfInsecureContent(allow_running_insecure_content);
  settings->setShouldPrintBackgrounds(should_print_backgrounds);
  settings->setEnableScrollAnimator(enable_scroll_animator);
  settings->setHixie76WebSocketProtocolEnabled(
      hixie76_websocket_protocol_enabled);
  settings->setVisualWordMovementEnabled(visual_word_movement_enabled);

  WebNetworkStateNotifier::setOnLine(is_online);
}
