// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OpenTypeCapsSupport_h
#define OpenTypeCapsSupport_h

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/opentype/OpenTypeCapsSupport.h"
#include "platform/fonts/shaping/CaseMappingHarfBuzzBufferFiller.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"

#include <hb.h>

namespace blink {

class PLATFORM_EXPORT OpenTypeCapsSupport {
 public:
  OpenTypeCapsSupport();
  OpenTypeCapsSupport(const HarfBuzzFace*,
                      FontDescription::FontVariantCaps requested_caps,
                      hb_script_t);

  bool NeedsRunCaseSplitting();
  bool NeedsSyntheticFont(SmallCapsIterator::SmallCapsBehavior run_case);
  FontDescription::FontVariantCaps FontFeatureToUse(
      SmallCapsIterator::SmallCapsBehavior run_case);
  CaseMapIntend NeedsCaseChange(SmallCapsIterator::SmallCapsBehavior run_case);

 private:
  void DetermineFontSupport(hb_script_t);
  bool SupportsOpenTypeFeature(hb_script_t, uint32_t tag) const;

  const HarfBuzzFace* harf_buzz_face_;
  FontDescription::FontVariantCaps requested_caps_;

  enum class FontSupport {
    kFull,
    kFallback,  // Fall back to 'smcp' or 'smcp' + 'c2sc'
    kNone
  };

  enum class CapsSynthesis {
    kNone,
    kLowerToSmallCaps,
    kUpperToSmallCaps,
    kBothToSmallCaps
  };

  FontSupport font_support_;
  CapsSynthesis caps_synthesis_;
};

};  // namespace blink

#endif
