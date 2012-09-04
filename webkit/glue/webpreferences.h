// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing webkit's settings.
//
// Adding new values to this class probably involves updating
// WebKit::WebSettings, content/common/view_messages.h, browser/tab_contents/
// render_view_host_delegate_helper.cc, and browser/profiles/profile.cc.

#ifndef WEBKIT_GLUE_WEBPREFERENCES_H__
#define WEBKIT_GLUE_WEBPREFERENCES_H__

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
class WebView;
}

namespace webkit_glue {

struct WEBKIT_GLUE_EXPORT WebPreferences {
  // Map of ISO 15924 four-letter script code to font family.  For example,
  // "Arab" to "My Arabic Font".
  typedef std::map<std::string, string16> ScriptFontFamilyMap;

  // The ISO 15924 script code for undetermined script aka Common. It's the
  // default used on WebKit's side to get/set a font setting when no script is
  // specified.
  static const char kCommonScript[];

  ScriptFontFamilyMap standard_font_family_map;
  ScriptFontFamilyMap fixed_font_family_map;
  ScriptFontFamilyMap serif_font_family_map;
  ScriptFontFamilyMap sans_serif_font_family_map;
  ScriptFontFamilyMap cursive_font_family_map;
  ScriptFontFamilyMap fantasy_font_family_map;
  int default_font_size;
  int default_fixed_font_size;
  int minimum_font_size;
  int minimum_logical_font_size;
  std::string default_encoding;
  bool apply_default_device_scale_factor_in_compositor;
  bool javascript_enabled;
  bool web_security_enabled;
  bool javascript_can_open_windows_automatically;
  bool loads_images_automatically;
  bool images_enabled;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool developer_extras_enabled;
  typedef std::vector<std::pair<std::string, std::string> >
      WebInspectorPreferences;
  WebInspectorPreferences inspector_settings;
  bool site_specific_quirks_enabled;
  bool shrinks_standalone_images_to_fit;
  bool uses_universal_detector;
  bool text_areas_are_resizable;
  bool java_enabled;
  bool allow_scripts_to_close_windows;
  bool uses_page_cache;
  bool remote_fonts_enabled;
  bool javascript_can_access_clipboard;
  bool xss_auditor_enabled;
  // We don't use dns_prefetching_enabled to disable DNS prefetching.  Instead,
  // we disable the feature at a lower layer so that we catch non-WebKit uses
  // of DNS prefetch as well.
  bool dns_prefetching_enabled;
  bool local_storage_enabled;
  bool databases_enabled;
  bool application_cache_enabled;
  bool tabs_to_links;
  bool caret_browsing_enabled;
  bool hyperlink_auditing_enabled;
  bool is_online;
  bool user_style_sheet_enabled;
  GURL user_style_sheet_location;
  bool author_and_user_styles_enabled;
  bool frame_flattening_enabled;
  bool allow_universal_access_from_file_urls;
  bool allow_file_access_from_file_urls;
  bool webaudio_enabled;
  bool experimental_webgl_enabled;
  bool flash_3d_enabled;
  bool flash_stage3d_enabled;
  bool gl_multisampling_enabled;
  bool privileged_webgl_extensions_enabled;
  bool webgl_errors_to_console_enabled;
  bool show_composited_layer_borders;
  bool show_composited_layer_tree;
  bool show_fps_counter;
  bool show_paint_rects;
  bool render_vsync_enabled;
  bool asynchronous_spell_checking_enabled;
  bool unified_textchecker_enabled;
  bool accelerated_compositing_enabled;
  bool force_compositing_mode;
  bool fixed_position_compositing_enabled;
  bool accelerated_layers_enabled;
  bool accelerated_animation_enabled;
  bool accelerated_video_enabled;
  bool accelerated_2d_canvas_enabled;
  bool deferred_2d_canvas_enabled;
  bool accelerated_painting_enabled;
  bool accelerated_filters_enabled;
  bool gesture_tap_highlight_enabled;
  bool accelerated_plugins_enabled;
  bool memory_info_enabled;
  bool fullscreen_enabled;
  bool allow_displaying_insecure_content;
  bool allow_running_insecure_content;
  bool password_echo_enabled;
  bool should_print_backgrounds;
  bool enable_scroll_animator;
  bool visual_word_movement_enabled;
  bool css_regions_enabled;
  bool css_shaders_enabled;
  bool css_variables_enabled;
  bool device_supports_touch;
  bool device_supports_mouse;
  int default_tile_width;
  int default_tile_height;
  int max_untiled_layer_width;
  int max_untiled_layer_height;
  bool fixed_position_creates_stacking_context;
  bool sync_xhr_in_documents_enabled;
  int number_of_cpu_cores;

  // This flags corresponds to a Page's Settings' setCookieEnabled state. It
  // only controls whether or not the "document.cookie" field is properly
  // connected to the backing store, for instance if you wanted to be able to
  // define custom getters and setters from within a unique security content
  // without raising a DOM security exception.
  bool cookie_enabled;

  // We try to keep the default values the same as the default values in
  // chrome, except for the cases where it would require lots of extra work for
  // the embedder to use the same default value.
  WebPreferences();
  ~WebPreferences();

  void Apply(WebKit::WebView* web_view) const;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBPREFERENCES_H__
