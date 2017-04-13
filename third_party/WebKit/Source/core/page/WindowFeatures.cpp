/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights
 * reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "core/page/WindowFeatures.h"

#include "platform/geometry/IntRect.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't when
// parsing window features.
static bool IsWindowFeaturesSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\0';
}

WindowFeatures::WindowFeatures(const String& features)
    : x(0),
      x_set(false),
      y(0),
      y_set(false),
      width(0),
      width_set(false),
      height(0),
      height_set(false),
      resizable(true),
      fullscreen(false),
      dialog(false),
      noopener(false) {
  /*
     The IE rule is: all features except for channelmode and fullscreen default
     to YES, but if the user specifies a feature string, all features default to
     NO. (There is no public standard that applies to this method.)

     <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
     We always allow a window to be resized, which is consistent with Firefox.
     */

  if (features.IsEmpty()) {
    menu_bar_visible = true;
    status_bar_visible = true;
    tool_bar_visible = true;
    location_bar_visible = true;
    scrollbars_visible = true;
    return;
  }

  menu_bar_visible = false;
  status_bar_visible = false;
  tool_bar_visible = false;
  location_bar_visible = false;
  scrollbars_visible = false;

  // Tread lightly in this code -- it was specifically designed to mimic Win
  // IE's parsing behavior.
  unsigned key_begin, key_end;
  unsigned value_begin, value_end;

  String buffer = features.DeprecatedLower();
  unsigned length = buffer.length();
  for (unsigned i = 0; i < length;) {
    // skip to first non-separator, but don't skip past the end of the string
    while (i < length && IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_begin = i;

    // skip to first separator
    while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_end = i;

    SECURITY_DCHECK(i <= length);

    // skip to first '=', but don't skip past a ',' or the end of the string
    while (i < length && buffer[i] != '=') {
      if (buffer[i] == ',')
        break;
      i++;
    }

    SECURITY_DCHECK(i <= length);

    // Skip to first non-separator, but don't skip past a ',' or the end of the
    // string.
    while (i < length && IsWindowFeaturesSeparator(buffer[i])) {
      if (buffer[i] == ',')
        break;
      i++;
    }
    value_begin = i;

    SECURITY_DCHECK(i <= length);

    // skip to first separator
    while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
      i++;
    value_end = i;

    SECURITY_DCHECK(i <= length);

    String key_string(buffer.Substring(key_begin, key_end - key_begin));
    String value_string(buffer.Substring(value_begin, value_end - value_begin));

    SetWindowFeature(key_string, value_string);
  }
}

void WindowFeatures::SetWindowFeature(const String& key_string,
                                      const String& value_string) {
  int value;

  // Listing a key with no value is shorthand for key=yes
  if (value_string.IsEmpty() || value_string == "yes")
    value = 1;
  else
    value = value_string.ToInt();

  // We treat keyString of "resizable" here as an additional feature rather than
  // setting resizeable to true.  This is consistent with Firefox, but could
  // also be handled at another level.

  if (key_string == "left" || key_string == "screenx") {
    x_set = true;
    x = value;
  } else if (key_string == "top" || key_string == "screeny") {
    y_set = true;
    y = value;
  } else if (key_string == "width" || key_string == "innerwidth") {
    width_set = true;
    width = value;
  } else if (key_string == "height" || key_string == "innerheight") {
    height_set = true;
    height = value;
  } else if (key_string == "menubar") {
    menu_bar_visible = value;
  } else if (key_string == "toolbar") {
    tool_bar_visible = value;
  } else if (key_string == "location") {
    location_bar_visible = value;
  } else if (key_string == "status") {
    status_bar_visible = value;
  } else if (key_string == "fullscreen") {
    fullscreen = value;
  } else if (key_string == "scrollbars") {
    scrollbars_visible = value;
  } else if (key_string == "noopener") {
    noopener = true;
  } else if (value == 1) {
    additional_features.push_back(key_string);
  }
}

WindowFeatures::WindowFeatures(const String& dialog_features_string,
                               const IntRect& screen_available_rect)
    : width_set(true),
      height_set(true),
      menu_bar_visible(false),
      tool_bar_visible(false),
      location_bar_visible(false),
      fullscreen(false),
      dialog(true),
      noopener(false) {
  DialogFeaturesMap features;
  ParseDialogFeatures(dialog_features_string, features);

  const bool kTrusted = false;

  // The following features from Microsoft's documentation are not implemented:
  // - default font settings
  // - width, height, left, and top specified in units other than "px"
  // - edge (sunken or raised, default is raised)
  // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog
  //               hide when you print
  // - help: boolFeature(features, "help", true), makes help icon appear in
  //         dialog (what does it do on Windows?)
  // - unadorned: trusted && boolFeature(features, "unadorned");

  // default here came from frame size of dialog in MacIE
  width = IntFeature(features, "dialogwidth", 100,
                     screen_available_rect.Width(), 620);
  // default here came from frame size of dialog in MacIE
  height = IntFeature(features, "dialogheight", 100,
                      screen_available_rect.Height(), 450);

  x = IntFeature(features, "dialogleft", screen_available_rect.X(),
                 screen_available_rect.MaxX() - width, -1);
  x_set = x > 0;
  y = IntFeature(features, "dialogtop", screen_available_rect.Y(),
                 screen_available_rect.MaxY() - height, -1);
  y_set = y > 0;

  if (BoolFeature(features, "center", true)) {
    if (!x_set) {
      x = screen_available_rect.X() +
          (screen_available_rect.Width() - width) / 2;
      x_set = true;
    }
    if (!y_set) {
      y = screen_available_rect.Y() +
          (screen_available_rect.Height() - height) / 2;
      y_set = true;
    }
  }

  resizable = BoolFeature(features, "resizable");
  scrollbars_visible = BoolFeature(features, "scroll", true);
  status_bar_visible = BoolFeature(features, "status", !kTrusted);
}

bool WindowFeatures::BoolFeature(const DialogFeaturesMap& features,
                                 const char* key,
                                 bool default_value) {
  DialogFeaturesMap::const_iterator it = features.Find(key);
  if (it == features.end())
    return default_value;
  const String& value = it->value;
  return value.IsNull() || value == "1" || value == "yes" || value == "on";
}

int WindowFeatures::IntFeature(const DialogFeaturesMap& features,
                               const char* key,
                               int min,
                               int max,
                               int default_value) {
  DialogFeaturesMap::const_iterator it = features.Find(key);
  if (it == features.end())
    return default_value;
  bool ok;
  int parsed_number = it->value.ToInt(&ok);
  if (!ok)
    return default_value;
  if (parsed_number < min || max <= min)
    return min;
  if (parsed_number > max)
    return max;
  return parsed_number;
}

void WindowFeatures::ParseDialogFeatures(const String& string,
                                         DialogFeaturesMap& map) {
  Vector<String> vector;
  string.Split(';', vector);
  size_t size = vector.size();
  for (size_t i = 0; i < size; ++i) {
    const String& feature_string = vector[i];

    size_t separator_position = feature_string.Find('=');
    size_t colon_position = feature_string.Find(':');
    if (separator_position != kNotFound && colon_position != kNotFound)
      continue;  // ignore strings that have both = and :
    if (separator_position == kNotFound)
      separator_position = colon_position;

    String key = feature_string.Left(separator_position)
                     .StripWhiteSpace()
                     .DeprecatedLower();

    // Null string for value indicates key without value.
    String value;
    if (separator_position != kNotFound) {
      value = feature_string.Substring(separator_position + 1)
                  .StripWhiteSpace()
                  .DeprecatedLower();
      value = value.Left(value.Find(' '));
    }

    map.Set(key, value);
  }
}

}  // namespace blink
