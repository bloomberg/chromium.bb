// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter.h"

#include <stddef.h>
#include <string.h>

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_formatter {

namespace {

using base::ASCIIToUTF16;
using base::WideToUTF16;

struct IDNTestCase {
  // The IDNA/Punycode version of the domain (plain ASCII).
  const char* const input;
  // The equivalent Unicode version of the domain. Even if we expect the domain
  // to be displayed in Punycode, this should still contain the Unicode
  // equivalent (see |unicode_allowed|).
  const wchar_t* unicode_output;
  // Whether we expect the domain to be displayed decoded as a Unicode string
  // (true) or in its Punycode form (false).
  const bool unicode_allowed;
};

// These cases can be generated with the script
// tools/security/idn_test_case_generator.py.
// See documentation there: you can either run it from the command line or call
// the make_case function directly from the Python shell (which may be easier
// for entering Unicode text).
//
// Q: Why not just do this conversion right here in the test, rather than having
//    a Python script to generate it?
// A: Because then we would have to rely on complex logic (IDNA encoding) in the
//    test itself; the same code we are trying to test. By using Python's IDN
//    encoder to generate the test data, we independently verify that our
//    algorithm is correct.

// TODO(jshin): Replace L"..." with "..." in UTF-8 when it's easier to read.
const IDNTestCase kIdnCases[] = {
    // No IDN
    {"www.google.com", L"www.google.com", true},
    {"www.google.com.", L"www.google.com.", true},
    {".", L".", true},
    {"", L"", true},
    // IDN
    // Hanzi (Traditional Chinese)
    {"xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn", true},
    // Hanzi ('video' in Simplified Chinese
    {"xn--cy2a840a.com", L"\x89c6\x9891.com", true},
    // Hanzi + '123'
    {"www.xn--123-p18d.com",
     L"www.\x4e00"
     L"123.com",
     true},
    // Hanzi + Latin : U+56FD is simplified
    {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com", true},
    // Kanji + Kana (Japanese)
    {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp", true},
    // Katakana including U+30FC
    {"xn--tckm4i2e.jp", L"\x30b3\x30de\x30fc\x30b9.jp", true},
    {"xn--3ck7a7g.jp", L"\u30ce\u30f3\u30bd.jp", true},
    // Katakana + Latin (Japanese)
    {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp", true},
    {"xn--3bkxe.jp", L"\x30c8\x309a.jp", true},
    // Hangul (Korean)
    {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr", true},
    // b<u-umlaut>cher (German)
    {"xn--bcher-kva.de",
     L"b\x00fc"
     L"cher.de",
     true},
    // a with diaeresis
    {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se", true},
    // c-cedilla (French)
    {"www.xn--alliancefranaise-npb.fr",
     L"www.alliancefran\x00e7"
     L"aise.fr",
     true},
    // caf'e with acute accent' (French)
    {"xn--caf-dma.fr", L"caf\x00e9.fr", true},
    // c-cedillla and a with tilde (Portuguese)
    {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br", true},
    // s with caron
    {"xn--achy-f6a.com",
     L"\x0161"
     L"achy.com",
     true},
    {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
     true},
    // Eutopia + 123 (Greek)
    {"xn---123-pldm0haj2bk.gr",
     L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr", true},
    // Cyrillic (Russian)
    {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru", true},
    // Cyrillic + 123 (Russian)
    {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru", true},
    // 'president' in Russian. Is a wholescript confusable, but allowed.
    {"xn--d1abbgf6aiiy.xn--p1ai",
     L"\x043f\x0440\x0435\x0437\x0438\x0434\x0435\x043d\x0442.\x0440\x0444",
     true},
    // Arabic
    {"xn--mgba1fmg.eg", L"\x0627\x0641\x0644\x0627\x0645.eg", true},
    // Hebrew
    {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he", true},
    // Hebrew + Common
    {"xn---123-ptf2c5c6bt.il", L"\x05e2\x05d1\x05e8\x05d9\x05ea-123.il", true},
    // Thai
    {"xn--12c2cc4ag3b4ccu.th",
     L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th", true},
    // Thai + Common
    {"xn---123-9goxcp8c9db2r.th",
     L"\x0e20\x0e32\x0e29\x0e32\x0e44\x0e17\x0e22-123.th", true},
    // Devangari (Hindi)
    {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in", true},
    // Devanagari + Common
    {"xn---123-kbjl2j0bl2k.in", L"\x0939\x093f\x0928\x094d\x0926\x0940-123.in",
     true},

    // URL test with mostly numbers and one confusable character
    // Georgian 'd' 4000.com
    {"xn--4000-pfr.com",
     L"\x10eb"
     L"4000.com",
     false},

    // What used to be 5 Aspirational scripts in the earlier versions of UAX 31.
    // UAX 31 does not define aspirational scripts any more.
    // See http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
    // Unifieid Canadian Syllabary
    {"xn--dfe0tte.ca", L"\x1456\x14c2\x14ef.ca", false},
    // Tifinagh
    {"xn--4ljxa2bb4a6bxb.ma", L"\x2d5c\x2d49\x2d3c\x2d49\x2d4f\x2d30\x2d56.ma",
     false},
    // Tifinagh with a disallowed character(U+2D6F)
    {"xn--hmjzaby5d5f.ma", L"\x2d5c\x2d49\x2d3c\x2d6f\x2d49\x2d4f.ma", false},
    // Yi
    {"xn--4o7a6e1x64c.cn", L"\xa188\xa320\xa071\xa0b7.cn", false},
    // Mongolian - 'ordu' (place, camp)
    {"xn--56ec8bp.cn", L"\x1823\x1837\x1833\x1824.cn", false},
    // Mongolian with a disallowed character
    {"xn--95e5de3ds.cn", L"\x1823\x1837\x1804\x1833\x1824.cn", false},
    // Miao/Pollad
    {"xn--2u0fpf0a.cn", L"\U00016f04\U00016f62\U00016f59.cn", false},

    // Script mixing tests
    // The following script combinations are allowed.
    // HIGHLY_RESTRICTIVE with Latin limited to ASCII-Latin.
    // ASCII-Latin + Japn (Kana + Han)
    // ASCII-Latin + Kore (Hangul + Han)
    // ASCII-Latin + Han + Bopomofo
    // "payp<alpha>l.com"
    {"www.xn--paypl-g9d.com", L"payp\x03b1l.com", false},
    // google.gr with Greek omicron and epsilon
    {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr", false},
    // google.ru with Cyrillic o
    {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru", false},
    // h<e with acute>llo<China in Han>.cn
    {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn", false},
    // <Greek rho><Cyrillic a><Cyrillic u>.ru
    {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru", false},
    // Georgian + Latin
    {"xn--abcef-vuu.test",
     L"abc\x10eb"
     L"ef.test",
     false},
    // Hangul + Latin
    {"xn--han-eb9ll88m.kr", L"\xd55c\xae00han.kr", true},
    // Hangul + Latin + Han with IDN ccTLD
    {"xn--han-or0kq92gkm3c.xn--3e0b707e", L"\xd55c\xae00han\x97d3.\xd55c\xad6d",
     true},
    // non-ASCII Latin + Hangul
    {"xn--caf-dma9024xvpg.kr", L"caf\x00e9\xce74\xd398.kr", false},
    // Hangul + Hiragana
    {"xn--y9j3b9855e.kr", L"\xd55c\x3072\x3089.kr", false},
    // <Hiragana>.<Hangul> is allowed because script mixing check is per label.
    {"xn--y9j3b.xn--3e0b707e", L"\x3072\x3089.\xd55c\xad6d", true},
    //  Traditional Han + Latin
    {"xn--hanzi-u57ii69i.tw", L"\x6f22\x5b57hanzi.tw", true},
    //  Simplified Han + Latin
    {"xn--hanzi-u57i952h.cn", L"\x6c49\x5b57hanzi.cn", true},
    // Simplified Han + Traditonal Han
    {"xn--hanzi-if9kt8n.cn", L"\x6c49\x6f22hanzi.cn", true},
    //  Han + Hiragana + Katakana + Latin
    {"xn--kanji-ii4dpizfq59yuykqr4b.jp",
     L"\x632f\x308a\x4eee\x540d\x30ab\x30bfkanji.jp", true},
    // Han + Bopomofo
    {"xn--5ekcde0577e87tc.tw", L"\x6ce8\x97f3\x3105\x3106\x3107\x3108.tw",
     true},
    // Han + Latin + Bopomofo
    {"xn--bopo-ty4cghi8509kk7xd.tw",
     L"\x6ce8\x97f3"
     L"bopo\x3105\x3106\x3107\x3108.tw",
     true},
    // Latin + Bopomofo
    {"xn--bopomofo-hj5gkalm.tw", L"bopomofo\x3105\x3106\x3107\x3108.tw", true},
    // Bopomofo + Katakana
    {"xn--lcka3d1bztghi.tw",
     L"\x3105\x3106\x3107\x3108\x30ab\x30bf\x30ab\x30ca.tw", false},
    //  Bopomofo + Hangul
    {"xn--5ekcde4543qbec.tw", L"\x3105\x3106\x3107\x3108\xc8fc\xc74c.tw",
     false},
    // Devanagari + Latin
    {"xn--ab-3ofh8fqbj6h.in", L"ab\x0939\x093f\x0928\x094d\x0926\x0940.in",
     false},
    // Thai + Latin
    {"xn--ab-jsi9al4bxdb6n.th",
     L"ab\x0e20\x0e32\x0e29\x0e32\x0e44\x0e17\x0e22.th", false},
    // Armenian + Latin
    {"xn--bs-red.com", L"b\x057ds.com", false},
    // Tibetan + Latin
    {"xn--foo-vkm.com", L"foo\x0f37.com", false},
    // Oriya + Latin
    {"xn--fo-h3g.com", L"fo\x0b66.com", false},
    // Gujarati + Latin
    {"xn--fo-isg.com", L"fo\x0ae6.com", false},
    // <vitamin in Katakana>b1.com
    {"xn--b1-xi4a7cvc9f.com",
     L"\x30d3\x30bf\x30df\x30f3"
     L"b1.com",
     true},
    // Devanagari + Han
    {"xn--t2bes3ds6749n.com", L"\x0930\x094b\x0932\x0947\x76e7\x0938.com",
     false},
    // Devanagari + Bengali
    {"xn--11b0x.in", L"\x0915\x0995.in", false},
    // Canadian Syllabary + Latin
    {"xn--ab-lym.com", L"ab\x14BF.com", false},
    {"xn--ab1-p6q.com", L"ab1\x14BF.com", false},
    {"xn--1ab-m6qd.com",
     L"\x14BF"
     L"1ab.com",
     false},
    {"xn--ab-jymc.com",
     L"\x14BF"
     L"ab.com",
     false},
    // Tifinagh + Latin
    {"xn--liy-go4a.com", L"li\u24dfy.com", false},
    {"xn--rol-ho4a.com", L"rol\u24df.com", false},
    {"xn--ily-eo4a.com", L"\u24dfily.com", false},
    {"xn--1ly-eo4a.com", L"\u24df1ly.com", false},

    // Invisibility check
    // Thai tone mark malek(U+0E48) repeated
    {"xn--03c0b3ca.th", L"\x0e23\x0e35\x0e48\x0e48.th", false},
    // Accute accent repeated
    {"xn--a-xbba.com", L"a\x0301\x0301.com", false},
    // 'a' with acuted accent + another acute accent
    {"xn--1ca20i.com", L"\x00e1\x0301.com", false},
    // Combining mark at the beginning
    {"xn--abc-fdc.jp",
     L"\x0300"
     L"abc.jp",
     false},

    // The following three are detected by |dangerous_pattern| regex, but
    // can be regarded as an extension of blocking repeated diacritic marks.
    // i followed by U+0307 (combining dot above)
    {"xn--pixel-8fd.com", L"pi\x0307xel.com", false},
    // U+0131 (dotless i) followed by U+0307
    {"xn--pxel-lza43z.com", L"p\x0131\x0307xel.com", false},
    // j followed by U+0307 (combining dot above)
    {"xn--jack-qwc.com",
     L"j\x0307"
     L"ack.com",
     false},
    // l followed by U+0307
    {"xn--lace-qwc.com",
     L"l\x0307"
     L"ace.com",
     false},

    // Do not allow a combining mark after dotless i/j.
    {"xn--pxel-lza29y.com", L"p\x0131\x0300xel.com", false},
    {"xn--ack-gpb42h.com",
     L"\x0237\x0301"
     L"ack.com",
     false},

    // Mixed script confusable
    // google with Armenian Small Letter Oh(U+0585)
    {"xn--gogle-lkg.com", L"g\x0585ogle.com", false},
    {"xn--range-kkg.com", L"\x0585range.com", false},
    {"xn--cucko-pkg.com", L"cucko\x0585.com", false},
    // Latin 'o' in Armenian.
    {"xn--o-ybcg0cu0cq.com", L"o\x0585\x0580\x0574\x0578\x0582\x0566\x0568.com",
     false},
    // Hiragana HE(U+3078) mixed with Katakana
    {"xn--49jxi3as0d0fpc.com",
     L"\x30e2\x30d2\x30fc\x30c8\x3078\x30d6\x30f3.com", false},

    // U+30FC should be preceded by a Hiragana/Katakana.
    // Katakana + U+30FC + Han
    {"xn--lck0ip02qw5ya.jp", L"\x30ab\x30fc\x91ce\x7403.jp", true},
    // Hiragana + U+30FC + Han
    {"xn--u8j5tr47nw5ya.jp", L"\x304b\x30fc\x91ce\x7403.jp", true},
    // U+30FC + Han
    {"xn--weka801xo02a.com", L"\x30fc\x52d5\x753b\x30fc.com", false},
    // Han + U+30FC + Han
    {"xn--wekz60nb2ay85atj0b.jp", L"\x65e5\x672c\x30fc\x91ce\x7403.jp", false},
    // U+30FC at the beginning
    {"xn--wek060nb2a.jp", L"\x30fc\x65e5\x672c", false},
    // Latin + U+30FC + Latin
    {"xn--abcdef-r64e.jp",
     L"abc\x30fc"
     L"def.jp",
     false},

    // U+30FB (・) is not allowed next to Latin, but allowed otherwise.
    // U+30FB + Han
    {"xn--vekt920a.jp", L"\x30fb\x91ce.jp", true},
    // Han + U+30FB + Han
    {"xn--vek160nb2ay85atj0b.jp", L"\x65e5\x672c\x30fb\x91ce\x7403.jp", true},
    // Latin + U+30FB + Latin
    {"xn--abcdef-k64e.jp",
     L"abc\x30fb"
     L"def.jp",
     false},
    // U+30FB + Latin
    {"xn--abc-os4b.jp",
     L"\x30fb"
     L"abc.jp",
     false},

    // U+30FD (ヽ) is allowed only after Katakana.
    // Katakana + U+30FD
    {"xn--lck2i.jp", L"\x30ab\x30fd.jp", true},
    // Hiragana + U+30FD
    {"xn--u8j7t.jp", L"\x304b\x30fd.jp", false},
    // Han + U+30FD
    {"xn--xek368f.jp", L"\x4e00\x30fd.jp", false},
    {"xn--aa-mju.jp", L"a\x30fd.jp", false},
    {"xn--a1-bo4a.jp", L"a1\x30fd.jp", false},

    // U+30FE (ヾ) is allowed only after Katakana.
    // Katakana + U+30FE
    {"xn--lck4i.jp", L"\x30ab\x30fe.jp", true},
    // Hiragana + U+30FE
    {"xn--u8j9t.jp", L"\x304b\x30fe.jp", false},
    // Han + U+30FE
    {"xn--yek168f.jp", L"\x4e00\x30fe.jp", false},
    {"xn--a-oju.jp", L"a\x30fe.jp", false},
    {"xn--a1-eo4a.jp", L"a1\x30fe.jp", false},

    // Cyrillic labels made of Latin-look-alike Cyrillic letters.
    // ѕсоре.com with ѕсоре in Cyrillic
    {"xn--e1argc3h.com", L"\x0455\x0441\x043e\x0440\x0435.com", false},
    // ѕсоре123.com with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.com",
     L"\x0455\x0441\x043e\x0440\x0435"
     L"123.com",
     false},
    // ѕсоре-рау.com with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.com",
     L"\x0455\x0441\x043e\x0440\x0435-\x0440\x0430\x0443.com", false},
    // ѕсоре·рау.com with scope and pay in Cyrillic and U+00B7 between them.
    {"xn--uba29ona9akccw8m.com",
     L"\x0455\x0441\x043e\x0440\x0435\u00b7\x0440\x0430\x0443.com", false},

    // The same as above three, but in IDN TLD.
    {"xn--e1argc3h.xn--p1ai", L"\x0455\x0441\x043e\x0440\x0435.\x0440\x0444",
     true},
    {"xn--123-qdd8bmf3n.xn--p1ai",
     L"\x0455\x0441\x043e\x0440\x0435"
     L"123.\x0440\x0444",
     true},
    {"xn--uba29ona9akccw8m.xn--p1ai",
     L"\x0455\x0441\x043e\x0440\x0435\u00b7\x0440\x0430\x0443.\x0440\x0444",
     true},

    // ѕсоре-рау.한국 with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.xn--3e0b707e",
     L"\x0455\x0441\x043e\x0440\x0435-\x0440\x0430\x0443.\xd55c\xad6d", true},

    // музей (museum in Russian) has characters without a Latin-look-alike.
    {"xn--e1adhj9a.com", L"\x043c\x0443\x0437\x0435\x0439.com", true},

    // ѕсоԗе.com is Cyrillic with Latin lookalikes.
    {"xn--e1ari3f61c.com", L"\x0455\x0441\x043e\x0517\x0435.com", false},

    // Combining Diacritic marks after a script other than Latin-Greek-Cyrillic
    {"xn--rsa2568fvxya.com", L"\xd55c\x0301\xae00.com", false},  // 한́글.com
    {"xn--rsa0336bjom.com", L"\x6f22\x0307\x5b57.com", false},   // 漢̇字.com
    // नागरी́.com
    {"xn--lsa922apb7a6do.com", L"\x0928\x093e\x0917\x0930\x0940\x0301.com",
     false},

    // Similarity checks against the list of top domains. "digklmo68.com" and
    // 'digklmo68.co.uk" are listed for unittest in the top domain list.
    {"xn--igklmo68-nea32c.com", L"\x0111igklmo68.com", false},  // đigklmo68.com
    {"www.xn--igklmo68-nea32c.com", L"www.\x0111igklmo68.com", false},
    {"foo.bar.xn--igklmo68-nea32c.com", L"foo.bar.\x0111igklmo68.com", false},
    {"xn--igklmo68-nea32c.co.uk", L"\x0111igklmo68.co.uk", false},
    {"mail.xn--igklmo68-nea32c.co.uk", L"mail.\x0111igklmo68.co.uk", false},
    {"xn--digklmo68-6jf.com", L"di\x0307gklmo68.com", false},  // di̇gklmo68.com
    {"xn--digklmo68-7vf.com", L"dig\x0331klmo68.com", false},  // dig̱klmo68.com
    {"xn--diglmo68-omb.com", L"dig\x0138lmo68.com", false},    // digĸlmo68.com
    {"xn--digkmo68-9ob.com", L"digk\x0142mo68.com", false},    // digkłmo68.com
    {"xn--digklo68-l89c.com", L"digkl\x1e43o68.com", false},  // digklṃo68.com
    {"xn--digklm68-b5a.com",
     L"digklm\x00f8"
     L"68.com",
     false},  // digklmø68.com
    {"xn--digklmo8-h7g.com",
     L"digklmo\x0431"
     L"8.com",
     false},                                                 // digklmoб8.com
    {"xn--digklmo6-7yr.com", L"digklmo6\x09ea.com", false},  // digklmo6৪.com

    // 'islkpx123.com' is in the test domain list.
    // 'іѕӏкрх123' can look like 'islkpx123' in some fonts.
    {"xn--123-bed4a4a6hh40i.com",
     L"\x0456\x0455\x04cf\x043a\x0440\x0445"
     L"123.com",
     false},

    // 'o2.com', '28.com', '39.com', '43.com', '89.com', 'oo.com' and 'qq.com'
    // are all explicitly added to the test domain list to aid testing of
    // Latin-lookalikes that are numerics in other character sets and similar
    // edge cases.
    //
    // Bengali:
    {"xn--07be.com", L"\x09e6\x09e8.com", false},
    {"xn--27be.com", L"\x09e8\x09ea.com", false},
    {"xn--77ba.com", L"\x09ed\x09ed.com", false},
    // Gurmukhi:
    {"xn--qcce.com", L"\x0a68\x0a6a.com", false},
    {"xn--occe.com", L"\x0a66\x0a68.com", false},
    {"xn--rccd.com", L"\x0a6b\x0a69.com", false},
    {"xn--pcca.com", L"\x0a67\x0a67.com", false},
    // Telugu:
    {"xn--drcb.com", L"\x0c69\x0c68.com", false},
    // Devanagari:
    {"xn--d4be.com", L"\x0966\x0968.com", false},
    // Kannada:
    {"xn--yucg.com", L"\x0ce6\x0ce9.com", false},
    {"xn--yuco.com", L"\x0ce6\x0ced.com", false},
    // Oriya:
    {"xn--1jcf.com", L"\x0b6b\x0b68.com", false},
    {"xn--zjca.com", L"\x0b66\x0b66.com", false},
    // Gujarati:
    {"xn--cgce.com", L"\x0ae6\x0ae8.com", false},
    {"xn--fgci.com", L"\x0ae9\x0aed.com", false},
    {"xn--dgca.com", L"\x0ae7\x0ae7.com", false},

    // wmhtb.com
    {"xn--l1acpvx.com", L"\x0448\x043c\x043d\x0442\x044c.com", false},
    // щмнть.com
    {"xn--l1acpzs.com", L"\x0449\x043c\x043d\x0442\x044c.com", false},
    // шмнтв.com
    {"xn--b1atdu1a.com", L"\x0448\x043c\x043d\x0442\x0432.com", false},
    // шмԋтв.com
    {"xn--b1atsw09g.com", L"\x0448\x043c\x050b\x0442\x0432.com", false},
    // шмԧтв.com
    {"xn--b1atsw03i.com", L"\x0448\x043c\x0527\x0442\x0432.com", false},
    // шмԋԏв.com
    {"xn--b1at9a12dua.com", L"\x0448\x043c\x050b\x050f\x0432.com", false},
    // ഠട345.com
    {"xn--345-jtke.com",
     L"\x0d20\x0d1f"
     L"345.com",
     false},

    // Test additional confusable LGC characters (most of them without
    // decomposition into base + diacritc mark). The corresponding ASCII
    // domain names are in the test top domain list.
    // ϼκαωχ.com
    {"xn--mxar4bh6w.com", L"\x03fc\x03ba\x03b1\x03c9\x03c7.com", false},
    // þħĸŧƅ.com
    {"xn--vda6f3b2kpf.com", L"\x00fe\x0127\x0138\x0167\x0185.com", false},
    // þhktb.com
    {"xn--hktb-9ra.com", L"\x00fehktb.com", false},
    // pħktb.com
    {"xn--pktb-5xa.com", L"p\x0127ktb.com", false},
    // phĸtb.com
    {"xn--phtb-m0a.com", L"ph\x0138tb.com", false},
    // phkŧb.com
    {"xn--phkb-d7a.com",
     L"phk\x0167"
     L"b.com",
     false},
    // phktƅ.com
    {"xn--phkt-ocb.com", L"phkt\x0185.com", false},
    // ҏнкть.com
    {"xn--j1afq4bxw.com", L"\x048f\x043d\x043a\x0442\x044c.com", false},
    // ҏћкть.com
    {"xn--j1aq4a7cvo.com", L"\x048f\x045b\x043a\x0442\x044c.com", false},
    // ҏңкть.com
    {"xn--j1aq4azund.com", L"\x048f\x04a3\x043a\x0442\x044c.com", false},
    // ҏҥкть.com
    {"xn--j1aq4azuxd.com", L"\x048f\x04a5\x043a\x0442\x044c.com", false},
    // ҏӈкть.com
    {"xn--j1aq4azuyj.com", L"\x048f\x04c8\x043a\x0442\x044c.com", false},
    // ҏԧкть.com
    {"xn--j1aq4azu9z.com", L"\x048f\x0527\x043a\x0442\x044c.com", false},
    // ҏԩкть.com
    {"xn--j1aq4azuq0a.com", L"\x048f\x0529\x043a\x0442\x044c.com", false},
    // ҏнқть.com
    {"xn--m1ak4azu6b.com", L"\x048f\x043d\x049b\x0442\x044c.com", false},
    // ҏнҝть.com
    {"xn--m1ak4azunc.com", L"\x048f\x043d\x049d\x0442\x044c.com", false},
    // ҏнҟть.com
    {"xn--m1ak4azuxc.com", L"\x048f\x043d\x049f\x0442\x044c.com", false},
    // ҏнҡть.com
    {"xn--m1ak4azu7c.com", L"\x048f\x043d\x04a1\x0442\x044c.com", false},
    // ҏнӄть.com
    {"xn--m1ak4azu8i.com", L"\x048f\x043d\x04c4\x0442\x044c.com", false},
    // ҏнԟть.com
    {"xn--m1ak4azuzy.com", L"\x048f\x043d\x051f\x0442\x044c.com", false},
    // ҏнԟҭь.com
    {"xn--m1a4a4nnery.com", L"\x048f\x043d\x051f\x04ad\x044c.com", false},
    // ҏнԟҭҍ.com
    {"xn--m1a4ne5jry.com", L"\x048f\x043d\x051f\x04ad\x048d.com", false},
    // ҏнԟҭв.com
    {"xn--b1av9v8dry.com", L"\x048f\x043d\x051f\x04ad\x0432.com", false},
    // ҏӊԟҭв.com
    {"xn--b1a9p8c1e8r.com", L"\x048f\x04ca\x051f\x04ad\x0432.com", false},
    // wmŋr.com
    {"xn--wmr-jxa.com", L"wm\x014br.com", false},
    // шмпґ.com
    {"xn--l1agz80a.com", L"\x0448\x043c\x043f\x0491.com", false},
    // щмпґ.com
    {"xn--l1ag2a0y.com", L"\x0449\x043c\x043f\x0491.com", false},
    // щӎпґ.com
    {"xn--o1at1tsi.com", L"\x0449\x04ce\x043f\x0491.com", false},
    // ґғ.com
    {"xn--03ae.com", L"\x0491\x0493.com", false},
    // ґӻ.com
    {"xn--03a6s.com", L"\x0491\x04fb.com", false},
    // ҫұҳҽ.com
    {"xn--r4amg4b.com", L"\x04ab\x04b1\x04b3\x04bd.com", false},
    // ҫұӽҽ.com
    {"xn--r4am0b8r.com", L"\x04ab\x04b1\x04fd\x04bd.com", false},
    // ҫұӿҽ.com
    {"xn--r4am0b3s.com", L"\x04ab\x04b1\x04ff\x04bd.com", false},
    // ҫұӿҿ.com
    {"xn--r4am6b4p.com", L"\x04ab\x04b1\x04ff\x04bf.com", false},
    // ҫұӿє.com
    {"xn--91a7osa62a.com", L"\x04ab\x04b1\x04ff\x0454.com", false},
    // ӏԃԍ.com
    {"xn--s5a8h4a.com", L"\x04cf\x0503\x050d.com", false},

    // U+04CF(ӏ) is mapped to multiple characters, lowercase L(l) and
    // lowercase I(i). Lowercase L is also regarded as similar to digit 1.
    // The test domain list has {ig, ld, 1gd}.com for Cyrillic.
    // ӏԍ.com
    {"xn--s5a8j.com", L"\x04cf\x050d.com", false},
    // ӏԃ.com
    {"xn--s5a8h.com", L"\x04cf\x0503.com", false},
    // ӏԍԃ.com
    {"xn--s5a8h3a.com", L"\x04cf\x050d\x0503.com", false},

    // ꓲ2345б7890.com
    {"xn--23457890-e7g93622b.com",
     L"\xa4f2"
     L"2345\x0431"
     L"7890.com",
     false},
    // 1ᒿ345б7890.com
    {"xn--13457890-e7g0943b.com",
     L"1\x14bf"
     L"345\x0431"
     L"7890.com",
     false},
    // 12з4567890.com
    {"xn--124567890-10h.com",
     L"12\x0437"
     L"4567890.com",
     false},
    // 12ҙ4567890.com
    {"xn--124567890-1ti.com",
     L"12\x0499"
     L"4567890.com",
     false},
    // 12ӡ4567890.com
    {"xn--124567890-mfj.com",
     L"12\x04e1"
     L"4567890.com",
     false},
    // 12उ4567890.com
    {"xn--124567890-m3r.com",
     L"12\u0909"
     L"4567890.com",
     false},
    // 12ও4567890.com
    {"xn--124567890-17s.com",
     L"12\u0993"
     L"4567890.com",
     false},
    // 12ਤ4567890.com
    {"xn--124567890-hfu.com",
     L"12\u0a24"
     L"4567890.com",
     false},
    // 12ဒ4567890.com
    {"xn--124567890-6s6a.com",
     L"12\x1012"
     L"4567890.com",
     false},
    // 12ვ4567890.com
    {"xn--124567890-we8a.com",
     L"12\x10D5"
     L"4567890.com",
     false},
    // 12პ4567890.com
    {"xn--124567890-hh8a.com",
     L"12\x10DE"
     L"4567890.com",
     false},
    // 123Ꮞ567890.com
    {"xn--123567890-dm4b.com",
     L"123\x13ce"
     L"567890.com",
     false},
    // 12345б7890.com
    {"xn--123457890-fzh.com",
     L"12345\x0431"
     L"7890.com",
     false},
    // 1234567ȣ90.com
    {"xn--123456790-6od.com",
     L"1234567\x0223"
     L"90.com",
     false},
    // 12345678୨0.com
    {"xn--123456780-71w.com",
     L"12345678\x0b68"
     L"0.com",
     false},
    // 123456789ଠ.com
    {"xn--http://123456789-v01b.com", L"http://123456789\x0b20.com", false},
    // 123456789ꓳ.com
    {"xn--123456789-tx75a.com", L"123456789\xa4f3.com", false},

    // aeœ.com
    {"xn--ae-fsa.com", L"ae\x0153.com", false},
    // æce.com
    {"xn--ce-0ia.com",
     L"\x00e6"
     L"ce.com",
     false},
    // æœ.com
    {"xn--6ca2t.com", L"\x00e6\x0153.com", false},
    // ӕԥ.com
    {"xn--y5a4n.com", L"\x04d5\x0525.com", false},

    // ငၔဌ၂ဝ.com (entirely made of Myanmar characters)
    {"xn--ridq5c9hnd.com",
     L"\x1004\x1054\x100c"
     L"\x1042\x101d.com",
     false},

    // ฟรฟร.com (made of two Thai characters. similar to wsws.com in
    // some fonts)
    {"xn--w3calb.com", L"\x0e1f\x0e23\x0e1f\x0e23.com", false},
    // พรบ.com
    {"xn--r3chp.com", L"\x0e1e\x0e23\x0e1a.com", false},
    // ฟรบ.com
    {"xn--r3cjm.com", L"\x0e1f\x0e23\x0e1a.com", false},

    // Lao characters that look like w, s, o, and u.
    // ພຣບ.com
    {"xn--f7chp.com", L"\x0e9e\x0ea3\x0e9a.com", false},
    // ຟຣບ.com
    {"xn--f7cjm.com", L"\x0e9f\x0ea3\x0e9a.com", false},
    // ຟຮບ.com
    {"xn--f7cj9b.com", L"\x0e9f\x0eae\x0e9a.com", false},
    // ຟຮ໐ບ.com
    {"xn--f7cj9b5h.com",
     L"\x0e9f\x0eae"
     L"\x0ed0\x0e9a.com",
     false},

    // Lao character that looks like n.
    // ก11.com
    {"xn--11-lqi.com",
     L"\x0e01"
     L"11.com",
     false},

    // At one point the skeleton of 'w' was 'vv', ensure that
    // that it's treated as 'w'.
    {"xn--wder-qqa.com",
     L"w\x00f3"
     L"der.com",
     false},

    // Mixed digits: the first two will also fail mixed script test
    // Latin + ASCII digit + Deva digit
    {"xn--asc1deva-j0q.co.in", L"asc1deva\x0967.co.in", false},
    // Latin + Deva digit + Beng digit
    {"xn--devabeng-f0qu3f.co.in",
     L"deva\x0967"
     L"beng\x09e7.co.in",
     false},
    // ASCII digit + Deva digit
    {"xn--79-v5f.co.in",
     L"7\x09ea"
     L"9.co.in",
     false},
    //  Deva digit + Beng digit
    {"xn--e4b0x.co.in", L"\x0967\x09e7.co.in", false},
    // U+4E00 (CJK Ideograph One) is not a digit
    {"xn--d12-s18d.cn", L"d12\x4e00.cn", true},
    // One that's really long that will force a buffer realloc
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaa",
     L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     L"aaaaaaaa",
     true},

    // Not allowed; characters outside [:Identifier_Status=Allowed:]
    // Limited Use Scripts: UTS 31 Table 7.
    // Vai
    {"xn--sn8a.com", L"\xa50b.com", false},
    // 'CARD' look-alike in Cherokee
    {"xn--58db0a9q.com", L"\x13df\x13aa\x13a1\x13a0.com", false},
    // Scripts excluded from Identifiers: UTS 31 Table 4
    // Coptic
    {"xn--5ya.com", L"\x03e7.com", false},
    // Old Italic
    {"xn--097cc.com", L"\U00010300\U00010301.com", false},

    // U+115F (Hangul Filler)
    {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr", false},
    {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com", false},
    // Latin small capital w: hardᴡare.com
    {"xn--hardare-l41c.com",
     L"hard\x1d21"
     L"are.com",
     false},
    // Minus Sign(U+2212)
    {"xn--t9g238xc2a.jp", L"\x65e5\x2212\x672c.jp", false},
    // Latin Small Letter Script G: ɡɡ.com
    {"xn--0naa.com", L"\x0261\x0261.com", false},
    // Hangul Jamo(U+11xx)
    {"xn--0pdc3b.com", L"\x1102\x1103\x1110.com", false},
    // degree sign: 36°c.com
    {"xn--36c-tfa.com",
     L"36\x00b0"
     L"c.com",
     false},
    // Pound sign
    {"xn--5free-9ga.com", L"5free\x00a8.com", false},
    // Hebrew points (U+05B0, U+05B6)
    {"xn--7cbl2kc2a.com", L"\x05e1\x05b6\x05e7\x05b0\x05e1.com", false},
    // Danda(U+0964)
    {"xn--81bp1b6ch8s.com", L"\x0924\x093f\x091c\x0964\x0930\x0940.com", false},
    // Small letter script G(U+0261)
    {"xn--oogle-qmc.com", L"\x0261oogle.com", false},
    // Small Katakana Extension(U+31F1)
    {"xn--wlk.com", L"\x31f1.com", false},
    // Heart symbol: ♥
    {"xn--ab-u0x.com", L"ab\x2665.com", false},
    // Emoji
    {"xn--vi8hiv.xyz", L"\U0001f355\U0001f4a9.xyz", false},
    // Registered trade mark
    {"xn--egistered-fna.com",
     L"\x00ae"
     L"egistered.com",
     false},
    // Latin Letter Retroflex Click
    {"xn--registered-25c.com", L"registered\x01c3.com", false},
    // ASCII '!' not allowed in IDN
    {"xn--!-257eu42c.kr", L"\xc548\xb155!.kr", false},
    // 'GOOGLE' in IPA extension: ɢᴏᴏɢʟᴇ
    {"xn--1naa7pn51hcbaa.com", L"\x0262\x1d0f\x1d0f\x0262\x029f\x1d07.com",
     false},
    // Padlock icon spoof.
    {"xn--google-hj64e", L"\U0001f512google.com", false},

    // Custom black list
    // Combining Long Solidus Overlay
    {"google.xn--comabc-k8d",
     L"google.com\x0338"
     L"abc",
     false},
    // Hyphenation Point instead of Katakana Middle dot
    {"xn--svgy16dha.jp", L"\x30a1\x2027\x30a3.jp", false},
    // Gershayim with other Hebrew characters is allowed.
    {"xn--5db6bh9b.il", L"\x05e9\x05d1\x05f4\x05e6.il", true},
    // Hebrew Gershayim with Latin is disallowed.
    {"xn--ab-yod.com",
     L"a\x05f4"
     L"b.com",
     false},
    // Hebrew Gershayim with Arabic is disallowed.
    {"xn--5eb7h.eg", L"\x0628\x05f4.eg", false},
#if defined(OS_MACOSX)
    // These characters are blocked due to a font issue on Mac.
    // Tibetan transliteration characters.
    {"xn--com-luma.test.pl", L"\u0f8c.test.pl", false},
    // Arabic letter KASHMIRI YEH
    {"xn--fgb.com", L"\u0620.com", false},
#endif

    // Hyphens (http://unicode.org/cldr/utility/confusables.jsp?a=-)
    // Hyphen-Minus (the only hyphen allowed)
    // abc-def
    {"abc-def.com", L"abc-def.com", true},
    // Modifier Letter Minus Sign
    {"xn--abcdef-5od.com",
     L"abc\x02d7"
     L"def.com",
     false},
    // Hyphen
    {"xn--abcdef-dg0c.com",
     L"abc\x2010"
     L"def.com",
     false},
    // Non-Breaking Hyphen
    // This is actually an invalid IDNA domain (U+2011 normalizes to U+2010),
    // but
    // it is included to ensure that we do not inadvertently allow this
    // character
    // to be displayed as Unicode.
    {"xn--abcdef-kg0c.com",
     L"abc\x2011"
     L"def.com",
     false},
    // Figure Dash
    {"xn--abcdef-rg0c.com",
     L"abc\x2012"
     L"def.com",
     false},
    // En Dash
    {"xn--abcdef-yg0c.com",
     L"abc\x2013"
     L"def.com",
     false},
    // Hyphen Bullet
    {"xn--abcdef-kq0c.com",
     L"abc\x2043"
     L"def.com",
     false},
    // Minus Sign
    {"xn--abcdef-5d3c.com",
     L"abc\x2212"
     L"def.com",
     false},
    // Heavy Minus Sign
    {"xn--abcdef-kg1d.com",
     L"abc\x2796"
     L"def.com",
     false},
    // Coptic Capital Letter Dialect-P Ni
    {"xn--abcdef-yy8d.com",
     L"abc\x2cba"
     L"def.com",
     false},
    // Small Em Dash
    {"xn--abcdef-5g0c.com",
     L"abc\xfe58"
     L"def.com",
     false},

    // Block NV8 (Not valid in IDN 2008) characters.
    // U+058A (֊)
    {"xn--ab-vfd.com",
     L"a\x058a"
     L"b.com",
     false},
    {"xn--y9ac3j.com", L"\x0561\x058a\x0562.com", false},
    // U+2019 (’)
    {"xn--ab-n2t.com",
     L"a\x2019"
     L"b.com",
     false},
    // U+2027 (‧)
    {"xn--ab-u3t.com",
     L"a\x2027"
     L"b.com",
     false},
    // U+30A0 (゠)
    {"xn--ab-bg4a.com",
     L"a\x30a0"
     L"b.com",
     false},
    {"xn--9bk3828aea.com", L"\xac00\x30a0\xac01.com", false},
    {"xn--9bk279fba.com", L"\x4e00\x30a0\x4e00.com", false},
    {"xn--n8jl2x.com", L"\x304a\x30a0\x3044.com", false},
    {"xn--fbke7f.com", L"\x3082\x30a0\x3084.com", false},

    // Block single/double-quote-like characters.
    // U+02BB (ʻ)
    {"xn--ab-8nb.com",
     L"a\x02bb"
     L"b.com",
     false},
    // U+02BC (ʼ)
    {"xn--ab-cob.com",
     L"a\x02bc"
     L"b.com",
     false},
    // U+144A: Not allowed to mix with scripts other than Canadian Syllabics.
    {"xn--ab-jom.com",
     L"a\x144a"
     L"b.com",
     false},
    {"xn--xcec9s.com", L"\x1401\x144a\x1402.com", false},

    // Custom dangerous patterns
    // Two Katakana-Hiragana combining mark in a row
    {"google.xn--com-oh4ba.evil.jp", L"google.com\x309a\x309a.evil.jp", false},
    // Katakana Letter No not enclosed by {Han,Hiragana,Katakana}.
    {"google.xn--comevil-v04f.jp",
     L"google.com\x30ce"
     L"evil.jp",
     false},
    // TODO(jshin): Review the danger of allowing the following two.
    // Hiragana 'No' by itself is allowed.
    {"xn--ldk.jp", L"\x30ce.jp", true},
    // Hebrew Gershayim used by itself is allowed.
    {"xn--5eb.il", L"\x05f4.il", true},

    // Block RTL nonspacing marks (NSM) after unrelated scripts.
    {"xn--foog-ycg.com", L"foog\x0650.com", false},    // Latin + Arabic NSM
    {"xn--foog-jdg.com", L"foog\x0654.com", false},    // Latin + Arabic NSM
    {"xn--foog-jhg.com", L"foog\x0670.com", false},    // Latin + Arbic NSM
    {"xn--foog-opf.com", L"foog\x05b4.com", false},    // Latin + Hebrew NSM
    {"xn--shb5495f.com", L"\xac00\x0650.com", false},  // Hang + Arabic NSM

    // 4 Deviation characters between IDNA 2003 and IDNA 2008
    // When entered in Unicode, the first two are mapped to 'ss' and Greek sigma
    // and the latter two are mapped away. However, the punycode form should
    // remain in punycode.
    // U+00DF(sharp-s)
    {"xn--fu-hia.de", L"fu\x00df.de", false},
    // U+03C2(final-sigma)
    {"xn--mxac2c.gr", L"\x03b1\x03b2\x03c2.gr", false},
    // U+200C(ZWNJ)
    {"xn--h2by8byc123p.in", L"\x0924\x094d\x200c\x0930\x093f.in", false},
    // U+200C(ZWJ)
    {"xn--11b6iy14e.in", L"\x0915\x094d\x200d.in", false},

    // Math Monospace Small A. When entered in Unicode, it's canonicalized to
    // 'a'. The punycode form should remain in punycode.
    {"xn--bc-9x80a.xyz",
     L"\U0001d68a"
     L"bc.xyz",
     false},
    // Math Sans Bold Capital Alpha
    {"xn--bc-rg90a.xyz",
     L"\U0001d756"
     L"bc.xyz",
     false},
    // U+3000 is canonicalized to a space(U+0020), but the punycode form
    // should remain in punycode.
    {"xn--p6j412gn7f.cn", L"\x4e2d\x56fd\x3000", false},
    // U+3002 is canonicalized to ASCII fullstop(U+002E), but the punycode form
    // should remain in punycode.
    {"xn--r6j012gn7f.cn", L"\x4e2d\x56fd\x3002", false},
    // Invalid punycode
    // Has a codepoint beyond U+10FFFF.
    {"xn--krank-kg706554a", nullptr, false},
    // '?' in punycode.
    {"xn--hello?world.com", nullptr, false},

    // Not allowed in UTS46/IDNA 2008
    // Georgian Capital Letter(U+10BD)
    {"xn--1nd.com", L"\x10bd.com", false},
    // 3rd and 4th characters are '-'.
    {"xn-----8kci4dhsd", L"\x0440\x0443--\x0430\x0432\x0442\x043e", false},
    // Leading combining mark
    {"xn--72b.com", L"\x093e.com", false},
    // BiDi check per IDNA 2008/UTS 46
    // Cannot starts with AN(Arabic-Indic Number)
    {"xn--8hbae.eg", L"\x0662\x0660\x0660.eg", false},
    // Cannot start with a RTL character and ends with a LTR
    {"xn--x-ymcov.eg", L"\x062c\x0627\x0631x.eg", false},
    // Can start with a RTL character and ends with EN(European Number)
    {"xn--2-ymcov.eg",
     L"\x062c\x0627\x0631"
     L"2.eg",
     true},
    // Can start with a RTL and end with AN
    {"xn--mgbjq0r.eg", L"\x062c\x0627\x0631\x0662.eg", true},

    // Extremely rare Latin letters
    // Latin Ext B - Pinyin: ǔnion.com
    {"xn--nion-unb.com", L"\x01d4nion.com", false},
    // Latin Ext C: ⱴase.com
    {"xn--ase-7z0b.com",
     L"\x2c74"
     L"ase.com",
     false},
    // Latin Ext D: ꝴode.com
    {"xn--ode-ut3l.com", L"\xa774ode.com", false},
    // Latin Ext Additional: ḷily.com
    {"xn--ily-n3y.com", L"\x1e37ily.com", false},
    // Latin Ext E: ꬺove.com
    {"xn--ove-8y6l.com", L"\xab3aove.com", false},
    // Greek Ext: ᾳβγ.com
    {"xn--nxac616s.com", L"\x1fb3\x03b2\x03b3.com", false},
    // Cyrillic Ext A
    {"xn--lrj.com", L"\x2def.com", false},
    // Cyrillic Ext B: ꙡ.com
    {"xn--kx8a.com", L"\xa661.com", false},
    // Cyrillic Ext C: ᲂ.com (Narrow o)
    {"xn--43f.com", L"\x1c82.com", false},

    // The skeleton of Extended Arabic-Indic Digit Zero (۰) is a dot. Check that
    // this is handled correctly (crbug/877045).
    {"xn--dmb", L"\x06f0", true},

    // Test that top domains whose skeletons are the same as the domain name are
    // handled properly. In this case, tést.net should match test.net top
    // domain and not be converted to unicode.
    {"xn--tst-bma.net", L"t\x00e9st.net", false},
    // Variations of the above, for testing crbug.com/925199.
    // some.tést.net should match test.net.
    {"some.xn--tst-bma.net", L"some.t\x00e9st.net", false},
    // The following should not match test.net, so should be converted to
    // unicode.
    // ést.net (a suffix of tést.net).
    {"xn--st-9ia.net", L"\x00e9st.net", true},
    // some.ést.net
    {"some.xn--st-9ia.net", L"some.\x00e9st.net", true},
    // atést.net (tést.net is a suffix of atést.net)
    {"xn--atst-cpa.net", L"at\x00e9st.net", true},
    // some.atést.net
    {"some.xn--atst-cpa.net", L"some.at\x00e9st.net", true},

    // Modifier-letter-voicing should be blocked (wwwˬtest.com).
    {"xn--wwwtest-2be.com", L"www\x02ectest.com", false},

    // oĸ.com: Not a top domain, should be blocked because of Kra.
    {"xn--o-tka.com", L"o\x0138.com", false},

    // U+4E00 and U+3127 should be blocked when next to non-CJK.
    {"xn--ipaddress-w75n.com", L"ip一address.com", false},
    {"xn--ipaddress-wx5h.com", L"ipㄧaddress.com", false},
    // These are allowed because 一 is not immediately next to non-CJK.
    {"xn--gamer-fg1hz05u.com", L"一生gamer.com", true},
    {"xn--gamer-kg1hy05u.com", L"gamer生一.com", true},
    {"xn--4gqz91g.com", L"一猫.com", true},
    {"xn--4fkv10r.com", L"ㄧ猫.com", true},
    // U+4E00 with another ideograph.
    {"xn--4gqc.com", L"一丁.com", true},

    // CJK ideographs looking like slashes should be blocked when next to
    // non-CJK.
    {"example.xn--comtest-k63k", L"example.com丶test", false},
    {"example.xn--comtest-u83k", L"example.com乀test", false},
    {"example.xn--comtest-283k", L"example.com乁test", false},
    {"example.xn--comtest-m83k", L"example.com丿test", false},
    // This is allowed because the ideographs are not immediately next to
    // non-CJK.
    {"xn--oiqsace.com", L"丶乀乁丿.com", true},

    // Kana voiced sound marks are not allowed.
    {"xn--google-1m4e.com", L"google\x3099.com", false},
    {"xn--google-8m4e.com", L"google\x309A.com", false},
};

namespace test {
#include "components/url_formatter/top_domains/test_domains-trie-inc.cc"
}

}  // namespace

TEST(IDNSpoofCheckerTest, IDNToUnicode) {
  IDNSpoofChecker::HuffmanTrieParams trie_params{
      test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
      test::kTopDomainsTrie, test::kTopDomainsTrieBits,
      test::kTopDomainsRootPosition};
  IDNSpoofChecker::SetTrieParamsForTesting(trie_params);

  for (size_t i = 0; i < base::size(kIdnCases); i++) {
    base::string16 output(IDNToUnicode(kIdnCases[i].input));
    base::string16 expected(kIdnCases[i].unicode_allowed
                                ? WideToUTF16(kIdnCases[i].unicode_output)
                                : ASCIIToUTF16(kIdnCases[i].input));
    EXPECT_EQ(expected, output)
        << "input # " << i << ": \"" << kIdnCases[i].input << "\"";
  }
  IDNSpoofChecker::RestoreTrieParamsForTesting();
}

// Check the unsafe version of IDNToUnicode. Even though the input domain
// matches a top domain, it should still be converted to unicode.
TEST(IDNSpoofCheckerTest, UnsafeIDNToUnicodeWithDetails) {
  const struct TestCase {
    // The IDNA/Punycode version of the domain (plain ASCII).
    const char* const punycode;
    // The equivalent Unicode version of the domain, if converted.
    const wchar_t* const expected_unicode;
    // Whether the input (punycode) has idn.
    const bool expected_has_idn;
    // The top domain that |punycode| matched to, if any.
    const char* const expected_matching_domain;
  } kTestCases[] = {
      {// An ASCII, top domain.
       "google.com", L"google.com", false,
       // Since it's not unicode, we won't attempt to match it to a top domain.
       ""},
      {// An ASCII domain that's not a top domain.
       "not-top-domain.com", L"not-top-domain.com", false, ""},
      {// A unicode domain that's valid according to all of the rules in IDN
       // spoof checker except that it matches a top domain. Should be
       // converted to punycode.
       "xn--googl-fsa.com", L"googlé.com", true, "google.com"},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (mixed script) and it matches a top domain. Should be
       // converted to punycode.
       "xn--80ak6aa92e.com", L"аррӏе.com", true, "apple.com"},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (mixed script) but it doesn't match a top domain.
       "xn--o-o-oai-26a223aia177a7ab7649d.com", L"ɴoτ-τoρ-ďoᛖaiɴ.com", true,
       ""},
  };

  for (const TestCase& test_case : kTestCases) {
    const url_formatter::IDNConversionResult result =
        UnsafeIDNToUnicodeWithDetails(test_case.punycode);
    EXPECT_EQ(base::WideToUTF16(test_case.expected_unicode), result.result);
    EXPECT_EQ(test_case.expected_has_idn, result.has_idn_component);
    EXPECT_EQ(test_case.expected_matching_domain, result.matching_top_domain);
  }
}

}  // namespace url_formatter
