/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/LinkRelAttribute.h"

#include "platform/wtf/text/CString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static inline void TestLinkRelAttribute(String value,
                                        bool is_style_sheet,
                                        IconType icon_type,
                                        bool is_alternate,
                                        bool is_dns_prefetch,
                                        bool is_link_prerender,
                                        bool is_import = false,
                                        bool is_preconnect = false) {
  LinkRelAttribute link_rel_attribute(value);
  ASSERT_EQ(is_style_sheet, link_rel_attribute.IsStyleSheet())
      << value.Utf8().data();
  ASSERT_EQ(icon_type, link_rel_attribute.GetIconType()) << value.Utf8().data();
  ASSERT_EQ(is_alternate, link_rel_attribute.IsAlternate())
      << value.Utf8().data();
  ASSERT_EQ(is_dns_prefetch, link_rel_attribute.IsDNSPrefetch())
      << value.Utf8().data();
  ASSERT_EQ(is_link_prerender, link_rel_attribute.IsLinkPrerender())
      << value.Utf8().data();
  ASSERT_EQ(is_import, link_rel_attribute.IsImport()) << value.Utf8().data();
  ASSERT_EQ(is_preconnect, link_rel_attribute.IsPreconnect())
      << value.Utf8().data();
}

TEST(LinkRelAttributeTest, Constructor) {
  TestLinkRelAttribute("stylesheet", true, kInvalidIcon, false, false, false);
  TestLinkRelAttribute("sTyLeShEeT", true, kInvalidIcon, false, false, false);

  TestLinkRelAttribute("icon", false, kFavicon, false, false, false);
  TestLinkRelAttribute("iCoN", false, kFavicon, false, false, false);
  TestLinkRelAttribute("shortcut icon", false, kFavicon, false, false, false);
  TestLinkRelAttribute("sHoRtCuT iCoN", false, kFavicon, false, false, false);

  TestLinkRelAttribute("dns-prefetch", false, kInvalidIcon, false, true, false);
  TestLinkRelAttribute("dNs-pReFeTcH", false, kInvalidIcon, false, true, false);
  TestLinkRelAttribute("alternate dNs-pReFeTcH", false, kInvalidIcon, true,
                       true, false);

  TestLinkRelAttribute("apple-touch-icon", false, kTouchIcon, false, false,
                       false);
  TestLinkRelAttribute("aPpLe-tOuCh-IcOn", false, kTouchIcon, false, false,
                       false);
  TestLinkRelAttribute("apple-touch-icon-precomposed", false,
                       kTouchPrecomposedIcon, false, false, false);
  TestLinkRelAttribute("aPpLe-tOuCh-IcOn-pReCoMpOsEd", false,
                       kTouchPrecomposedIcon, false, false, false);

  TestLinkRelAttribute("alternate stylesheet", true, kInvalidIcon, true, false,
                       false);
  TestLinkRelAttribute("stylesheet alternate", true, kInvalidIcon, true, false,
                       false);
  TestLinkRelAttribute("aLtErNaTe sTyLeShEeT", true, kInvalidIcon, true, false,
                       false);
  TestLinkRelAttribute("sTyLeShEeT aLtErNaTe", true, kInvalidIcon, true, false,
                       false);

  TestLinkRelAttribute("stylesheet icon prerender aLtErNaTe", true, kFavicon,
                       true, false, true);
  TestLinkRelAttribute("alternate icon stylesheet", true, kFavicon, true, false,
                       false);

  TestLinkRelAttribute("import", false, kInvalidIcon, false, false, false,
                       true);
  TestLinkRelAttribute("alternate import", false, kInvalidIcon, true, false,
                       false, true);
  TestLinkRelAttribute("stylesheet import", true, kInvalidIcon, false, false,
                       false, false);

  TestLinkRelAttribute("preconnect", false, kInvalidIcon, false, false, false,
                       false, true);
  TestLinkRelAttribute("pReCoNnEcT", false, kInvalidIcon, false, false, false,
                       false, true);
}

}  // namespace blink
