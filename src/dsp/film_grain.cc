// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/film_grain.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/film_grain_impl.h"
#include "src/utils/array_2d.h"
#include "src/utils/common.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/logging.h"

namespace libgav1 {
namespace dsp {
namespace film_grain {

namespace {

// The kGaussianSequence array contains random samples from a Gaussian
// distribution with zero mean and standard deviation of about 512 clipped to
// the range of [-2048, 2047] (representable by a signed integer using 12 bits
// of precision) and rounded to the nearest multiple of 4.
//
// Note: It is important that every element in the kGaussianSequence array be
// less than 2040, so that RightShiftWithRounding(kGaussianSequence[i], 4) is
// less than 128 for bitdepth=8 (GrainType=int8_t).
constexpr int16_t kGaussianSequence[/*2048*/] = {
    56,    568,   -180,  172,   124,   -84,   172,   -64,   -900,  24,   820,
    224,   1248,  996,   272,   -8,    -916,  -388,  -732,  -104,  -188, 800,
    112,   -652,  -320,  -376,  140,   -252,  492,   -168,  44,    -788, 588,
    -584,  500,   -228,  12,    680,   272,   -476,  972,   -100,  652,  368,
    432,   -196,  -720,  -192,  1000,  -332,  652,   -136,  -552,  -604, -4,
    192,   -220,  -136,  1000,  -52,   372,   -96,   -624,  124,   -24,  396,
    540,   -12,   -104,  640,   464,   244,   -208,  -84,   368,   -528, -740,
    248,   -968,  -848,  608,   376,   -60,   -292,  -40,   -156,  252,  -292,
    248,   224,   -280,  400,   -244,  244,   -60,   76,    -80,   212,  532,
    340,   128,   -36,   824,   -352,  -60,   -264,  -96,   -612,  416,  -704,
    220,   -204,  640,   -160,  1220,  -408,  900,   336,   20,    -336, -96,
    -792,  304,   48,    -28,   -1232, -1172, -448,  104,   -292,  -520, 244,
    60,    -948,  0,     -708,  268,   108,   356,   -548,  488,   -344, -136,
    488,   -196,  -224,  656,   -236,  -1128, 60,    4,     140,   276,  -676,
    -376,  168,   -108,  464,   8,     564,   64,    240,   308,   -300, -400,
    -456,  -136,  56,    120,   -408,  -116,  436,   504,   -232,  328,  844,
    -164,  -84,   784,   -168,  232,   -224,  348,   -376,  128,   568,  96,
    -1244, -288,  276,   848,   832,   -360,  656,   464,   -384,  -332, -356,
    728,   -388,  160,   -192,  468,   296,   224,   140,   -776,  -100, 280,
    4,     196,   44,    -36,   -648,  932,   16,    1428,  28,    528,  808,
    772,   20,    268,   88,    -332,  -284,  124,   -384,  -448,  208,  -228,
    -1044, -328,  660,   380,   -148,  -300,  588,   240,   540,   28,   136,
    -88,   -436,  256,   296,   -1000, 1400,  0,     -48,   1056,  -136, 264,
    -528,  -1108, 632,   -484,  -592,  -344,  796,   124,   -668,  -768, 388,
    1296,  -232,  -188,  -200,  -288,  -4,    308,   100,   -168,  256,  -500,
    204,   -508,  648,   -136,  372,   -272,  -120,  -1004, -552,  -548, -384,
    548,   -296,  428,   -108,  -8,    -912,  -324,  -224,  -88,   -112, -220,
    -100,  996,   -796,  548,   360,   -216,  180,   428,   -200,  -212, 148,
    96,    148,   284,   216,   -412,  -320,  120,   -300,  -384,  -604, -572,
    -332,  -8,    -180,  -176,  696,   116,   -88,   628,   76,    44,   -516,
    240,   -208,  -40,   100,   -592,  344,   -308,  -452,  -228,  20,   916,
    -1752, -136,  -340,  -804,  140,   40,    512,   340,   248,   184,  -492,
    896,   -156,  932,   -628,  328,   -688,  -448,  -616,  -752,  -100, 560,
    -1020, 180,   -800,  -64,   76,    576,   1068,  396,   660,   552,  -108,
    -28,   320,   -628,  312,   -92,   -92,   -472,  268,   16,    560,  516,
    -672,  -52,   492,   -100,  260,   384,   284,   292,   304,   -148, 88,
    -152,  1012,  1064,  -228,  164,   -376,  -684,  592,   -392,  156,  196,
    -524,  -64,   -884,  160,   -176,  636,   648,   404,   -396,  -436, 864,
    424,   -728,  988,   -604,  904,   -592,  296,   -224,  536,   -176, -920,
    436,   -48,   1176,  -884,  416,   -776,  -824,  -884,  524,   -548, -564,
    -68,   -164,  -96,   692,   364,   -692,  -1012, -68,   260,   -480, 876,
    -1116, 452,   -332,  -352,  892,   -1088, 1220,  -676,  12,    -292, 244,
    496,   372,   -32,   280,   200,   112,   -440,  -96,   24,    -644, -184,
    56,    -432,  224,   -980,  272,   -260,  144,   -436,  420,   356,  364,
    -528,  76,    172,   -744,  -368,  404,   -752,  -416,  684,   -688, 72,
    540,   416,   92,    444,   480,   -72,   -1416, 164,   -1172, -68,  24,
    424,   264,   1040,  128,   -912,  -524,  -356,  64,    876,   -12,  4,
    -88,   532,   272,   -524,  320,   276,   -508,  940,   24,    -400, -120,
    756,   60,    236,   -412,  100,   376,   -484,  400,   -100,  -740, -108,
    -260,  328,   -268,  224,   -200,  -416,  184,   -604,  -564,  -20,  296,
    60,    892,   -888,  60,    164,   68,    -760,  216,   -296,  904,  -336,
    -28,   404,   -356,  -568,  -208,  -1480, -512,  296,   328,   -360, -164,
    -1560, -776,  1156,  -428,  164,   -504,  -112,  120,   -216,  -148, -264,
    308,   32,    64,    -72,   72,    116,   176,   -64,   -272,  460,  -536,
    -784,  -280,  348,   108,   -752,  -132,  524,   -540,  -776,  116,  -296,
    -1196, -288,  -560,  1040,  -472,  116,   -848,  -1116, 116,   636,  696,
    284,   -176,  1016,  204,   -864,  -648,  -248,  356,   972,   -584, -204,
    264,   880,   528,   -24,   -184,  116,   448,   -144,  828,   524,  212,
    -212,  52,    12,    200,   268,   -488,  -404,  -880,  824,   -672, -40,
    908,   -248,  500,   716,   -576,  492,   -576,  16,    720,   -108, 384,
    124,   344,   280,   576,   -500,  252,   104,   -308,  196,   -188, -8,
    1268,  296,   1032,  -1196, 436,   316,   372,   -432,  -200,  -660, 704,
    -224,  596,   -132,  268,   32,    -452,  884,   104,   -1008, 424,  -1348,
    -280,  4,     -1168, 368,   476,   696,   300,   -8,    24,    180,  -592,
    -196,  388,   304,   500,   724,   -160,  244,   -84,   272,   -256, -420,
    320,   208,   -144,  -156,  156,   364,   452,   28,    540,   316,  220,
    -644,  -248,  464,   72,    360,   32,    -388,  496,   -680,  -48,  208,
    -116,  -408,  60,    -604,  -392,  548,   -840,  784,   -460,  656,  -544,
    -388,  -264,  908,   -800,  -628,  -612,  -568,  572,   -220,  164,  288,
    -16,   -308,  308,   -112,  -636,  -760,  280,   -668,  432,   364,  240,
    -196,  604,   340,   384,   196,   592,   -44,   -500,  432,   -580, -132,
    636,   -76,   392,   4,     -412,  540,   508,   328,   -356,  -36,  16,
    -220,  -64,   -248,  -60,   24,    -192,  368,   1040,  92,    -24,  -1044,
    -32,   40,    104,   148,   192,   -136,  -520,  56,    -816,  -224, 732,
    392,   356,   212,   -80,   -424,  -1008, -324,  588,   -1496, 576,  460,
    -816,  -848,  56,    -580,  -92,   -1372, -112,  -496,  200,   364,  52,
    -140,  48,    -48,   -60,   84,    72,    40,    132,   -356,  -268, -104,
    -284,  -404,  732,   -520,  164,   -304,  -540,  120,   328,   -76,  -460,
    756,   388,   588,   236,   -436,  -72,   -176,  -404,  -316,  -148, 716,
    -604,  404,   -72,   -88,   -888,  -68,   944,   88,    -220,  -344, 960,
    472,   460,   -232,  704,   120,   832,   -228,  692,   -508,  132,  -476,
    844,   -748,  -364,  -44,   1116,  -1104, -1056, 76,    428,   552,  -692,
    60,    356,   96,    -384,  -188,  -612,  -576,  736,   508,   892,  352,
    -1132, 504,   -24,   -352,  324,   332,   -600,  -312,  292,   508,  -144,
    -8,    484,   48,    284,   -260,  -240,  256,   -100,  -292,  -204, -44,
    472,   -204,  908,   -188,  -1000, -256,  92,    1164,  -392,  564,  356,
    652,   -28,   -884,  256,   484,   -192,  760,   -176,  376,   -524, -452,
    -436,  860,   -736,  212,   124,   504,   -476,  468,   76,    -472, 552,
    -692,  -944,  -620,  740,   -240,  400,   132,   20,    192,   -196, 264,
    -668,  -1012, -60,   296,   -316,  -828,  76,    -156,  284,   -768, -448,
    -832,  148,   248,   652,   616,   1236,  288,   -328,  -400,  -124, 588,
    220,   520,   -696,  1032,  768,   -740,  -92,   -272,  296,   448,  -464,
    412,   -200,  392,   440,   -200,  264,   -152,  -260,  320,   1032, 216,
    320,   -8,    -64,   156,   -1016, 1084,  1172,  536,   484,   -432, 132,
    372,   -52,   -256,  84,    116,   -352,  48,    116,   304,   -384, 412,
    924,   -300,  528,   628,   180,   648,   44,    -980,  -220,  1320, 48,
    332,   748,   524,   -268,  -720,  540,   -276,  564,   -344,  -208, -196,
    436,   896,   88,    -392,  132,   80,    -964,  -288,  568,   56,   -48,
    -456,  888,   8,     552,   -156,  -292,  948,   288,   128,   -716, -292,
    1192,  -152,  876,   352,   -600,  -260,  -812,  -468,  -28,   -120, -32,
    -44,   1284,  496,   192,   464,   312,   -76,   -516,  -380,  -456, -1012,
    -48,   308,   -156,  36,    492,   -156,  -808,  188,   1652,  68,   -120,
    -116,  316,   160,   -140,  352,   808,   -416,  592,   316,   -480, 56,
    528,   -204,  -568,  372,   -232,  752,   -344,  744,   -4,    324,  -416,
    -600,  768,   268,   -248,  -88,   -132,  -420,  -432,  80,    -288, 404,
    -316,  -1216, -588,  520,   -108,  92,    -320,  368,   -480,  -216, -92,
    1688,  -300,  180,   1020,  -176,  820,   -68,   -228,  -260,  436,  -904,
    20,    40,    -508,  440,   -736,  312,   332,   204,   760,   -372, 728,
    96,    -20,   -632,  -520,  -560,  336,   1076,  -64,   -532,  776,  584,
    192,   396,   -728,  -520,  276,   -188,  80,    -52,   -612,  -252, -48,
    648,   212,   -688,  228,   -52,   -260,  428,   -412,  -272,  -404, 180,
    816,   -796,  48,    152,   484,   -88,   -216,  988,   696,   188,  -528,
    648,   -116,  -180,  316,   476,   12,    -564,  96,    476,   -252, -364,
    -376,  -392,  556,   -256,  -576,  260,   -352,  120,   -16,   -136, -260,
    -492,  72,    556,   660,   580,   616,   772,   436,   424,   -32,  -324,
    -1268, 416,   -324,  -80,   920,   160,   228,   724,   32,    -516, 64,
    384,   68,    -128,  136,   240,   248,   -204,  -68,   252,   -932, -120,
    -480,  -628,  -84,   192,   852,   -404,  -288,  -132,  204,   100,  168,
    -68,   -196,  -868,  460,   1080,  380,   -80,   244,   0,     484,  -888,
    64,    184,   352,   600,   460,   164,   604,   -196,  320,   -64,  588,
    -184,  228,   12,    372,   48,    -848,  -344,  224,   208,   -200, 484,
    128,   -20,   272,   -468,  -840,  384,   256,   -720,  -520,  -464, -580,
    112,   -120,  644,   -356,  -208,  -608,  -528,  704,   560,   -424, 392,
    828,   40,    84,    200,   -152,  0,     -144,  584,   280,   -120, 80,
    -556,  -972,  -196,  -472,  724,   80,    168,   -32,   88,    160,  -688,
    0,     160,   356,   372,   -776,  740,   -128,  676,   -248,  -480, 4,
    -364,  96,    544,   232,   -1032, 956,   236,   356,   20,    -40,  300,
    24,    -676,  -596,  132,   1120,  -104,  532,   -1096, 568,   648,  444,
    508,   380,   188,   -376,  -604,  1488,  424,   24,    756,   -220, -192,
    716,   120,   920,   688,   168,   44,    -460,  568,   284,   1144, 1160,
    600,   424,   888,   656,   -356,  -320,  220,   316,   -176,  -724, -188,
    -816,  -628,  -348,  -228,  -380,  1012,  -452,  -660,  736,   928,  404,
    -696,  -72,   -268,  -892,  128,   184,   -344,  -780,  360,   336,  400,
    344,   428,   548,   -112,  136,   -228,  -216,  -820,  -516,  340,  92,
    -136,  116,   -300,  376,   -244,  100,   -316,  -520,  -284,  -12,  824,
    164,   -548,  -180,  -128,  116,   -924,  -828,  268,   -368,  -580, 620,
    192,   160,   0,     -1676, 1068,  424,   -56,   -360,  468,   -156, 720,
    288,   -528,  556,   -364,  548,   -148,  504,   316,   152,   -648, -620,
    -684,  -24,   -376,  -384,  -108,  -920,  -1032, 768,   180,   -264, -508,
    -1268, -260,  -60,   300,   -240,  988,   724,   -376,  -576,  -212, -736,
    556,   192,   1092,  -620,  -880,  376,   -56,   -4,    -216,  -32,  836,
    268,   396,   1332,  864,   -600,  100,   56,    -412,  -92,   356,  180,
    884,   -468,  -436,  292,   -388,  -804,  -704,  -840,  368,   -348, 140,
    -724,  1536,  940,   372,   112,   -372,  436,   -480,  1136,  296,  -32,
    -228,  132,   -48,   -220,  868,   -1016, -60,   -1044, -464,  328,  916,
    244,   12,    -736,  -296,  360,   468,   -376,  -108,  -92,   788,  368,
    -56,   544,   400,   -672,  -420,  728,   16,    320,   44,    -284, -380,
    -796,  488,   132,   204,   -596,  -372,  88,    -152,  -908,  -636, -572,
    -624,  -116,  -692,  -200,  -56,   276,   -88,   484,   -324,  948,  864,
    1000,  -456,  -184,  -276,  292,   -296,  156,   676,   320,   160,  908,
    -84,   -1236, -288,  -116,  260,   -372,  -644,  732,   -756,  -96,  84,
    344,   -520,  348,   -688,  240,   -84,   216,   -1044, -136,  -676, -396,
    -1500, 960,   -40,   176,   168,   1516,  420,   -504,  -344,  -364, -360,
    1216,  -940,  -380,  -212,  252,   -660,  -708,  484,   -444,  -152, 928,
    -120,  1112,  476,   -260,  560,   -148,  -344,  108,   -196,  228,  -288,
    504,   560,   -328,  -88,   288,   -1008, 460,   -228,  468,   -836, -196,
    76,    388,   232,   412,   -1168, -716,  -644,  756,   -172,  -356, -504,
    116,   432,   528,   48,    476,   -168,  -608,  448,   160,   -532, -272,
    28,    -676,  -12,   828,   980,   456,   520,   104,   -104,  256,  -344,
    -4,    -28,   -368,  -52,   -524,  -572,  -556,  -200,  768,   1124, -208,
    -512,  176,   232,   248,   -148,  -888,  604,   -600,  -304,  804,  -156,
    -212,  488,   -192,  -804,  -256,  368,   -360,  -916,  -328,  228,  -240,
    -448,  -472,  856,   -556,  -364,  572,   -12,   -156,  -368,  -340, 432,
    252,   -752,  -152,  288,   268,   -580,  -848,  -592,  108,   -76,  244,
    312,   -716,  592,   -80,   436,   360,   4,     -248,  160,   516,  584,
    732,   44,    -468,  -280,  -292,  -156,  -588,  28,    308,   912,  24,
    124,   156,   180,   -252,  944,   -924,  -772,  -520,  -428,  -624, 300,
    -212,  -1144, 32,    -724,  800,   -1128, -212,  -1288, -848,  180,  -416,
    440,   192,   -576,  -792,  -76,   -1080, 80,    -532,  -352,  -132, 380,
    -820,  148,   1112,  128,   164,   456,   700,   -924,  144,   -668, -384,
    648,   -832,  508,   552,   -52,   -100,  -656,  208,   -568,  748,  -88,
    680,   232,   300,   192,   -408,  -1012, -152,  -252,  -268,  272,  -876,
    -664,  -648,  -332,  -136,  16,    12,    1152,  -28,   332,   -536, 320,
    -672,  -460,  -316,  532,   -260,  228,   -40,   1052,  -816,  180,  88,
    -496,  -556,  -672,  -368,  428,   92,    356,   404,   -408,  252,  196,
    -176,  -556,  792,   268,   32,    372,   40,    96,    -332,  328,  120,
    372,   -900,  -40,   472,   -264,  -592,  952,   128,   656,   112,  664,
    -232,  420,   4,     -344,  -464,  556,   244,   -416,  -32,   252,  0,
    -412,  188,   -696,  508,   -476,  324,   -1096, 656,   -312,  560,  264,
    -136,  304,   160,   -64,   -580,  248,   336,   -720,  560,   -348, -288,
    -276,  -196,  -500,  852,   -544,  -236,  -1128, -992,  -776,  116,  56,
    52,    860,   884,   212,   -12,   168,   1020,  512,   -552,  924,  -148,
    716,   188,   164,   -340,  -520,  -184,  880,   -152,  -680,  -208, -1156,
    -300,  -528,  -472,  364,   100,   -744,  -1056, -32,   540,   280,  144,
    -676,  -32,   -232,  -280,  -224,  96,    568,   -76,   172,   148,  148,
    104,   32,    -296,  -32,   788,   -80,   32,    -16,   280,   288,  944,
    428,   -484};
static_assert(sizeof(kGaussianSequence) / sizeof(kGaussianSequence[0]) == 2048,
              "");

// Section 7.18.3.1.
template <int bitdepth>
bool FilmGrainSynthesis_C(const void* source_plane_y, ptrdiff_t source_stride_y,
                          const void* source_plane_u, ptrdiff_t source_stride_u,
                          const void* source_plane_v, ptrdiff_t source_stride_v,
                          const FilmGrainParams& film_grain_params,
                          const bool is_monochrome,
                          const bool color_matrix_is_identity, const int width,
                          const int height, const int subsampling_x,
                          const int subsampling_y, void* dest_plane_y,
                          ptrdiff_t dest_stride_y, void* dest_plane_u,
                          ptrdiff_t dest_stride_u, void* dest_plane_v,
                          ptrdiff_t dest_stride_v) {
  FilmGrain<bitdepth> film_grain(film_grain_params, is_monochrome,
                                 color_matrix_is_identity, subsampling_x,
                                 subsampling_y, width, height);
  return film_grain.AddNoise(source_plane_y, source_stride_y, source_plane_u,
                             source_stride_u, source_plane_v, source_stride_v,
                             dest_plane_y, dest_stride_y, dest_plane_u,
                             dest_stride_u, dest_plane_v, dest_stride_v);
}

// Making this a template function prevents it from adding to code size when it
// is not placed in the DSP table. Most functions in the dsp directory change
// behavior by bitdepth, but because this one doesn't, it receives a dummy
// parameter with one enforced value, ensuring only one copy is made.
template <int singleton>
void InitializeScalingLookupTable_C(
    int num_points, const uint8_t point_value[], const uint8_t point_scaling[],
    uint8_t scaling_lut[kScalingLookupTableSize]) {
  static_assert(singleton == 0,
                "Improper instantiation of InitializeScalingLookupTable_C. "
                "There should be only one copy of this function.");
  if (num_points == 0) {
    memset(scaling_lut, 0, sizeof(scaling_lut[0]) * kScalingLookupTableSize);
    return;
  }
  static_assert(sizeof(scaling_lut[0]) == 1, "");
  memset(scaling_lut, point_scaling[0], point_value[0]);
  for (int i = 0; i < num_points - 1; ++i) {
    const int delta_y = point_scaling[i + 1] - point_scaling[i];
    const int delta_x = point_value[i + 1] - point_value[i];
    const int delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);
    for (int x = 0; x < delta_x; ++x) {
      const int v = point_scaling[i] + ((x * delta + 32768) >> 16);
      assert(v >= 0 && v <= UINT8_MAX);
      scaling_lut[point_value[i] + x] = v;
    }
  }
  const uint8_t last_point_value = point_value[num_points - 1];
  memset(&scaling_lut[last_point_value], point_scaling[num_points - 1],
         kScalingLookupTableSize - last_point_value);
}

// Section 7.18.3.5.
// Performs a piecewise linear interpolation into the scaling table.
template <int bitdepth>
int ScaleLut(const uint8_t scaling_lut[kScalingLookupTableSize], int index) {
  const int shift = bitdepth - 8;
  const int quotient = index >> shift;
  const int remainder = index - (quotient << shift);
  if (bitdepth == 8) {
    assert(quotient < kScalingLookupTableSize);
    return scaling_lut[quotient];
  }
  assert(quotient + 1 < kScalingLookupTableSize);
  const int start = scaling_lut[quotient];
  const int end = scaling_lut[quotient + 1];
  return start + RightShiftWithRounding((end - start) * remainder, shift);
}

}  // namespace

template <int bitdepth>
FilmGrain<bitdepth>::FilmGrain(const FilmGrainParams& params,
                               bool is_monochrome,
                               bool color_matrix_is_identity, int subsampling_x,
                               int subsampling_y, int width, int height)
    : params_(params),
      is_monochrome_(is_monochrome),
      color_matrix_is_identity_(color_matrix_is_identity),
      subsampling_x_(subsampling_x),
      subsampling_y_(subsampling_y),
      width_(width),
      height_(height),
      chroma_width_((subsampling_x != 0) ? kMinChromaWidth : kMaxChromaWidth),
      chroma_height_((subsampling_y != 0) ? kMinChromaHeight
                                          : kMaxChromaHeight) {}

template <int bitdepth>
bool FilmGrain<bitdepth>::Init() {
  // Section 7.18.3.3. Generate grain process.
  const Dsp* dsp = GetDspTable(bitdepth);
  assert(dsp != nullptr);
  // If params_.num_y_points is 0, luma_grain_ will never be read, so we don't
  // need to generate it.
  const bool use_luma = params_.num_y_points > 0;
  if (use_luma) {
    GenerateLumaGrain(params_, luma_grain_);
    // If params_.auto_regression_coeff_lag is 0, the filter is the identity
    // filter and therefore can be skipped.
    if (params_.auto_regression_coeff_lag > 0) {
      dsp->film_grain.luma_auto_regression[params_.auto_regression_coeff_lag](
          params_, luma_grain_);
    }
  } else {
    // Have AddressSanitizer warn if luma_grain_ is used.
    ASAN_POISON_MEMORY_REGION(luma_grain_, sizeof(luma_grain_));
  }
  if (!is_monochrome_) {
    GenerateChromaGrains(params_, chroma_width_, chroma_height_, u_grain_,
                         v_grain_);
    if (params_.auto_regression_coeff_lag > 0 || use_luma) {
      dsp->film_grain.chroma_auto_regression[static_cast<int>(
          use_luma)][params_.auto_regression_coeff_lag](
          params_, luma_grain_, subsampling_x_, subsampling_y_, u_grain_,
          v_grain_);
    }
  }

  // Section 7.18.3.4. Scaling lookup initialization process.

  // Initialize scaling_lut_y_. If params_.num_y_points > 0, scaling_lut_y_
  // is used for the Y plane. If params_.chroma_scaling_from_luma is true,
  // scaling_lut_u_ and scaling_lut_v_ are the same as scaling_lut_y_ and are
  // set up as aliases. So we need to initialize scaling_lut_y_ under these
  // two conditions.
  //
  // Note: Although it does not seem to make sense, there are test vectors
  // with chroma_scaling_from_luma=true and params_.num_y_points=0.
  if (use_luma || params_.chroma_scaling_from_luma) {
    dsp->film_grain.initialize_scaling_lut(
        params_.num_y_points, params_.point_y_value, params_.point_y_scaling,
        scaling_lut_y_);
  } else {
    ASAN_POISON_MEMORY_REGION(scaling_lut_y_, sizeof(scaling_lut_y_));
  }
  if (!is_monochrome_) {
    if (params_.chroma_scaling_from_luma) {
      scaling_lut_u_ = scaling_lut_y_;
      scaling_lut_v_ = scaling_lut_y_;
    } else if (params_.num_u_points > 0 || params_.num_v_points > 0) {
      const size_t buffer_size =
          (kScalingLookupTableSize + kScalingLookupTablePadding) *
          ((params_.num_u_points > 0) + (params_.num_v_points > 0));
      scaling_lut_chroma_buffer_.reset(new (std::nothrow) uint8_t[buffer_size]);
      if (scaling_lut_chroma_buffer_ == nullptr) return false;

      uint8_t* buffer = scaling_lut_chroma_buffer_.get();
      if (params_.num_u_points > 0) {
        scaling_lut_u_ = buffer;
        dsp->film_grain.initialize_scaling_lut(
            params_.num_u_points, params_.point_u_value,
            params_.point_u_scaling, scaling_lut_u_);
        buffer += kScalingLookupTableSize + kScalingLookupTablePadding;
      }
      if (params_.num_v_points > 0) {
        scaling_lut_v_ = buffer;
        dsp->film_grain.initialize_scaling_lut(
            params_.num_v_points, params_.point_v_value,
            params_.point_v_scaling, scaling_lut_v_);
      }
    }
  }
  return true;
}

// Section 7.18.3.2.
// |bits| is the number of random bits to return.
int GetRandomNumber(int bits, uint16_t* seed) {
  uint16_t s = *seed;
  uint16_t bit = (s ^ (s >> 1) ^ (s >> 3) ^ (s >> 12)) & 1;
  s = (s >> 1) | (bit << 15);
  *seed = s;
  return s >> (16 - bits);
}

template <int bitdepth>
void FilmGrain<bitdepth>::GenerateLumaGrain(const FilmGrainParams& params,
                                            GrainType* luma_grain) {
  // If params.num_y_points is equal to 0, Section 7.18.3.3 specifies we set
  // the luma_grain array to all zeros. But the Note at the end of Section
  // 7.18.3.3 says luma_grain "will never be read in this case". So we don't
  // call GenerateLumaGrain if params.num_y_points is equal to 0.
  assert(params.num_y_points > 0);
  const int shift = 12 - bitdepth + params.grain_scale_shift;
  uint16_t seed = params.grain_seed;
  GrainType* luma_grain_row = luma_grain;
  for (int y = 0; y < kLumaHeight; ++y) {
    for (int x = 0; x < kLumaWidth; ++x) {
      luma_grain_row[x] = RightShiftWithRounding(
          kGaussianSequence[GetRandomNumber(11, &seed)], shift);
    }
    luma_grain_row += kLumaWidth;
  }
}

// Applies an auto-regressive filter to the white noise in luma_grain.
template <int bitdepth, typename GrainType>
void ApplyAutoRegressiveFilterToLumaGrain_C(const FilmGrainParams& params,
                                            void* luma_grain_buffer) {
  auto* luma_grain = static_cast<GrainType*>(luma_grain_buffer);
  const int grain_min = GetGrainMin<bitdepth>();
  const int grain_max = GetGrainMax<bitdepth>();
  const int auto_regression_coeff_lag = params.auto_regression_coeff_lag;
  assert(auto_regression_coeff_lag > 0 && auto_regression_coeff_lag <= 3);
  // A pictorial representation of the auto-regressive filter for various values
  // of auto_regression_coeff_lag. The letter 'O' represents the current sample.
  // (The filter always operates on the current sample with filter
  // coefficient 1.) The letters 'X' represent the neighboring samples that the
  // filter operates on.
  //
  // auto_regression_coeff_lag == 3:
  //   X X X X X X X
  //   X X X X X X X
  //   X X X X X X X
  //   X X X O
  // auto_regression_coeff_lag == 2:
  //     X X X X X
  //     X X X X X
  //     X X O
  // auto_regression_coeff_lag == 1:
  //       X X X
  //       X O
  // auto_regression_coeff_lag == 0:
  //         O
  //
  // Note that if auto_regression_coeff_lag is 0, the filter is the identity
  // filter and therefore can be skipped. This implementation assumes it is not
  // called in that case.
  const int shift = params.auto_regression_shift;
  for (int y = kAutoRegressionBorder; y < kLumaHeight; ++y) {
    for (int x = kAutoRegressionBorder; x < kLumaWidth - kAutoRegressionBorder;
         ++x) {
      int sum = 0;
      int pos = 0;
      int delta_row = -auto_regression_coeff_lag;
      // The last iteration (delta_row == 0) is shorter and is handled
      // separately.
      do {
        int delta_column = -auto_regression_coeff_lag;
        do {
          const int coeff = params.auto_regression_coeff_y[pos];
          sum += luma_grain[(y + delta_row) * kLumaWidth + (x + delta_column)] *
                 coeff;
          ++pos;
        } while (++delta_column <= auto_regression_coeff_lag);
      } while (++delta_row < 0);
      // Last iteration: delta_row == 0.
      {
        int delta_column = -auto_regression_coeff_lag;
        do {
          const int coeff = params.auto_regression_coeff_y[pos];
          sum += luma_grain[y * kLumaWidth + (x + delta_column)] * coeff;
          ++pos;
        } while (++delta_column < 0);
      }
      luma_grain[y * kLumaWidth + x] = Clip3(
          luma_grain[y * kLumaWidth + x] + RightShiftWithRounding(sum, shift),
          grain_min, grain_max);
    }
  }
}

template <int bitdepth>
void FilmGrain<bitdepth>::GenerateChromaGrains(const FilmGrainParams& params,
                                               int chroma_width,
                                               int chroma_height,
                                               GrainType* u_grain,
                                               GrainType* v_grain) {
  const int shift = 12 - bitdepth + params.grain_scale_shift;
  if (params.num_u_points == 0 && !params.chroma_scaling_from_luma) {
    memset(u_grain, 0, chroma_height * chroma_width * sizeof(*u_grain));
  } else {
    uint16_t seed = params.grain_seed ^ 0xb524;
    GrainType* u_grain_row = u_grain;
    assert(chroma_width > 0);
    assert(chroma_height > 0);
    int y = 0;
    do {
      int x = 0;
      do {
        u_grain_row[x] = RightShiftWithRounding(
            kGaussianSequence[GetRandomNumber(11, &seed)], shift);
      } while (++x < chroma_width);

      u_grain_row += chroma_width;
    } while (++y < chroma_height);
  }
  if (params.num_v_points == 0 && !params.chroma_scaling_from_luma) {
    memset(v_grain, 0, chroma_height * chroma_width * sizeof(*v_grain));
  } else {
    GrainType* v_grain_row = v_grain;
    uint16_t seed = params.grain_seed ^ 0x49d8;
    int y = 0;
    do {
      int x = 0;
      do {
        v_grain_row[x] = RightShiftWithRounding(
            kGaussianSequence[GetRandomNumber(11, &seed)], shift);
      } while (++x < chroma_width);

      v_grain_row += chroma_width;
    } while (++y < chroma_height);
  }
}

template <int bitdepth, typename GrainType, int auto_regression_coeff_lag,
          bool use_luma>
void ApplyAutoRegressiveFilterToChromaGrains_C(const FilmGrainParams& params,
                                               const void* luma_grain_buffer,
                                               int subsampling_x,
                                               int subsampling_y,
                                               void* u_grain_buffer,
                                               void* v_grain_buffer) {
  static_assert(
      auto_regression_coeff_lag >= 0 && auto_regression_coeff_lag <= 3,
      "Unsupported autoregression lag for chroma.");
  const auto* luma_grain = static_cast<const GrainType*>(luma_grain_buffer);
  const int grain_min = GetGrainMin<bitdepth>();
  const int grain_max = GetGrainMax<bitdepth>();
  auto* u_grain = static_cast<GrainType*>(u_grain_buffer);
  auto* v_grain = static_cast<GrainType*>(v_grain_buffer);
  const int shift = params.auto_regression_shift;
  const int chroma_height =
      (subsampling_y == 0) ? kMaxChromaHeight : kMinChromaHeight;
  const int chroma_width =
      (subsampling_x == 0) ? kMaxChromaWidth : kMinChromaWidth;
  for (int y = kAutoRegressionBorder; y < chroma_height; ++y) {
    const int luma_y =
        ((y - kAutoRegressionBorder) << subsampling_y) + kAutoRegressionBorder;
    for (int x = kAutoRegressionBorder;
         x < chroma_width - kAutoRegressionBorder; ++x) {
      int sum_u = 0;
      int sum_v = 0;
      int pos = 0;
      int delta_row = -auto_regression_coeff_lag;
      do {
        int delta_column = -auto_regression_coeff_lag;
        do {
          if (delta_row == 0 && delta_column == 0) {
            break;
          }
          const int coeff_u = params.auto_regression_coeff_u[pos];
          const int coeff_v = params.auto_regression_coeff_v[pos];
          sum_u +=
              u_grain[(y + delta_row) * chroma_width + (x + delta_column)] *
              coeff_u;
          sum_v +=
              v_grain[(y + delta_row) * chroma_width + (x + delta_column)] *
              coeff_v;
          ++pos;
        } while (++delta_column <= auto_regression_coeff_lag);
      } while (++delta_row <= 0);
      if (use_luma) {
        int luma = 0;
        const int luma_x = ((x - kAutoRegressionBorder) << subsampling_x) +
                           kAutoRegressionBorder;
        int i = 0;
        do {
          int j = 0;
          do {
            luma += luma_grain[(luma_y + i) * kLumaWidth + (luma_x + j)];
          } while (++j <= subsampling_x);
        } while (++i <= subsampling_y);
        luma = RightShiftWithRounding(luma, subsampling_x + subsampling_y);
        const int coeff_u = params.auto_regression_coeff_u[pos];
        const int coeff_v = params.auto_regression_coeff_v[pos];
        sum_u += luma * coeff_u;
        sum_v += luma * coeff_v;
      }
      u_grain[y * chroma_width + x] = Clip3(
          u_grain[y * chroma_width + x] + RightShiftWithRounding(sum_u, shift),
          grain_min, grain_max);
      v_grain[y * chroma_width + x] = Clip3(
          v_grain[y * chroma_width + x] + RightShiftWithRounding(sum_v, shift),
          grain_min, grain_max);
    }
  }
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AllocateNoiseStripes() {
  const int half_height = DivideBy2(height_ + 1);
  assert(half_height > 0);
  // ceil(half_height / 16.0)
  const int max_luma_num = DivideBy16(half_height + 15);
  constexpr int kNoiseStripeHeight = 34;
  size_t noise_buffer_size = kNoiseStripePadding;
  if (params_.num_y_points > 0) {
    noise_buffer_size += max_luma_num * kNoiseStripeHeight * width_;
  }
  if (!is_monochrome_) {
    noise_buffer_size += 2 * max_luma_num *
                         (kNoiseStripeHeight >> subsampling_y_) *
                         RightShiftWithRounding(width_, subsampling_x_);
  }
  noise_buffer_.reset(new (std::nothrow) GrainType[noise_buffer_size]);
  if (noise_buffer_ == nullptr) return false;
  GrainType* noise_buffer = noise_buffer_.get();
  if (params_.num_y_points > 0) {
    noise_stripes_[kPlaneY].Reset(max_luma_num, kNoiseStripeHeight * width_,
                                  noise_buffer);
    noise_buffer += max_luma_num * kNoiseStripeHeight * width_;
  }
  if (!is_monochrome_) {
    noise_stripes_[kPlaneU].Reset(
        max_luma_num,
        (kNoiseStripeHeight >> subsampling_y_) *
            RightShiftWithRounding(width_, subsampling_x_),
        noise_buffer);
    noise_buffer += max_luma_num * (kNoiseStripeHeight >> subsampling_y_) *
                    RightShiftWithRounding(width_, subsampling_x_);
    noise_stripes_[kPlaneV].Reset(
        max_luma_num,
        (kNoiseStripeHeight >> subsampling_y_) *
            RightShiftWithRounding(width_, subsampling_x_),
        noise_buffer);
  }
  return true;
}

// This implementation is for the condition overlap_flag == false.
template <int bitdepth, typename GrainType>
void ConstructNoiseStripes_C(const void* grain_buffer, int grain_seed,
                             int width, int height, int subsampling_x,
                             int subsampling_y, void* noise_stripes_buffer) {
  auto* noise_stripes =
      static_cast<Array2DView<GrainType>*>(noise_stripes_buffer);
  const auto* grain = static_cast<const GrainType*>(grain_buffer);
  const int half_width = DivideBy2(width + 1);
  const int half_height = DivideBy2(height + 1);
  assert(half_width > 0);
  assert(half_height > 0);
  static_assert(kLumaWidth == kMaxChromaWidth,
                "kLumaWidth width should be equal to kMaxChromaWidth");
  const int grain_width =
      (subsampling_x == 0) ? kMaxChromaWidth : kMinChromaWidth;
  const int plane_width = (width + subsampling_x) >> subsampling_x;
  constexpr int kNoiseStripeHeight = 34;
  int luma_num = 0;
  int y = 0;
  do {
    GrainType* const noise_stripe = (*noise_stripes)[luma_num];
    uint16_t seed = grain_seed;
    seed ^= ((luma_num * 37 + 178) & 255) << 8;
    seed ^= ((luma_num * 173 + 105) & 255);
    int x = 0;
    do {
      const int rand = GetRandomNumber(8, &seed);
      const int offset_x = rand >> 4;
      const int offset_y = rand & 15;
      const int plane_offset_x =
          (subsampling_x != 0) ? 6 + offset_x : 9 + offset_x * 2;
      const int plane_offset_y =
          (subsampling_y != 0) ? 6 + offset_y : 9 + offset_y * 2;
      int i = 0;
      do {
        // Section 7.18.3.5 says:
        //   noiseStripe[ lumaNum ][ 0 ] is 34 samples high and w samples
        //   wide (a few additional samples across are actually written to
        //   the array, but these are never read) ...
        //
        // Note: The warning in the parentheses also applies to
        // noiseStripe[ lumaNum ][ 1 ] and noiseStripe[ lumaNum ][ 2 ].
        //
        // Writes beyond the width of each row could happen below. To
        // prevent those writes, we clip the number of pixels to copy against
        // the remaining width.
        // TODO(petersonab): Allocate aligned stripes with extra width to cover
        // the size of the final stripe block, then remove this call to min.
        const int copy_size =
            std::min(kNoiseStripeHeight >> subsampling_x,
                     plane_width - (x << (1 - subsampling_x)));
        memcpy(&noise_stripe[i * plane_width + (x << (1 - subsampling_x))],
               &grain[(plane_offset_y + i) * grain_width + plane_offset_x],
               copy_size * sizeof(noise_stripe[0]));
      } while (++i < (kNoiseStripeHeight >> subsampling_y));
      x += 16;
    } while (x < half_width);

    ++luma_num;
    y += 16;
  } while (y < half_height);
}

// This implementation is for the condition overlap_flag == true.
template <int bitdepth, typename GrainType>
void ConstructNoiseStripesWithOverlap_C(const void* grain_buffer,
                                        int grain_seed, int width, int height,
                                        int subsampling_x, int subsampling_y,
                                        void* noise_stripes_buffer) {
  auto* noise_stripes =
      static_cast<Array2DView<GrainType>*>(noise_stripes_buffer);
  const auto* grain = static_cast<const GrainType*>(grain_buffer);
  const int half_width = DivideBy2(width + 1);
  const int half_height = DivideBy2(height + 1);
  assert(half_width > 0);
  assert(half_height > 0);
  static_assert(kLumaWidth == kMaxChromaWidth,
                "kLumaWidth width should be equal to kMaxChromaWidth");
  const int grain_width =
      (subsampling_x == 0) ? kMaxChromaWidth : kMinChromaWidth;
  const int plane_width = (width + subsampling_x) >> subsampling_x;
  constexpr int kNoiseStripeHeight = 34;
  int luma_num = 0;
  int y = 0;
  do {
    GrainType* const noise_stripe = (*noise_stripes)[luma_num];
    uint16_t seed = grain_seed;
    seed ^= ((luma_num * 37 + 178) & 255) << 8;
    seed ^= ((luma_num * 173 + 105) & 255);
    // Begin special iteration for x == 0.
    const int rand = GetRandomNumber(8, &seed);
    const int offset_x = rand >> 4;
    const int offset_y = rand & 15;
    const int plane_offset_x =
        (subsampling_x != 0) ? 6 + offset_x : 9 + offset_x * 2;
    const int plane_offset_y =
        (subsampling_y != 0) ? 6 + offset_y : 9 + offset_y * 2;
    // The overlap computation only occurs when x > 0, so it is omitted here.
    int i = 0;
    do {
      // TODO(petersonab): Allocate aligned stripes with extra width to cover
      // the size of the final stripe block, then remove this call to min.
      const int copy_size =
          std::min(kNoiseStripeHeight >> subsampling_x, plane_width);
      memcpy(&noise_stripe[i * plane_width],
             &grain[(plane_offset_y + i) * grain_width + plane_offset_x],
             copy_size * sizeof(noise_stripe[0]));
    } while (++i < (kNoiseStripeHeight >> subsampling_y));
    // End special iteration for x == 0.
    for (int x = 16; x < half_width; x += 16) {
      const int rand = GetRandomNumber(8, &seed);
      const int offset_x = rand >> 4;
      const int offset_y = rand & 15;
      const int plane_offset_x =
          (subsampling_x != 0) ? 6 + offset_x : 9 + offset_x * 2;
      const int plane_offset_y =
          (subsampling_y != 0) ? 6 + offset_y : 9 + offset_y * 2;
      int i = 0;
      do {
        int j = 0;
        int grain_sample =
            grain[(plane_offset_y + i) * grain_width + plane_offset_x];
        // The first pixel(s) of each segment of the noise_stripe are subject to
        // the "overlap" computation.
        if (subsampling_x == 0) {
          // Corresponds to the line in the spec:
          // if (j < 2 && x > 0)
          // j = 0
          int old = noise_stripe[i * plane_width + x * 2];
          grain_sample = old * 27 + grain_sample * 17;
          grain_sample =
              Clip3(RightShiftWithRounding(grain_sample, 5),
                    GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>());
          noise_stripe[i * plane_width + x * 2] = grain_sample;

          // This check prevents overwriting for the iteration j = 1. The
          // continue applies to the i-loop.
          if (x * 2 + 1 >= plane_width) continue;
          // j = 1
          grain_sample =
              grain[(plane_offset_y + i) * grain_width + plane_offset_x + 1];
          old = noise_stripe[i * plane_width + x * 2 + 1];
          grain_sample = old * 17 + grain_sample * 27;
          grain_sample =
              Clip3(RightShiftWithRounding(grain_sample, 5),
                    GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>());
          noise_stripe[i * plane_width + x * 2 + 1] = grain_sample;
          j = 2;
        } else {
          // Corresponds to the line in the spec:
          // if (j == 0 && x > 0)
          const int old = noise_stripe[i * plane_width + x];
          grain_sample = old * 23 + grain_sample * 22;
          grain_sample =
              Clip3(RightShiftWithRounding(grain_sample, 5),
                    GetGrainMin<bitdepth>(), GetGrainMax<bitdepth>());
          noise_stripe[i * plane_width + x] = grain_sample;
          j = 1;
        }
        // The following covers the rest of the loop over j as described in the
        // spec.
        //
        // Section 7.18.3.5 says:
        //   noiseStripe[ lumaNum ][ 0 ] is 34 samples high and w samples
        //   wide (a few additional samples across are actually written to
        //   the array, but these are never read) ...
        //
        // Note: The warning in the parentheses also applies to
        // noiseStripe[ lumaNum ][ 1 ] and noiseStripe[ lumaNum ][ 2 ].
        //
        // Writes beyond the width of each row could happen below. To
        // prevent those writes, we clip the number of pixels to copy against
        // the remaining width.
        // TODO(petersonab): Allocate aligned stripes with extra width to cover
        // the size of the final stripe block, then remove this call to min.
        const int copy_size =
            std::min(kNoiseStripeHeight >> subsampling_x,
                     plane_width - (x << (1 - subsampling_x))) -
            j;
        memcpy(&noise_stripe[i * plane_width + (x << (1 - subsampling_x)) + j],
               &grain[(plane_offset_y + i) * grain_width + plane_offset_x + j],
               copy_size * sizeof(noise_stripe[0]));
      } while (++i < (kNoiseStripeHeight >> subsampling_y));
    }

    ++luma_num;
    y += 16;
  } while (y < half_height);
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AllocateNoiseImage() {
  if (params_.num_y_points > 0 &&
      !noise_image_[kPlaneY].Reset(height_, width_ + kNoiseImagePadding,
                                   /*zero_initialize=*/false)) {
    return false;
  }
  if (!is_monochrome_) {
    if (!noise_image_[kPlaneU].Reset(
            (height_ + subsampling_y_) >> subsampling_y_,
            ((width_ + subsampling_x_) >> subsampling_x_) + kNoiseImagePadding,
            /*zero_initialize=*/false)) {
      return false;
    }
    if (!noise_image_[kPlaneV].Reset(
            (height_ + subsampling_y_) >> subsampling_y_,
            ((width_ + subsampling_x_) >> subsampling_x_) + kNoiseImagePadding,
            /*zero_initialize=*/false)) {
      return false;
    }
  }
  return true;
}

template <int bitdepth, typename GrainType>
inline void WriteOverlapLine_C(const GrainType* noise_stripe_row,
                               const GrainType* noise_stripe_row_prev,
                               int plane_width, int grain_coeff, int old_coeff,
                               GrainType* noise_image_row) {
  int x = 0;
  do {
    int grain = noise_stripe_row[x];
    const int old = noise_stripe_row_prev[x];
    grain = old * old_coeff + grain * grain_coeff;
    grain = Clip3(RightShiftWithRounding(grain, 5), GetGrainMin<bitdepth>(),
                  GetGrainMax<bitdepth>());
    noise_image_row[x] = grain;
  } while (++x < plane_width);
}

template <int bitdepth, typename GrainType>
void ConstructNoiseImageOverlap_C(const void* noise_stripes_buffer, int width,
                                  int height, int subsampling_x,
                                  int subsampling_y, void* noise_image_buffer) {
  const auto* noise_stripes =
      static_cast<const Array2DView<GrainType>*>(noise_stripes_buffer);
  auto* noise_image = static_cast<Array2D<GrainType>*>(noise_image_buffer);
  const int plane_width = (width + subsampling_x) >> subsampling_x;
  const int plane_height = (height + subsampling_y) >> subsampling_y;
  const int stripe_height = 32 >> subsampling_y;
  const int stripe_mask = stripe_height - 1;
  int y = stripe_height;
  int luma_num = 1;
  if (subsampling_y == 0) {
    // Begin complete stripes section. This is when we are guaranteed to have
    // two overlap rows in each stripe.
    for (; y < (plane_height & ~stripe_mask); ++luma_num, y += stripe_height) {
      const GrainType* noise_stripe = (*noise_stripes)[luma_num];
      const GrainType* noise_stripe_prev = (*noise_stripes)[luma_num - 1];
      // First overlap row.
      WriteOverlapLine_C<bitdepth>(noise_stripe,
                                   &noise_stripe_prev[32 * plane_width],
                                   plane_width, 17, 27, (*noise_image)[y]);
      // Second overlap row.
      WriteOverlapLine_C<bitdepth>(&noise_stripe[plane_width],
                                   &noise_stripe_prev[(32 + 1) * plane_width],
                                   plane_width, 27, 17, (*noise_image)[y + 1]);
    }
    // End complete stripes section.

    const int remaining_height = plane_height - y;
    // Either one partial stripe remains (remaining_height  > 0),
    // OR image is less than one stripe high (remaining_height < 0),
    // OR all stripes are completed (remaining_height == 0).
    if (remaining_height <= 0) {
      return;
    }
    const GrainType* noise_stripe = (*noise_stripes)[luma_num];
    const GrainType* noise_stripe_prev = (*noise_stripes)[luma_num - 1];
    WriteOverlapLine_C<bitdepth>(noise_stripe,
                                 &noise_stripe_prev[32 * plane_width],
                                 plane_width, 17, 27, (*noise_image)[y]);

    // Check if second overlap row is in the image.
    if (remaining_height > 1) {
      WriteOverlapLine_C<bitdepth>(&noise_stripe[plane_width],
                                   &noise_stripe_prev[(32 + 1) * plane_width],
                                   plane_width, 27, 17, (*noise_image)[y + 1]);
    }
  } else {  // |subsampling_y| == 1
    // No special checks needed for partial stripes, because if one exists, the
    // first and only overlap row is guaranteed to exist.
    for (; y < plane_height; ++luma_num, y += stripe_height) {
      const GrainType* noise_stripe = (*noise_stripes)[luma_num];
      const GrainType* noise_stripe_prev = (*noise_stripes)[luma_num - 1];
      WriteOverlapLine_C<bitdepth>(noise_stripe,
                                   &noise_stripe_prev[16 * plane_width],
                                   plane_width, 22, 23, (*noise_image)[y]);
    }
  }
}

// Uses |overlap_flag| to skip rows that are covered by the overlap computation.
template <int bitdepth>
void FilmGrain<bitdepth>::ConstructNoiseImage(
    const Array2DView<GrainType>* noise_stripes, int width, int height,
    int subsampling_x, int subsampling_y, int stripe_start_offset,
    Array2D<GrainType>* noise_image) {
  const int plane_width = (width + subsampling_x) >> subsampling_x;
  const int plane_height = (height + subsampling_y) >> subsampling_y;
  const int stripe_height = 32 >> subsampling_y;
  const int stripe_mask = stripe_height - 1;
  int y = 0;
  // |luma_num| = y >> (5 - |subsampling_y|). Hence |luma_num| == 0 for all y up
  // to either 16 or 32.
  const GrainType* first_noise_stripe = (*noise_stripes)[0];
  do {
    memcpy((*noise_image)[y], first_noise_stripe + y * plane_width,
           plane_width * sizeof(first_noise_stripe[0]));
  } while (++y < std::min(stripe_height, plane_height));
  // End special iterations for luma_num == 0.

  int luma_num = 1;
  for (; y < (plane_height & ~stripe_mask); ++luma_num, y += stripe_height) {
    const GrainType* noise_stripe = (*noise_stripes)[luma_num];
    int i = stripe_start_offset;
    do {
      memcpy((*noise_image)[y + i], noise_stripe + i * plane_width,
             plane_width * sizeof(noise_stripe[0]));
    } while (++i < stripe_height);
  }

  // If there is a partial stripe, copy any rows beyond the overlap rows.
  const int remaining_height = plane_height - y;
  if (remaining_height > stripe_start_offset) {
    assert(luma_num < noise_stripes->rows());
    const GrainType* noise_stripe = (*noise_stripes)[luma_num];
    int i = stripe_start_offset;
    do {
      memcpy((*noise_image)[y + i], noise_stripe + i * plane_width,
             plane_width * sizeof(noise_stripe[0]));
    } while (++i < remaining_height);
  }
}

// |width| and |height| refer to the plane, not the frame, meaning any
// subsampling should be applied by the caller.
template <typename Pixel>
inline void CopyImagePlane(const void* source_plane, ptrdiff_t source_stride,
                           int width, int height, void* dest_plane,
                           ptrdiff_t dest_stride) {
  // If it's the same buffer we should not call the function at all.
  assert(source_plane != dest_plane);
  const auto* in_plane = static_cast<const Pixel*>(source_plane);
  source_stride /= sizeof(Pixel);
  auto* out_plane = static_cast<Pixel*>(dest_plane);
  dest_stride /= sizeof(Pixel);

  const Pixel* in_row = in_plane;
  Pixel* out_row = out_plane;
  int y = 0;
  do {
    memcpy(out_row, in_row, width * sizeof(*out_row));
    in_row += source_stride;
    out_row += dest_stride;
  } while (++y < height);
}

template <int bitdepth, typename GrainType, typename Pixel>
void BlendNoiseWithImageLuma_C(const void* noise_image_ptr, int min_value,
                               int max_luma, int scaling_shift, int width,
                               int height, const uint8_t scaling_lut_y[256],
                               const void* source_plane_y,
                               ptrdiff_t source_stride_y, void* dest_plane_y,
                               ptrdiff_t dest_stride_y) {
  const auto* noise_image =
      static_cast<const Array2D<GrainType>*>(noise_image_ptr);
  const auto* in_y = static_cast<const Pixel*>(source_plane_y);
  source_stride_y /= sizeof(Pixel);
  auto* out_y = static_cast<Pixel*>(dest_plane_y);
  dest_stride_y /= sizeof(Pixel);

  int y = 0;
  do {
    int x = 0;
    do {
      const int orig = in_y[y * source_stride_y + x];
      int noise = noise_image[kPlaneY][y][x];
      noise = RightShiftWithRounding(
          ScaleLut<bitdepth>(scaling_lut_y, orig) * noise, scaling_shift);
      out_y[y * dest_stride_y + x] = Clip3(orig + noise, min_value, max_luma);
    } while (++x < width);
  } while (++y < height);
}

// This function is for the case params_.chroma_scaling_from_luma == false.
template <int bitdepth, typename GrainType, typename Pixel>
void BlendNoiseWithImageChroma_C(
    const FilmGrainParams& params, const void* noise_image_ptr, int min_value,
    int max_chroma, int width, int height, int subsampling_x, int subsampling_y,
    const uint8_t scaling_lut_u[256], const uint8_t scaling_lut_v[256],
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v, void* dest_plane_u,
    ptrdiff_t dest_stride_u, void* dest_plane_v, ptrdiff_t dest_stride_v) {
  const auto* noise_image =
      static_cast<const Array2D<GrainType>*>(noise_image_ptr);

  const int chroma_width = (width + subsampling_x) >> subsampling_x;
  const int chroma_height = (height + subsampling_y) >> subsampling_y;
  if (params.num_u_points == 0) {
    if (source_plane_u != dest_plane_u) {
      CopyImagePlane<Pixel>(source_plane_u, source_stride_u, chroma_width,
                            chroma_height, dest_plane_u, dest_stride_u);
    }
  }
  if (params.num_v_points == 0) {
    if (source_plane_v != dest_plane_v) {
      CopyImagePlane<Pixel>(source_plane_v, source_stride_v, chroma_width,
                            chroma_height, dest_plane_v, dest_stride_v);
    }
  }

  const auto* in_y = static_cast<const Pixel*>(source_plane_y);
  source_stride_y /= sizeof(Pixel);
  const auto* in_u = static_cast<const Pixel*>(source_plane_u);
  source_stride_u /= sizeof(Pixel);
  const auto* in_v = static_cast<const Pixel*>(source_plane_v);
  source_stride_v /= sizeof(Pixel);
  auto* out_u = static_cast<Pixel*>(dest_plane_u);
  dest_stride_u /= sizeof(Pixel);
  auto* out_v = static_cast<Pixel*>(dest_plane_v);
  dest_stride_v /= sizeof(Pixel);

  const int scaling_shift = params.chroma_scaling;
  int y = 0;
  do {
    int x = 0;
    do {
      const int luma_x = x << subsampling_x;
      const int luma_y = y << subsampling_y;
      const int luma_next_x = std::min(luma_x + 1, width - 1);
      int average_luma;
      if (subsampling_x != 0) {
        average_luma = RightShiftWithRounding(
            in_y[luma_y * source_stride_y + luma_x] +
                in_y[luma_y * source_stride_y + luma_next_x],
            1);
      } else {
        average_luma = in_y[luma_y * source_stride_y + luma_x];
      }
      if (params.num_u_points > 0) {
        const int orig = in_u[y * source_stride_u + x];
        const int combined = average_luma * params.u_luma_multiplier +
                             orig * params.u_multiplier;
        const int merged =
            Clip3((combined >> 6) + LeftShift(params.u_offset, bitdepth - 8), 0,
                  (1 << bitdepth) - 1);
        int noise = noise_image[kPlaneU][y][x];
        noise = RightShiftWithRounding(
            ScaleLut<bitdepth>(scaling_lut_u, merged) * noise, scaling_shift);
        out_u[y * dest_stride_u + x] =
            Clip3(orig + noise, min_value, max_chroma);
      }
      if (params.num_v_points > 0) {
        const int orig = in_v[y * source_stride_v + x];
        const int combined = average_luma * params.v_luma_multiplier +
                             orig * params.v_multiplier;
        const int merged =
            Clip3((combined >> 6) + LeftShift(params.v_offset, bitdepth - 8), 0,
                  (1 << bitdepth) - 1);
        int noise = noise_image[kPlaneV][y][x];
        noise = RightShiftWithRounding(
            ScaleLut<bitdepth>(scaling_lut_v, merged) * noise, scaling_shift);
        out_v[y * dest_stride_v + x] =
            Clip3(orig + noise, min_value, max_chroma);
      }
    } while (++x < chroma_width);
  } while (++y < chroma_height);
}

// This function is for the case params_.chroma_scaling_from_luma == true.
// This further implies that scaling_lut_u == scaling_lut_v == scaling_lut_y.
template <int bitdepth, typename GrainType, typename Pixel>
void BlendNoiseWithImageChromaWithCfl_C(
    const FilmGrainParams& params, const void* noise_image_ptr, int min_value,
    int max_chroma, int width, int height, int subsampling_x, int subsampling_y,
    const uint8_t scaling_lut[256], const uint8_t /*scaling_lut_v*/[256],
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v, void* dest_plane_u,
    ptrdiff_t dest_stride_u, void* dest_plane_v, ptrdiff_t dest_stride_v) {
  const auto* noise_image =
      static_cast<const Array2D<GrainType>*>(noise_image_ptr);
  const auto* in_y = static_cast<const Pixel*>(source_plane_y);
  source_stride_y /= sizeof(Pixel);
  const auto* in_u = static_cast<const Pixel*>(source_plane_u);
  source_stride_u /= sizeof(Pixel);
  const auto* in_v = static_cast<const Pixel*>(source_plane_v);
  source_stride_v /= sizeof(Pixel);
  auto* out_u = static_cast<Pixel*>(dest_plane_u);
  dest_stride_u /= sizeof(Pixel);
  auto* out_v = static_cast<Pixel*>(dest_plane_v);
  dest_stride_v /= sizeof(Pixel);

  const int chroma_width = (width + subsampling_x) >> subsampling_x;
  const int chroma_height = (height + subsampling_y) >> subsampling_y;
  const int scaling_shift = params.chroma_scaling;
  int y = 0;
  do {
    int x = 0;
    do {
      const int luma_x = x << subsampling_x;
      const int luma_y = y << subsampling_y;
      const int luma_next_x = std::min(luma_x + 1, width - 1);
      int average_luma;
      if (subsampling_x != 0) {
        average_luma = RightShiftWithRounding(
            in_y[luma_y * source_stride_y + luma_x] +
                in_y[luma_y * source_stride_y + luma_next_x],
            1);
      } else {
        average_luma = in_y[luma_y * source_stride_y + luma_x];
      }
      const int orig_u = in_u[y * source_stride_u + x];
      int noise_u = noise_image[kPlaneU][y][x];
      noise_u = RightShiftWithRounding(
          ScaleLut<bitdepth>(scaling_lut, average_luma) * noise_u,
          scaling_shift);
      out_u[y * dest_stride_u + x] =
          Clip3(orig_u + noise_u, min_value, max_chroma);

      const int orig_v = in_v[y * source_stride_v + x];
      int noise_v = noise_image[kPlaneV][y][x];
      noise_v = RightShiftWithRounding(
          ScaleLut<bitdepth>(scaling_lut, average_luma) * noise_v,
          scaling_shift);
      out_v[y * dest_stride_v + x] =
          Clip3(orig_v + noise_v, min_value, max_chroma);
    } while (++x < chroma_width);
  } while (++y < chroma_height);
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AddNoise(
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v, void* dest_plane_y,
    ptrdiff_t dest_stride_y, void* dest_plane_u, ptrdiff_t dest_stride_u,
    void* dest_plane_v, ptrdiff_t dest_stride_v) {
  if (!Init()) {
    LIBGAV1_DLOG(ERROR, "Init() failed.");
    return false;
  }
  if (!AllocateNoiseStripes()) {
    LIBGAV1_DLOG(ERROR, "AllocateNoiseStripes() failed.");
    return false;
  }

  const Dsp* dsp = GetDspTable(bitdepth);
  assert(dsp != nullptr);
  const bool use_luma = params_.num_y_points > 0;

  // Construct noise stripes.
  if (use_luma) {
    // The luma plane is never subsampled.
    dsp->film_grain
        .construct_noise_stripes[static_cast<int>(params_.overlap_flag)](
            luma_grain_, params_.grain_seed, width_, height_,
            /*subsampling_x=*/0, /*subsampling_y=*/0, &noise_stripes_[kPlaneY]);
  }
  if (!is_monochrome_) {
    dsp->film_grain
        .construct_noise_stripes[static_cast<int>(params_.overlap_flag)](
            u_grain_, params_.grain_seed, width_, height_, subsampling_x_,
            subsampling_y_, &noise_stripes_[kPlaneU]);
    dsp->film_grain
        .construct_noise_stripes[static_cast<int>(params_.overlap_flag)](
            v_grain_, params_.grain_seed, width_, height_, subsampling_x_,
            subsampling_y_, &noise_stripes_[kPlaneV]);
  }

  if (!AllocateNoiseImage()) {
    LIBGAV1_DLOG(ERROR, "AllocateNoiseImage() failed.");
    return false;
  }

  // Construct noise image.
  if (use_luma) {
    ConstructNoiseImage(
        &noise_stripes_[kPlaneY], width_, height_, /*subsampling_x=*/0,
        /*subsampling_y=*/0, params_.overlap_flag << 1, &noise_image_[kPlaneY]);
    if (params_.overlap_flag) {
      dsp->film_grain.construct_noise_image_overlap(
          &noise_stripes_[kPlaneY], width_, height_, /*subsampling_x=*/0,
          /*subsampling_y=*/0, &noise_image_[kPlaneY]);
    }
  }
  if (!is_monochrome_) {
    ConstructNoiseImage(&noise_stripes_[kPlaneU], width_, height_,
                        subsampling_x_, subsampling_y_,
                        params_.overlap_flag << (1 - subsampling_y_),
                        &noise_image_[kPlaneU]);
    ConstructNoiseImage(&noise_stripes_[kPlaneV], width_, height_,
                        subsampling_x_, subsampling_y_,
                        params_.overlap_flag << (1 - subsampling_y_),
                        &noise_image_[kPlaneV]);
    if (params_.overlap_flag) {
      dsp->film_grain.construct_noise_image_overlap(
          &noise_stripes_[kPlaneU], width_, height_, subsampling_x_,
          subsampling_y_, &noise_image_[kPlaneU]);
      dsp->film_grain.construct_noise_image_overlap(
          &noise_stripes_[kPlaneV], width_, height_, subsampling_x_,
          subsampling_y_, &noise_image_[kPlaneV]);
    }
  }

  // Blend noise image.
  int min_value;
  int max_luma;
  int max_chroma;
  if (params_.clip_to_restricted_range) {
    min_value = 16 << (bitdepth - 8);
    max_luma = 235 << (bitdepth - 8);
    if (color_matrix_is_identity_) {
      max_chroma = max_luma;
    } else {
      max_chroma = 240 << (bitdepth - 8);
    }
  } else {
    min_value = 0;
    max_luma = (256 << (bitdepth - 8)) - 1;
    max_chroma = max_luma;
  }
  if (!is_monochrome_) {
    dsp->film_grain.blend_noise_chroma[params_.chroma_scaling_from_luma](
        params_, noise_image_, min_value, max_chroma, width_, height_,
        subsampling_x_, subsampling_y_, scaling_lut_u_, scaling_lut_v_,
        source_plane_y, source_stride_y, source_plane_u, source_stride_u,
        source_plane_v, source_stride_v, dest_plane_u, dest_stride_u,
        dest_plane_v, dest_stride_v);
  }
  if (use_luma) {
    dsp->film_grain.blend_noise_luma(
        noise_image_, min_value, max_luma, params_.chroma_scaling, width_,
        height_, scaling_lut_y_, source_plane_y, source_stride_y, dest_plane_y,
        dest_stride_y);
  } else {
    if (source_plane_y != dest_plane_y) {
      CopyImagePlane<Pixel>(source_plane_y, source_stride_y, width_, height_,
                            dest_plane_y, dest_stride_y);
    }
  }

  return true;
}

// Explicit instantiations.
template class FilmGrain<8>;
#if LIBGAV1_MAX_BITDEPTH >= 10
template class FilmGrain<10>;
#endif

namespace {
void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->film_grain.synthesis = FilmGrainSynthesis_C<8>;

  // LumaAutoRegressionFunc
  // Luma autoregression should never be called when lag is 0.
  dsp->film_grain.luma_auto_regression[0] = nullptr;
  dsp->film_grain.luma_auto_regression[1] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;
  dsp->film_grain.luma_auto_regression[2] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;
  dsp->film_grain.luma_auto_regression[3] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;

  // ChromaAutoRegressionFunc
  // Chroma autoregression should never be called when lag is 0 and use_luma is
  // false.
  dsp->film_grain.chroma_auto_regression[0][0] = nullptr;
  dsp->film_grain.chroma_auto_regression[0][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 1, false>;
  dsp->film_grain.chroma_auto_regression[0][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 2, false>;
  dsp->film_grain.chroma_auto_regression[0][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 3, false>;
  dsp->film_grain.chroma_auto_regression[1][0] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 0, true>;
  dsp->film_grain.chroma_auto_regression[1][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 1, true>;
  dsp->film_grain.chroma_auto_regression[1][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 2, true>;
  dsp->film_grain.chroma_auto_regression[1][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 3, true>;

  // ConstructNoiseStripesFunc
  dsp->film_grain.construct_noise_stripes[0] =
      ConstructNoiseStripes_C<8, int8_t>;
  dsp->film_grain.construct_noise_stripes[1] =
      ConstructNoiseStripesWithOverlap_C<8, int8_t>;

  // ConstructNoiseImageOverlapFunc
  dsp->film_grain.construct_noise_image_overlap =
      ConstructNoiseImageOverlap_C<8, int8_t>;

  // InitializeScalingLutFunc
  dsp->film_grain.initialize_scaling_lut = InitializeScalingLookupTable_C<0>;

  // BlendNoiseWithImageLumaFunc
  dsp->film_grain.blend_noise_luma =
      BlendNoiseWithImageLuma_C<8, int8_t, uint8_t>;

  // BlendNoiseWithImageChromaFunc
  dsp->film_grain.blend_noise_chroma[0] =
      BlendNoiseWithImageChroma_C<8, int8_t, uint8_t>;
  dsp->film_grain.blend_noise_chroma[1] =
      BlendNoiseWithImageChromaWithCfl_C<8, int8_t, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_FilmGrainSynthesis
  dsp->film_grain.synthesis = FilmGrainSynthesis_C<8>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainAutoregressionLuma
  // Luma autoregression should never be called when lag is 0.
  dsp->film_grain.luma_auto_regression[0] = nullptr;
  dsp->film_grain.luma_auto_regression[1] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;
  dsp->film_grain.luma_auto_regression[2] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;
  dsp->film_grain.luma_auto_regression[3] =
      ApplyAutoRegressiveFilterToLumaGrain_C<8, int8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainAutoregressionChroma
  // Chroma autoregression should never be called when lag is 0 and use_luma is
  // false.
  dsp->film_grain.chroma_auto_regression[0][0] = nullptr;
  dsp->film_grain.chroma_auto_regression[0][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 1, false>;
  dsp->film_grain.chroma_auto_regression[0][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 2, false>;
  dsp->film_grain.chroma_auto_regression[0][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 3, false>;
  dsp->film_grain.chroma_auto_regression[1][0] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 0, true>;
  dsp->film_grain.chroma_auto_regression[1][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 1, true>;
  dsp->film_grain.chroma_auto_regression[1][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 2, true>;
  dsp->film_grain.chroma_auto_regression[1][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<8, int8_t, 3, true>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainConstructNoiseStripes
  dsp->film_grain.construct_noise_stripes[0] =
      ConstructNoiseStripes_C<8, int8_t>;
  dsp->film_grain.construct_noise_stripes[1] =
      ConstructNoiseStripesWithOverlap_C<8, int8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainConstructNoiseImageOverlap
  dsp->film_grain.construct_noise_image_overlap =
      ConstructNoiseImageOverlap_C<8, int8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainInitializeScalingLutFunc
  dsp->film_grain.initialize_scaling_lut = InitializeScalingLookupTable_C<0>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainBlendNoiseLuma
  dsp->film_grain.blend_noise_luma =
      BlendNoiseWithImageLuma_C<8, int8_t, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainBlendNoiseChroma
  dsp->film_grain.blend_noise_chroma[0] =
      BlendNoiseWithImageChroma_C<8, int8_t, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_FilmGrainBlendNoiseChromaWithCfl
  dsp->film_grain.blend_noise_chroma[1] =
      BlendNoiseWithImageChromaWithCfl_C<8, int8_t, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->film_grain.synthesis = FilmGrainSynthesis_C<10>;

  // LumaAutoRegressionFunc
  // Luma autoregression should never be called when lag is 0.
  dsp->film_grain.luma_auto_regression[0] = nullptr;
  dsp->film_grain.luma_auto_regression[1] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;
  dsp->film_grain.luma_auto_regression[2] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;
  dsp->film_grain.luma_auto_regression[3] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;

  // ChromaAutoRegressionFunc
  // Chroma autoregression should never be called when lag is 0 and use_luma is
  // false.
  dsp->film_grain.chroma_auto_regression[0][0] = nullptr;
  dsp->film_grain.chroma_auto_regression[0][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 1, false>;
  dsp->film_grain.chroma_auto_regression[0][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 2, false>;
  dsp->film_grain.chroma_auto_regression[0][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 3, false>;
  dsp->film_grain.chroma_auto_regression[1][0] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 0, true>;
  dsp->film_grain.chroma_auto_regression[1][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 1, true>;
  dsp->film_grain.chroma_auto_regression[1][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 2, true>;
  dsp->film_grain.chroma_auto_regression[1][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 3, true>;

  // ConstructNoiseStripesFunc
  dsp->film_grain.construct_noise_stripes[0] =
      ConstructNoiseStripes_C<10, int16_t>;
  dsp->film_grain.construct_noise_stripes[1] =
      ConstructNoiseStripesWithOverlap_C<10, int16_t>;

  // ConstructNoiseImageOverlapFunc
  dsp->film_grain.construct_noise_image_overlap =
      ConstructNoiseImageOverlap_C<10, int16_t>;

  // InitializeScalingLutFunc
  dsp->film_grain.initialize_scaling_lut = InitializeScalingLookupTable_C<0>;

  // BlendNoiseWithImageLumaFunc
  dsp->film_grain.blend_noise_luma =
      BlendNoiseWithImageLuma_C<10, int16_t, uint16_t>;

  // BlendNoiseWithImageChromaFunc
  dsp->film_grain.blend_noise_chroma[0] =
      BlendNoiseWithImageChroma_C<10, int16_t, uint16_t>;
  dsp->film_grain.blend_noise_chroma[1] =
      BlendNoiseWithImageChromaWithCfl_C<10, int16_t, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_FilmGrainSynthesis
  dsp->film_grain.synthesis = FilmGrainSynthesis_C<10>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainAutoregressionLuma
  // Luma autoregression should never be called when lag is 0.
  dsp->film_grain.luma_auto_regression[0] = nullptr;
  dsp->film_grain.luma_auto_regression[1] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;
  dsp->film_grain.luma_auto_regression[2] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;
  dsp->film_grain.luma_auto_regression[3] =
      ApplyAutoRegressiveFilterToLumaGrain_C<10, int16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainAutoregressionChroma
  // Chroma autoregression should never be called when lag is 0 and use_luma is
  // false.
  dsp->film_grain.chroma_auto_regression[0][0] = nullptr;
  dsp->film_grain.chroma_auto_regression[0][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 1, false>;
  dsp->film_grain.chroma_auto_regression[0][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 2, false>;
  dsp->film_grain.chroma_auto_regression[0][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 3, false>;
  dsp->film_grain.chroma_auto_regression[1][0] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 0, true>;
  dsp->film_grain.chroma_auto_regression[1][1] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 1, true>;
  dsp->film_grain.chroma_auto_regression[1][2] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 2, true>;
  dsp->film_grain.chroma_auto_regression[1][3] =
      ApplyAutoRegressiveFilterToChromaGrains_C<10, int16_t, 3, true>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainConstructNoiseStripes
  dsp->film_grain.construct_noise_stripes[0] =
      ConstructNoiseStripes_C<10, int16_t>;
  dsp->film_grain.construct_noise_stripes[1] =
      ConstructNoiseStripesWithOverlap_C<10, int16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainConstructNoiseImageOverlap
  dsp->film_grain.construct_noise_image_overlap =
      ConstructNoiseImageOverlap_C<10, int16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainInitializeScalingLutFunc
  dsp->film_grain.initialize_scaling_lut = InitializeScalingLookupTable_C<0>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainBlendNoiseLuma
  dsp->film_grain.blend_noise_luma =
      BlendNoiseWithImageLuma_C<10, int16_t, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainBlendNoiseChroma
  dsp->film_grain.blend_noise_chroma[0] =
      BlendNoiseWithImageChroma_C<10, int16_t, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_FilmGrainBlendNoiseChromaWithCfl
  dsp->film_grain.blend_noise_chroma[1] =
      BlendNoiseWithImageChromaWithCfl_C<10, int16_t, uint16_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace film_grain

void FilmGrainInit_C() {
  film_grain::Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  film_grain::Init10bpp();
#endif
}

}  // namespace dsp
}  // namespace libgav1
