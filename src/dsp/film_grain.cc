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
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {
namespace dsp {
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

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->film_grain_synthesis = FilmGrainSynthesis_C<8>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_FilmGrainSynthesis
  dsp->film_grain_synthesis = FilmGrainSynthesis_C<8>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10
void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->film_grain_synthesis = FilmGrainSynthesis_C<10>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_FilmGrainSynthesis
  dsp->film_grain_synthesis = FilmGrainSynthesis_C<10>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}
#endif

}  // namespace

void FilmGrainInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
}

// Static data member definitions.
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kLumaWidth;
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kLumaHeight;
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kMinChromaWidth;
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kMaxChromaWidth;
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kMinChromaHeight;
template <int bitdepth>
constexpr int FilmGrain<bitdepth>::kMaxChromaHeight;

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
                                          : kMaxChromaHeight) {
}

template <int bitdepth>
bool FilmGrain<bitdepth>::Init() {
  // Section 7.18.3.3. Generate grain process.
  GenerateLumaGrain(params_, luma_grain_);
  // If params_.auto_regression_coeff_lag is 0, the filter is the identity
  // filter and therefore can be skipped. If params_.num_y_points is 0,
  // luma_grain_ is all zeros (see GenerateLumaGrain), so we can also skip the
  // filter. (In fact, the Note at the end of Section 7.18.3.3 says luma_grain_
  // will never be read in this case.)
  if (params_.auto_regression_coeff_lag > 0 && params_.num_y_points > 0) {
    ApplyAutoRegressiveFilterToLumaGrain(params_, kGrainMin, kGrainMax,
                                         luma_grain_);
  }
  if (!is_monochrome_) {
    GenerateChromaGrains(params_, chroma_width_, chroma_height_, u_grain_,
                         v_grain_);
    ApplyAutoRegressiveFilterToChromaGrains(
        params_, kGrainMin, kGrainMax, luma_grain_, subsampling_x_,
        subsampling_y_, chroma_width_, chroma_height_, u_grain_, v_grain_);
  }

  // Section 7.18.3.4. Scaling lookup initialization process.
  InitializeScalingLookupTable(params_.num_y_points, params_.point_y_value,
                               params_.point_y_scaling, scaling_lut_y_);
  if (!is_monochrome_) {
    if (params_.chroma_scaling_from_luma) {
      scaling_lut_u_ = scaling_lut_y_;
      scaling_lut_v_ = scaling_lut_y_;
    } else {
      scaling_lut_chroma_buffer_.reset(new (std::nothrow) uint8_t[256 * 2]);
      if (scaling_lut_chroma_buffer_ == nullptr) return false;
      scaling_lut_u_ = &scaling_lut_chroma_buffer_[0];
      scaling_lut_v_ = &scaling_lut_chroma_buffer_[256];
      InitializeScalingLookupTable(params_.num_u_points, params_.point_u_value,
                                   params_.point_u_scaling, scaling_lut_u_);
      InitializeScalingLookupTable(params_.num_v_points, params_.point_v_value,
                                   params_.point_v_scaling, scaling_lut_v_);
    }
  }
  return true;
}

// Section 7.18.3.2.
// |bits| is the number of random bits to return.
template <int bitdepth>
int FilmGrain<bitdepth>::GetRandomNumber(int bits, uint16_t* seed) {
  uint16_t s = *seed;
  uint16_t bit = (s ^ (s >> 1) ^ (s >> 3) ^ (s >> 12)) & 1;
  s = (s >> 1) | (bit << 15);
  *seed = s;
  return s >> (16 - bits);
}

