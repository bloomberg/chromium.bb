/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AlternateFontFamily_h
#define AlternateFontFamily_h

#include "build/build_config.h"
#include "platform/fonts/FontDescription.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

// We currently do not support bitmap fonts on windows.
// Instead of trying to construct a bitmap font and then going down the fallback
// path map certain common bitmap fonts to their truetype equivalent up front.
inline const AtomicString& AdjustFamilyNameToAvoidUnsupportedFonts(
    const AtomicString& family_name) {
#if defined(OS_WIN)
  // On Windows, 'Courier New' (truetype font) is always present and
  // 'Courier' is a bitmap font. On Mac on the other hand 'Courier' is
  // a truetype font. Thus pages asking for Courier are better of
  // using 'Courier New' on windows.
  DEFINE_STATIC_LOCAL(AtomicString, courier, ("Courier"));
  DEFINE_STATIC_LOCAL(AtomicString, courier_new, ("Courier New"));
  if (DeprecatedEqualIgnoringCase(family_name, courier))
    return courier_new;

  // Alias 'MS Sans Serif' (bitmap font) -> 'Microsoft Sans Serif'
  // (truetype font).
  DEFINE_STATIC_LOCAL(AtomicString, ms_sans, ("MS Sans Serif"));
  DEFINE_STATIC_LOCAL(AtomicString, microsoft_sans, ("Microsoft Sans Serif"));
  if (DeprecatedEqualIgnoringCase(family_name, ms_sans))
    return microsoft_sans;

  // Alias 'MS Serif' (bitmap) -> 'Times New Roman' (truetype font).
  // Alias 'Times' -> 'Times New Roman' (truetype font).
  // There's no 'Microsoft Sans Serif-equivalent' for Serif.
  DEFINE_STATIC_LOCAL(AtomicString, ms_serif, ("MS Serif"));
  DEFINE_STATIC_LOCAL(AtomicString, times, ("Times"));
  DEFINE_STATIC_LOCAL(AtomicString, times_new_roman, ("Times New Roman"));
  if (DeprecatedEqualIgnoringCase(family_name, ms_serif) ||
      DeprecatedEqualIgnoringCase(family_name, times))
    return times_new_roman;
#endif

  return family_name;
}

inline const AtomicString& AlternateFamilyName(
    const AtomicString& family_name) {
  // Alias Courier <-> Courier New
  DEFINE_STATIC_LOCAL(AtomicString, courier, ("Courier"));
  DEFINE_STATIC_LOCAL(AtomicString, courier_new, ("Courier New"));
  if (DeprecatedEqualIgnoringCase(family_name, courier))
    return courier_new;
#if !defined(OS_WIN)
  // On Windows, Courier New (truetype font) is always present and
  // Courier is a bitmap font. So, we don't want to map Courier New to
  // Courier.
  if (DeprecatedEqualIgnoringCase(family_name, courier_new))
    return courier;
#endif

  // Alias Times and Times New Roman.
  DEFINE_STATIC_LOCAL(AtomicString, times, ("Times"));
  DEFINE_STATIC_LOCAL(AtomicString, times_new_roman, ("Times New Roman"));
  if (DeprecatedEqualIgnoringCase(family_name, times))
    return times_new_roman;
  if (DeprecatedEqualIgnoringCase(family_name, times_new_roman))
    return times;

  // Alias Arial and Helvetica
  DEFINE_STATIC_LOCAL(AtomicString, arial, ("Arial"));
  DEFINE_STATIC_LOCAL(AtomicString, helvetica, ("Helvetica"));
  if (DeprecatedEqualIgnoringCase(family_name, arial))
    return helvetica;
  if (DeprecatedEqualIgnoringCase(family_name, helvetica))
    return arial;

  return g_empty_atom;
}

inline const AtomicString GetFallbackFontFamily(
    const FontDescription& description) {
  DEFINE_STATIC_LOCAL(const AtomicString, sans_str, ("sans-serif"));
  DEFINE_STATIC_LOCAL(const AtomicString, serif_str, ("serif"));
  DEFINE_STATIC_LOCAL(const AtomicString, monospace_str, ("monospace"));
  DEFINE_STATIC_LOCAL(const AtomicString, cursive_str, ("cursive"));
  DEFINE_STATIC_LOCAL(const AtomicString, fantasy_str, ("fantasy"));

  switch (description.GenericFamily()) {
    case FontDescription::kSansSerifFamily:
      return sans_str;
    case FontDescription::kSerifFamily:
      return serif_str;
    case FontDescription::kMonospaceFamily:
      return monospace_str;
    case FontDescription::kCursiveFamily:
      return cursive_str;
    case FontDescription::kFantasyFamily:
      return fantasy_str;
    default:
      // Let the caller use the system default font.
      return g_empty_atom;
  }
}

}  // namespace blink

#endif  // AlternateFontFamily_h