template <int bitdepth>
void FilmGrain<bitdepth>::GenerateLumaGrain(const FilmGrainParams& params,
                                            GrainType* luma_grain) {
  const int shift = 12 - bitdepth + params.grain_scale_shift;
  if (params.num_y_points == 0) {
    memset(luma_grain, 0, kLumaHeight * kLumaWidth * sizeof(*luma_grain));
  } else {
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
}

// Applies an auto-regressive filter to the white noise in luma_grain.
template <int bitdepth>
void FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToLumaGrain(
    const FilmGrainParams& params, int grain_min, int grain_max,
    GrainType* luma_grain) {
  assert(params.auto_regression_coeff_lag <= 3);
  // A pictorial representation of the auto-regressive filter for various values
  // of params.auto_regression_coeff_lag. The letter 'O' represents the current
  // sample. (The filter always operates on the current sample with filter
  // coefficient 1.) The letters 'X' represent the neighoring samples that the
  // filter operates on.
  //
  // params.auto_regression_coeff_lag == 3:
  //   X X X X X X X
  //   X X X X X X X
  //   X X X X X X X
  //   X X X O
  // params.auto_regression_coeff_lag == 2:
  //     X X X X X
  //     X X X X X
  //     X X O
  // params.auto_regression_coeff_lag == 1:
  //       X X X
  //       X O
  // params.auto_regression_coeff_lag == 0:
  //         O
  //
  // Note that if params.auto_regression_coeff_lag is 0, the filter is the
  // identity filter and therefore can be skipped. This implementation assumes
  // it is not called in that case.
  assert(params.auto_regression_coeff_lag > 0);
  const int shift = params.auto_regression_shift;
  for (int y = 3; y < kLumaHeight; ++y) {
    for (int x = 3; x < kLumaWidth - 3; ++x) {
      int sum = 0;
      int pos = 0;
      int delta_row = -params.auto_regression_coeff_lag;
      // The last iteration (delta_row == 0) is shorter and is handled
      // separately.
      do {
        int delta_column = -params.auto_regression_coeff_lag;
        do {
          const int coeff = params.auto_regression_coeff_y[pos];
          sum += luma_grain[(y + delta_row) * kLumaWidth + (x + delta_column)] *
                 coeff;
          ++pos;
        } while (++delta_column <= params.auto_regression_coeff_lag);
      } while (++delta_row < 0);
      // Last iteration: delta_row == 0.
      {
        int delta_column = -params.auto_regression_coeff_lag;
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

template <int bitdepth>
void FilmGrain<bitdepth>::ApplyAutoRegressiveFilterToChromaGrains(
    const FilmGrainParams& params, int grain_min, int grain_max,
    const GrainType* luma_grain, int subsampling_x, int subsampling_y,
    int chroma_width, int chroma_height, GrainType* u_grain,
    GrainType* v_grain) {
  assert(params.auto_regression_coeff_lag <= 3);
  const int shift = params.auto_regression_shift;
  for (int y = 3; y < chroma_height; ++y) {
    for (int x = 3; x < chroma_width - 3; ++x) {
      int sum_u = 0;
      int sum_v = 0;
      int pos = 0;
      int delta_row = -params.auto_regression_coeff_lag;
      do {
        int delta_column = -params.auto_regression_coeff_lag;
        do {
          const int coeff_u = params.auto_regression_coeff_u[pos];
          const int coeff_v = params.auto_regression_coeff_v[pos];
          if (delta_row == 0 && delta_column == 0) {
            if (params.num_y_points > 0) {
              int luma = 0;
              const int luma_x = ((x - 3) << subsampling_x) + 3;
              const int luma_y = ((y - 3) << subsampling_y) + 3;
              int i = 0;
              do {
                int j = 0;
                do {
                  luma += luma_grain[(luma_y + i) * kLumaWidth + (luma_x + j)];
                } while (++j <= subsampling_x);
              } while (++i <= subsampling_y);
              luma =
                  RightShiftWithRounding(luma, subsampling_x + subsampling_y);
              sum_u += luma * coeff_u;
              sum_v += luma * coeff_v;
            }
            break;
          }
          sum_u +=
              u_grain[(y + delta_row) * chroma_width + (x + delta_column)] *
              coeff_u;
          sum_v +=
              v_grain[(y + delta_row) * chroma_width + (x + delta_column)] *
              coeff_v;
          ++pos;
        } while (++delta_column <= params.auto_regression_coeff_lag);
      } while (++delta_row <= 0);
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
void FilmGrain<bitdepth>::InitializeScalingLookupTable(
    int num_points, const uint8_t point_value[], const uint8_t point_scaling[],
    uint8_t scaling_lut[256]) {
  if (num_points == 0) {
    memset(scaling_lut, 0, sizeof(scaling_lut[0]) * 256);
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
         256 - last_point_value);
}

// Section 7.18.3.5.
// Performs a piecewise linear interpolation into the scaling table.
template <int bitdepth>
int ScaleLut(const uint8_t scaling_lut[256], int index) {
  const int shift = bitdepth - 8;
  const int quotient = index >> shift;
  const int remainder = index - (quotient << shift);
  if (bitdepth == 8 || quotient == 255) {
    return scaling_lut[quotient];
  }
  const int start = scaling_lut[quotient];
  const int end = scaling_lut[quotient + 1];
  return start + RightShiftWithRounding((end - start) * remainder, shift);
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AllocateNoiseStripes() {
  const int num_planes = is_monochrome_ ? kMaxPlanesMonochrome : kMaxPlanes;
  const int half_height = DivideBy2(height_ + 1);
  // ceil(half_height / 16.0)
  const int max_luma_num = DivideBy16(half_height + 15);
  if (!noise_stripes_.Reset(max_luma_num, num_planes,
                            /*zero_initialize=*/false)) {
    return false;
  }
  constexpr int kNoiseStripeHeight = 34;
  size_t noise_buffer_size = max_luma_num * kNoiseStripeHeight * width_;
  if (!is_monochrome_) {
    noise_buffer_size += max_luma_num * 2 *
                         (kNoiseStripeHeight >> subsampling_y_) *
                         RightShiftWithRounding(width_, subsampling_x_);
  }
  noise_buffer_.reset(new (std::nothrow) GrainType[noise_buffer_size]);
  if (noise_buffer_ == nullptr) return false;
  GrainType* noise_stripe = noise_buffer_.get();
  int luma_num = 0;
  assert(half_height > 0);
  int y = 0;
  do {
    noise_stripes_[luma_num][kPlaneY] = noise_stripe;
    noise_stripe += kNoiseStripeHeight * width_;
    if (!is_monochrome_) {
      noise_stripes_[luma_num][kPlaneU] = noise_stripe;
      noise_stripe += (kNoiseStripeHeight >> subsampling_y_) *
                      RightShiftWithRounding(width_, subsampling_x_);
      noise_stripes_[luma_num][kPlaneV] = noise_stripe;
      noise_stripe += (kNoiseStripeHeight >> subsampling_y_) *
                      RightShiftWithRounding(width_, subsampling_x_);
    }
    ++luma_num;
    y += 16;
  } while (y < half_height);
  assert(noise_stripe == noise_buffer_.get() + noise_buffer_size);
  return true;
}

template <int bitdepth>
void FilmGrain<bitdepth>::ConstructNoiseStripes() {
  const int num_planes = is_monochrome_ ? kMaxPlanesMonochrome : kMaxPlanes;
  const int half_width = DivideBy2(width_ + 1);
  const int half_height = DivideBy2(height_ + 1);
  constexpr int kNoiseStripeHeight = 34;
  int luma_num = 0;
  assert(half_width > 0);
  assert(half_height > 0);
  int y = 0;
  do {
    uint16_t seed = params_.grain_seed;
    seed ^= ((luma_num * 37 + 178) & 255) << 8;
    seed ^= ((luma_num * 173 + 105) & 255);
    int x = 0;
    do {
      const int rand = GetRandomNumber(8, &seed);
      const int offset_x = rand >> 4;
      const int offset_y = rand & 15;
      for (int plane = kPlaneY; plane < num_planes; ++plane) {
        const int plane_sub_x = (plane > kPlaneY) ? subsampling_x_ : 0;
        const int plane_sub_y = (plane > kPlaneY) ? subsampling_y_ : 0;
        const int plane_offset_x =
            (plane_sub_x != 0) ? 6 + offset_x : 9 + offset_x * 2;
        const int plane_offset_y =
            (plane_sub_y != 0) ? 6 + offset_y : 9 + offset_y * 2;
        GrainType* const noise_stripe = noise_stripes_[luma_num][plane];
        const int plane_width = (width_ + plane_sub_x) >> plane_sub_x;
        int i = 0;
        do {
          int j = 0;
          do {
            int grain;
            if (plane == kPlaneY) {
              grain = luma_grain_[(plane_offset_y + i) * kLumaWidth +
                                  (plane_offset_x + j)];
            } else if (plane == kPlaneU) {
              grain = u_grain_[(plane_offset_y + i) * chroma_width_ +
                               (plane_offset_x + j)];
            } else {
              grain = v_grain_[(plane_offset_y + i) * chroma_width_ +
                               (plane_offset_x + j)];
            }
            // Section 7.18.3.5 says:
            //   noiseStripe[ lumaNum ][ 0 ] is 34 samples high and w samples
            //   wide (a few additional samples across are actually written to
            //   the array, but these are never read) ...
            //
            // Note: The warning in the parentheses also applies to
            // noiseStripe[ lumaNum ][ 1 ] and noiseStripe[ lumaNum ][ 2 ].
            //
            // The writes beyond the width of each row would happen below. To
            // prevent those writes, we skip the write if the column index
            // (x * 2 + j or x + j) is >= plane_width.
            if (plane_sub_x == 0) {
              if (x * 2 + j >= plane_width) continue;
              if (j < 2 && params_.overlap_flag && x > 0) {
                const int old = noise_stripe[i * plane_width + (x * 2 + j)];
                if (j == 0) {
                  grain = old * 27 + grain * 17;
                } else {
                  grain = old * 17 + grain * 27;
                }
                grain = Clip3(RightShiftWithRounding(grain, 5), kGrainMin,
                              kGrainMax);
              }
              noise_stripe[i * plane_width + (x * 2 + j)] = grain;
            } else {
              if (x + j >= plane_width) continue;
              if (j == 0 && params_.overlap_flag && x > 0) {
                const int old = noise_stripe[i * plane_width + (x + j)];
                grain = old * 23 + grain * 22;
                grain = Clip3(RightShiftWithRounding(grain, 5), kGrainMin,
                              kGrainMax);
              }
              noise_stripe[i * plane_width + (x + j)] = grain;
            }
          } while (++j < (kNoiseStripeHeight >> plane_sub_x));
        } while (++i < (kNoiseStripeHeight >> plane_sub_y));
      }
      x += 16;
    } while (x < half_width);

    ++luma_num;
    y += 16;
  } while (y < half_height);
}

template <int bitdepth>
bool FilmGrain<bitdepth>::AllocateNoiseImage() {
  if (!noise_image_[kPlaneY].Reset(height_, width_,
                                   /*zero_initialize=*/false)) {
    return false;
  }
  if (!is_monochrome_) {
    if (!noise_image_[kPlaneU].Reset(
            (height_ + subsampling_y_) >> subsampling_y_,
            (width_ + subsampling_x_) >> subsampling_x_,
            /*zero_initialize=*/false)) {
      return false;
    }
    if (!noise_image_[kPlaneV].Reset(
            (height_ + subsampling_y_) >> subsampling_y_,
            (width_ + subsampling_x_) >> subsampling_x_,
            /*zero_initialize=*/false)) {
      return false;
    }
  }
  return true;
}

template <int bitdepth>
void FilmGrain<bitdepth>::ConstructNoiseImage() {
  const int num_planes = is_monochrome_ ? kMaxPlanesMonochrome : kMaxPlanes;
  for (int plane = kPlaneY; plane < num_planes; ++plane) {
    const int plane_sub_x = (plane > kPlaneY) ? subsampling_x_ : 0;
    const int plane_sub_y = (plane > kPlaneY) ? subsampling_y_ : 0;
    const int plane_width = (width_ + plane_sub_x) >> plane_sub_x;
    int y = 0;
    do {
      const int luma_num = y >> (5 - plane_sub_y);
      const int i = y - (luma_num << (5 - plane_sub_y));
      int x = 0;
      do {
        int grain = noise_stripes_[luma_num][plane][i * plane_width + x];
        if (plane_sub_y == 0) {
          if (i < 2 && luma_num > 0 && params_.overlap_flag) {
            const int old =
                noise_stripes_[luma_num - 1][plane][(i + 32) * plane_width + x];
            if (i == 0) {
              grain = old * 27 + grain * 17;
            } else {
              grain = old * 17 + grain * 27;
            }
            grain =
                Clip3(RightShiftWithRounding(grain, 5), kGrainMin, kGrainMax);
          }
        } else {
          if (i < 1 && luma_num > 0 && params_.overlap_flag) {
            const int old =
                noise_stripes_[luma_num - 1][plane][(i + 16) * plane_width + x];
            grain = old * 23 + grain * 22;
            grain =
                Clip3(RightShiftWithRounding(grain, 5), kGrainMin, kGrainMax);
          }
        }
        noise_image_[plane][y][x] = grain;
      } while (++x < plane_width);
    } while (++y < ((height_ + plane_sub_y) >> plane_sub_y));
  }
}

template <int bitdepth>
void FilmGrain<bitdepth>::BlendNoiseWithImage(
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v, void* dest_plane_y,
    ptrdiff_t dest_stride_y, void* dest_plane_u, ptrdiff_t dest_stride_u,
    void* dest_plane_v, ptrdiff_t dest_stride_v) const {
  const auto* in_y = static_cast<const Pixel*>(source_plane_y);
  source_stride_y /= sizeof(Pixel);
  const auto* in_u = static_cast<const Pixel*>(source_plane_u);
  source_stride_u /= sizeof(Pixel);
  const auto* in_v = static_cast<const Pixel*>(source_plane_v);
  source_stride_v /= sizeof(Pixel);
  auto* out_y = static_cast<Pixel*>(dest_plane_y);
  dest_stride_y /= sizeof(Pixel);
  auto* out_u = static_cast<Pixel*>(dest_plane_u);
  dest_stride_u /= sizeof(Pixel);
  auto* out_v = static_cast<Pixel*>(dest_plane_v);
  dest_stride_v /= sizeof(Pixel);
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
  const int scaling_shift = params_.chroma_scaling;
  int y = 0;
  do {
    int x = 0;
    do {
      const int luma_x = x << subsampling_x_;
      const int luma_y = y << subsampling_y_;
      const int luma_next_x = std::min(luma_x + 1, width_ - 1);
      int average_luma;
      if (subsampling_x_ != 0) {
        average_luma = RightShiftWithRounding(
            in_y[luma_y * source_stride_y + luma_x] +
                in_y[luma_y * source_stride_y + luma_next_x],
            1);
      } else {
        average_luma = in_y[luma_y * source_stride_y + luma_x];
      }
      if (params_.num_u_points > 0 || params_.chroma_scaling_from_luma) {
        const int orig = in_u[y * source_stride_u + x];
        int merged;
        if (params_.chroma_scaling_from_luma) {
          merged = average_luma;
        } else {
          const int combined = average_luma * params_.u_luma_multiplier +
                               orig * params_.u_multiplier;
          merged =
              Clip3((combined >> 6) + LeftShift(params_.u_offset, bitdepth - 8),
                    0, (1 << bitdepth) - 1);
        }
        int noise = noise_image_[kPlaneU][y][x];
        noise = RightShiftWithRounding(
            ScaleLut<bitdepth>(scaling_lut_u_, merged) * noise, scaling_shift);
        out_u[y * dest_stride_u + x] =
            Clip3(orig + noise, min_value, max_chroma);
      } else {
        out_u[y * dest_stride_u + x] = in_u[y * source_stride_u + x];
      }
      if (params_.num_v_points > 0 || params_.chroma_scaling_from_luma) {
        const int orig = in_v[y * source_stride_v + x];
        int merged;
        if (params_.chroma_scaling_from_luma) {
          merged = average_luma;
        } else {
          const int combined = average_luma * params_.v_luma_multiplier +
                               orig * params_.v_multiplier;
          merged =
              Clip3((combined >> 6) + LeftShift(params_.v_offset, bitdepth - 8),
                    0, (1 << bitdepth) - 1);
        }
        int noise = noise_image_[kPlaneV][y][x];
        noise = RightShiftWithRounding(
            ScaleLut<bitdepth>(scaling_lut_v_, merged) * noise, scaling_shift);
        out_v[y * dest_stride_v + x] =
            Clip3(orig + noise, min_value, max_chroma);
      } else {
        out_v[y * dest_stride_v + x] = in_v[y * source_stride_v + x];
      }
    } while (++x < ((width_ + subsampling_x_) >> subsampling_x_));
  } while (++y < ((height_ + subsampling_y_) >> subsampling_y_));
  if (params_.num_y_points > 0) {
    int y = 0;
    do {
      int x = 0;
      do {
        const int orig = in_y[y * source_stride_y + x];
        int noise = noise_image_[kPlaneY][y][x];
        noise = RightShiftWithRounding(
            ScaleLut<bitdepth>(scaling_lut_y_, orig) * noise, scaling_shift);
        out_y[y * dest_stride_y + x] = Clip3(orig + noise, min_value, max_luma);
      } while (++x < width_);
    } while (++y < height_);
  } else if (in_y != out_y) {  // If in_y and out_y point to the same buffer,
                               // then do nothing.
    const Pixel* in_y_row = in_y;
    Pixel* out_y_row = out_y;
    int y = 0;
    do {
      memcpy(out_y_row, in_y_row, width_ * sizeof(*out_y_row));
      in_y_row += source_stride_y;
      out_y_row += dest_stride_y;
    } while (++y < height_);
  }
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
  ConstructNoiseStripes();

  if (!AllocateNoiseImage()) {
    LIBGAV1_DLOG(ERROR, "AllocateNoiseImage() failed.");
    return false;
  }
  ConstructNoiseImage();

  BlendNoiseWithImage(source_plane_y, source_stride_y, source_plane_u,
                      source_stride_u, source_plane_v, source_stride_v,
                      dest_plane_y, dest_stride_y, dest_plane_u, dest_stride_u,
                      dest_plane_v, dest_stride_v);

  return true;
}

// Explicit instantiations.
template class FilmGrain<8>;
#if LIBGAV1_MAX_BITDEPTH >= 10
template class FilmGrain<10>;
#endif

}  // namespace dsp
}  // namespace libgav1
