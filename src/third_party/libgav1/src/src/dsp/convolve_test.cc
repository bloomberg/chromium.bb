// Copyright 2021 The libgav1 Authors
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

#include "src/dsp/convolve.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <string>
#include <tuple>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"
#include "src/utils/cpu.h"
#include "src/utils/memory.h"
#include "tests/block_utils.h"
#include "tests/third_party/libvpx/acm_random.h"
#include "tests/third_party/libvpx/md5_helper.h"
#include "tests/utils.h"

namespace libgav1 {
namespace dsp {
namespace {

// The convolve function will access at most (block_height + 7) rows/columns
// from the beginning.
constexpr int kMaxBlockWidth = kMaxSuperBlockSizeInPixels + kSubPixelTaps;
constexpr int kMaxBlockHeight = kMaxSuperBlockSizeInPixels + kSubPixelTaps;

// Test all the filters in |kSubPixelFilters|. There are 6 different filters but
// filters [4] and [5] are only reached through GetFilterIndex().
constexpr int kMinimumViableRuns = 4 * 16;

// When is_scaled_convolve_ is true, we don't test every combination of
// type_param_, so some digests in ths array are redudant, marked as
// "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa".
// We keep it so that the logic of calculation id in GetDigestId() is clearer.
const char* GetDigest8bpp(int id) {
  static const char* const kDigest[] = {
      "ae5977a4ceffbac0cde72a04a43a9d57", "fab093b917d36f6b69fb4f50a6b5c822",
      "1168251e6261e2ff1fa69a93226dbd76", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d2f5ca2b7958c332a3fb771f66da01f0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6bbcc075f8b768a02cdc9149f150326d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c4e90cd202f9867517433b550afdc644", "43d6df191744f6c5d489c0673714a714",
      "bfe8197057b0f3f096344251047f481f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1681719b0f8905d99382f4132fe1472a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8d24b59c0f3942079ba4945ed6686269", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ae5977a4ceffbac0cde72a04a43a9d57", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "995318eff1fe62822366490192ad8b5e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0ef1c5beb3228c6d9ecf3ced584c4aa8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "fc02228efb85c665bd27a3dab72a9037", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6cf5f791fe0d8dcd3526be3c6b814035", "eaa0942097fd2b2dd621b77e0a659896",
      "4821befdf63f8c6da6440afeb57f320f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7aec92c3b65e456b64ae285c12b03b0d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4ae70d9db2ec36885394db7d59bdd4f7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "911212ae2492690de06d12bfaf71c7d4", "cb284b0ae039582039563638f682db26",
      "6b4393b2d7387dd291d3a7bd3aabcae4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0804d93136549388b6cd7fdcd187a578", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b25f037602efdb4eaacb3ade1dc5c28f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6cf5f791fe0d8dcd3526be3c6b814035", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "704b0bb4128aa163ef5899e6d8ad9664", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "abf3f31ec4daff000e80f7ab9628688b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "09e12a389cd454e10f750062102ea1b2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d905dfcad930aded7718587c05b48aaf", "fe85aaee8007d2130d56919242e01163",
      "c30fc44d83821141e84cc4793e127301", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f72a99ad63f6a88c23724e898b705d21", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5fee162fe52c11c823db4d5ede370654", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a9210113ff6873e5b50d5d3ad67e440f", "b7633a78f959b20ca27ffb700b44b45c",
      "6d1c5145be9fd636ababd64c64d23a10", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d55d8012ddddb55e6c3e51dafab92980", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b1948cb353fa308f0d5592b0ad338997", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d905dfcad930aded7718587c05b48aaf", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "04e3b7f46e748431c76cf6125057601c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "71362b65cffd008d1ca4a20adc8cc15f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "987f7a6a8bef47acbd1e49bb39f51ac4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6baf153feff04cc5b7e87c0bb60a905d", "fa1ad095bf696745599079fb73975b75",
      "a8293b933d9f2e5d7f922ea40111d643", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "07a1f07f114c4a38ba08d2f44e1e1132", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9365186c59ef66d9def40f437022ad93", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a7305087fae23de53d21a6909009ff69", "bd44440b5757b74bcc3e2f7f32ef42af",
      "a5a1ac658d7ce4a846a32b9fcfaa3475", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3b1ceebf0579fcbbfd6136938c595b91", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3bfad931bce82335219e0e29c15f2b21", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6baf153feff04cc5b7e87c0bb60a905d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4cfad2c437084a93ea76913e21c2dd89", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1a0bdfc96a3b9fd904e658f238ab1076", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b8a710baa6a9fc784909671d450ecd99", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "871ed5a69ca31e6444faa720895949bf", "e55d0c54fd28355d32e29d411488b571",
      "354a54861a94e8b027afd9931e61f997", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "26b9de95edb45b31ac5aa19825831c7a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0f95fb0276c9c7910937fbdf75f2811d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8dcce009395264379c1a51239f4bb22c", "06925f05ea49811e3efc2a44b111b32b",
      "2370f4e4a83edf91b7f504bbe4b00e90", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ecafabcad1045f15d31ce2f3b13132f2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "68a701313d2247d2b32636ebc1f2a008", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "871ed5a69ca31e6444faa720895949bf", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d372f0c17bce98855d6d59fbee814c3d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "56d16e54afe205e97527902770e71c71", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f9e6a56382d8d12da676d6631bb6ef75", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "68e2f90eaa0ab5da7e6f5776993f7eea", "8718965c4831a363a321a25f4aada7ba",
      "eeeb8589c1b31cbb565154736ca939ec", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c1b836a6ce023663b90db0e320389414", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b355dab2dbb6f5869018563eece22862", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8dcce009395264379c1a51239f4bb22c", "e7c2bfd356c860c36053dea19f8d0705",
      "ae5464066a049622a7a264cdf9394b55", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5f211eba020e256a5781b203c5aa1d2e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "05afe1f40d37a45a97a5e0aadd5066fb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "68e2f90eaa0ab5da7e6f5776993f7eea", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d99ffd2579eb781c30bc0df7b76ad61e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1f7b5b8282ff3cf4d8e8c52d80ef5b4d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3bf8e11e18527b16f0d7c0361d74a52d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f1f8282fb33c30eb68c0c315b7a4bc01", "4c718ddbe8b5aa7118c8bc1c2f5ea158",
      "f49dab626ddd977ed171f79295c24935", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5befcf222152ebc8d779fcc10b95320a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cf6ff8c43d8059cea6090a23ab66a0ef", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d90a69e7bae8aa46ed0e1e5f911d7a07", "1d7113d705fa0edeef49e5c50a91151d",
      "45368b6db3d1fee739a64b0bc823ea9c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3b04497634364dd2cd3f2482b5d4b32f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9e1f0e0bddb58d15d0925eeaede9b84c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f1f8282fb33c30eb68c0c315b7a4bc01", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4e139e57cbb049a0f4ef816adc48d026", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "79e9e260a2028c5fe320005c272064b9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b9ff54c6f1e3b41fc7fc0f3fa0e75cf2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9412064b0eebf8123f23d74147d04dff", "0dee657827cd48c4ce4a7657f6f92233",
      "78d2f27e0d4708cb16856d7d40dc16fb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "62adf407fc27d8682ced4dd7b55af14e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a336f8b7bcf188840ca65c0d0e66518a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6ab4dc87be03be1dcc5d956ca819d938", "78cef82670ff99b1e4a279de3538c233",
      "8dff0f28192d9f8c0bf7fb5405719dd8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a8ac7b5dc65ffb758b0643508a0e744e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "03313cdaa593a1a7b4869010dcc7b241", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9412064b0eebf8123f23d74147d04dff", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "be53b2507048e7ff50226d15c0b28865", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2418ebcdf85551b9ae6e3725f04aae6d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "06ef1504f31af5f173d3317866ca57cb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cc08936effe309ab9a4fa1bf7e28e24e", "a81bcdeb021d3a23477c40c47548df52",
      "9d2393ea156a1c2083f5b4207793064b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "35be0786a072bf2f1286989261bf6580", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "de953f03895923359c6a719e6a537b89", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6ab4dc87be03be1dcc5d956ca819d938", "e053321d7c75951d5ff3dce85762acd3",
      "632738ef3ff3021cff45045c41978849", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "561ed8be43c221a561f8885a0d74c7ef", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "88a50d2b4107ee5b5074b2520183f8ac", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cc08936effe309ab9a4fa1bf7e28e24e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b73f3c1a10405de89d1f9e812ff73b5a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "98bdf907ebacacb734c9eef1ee727c6e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "635e8ee11cf04d73598549234ad732a0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "36cbef36fa21b98df03536c918bf752a", "b7a4d080e2f24040eebb785f437de66a",
      "a9c62745b95c66fa497a524886af57e2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "90562fc42dc5d879ae74c4909c1dec30", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8463ade9347ed602663e2cec5c4c3fe6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8f2afdb2f03cd04ffacd421b958caaa0", "2e15a26905467e5ad9f8da04b94e60b6",
      "f7ec43384037e8d6c618e0df826ec029", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8159619fc234598c8c75154d80021fd4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ac50ea9f7306da95a5092709442989cf", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "36cbef36fa21b98df03536c918bf752a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c7d51b1f2df49ab83962257e8a5934e5", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4dd5672d53c8f359e8f80badaa843dfc", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "fab693410d59ee88aa2895527efc31ac", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9d0da6321cf5311ea0bdd41271763030", "22ff7819c55ce6b2e0ce5431eb8c309c",
      "2c614ec4463386ec075a0f1dbb587933", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a1427352f9e413975a0949e2b300c657", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "392de11ffcd5c2ecf3db3480ee135340", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "710ccecc103033088d898a2b924551fb", "160c29a91e372d66b12e171e4d81bc18",
      "a6bc648197781a2dc99c487e66464320", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8f43645dce92cf7594aa4822aa53b17d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "739b17591437edffd36799237b962658", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9d0da6321cf5311ea0bdd41271763030", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "159e443d79cc59b11ca4a80aa7aa09be", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a1bef519bbf07138e2eec5a91694de46", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3041eb26c23a63a587fbec623919e2d2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "55a10165ee8a660d7dddacf7de558cdd", "355b691a656e6a287e4504ef2fbb8034",
      "7a8856480d752153370240b066b90f6a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bcbc418bc2beb243e463851cd95335a9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bddd31e3e852712e6244b616622af83d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "710ccecc103033088d898a2b924551fb", "f6cb80c4d5683553929b1e41f99e487e",
      "1112ebd509007154c72c5a485b220b62", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b6ccddb7dfa4eddc87b4eff08b5a3195", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b8a7eb7dd9c216e240517edfc6489397", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "55a10165ee8a660d7dddacf7de558cdd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6ef14b14882e1465b0482b0e0b16d8ce", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "df1cb51fe1a937cd7834e973dc5cb814", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c61d99d5daf575664fb7ad64976f4b03", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ac7fc9f9ea7213743fae5a023faaaf08", "a6307a981600c3fb5b9d3e89ddf55069",
      "beaef1dbffadc701fccb7c18a03e3a41", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cb8fedcbecee3947358dc61f95e56530", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "30a36245c40d978fc8976b442a8600c3", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a4093e3e5902dd659407ce6471635a4e", "658f0f51eb2f965f7490053852653fc0",
      "9714c4ce636b6fb0ad05cba246d48c76", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b4e605327b28db573d88844a1a09db8d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "75b755f199dbf4a0e5ebbb86c2bd871d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ac7fc9f9ea7213743fae5a023faaaf08", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "22a8d287b425c870f40c64a50f91ce54", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "317fe65abf81ef3ea07976ef8667baeb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "822f6c4eb5db760468d822b21f48d94d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "077e1b7b355c7ab3ca40230ee8efd8ea", "628229ce2484d67e72c51b2f4ad124a6",
      "72b1e700c949d06eaf62d664dafdb5b6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0d0154a7d573685285a83a4cf201ac57", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "93aa662b988b8502e5ea95659eafde59", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "375d7f5358d7a088a498b8b3aaecc0d5", "b726ef75b641c21519ecc2f802bbaf39",
      "2c93dde8884f09fb5bb5ad6d95cde86d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "15b00a15d1cc6cc96ca85d00b167e4dd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "31b0017ba1110e3d70b020901bc15564", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "077e1b7b355c7ab3ca40230ee8efd8ea", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f1d96db5a2e0a2160df38bd96d28d19b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2da29da97806ae0ee300c5e69c35a4aa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3f6fcb9fae3666e085b9e29002a802fc", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7a3e8de2a1caae206cf3e51a86dfd15a", "c266a1b65599686c771fad8a822e7a49",
      "684f5c3a25a080edaf79add6e9137a8e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b14bd8068f108905682b83cc15778065", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "70440ba9ee7f9d16d297dbb49e54a56e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "375d7f5358d7a088a498b8b3aaecc0d5", "4dca696cc0552c1d684c4fc963adc336",
      "a49e6160b5d1b56bc2046963101cd606", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7bf911888c11a9fefd604b8b9c82e9a1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0a1aa8f5ecfd11ddba080af0051c576a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7a3e8de2a1caae206cf3e51a86dfd15a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "637d1e5221422dfe9a6dbcfd7f62ebdd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "555475f5d1685638169ab904447e4f13", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d9b9fecd195736a6049c528d4cb886b5", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1ddf9020f18fa7883355cf8c0881186a", "e681b35b1fe02e2a6698525040015cd0",
      "3be970f49e4288988818b087201d54da", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c96c867d998473197dde9b587be14e3a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1eb2be4c05b50e427e29c72fa566bff5", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "08867ea5cc38c705ec52af821bc4736a", "c51c8bb294f4fa20bdab355ad1e7df37",
      "7f084953976111e9f65b57876e7552b1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bfb69b4d7d4aed73cfa75a0f55b66440", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "536181ee90de883cc383787aec089221", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1ddf9020f18fa7883355cf8c0881186a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f275af4f1f350ffaaf650310cb5dddec", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b3e3a6234e8045e6182cf90a09f767b2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "fed17fc391e6c3db4aa14ea1d6596c87", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2377dd167ef2707978bed6f10ffd4e76", "b1f6c0cd490b584b1883222a4c281e0f",
      "d2b9dba2968894a414756bb510ac389a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f596c63c7b14cada0174e17124c83942", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "52c0980bae63e8459e82eee7d8af2334", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2afb540e8063f58d1b03896486c5e89b", "b929f7956cf35dd6225ca6cf45eacb23",
      "0846ec82555b66197c5c45b08240fbcc", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "034d1d62581bd0d840c4cf1e28227931", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "29f82b0f3e4113944bd28aacd9b8489a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2377dd167ef2707978bed6f10ffd4e76", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f81c4d6b001a14584528880fa6988a87", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "849dfeca59074525dea59681a7f88ab4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d0d3482d981989e117cbb32fc4550267", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f918e0e4422967c6a7e47298135c7ae9", "fc8718e6f9e6663c2b6bf9710f835bfc",
      "9a3215eb97aedbbddd76c7440837d040", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "eb2822ad8204ed4ecbf0f30fcb210498", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "75e57104d6058cd2bce1d3d8142d273d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2afb540e8063f58d1b03896486c5e89b", "d9d9f3c699cd03ab9d698e6b235ddcc6",
      "ca7471c126ccd22189e874f0a6e41960", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8cba849640e9e2859d509bc81ca94acd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ee3e76371240d1f1ff811cea6a7d4f63", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f918e0e4422967c6a7e47298135c7ae9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a5a2f9c2e7759d8a3dec1bc4b56be587", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "39a68af80be11e1682b6f3c4ede33530", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "39561688bf6680054edbfae6035316ce", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b2264e129636368b5496760b39e64b7a", "4dbb4ce94d4948c990a51b15959d2fa6",
      "4e317feac6da46addf0e8b9d8d54304b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "538ce869ffd23b6963e61badfab7712b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b4c735269ade44419169adbd852d5ddc", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6ce47b11d2e60c5d183c84ce9f2e46cc", "3ac8d5b68ebb29fd1a41c5fa9d5f4382",
      "0802b6318fbd0969a33de8fdfcd07f10", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bc79acf2a0fe419194cdb4529bc7dcc8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "17a20dbbf09feae557d40aa5818fbe76", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b2264e129636368b5496760b39e64b7a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2317c57ab69a36eb3bf278cf8a8795a3", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b22d765af176d87e7d3048b4b89b86ad", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "087c5992ca6f829e1ba4ba5332d67947", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9cf1deba08dac5972b3b0a43eff8f98", "84777bdeb84e2530a1c8c1ee432ec934",
      "b384e9e3d81f9f4f9024028fbe451d8b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4e4677a0623d44237eb8d6a622cdc526", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "356d4003477283e157c8d2b5a79d913c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9cf1deba08dac5972b3b0a43eff8f98", "1e58b76ca365b0bd4fd3c4519ec4a500",
      "24accebe2e795b13fcb56dd3abacf53f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "98f584ceaf2d65af997f85d71ceeda1b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9cf1deba08dac5972b3b0a43eff8f98", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1e58b76ca365b0bd4fd3c4519ec4a500", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "24accebe2e795b13fcb56dd3abacf53f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "98f584ceaf2d65af997f85d71ceeda1b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  };
  return kDigest[id];
}

#if LIBGAV1_MAX_BITDEPTH >= 10
const char* GetDigest10bpp(int id) {
  static const char* const kDigest[] = {
      "b1b6903d60501c7bc11e5285beb26a52", "3fa4ebd556ea33cfa7f0129ddfda0c5b",
      "a693b4bd0334a3b98d45e67d3985bb63", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3e787534dff83c22b3033750e448865a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "fd1da8d197cb385f7917cd296d67afb9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d9941769b66d012c68f70accc1a3b664", "98728677401560d7c29ba8bec59c6a00",
      "2924788891caa175bb0725b57de6cbd2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "915a60e7bb2c38ad5a556098230d6092", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a25de86fd8d389c1c75405aac8049b58", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b1b6903d60501c7bc11e5285beb26a52", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cf792b94b1f3f321fa0c1d6362d89c90", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5f1622fde194bd04560b04f13dc47a7c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d935e0ec1d933d0c48fa529be4f998eb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a7855ed75772d7fa815978a202bbcd9f", "cd3e8b96ff6796650e138f5d106d70d4",
      "156de3172d9acf3c7f251cd7a18ad461", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4c91f676a054d582bcae1ca9adb87a31", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a984202c527b757337c605443f376915", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "20a390cc7e06a265ecc1e118f776c25a", "ab0da36b88021ed0efd806a1a4cd4fa0",
      "fc57a318fbf0c0f29c24edbc84e35ec6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "568055866caf274d67e984307cda2742", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3ff2b19730d6bb8b97f4d72085d2d5b8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a7855ed75772d7fa815978a202bbcd9f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "acc8588292b326f15076dd3a3d260072", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f990a13f7a062665d7f18a40bd5da2ae", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "931df73c3d50c4b2e4ec3502bc8774de", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bde291a4e8087c085fe8b3632f4d7351", "555eead3b67766f56b0e3714d431506f",
      "e545b8a3ff958f8363c7968cbae96732", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "eab5894046a99ad0a1a12c91b0f37bd7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c347f4a58fd784c5e88c1a23e4ff15d2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9272ee0820b09bfdc252a97b2e103862", "be8dd418158226a00d5e01ccc3e4f66b",
      "34b37b59ee49108276be28a2e4585c2d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f4deb462014249d4ab02db7f7f62308e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6ae557169928f3be15c7aad8d67205b1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bde291a4e8087c085fe8b3632f4d7351", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "14be0f12550c814f75655b4e1e22ddde", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "af4cadb78ee54aacebac76c8ad275375", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c0c4ebfd6dbbddd88114c36e8c9085da", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "238980eebc9e63ae3eea2771c7a70f12", "661c69a7b49984fa1e92cf8485ab28b6",
      "7842b2047356c1417d9d88219707f1a1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "765b4cfbfc1a4988878c412d53bcb597", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "29cbaadbff9adf4a3d49bd9900a9dd0b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7e3fa9c03bc3dfbdeb67f24c5d9a49cd", "a65e13b534b32fdff3f48d09389daaf1",
      "da1a6ff2be03ec8acde4cb1cd519a6f0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d54206c34785cc3d8a06c2ceac46378c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b1f26ee13df2e14a757416ba8a682278", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "238980eebc9e63ae3eea2771c7a70f12", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "e552466a4e7ff187251b8914b084d404", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aba5d5ef5e96fe418e65d20e506ea834", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "972aeba65e8a6d20dd0f95279be2aa75", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0eac13431bd7d8a573318408a72246d5", "71c57b774e4c3d9b965b060e2a895448",
      "1a487c658d684314d91bb6d961a94672", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bc63b29ec78c1efec5543885a45bb822", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c5997b802a6ba1cf5ba1057ddc5baa7e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f3454ca93cbb0c8c09b0695d90a0df3d", "d259b9c0d0e3322114b2bcce04ae35dd",
      "a4ca37cb869a0dbd1c4a2dcc449a8f31", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "85a11892ed884e3e74968435f6b16e64", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "996b6c166f9ed25bd07ea6acdf7597ff", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0eac13431bd7d8a573318408a72246d5", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "981b7c44b6f7b7ac2acf0cc4096e6bf4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d70bf16e2a31e90b7b3cdeaef1494cf9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "34165457282e2af2e9b3f5840e4dec5d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "73438155feb62595e3e406921102d748", "86d00d2e3dd4a198343f37e3dc4461c9",
      "0635a296be01b7e641de98ee27c33cd2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cecd57396a0033456408f3f3554c6912", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "59f33727e5beeb783a057770bec7b4cd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f3454ca93cbb0c8c09b0695d90a0df3d", "b11f98b5bb864413952d47a67b4add79",
      "1b5d1d4c7be8d5ec00a42a49eecf918f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "16434230d24b9522ae2680e8c37e1b95", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "34895d4c69a6c3303693e6f431bcd5d8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "73438155feb62595e3e406921102d748", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a4c75372af36162831cb872e24e1088c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6df80bb7f264f4f285d09a4d61533fae", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b8c5582b9bbb789c45471f93be83b41f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5871e0e88a776840d619670fbf107858", "57dd2cde826c50e0b0ec504396cb3ceb",
      "82dc120bf8c2043bc5eee81007309ebf", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5b37f94ef136c1eb9a6181c19491459c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0654d72f22306b28d9ae42515845240c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1a77d2af4d2b6cf8737cfbcacacdc4e4", "7123d4aa8083da90ec6986dda0e126ce",
      "98b77e88b0784baaea64c98c8707fe46", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "963dea92f3efbb99137d1de9c56728d3", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9497b00cb1bc3363dd126ffdddadc8e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5871e0e88a776840d619670fbf107858", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "497271227a70a72f9ad25b415d41563f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c8831118d1004a7cca015a4fca140018", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "257bf5467db570974d7cf2356bacf116", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1c6376ce55c9ee9e35d432edb1ffb3b7", "6fff9189c1d11f183f7c42d4ce5febdb",
      "58c826cad3c14cdf26a649265758c58b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "716ba3a25b454e44b46caa42622c128c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6c9d7d9e6ef81d76e775a85c53abe209", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "89bec831efea2f88129dedcad06bb3fa", "e1ef4ae726d864b36a9b64b1e43ede7e",
      "8148788044522edc3c497e1017efe2ce", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b72fb6a9a073c2fe65013af1842dc9b0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1e461869bb2ee9b6069c5e52cf817291", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1c6376ce55c9ee9e35d432edb1ffb3b7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c48bd7e11ec44ba7b2bc8b6a04592439", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b7f82c140369067c105c7967c75b6f9e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5255dded79f56b0078543b5a1814a668", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d675e0195c9feca956e637f3f1959f40", "670fa8c31c82fced9a810b64c03e87ee",
      "f166254037c0dfb140f54cd7b08bddfe", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9076f58c4ab20f2f06d701a6b53b1c4f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a35f435ccc67717a49251a07e62ae204", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "89bec831efea2f88129dedcad06bb3fa", "7c3a79a90f3f4b460540e796f3197ef1",
      "acf60abeda98bbea161139b915317423", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "86fa0c299737eb499cbcdce94abe2d33", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8d7f1d7ea6a0dcc922ad5d2e77bc74dd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d675e0195c9feca956e637f3f1959f40", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0960a9af91250e9faa1eaac32227bf6f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "130f47aae365aabfec4360fa5b5ff554", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ef745100f5f34c8ff841b2b0b57eb33f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b5681673903ade13d69e295f82fdd009", "9ccd4cc6216eab35ddcb66a76b55dd2f",
      "74ab206f14ac5f62653cd3dd71a7916d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d3212ab3922f147c3cf126c3b1aa17f6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c5325015cb0b7c42839ac4aa21803fa0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "dead0fe4030085c22e92d16bb110de9d", "3c6d97f25d6bc647c843850be007f512",
      "262c96b1f2c4f85c86c0e9c77fedff1e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6b80af04470b83673d98f46925e678a5", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "138855d9bf0ccd0c62ac14c7bff4fd37", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b5681673903ade13d69e295f82fdd009", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "746c2e0f96ae2246d534d67102be068c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "92483ed631de21b685ffe6ccadbbec8f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "edae8ed67286ca6a31573a541b3deb6f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3c43020105ae93a301404b4cd6238654", "cef7cfdcb8ca8d2612f31a1fe95ce371",
      "5621caef7cc1d6522903290ccc5c2cb8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "b55fea77f0e14a8bf8b6562b766fe91f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f81f31f1585c0f70438c09e829416f20", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "306a2f5dfd675df4ed9af44fd5cac8c0", "1dfda318021a05a7e72fd815ddb0dfc8",
      "f35a3d13516440f9168076d9b07c9e98", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "65baca6167fe5249f7a839ce5b2fd591", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "64035142864914d05a48ef8e013631d0", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3c43020105ae93a301404b4cd6238654", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d6f6db079da9b8909a153c07cc9d0e63", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cbb6ab31547df6b91cfb48630fdffb48", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "01adcd8bf15fbf70df47fbf3a953aa14", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "dd2c5880a94ed3758bfea0b0e8c78286", "5f6c1725f4c7c73a8d8f0d9468106624",
      "78ec6cf42cce4b1feb65e076c78ca241", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "59b578268ff26a1e21c5b4273f73f852", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ab10b22fb8dd8199040745565b28595d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "306a2f5dfd675df4ed9af44fd5cac8c0", "9209f83153ef6f09b5262536a2dc1671",
      "13782526fc2726100cb3cf375b3150ed", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "e47ded6c0eec1d5baadd02aff172f2b1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "205904fa3c644433b46e01c11dd2fe40", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "dd2c5880a94ed3758bfea0b0e8c78286", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7c8928a0d769f4264d195f39cb68a772", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1eea5e8a24d6aa11778eb3e5e5e9c9f2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ba539808a8501609ce052a1562a62b25", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4ebb1a7b25a39d8b9868ec8a1243103f", "c2732a08997e1f5176dfb297d2e89235",
      "42188e2dbb4e02cd353552ea147ad03f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "16761e7c8ba2645718153bed83ae78f6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0d928d6111f86c60ccefc6c6604d5659", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9d01c946a12f5ef9d9cebd9816e06014", "d738eb9f3f4f0b412b93687b55b6e45a",
      "13c07441b47b0c1ed80f015ac302d220", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c0950e609f278efb7050d319a9756bb3", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "291425aaf8206b20e88db8ebf3cf7e7f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4ebb1a7b25a39d8b9868ec8a1243103f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "db645c96fc8be04015e0eb538afec9ae", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9e193b6b28ce798c44c744efde19eee9", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ac8e6391200cec2abdebb00744a2ba82", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d34ec07845cd8523651e5f5112984a14", "745c794b557d4a0d734e45d720a7f7ad",
      "f9813870fc27941a7c00a0443d7c2fe7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a9e9805769fe1baf5c7933793ccca0d8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4ed1a6200912995d4f571bdb7822aa83", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "768f63912e43148c13688d7f23281531", "43fb786fd2e79610d6a6d912b95f4509",
      "02880fde51ac991ad18d8986f4e5145c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9051290279237f9fb1389989b142d2dd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "cb6238b8eb6b72980958e6fcceb2f2eb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d34ec07845cd8523651e5f5112984a14", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "946af3a8f5362def5f4e27cb0fd4e754", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "885c384d90aaa34acd8303958033c252", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "54b17120f7d71ddb4d70590ecd231cc1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2ce55308d873f4cd244f16da2b06e06e", "af7b76d3471cfbdc97d1e57bc2876ce7",
      "20b14a6b5af7aa356963bcaaf23d230d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "553a2c24939dff18ec5833c77f556cfb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "92e31a45513582f386dc9c22a57bbbbd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "768f63912e43148c13688d7f23281531", "4e255554dab9dfa1064e20a905538308",
      "aa25073115bad49432953254e7dce0bc", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "34cdc1be291c95981c98812c5c343a15", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "626321a6dfac542d0fc70321fac13ff3", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2ce55308d873f4cd244f16da2b06e06e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7ad78dfe7bbedf696dd58d9ad01bcfba", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "8110ed10e7234851dff3c7e4a51108a2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f6e36446a97611a4db4425df926974b2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a4bb5d5ff4b25f391265b5231049a09a", "cf4867c6b1b8be86a7e0bee708c28d83",
      "9c9c41435697f75fa118b6d6464ee7cb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5c1ec75a160c444fa90abf106fa1140e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6dbf310a9c8d85f76306d6a35545f8af", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2e7927158e7b8e40e7269fc909fb584b", "8b72feff8bb0901229a2bd7da2857c4b",
      "69e3361b7199e10e75685b90fb0df623", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "5b64a6911cb7c3d60bb8f961ed9782a2", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "1c6fda7501e0f8bdad972f7857cd9354", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a4bb5d5ff4b25f391265b5231049a09a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "f0fd9c09d454e4ce918faa97e9ac10be", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "6fb9383302eb7e7a13387464d2634e03", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "a82f4080699300b659bbe1b5c4463147", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9106e0c820b03bcdde3aa94efc11a3e", "0408e10e51a31ac756a57d5149a2b409",
      "38816245ed832ba313fefafcbed1e5c8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2266840f11ac4c066d941ec473b1a54f", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "80fce29dc82d5857c1ed5ef2aea16835", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "123028e18c2bfb334e34adb5a4f67de4", "1670eb8ed876e609ed81236a683b4a3d",
      "2f8ab35f6e7030e82ca922a68b29af4a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7133de9d03a4b07716a12226b5e493e8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4fd485dadcb570e5a0a5addaf9ba84da", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c9106e0c820b03bcdde3aa94efc11a3e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "af6ae5c0eb28417bd251184baf2eaba7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "864d51fcc737bc73a3f588b67515039a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "ecedb178f7cad3dc1b921eca67f9efb6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7ec2eae9e118506da8b33440b399511a", "108a4a6530a6b9c933ccf14edbd896be",
      "5d34137cc8ddba75347b0fa1d0a91791", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "9e194755b2a37b615a517d5f8746dfbb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "14f2c5b9d2cd621c178a39f1ec0c38eb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "123028e18c2bfb334e34adb5a4f67de4", "2fdc713ba418780d0be33a3ebbcb323c",
      "452f91b01833c57db4e909575a029ff6", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "3594eff52d5ed875bd9655ddbf106fae", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d3f140aea9e8eabf4e1e5190e0148288", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "7ec2eae9e118506da8b33440b399511a", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "866f8df540dd3b58ab1339314d139cbd", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2ecb7890f00234bcb28c1d969f489012", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "0609ca0ff3ca90069e8b48829b4b0891", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "78de867c8ee947ed6d29055747f26949", "0a7cb4f51f1acf0940b59295b2327465",
      "465dcb046a0449b9dfb3e0b297aa3863", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbf86f8174334f0b8d869fd8d58bf92d", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "da54cfb4530841bda29966cfa05f4879", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "2c979c2bddef79a760e72a802f83cc76", "545426be3436073ba63790aa3c4a5598",
      "1fabf0655bedb671e4d7287fec8119ba", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "90d7e13aa2f9a064493ff2b3b5b12109", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "e4938219593bbed5ae638a93f2f4a580", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "78de867c8ee947ed6d29055747f26949", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "72803589b453a29501540aeddc23e6f4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "c4793d431dbf2d88826bb440bf027512", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "839e86c681e97359f7819c766000dd1c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d05a237ed7a9ca877256b71555b1b8e4", "3052776d186fca6dd8011f4fe908a212",
      "94b3e5bcd6b849b66a4571ec3d23f9be", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "91d6bdbc62d4bb80c9b371d9704e3c9e", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "4f750f6375524311d260306deb233861", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d05a237ed7a9ca877256b71555b1b8e4", "03ce2d07cac044d6b68604d398571844",
      "68ece92dcbe70a2ae9776d72972740a7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "380d296d0d55a49dd86ee562b053a9d8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "d05a237ed7a9ca877256b71555b1b8e4", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "03ce2d07cac044d6b68604d398571844", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "68ece92dcbe70a2ae9776d72972740a7", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "380d296d0d55a49dd86ee562b053a9d8", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  };
  return kDigest[id];
}
#endif

struct ConvolveTestParam {
  ConvolveTestParam(int width, int height) : width(width), height(height) {}
  int width;
  int height;
};

struct ConvolveTypeParam {
  ConvolveTypeParam(bool is_intra_block_copy, bool is_compound,
                    bool has_vertical_filter, bool has_horizontal_filter)
      : is_intra_block_copy(is_intra_block_copy),
        is_compound(is_compound),
        has_vertical_filter(has_vertical_filter),
        has_horizontal_filter(has_horizontal_filter) {}
  bool is_intra_block_copy;
  bool is_compound;
  bool has_vertical_filter;
  bool has_horizontal_filter;
};

std::ostream& operator<<(std::ostream& os, const ConvolveTestParam& param) {
  return os << "BlockSize" << param.width << "x" << param.height;
}

std::ostream& operator<<(std::ostream& os, const ConvolveTypeParam& param) {
  return os << "is_intra_block_copy: " << param.is_intra_block_copy
            << ", is_compound: " << param.is_compound
            << ", has_(vertical/horizontal)_filter: "
            << param.has_vertical_filter << "/" << param.has_horizontal_filter;
}

// TODO(b/146062680): split this to ConvolveTest and ConvolveScaleTest to
// simplify the members and test logic.
template <int bitdepth, typename Pixel>
class ConvolveTest
    : public ::testing::TestWithParam<
          std::tuple<ConvolveTestParam, ConvolveTypeParam, bool>> {
 public:
  ConvolveTest() = default;
  ~ConvolveTest() override = default;

  void SetUp() override {
    ConvolveInit_C();

    const Dsp* const dsp = GetDspTable(bitdepth);
    ASSERT_NE(dsp, nullptr);
    GetConvolveFuncs(dsp, &base_convolve_func_, &base_convolve_scale_func_);

    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const absl::string_view test_case = test_info->test_suite_name();
    if (absl::StartsWith(test_case, "C/")) {
      base_convolve_func_ = nullptr;
      base_convolve_scale_func_ = nullptr;
    } else if (absl::StartsWith(test_case, "SSE41/")) {
      if ((GetCpuInfo() & kSSE4_1) != 0) {
        ConvolveInit_SSE4_1();
      }
    } else if (absl::StartsWith(test_case, "AVX2/")) {
      if ((GetCpuInfo() & kAVX2) != 0) {
        ConvolveInit_AVX2();
      }
    } else if (absl::StartsWith(test_case, "NEON/")) {
      ConvolveInit_NEON();
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }

    GetConvolveFuncs(dsp, &cur_convolve_func_, &cur_convolve_scale_func_);

    // Skip functions that have not been specialized for this particular
    // architecture.
    if (cur_convolve_func_ == base_convolve_func_) {
      cur_convolve_func_ = nullptr;
    }
    if (cur_convolve_scale_func_ == base_convolve_scale_func_) {
      cur_convolve_scale_func_ = nullptr;
    }
  }

 protected:
  int GetDigestId() const {
    // id is the combination of the 3-dimension array:
    // (param_, type_param_, is_scaled_convolve_)
    // The number of each array is 20, 16, 2.
    // The range of id is from 0 to 20x16x2 - 1.
    // is_scaled_convolve_: false, id += 0; true, id += 1;
    // type_param_: (0, 0, 0, 0), id += 0 * 2.
    // (0, 0, 0, 1), id += 1 * 2; (0, 0, 1, 0), id += 2 * 2;
    // ...
    // param_: (2, 2), id += 0 * 32; (2, 4), id += 1 * 32;
    // (4, 2), id += 2 * 32; (4, 4), id += 3 * 32;
    // ...
    int id = static_cast<int>(is_scaled_convolve_);
    id += 2 * static_cast<int>(type_param_.has_horizontal_filter);
    id += 2 * 2 * static_cast<int>(type_param_.has_vertical_filter);
    id += 2 * 4 * static_cast<int>(type_param_.is_compound);
    id += 2 * 8 * static_cast<int>(type_param_.is_intra_block_copy);
    if (param_.width == param_.height) {
      id += 32 * 3 * static_cast<int>(std::log2(param_.width) - 1);
    } else if (param_.width < param_.height) {
      id += 32 * (1 + 3 * static_cast<int>(std::log2(param_.width) - 1));
    } else {
      // param_.width > param_.height
      if (param_.width == 8 && param_.height == 2) {
        // Special case is at the end of the array.
        id += 32 * 19;
      } else {
        id += 32 * (2 + 3 * static_cast<int>(std::log2(param_.height) - 1));
      }
    }
    return id;
  }

  void GetConvolveFuncs(const Dsp* dsp, ConvolveFunc* func,
                        ConvolveScaleFunc* scale_func);
  void SetInputData(bool use_fixed_values, int value);
  void Check(bool use_fixed_values, const Pixel* src, const Pixel* dest,
             libvpx_test::MD5* md5_digest);
  void Check16Bit(bool use_fixed_values, const uint16_t* src,
                  const uint16_t* dest, libvpx_test::MD5* md5_digest);
  // |num_runs| covers the categories of filters (6) and the number of filters
  // under each category (16).
  void Test(bool use_fixed_values, int value,
            int num_runs = kMinimumViableRuns);

  const ConvolveTestParam param_ = std::get<0>(GetParam());
  const ConvolveTypeParam type_param_ = std::get<1>(GetParam());
  const bool is_scaled_convolve_ = std::get<2>(GetParam());

 private:
  ConvolveFunc base_convolve_func_;
  ConvolveFunc cur_convolve_func_;
  ConvolveScaleFunc base_convolve_scale_func_;
  ConvolveScaleFunc cur_convolve_scale_func_;
  // Convolve filters are 7-tap, which needs 3 pixels (kRestorationBoder)
  // padding.
  // When is_scaled_convolve_ is true, the source can be at most 2 times of
  // max width/height. So we allocate a larger memory for it and setup the
  // extra memory when is_scaled_convolve_ is true.
  Pixel source_[kMaxBlockHeight * kMaxBlockWidth * 4] = {};
  uint16_t source_16bit_[kMaxBlockHeight * kMaxBlockWidth * 4] = {};
  uint16_t dest_16bit_[kMaxBlockHeight * kMaxBlockWidth] = {};
  Pixel dest_clipped_[kMaxBlockHeight * kMaxBlockWidth] = {};

  const int source_stride_ =
      is_scaled_convolve_ ? kMaxBlockWidth * 2 : kMaxBlockWidth;
  const int source_height_ =
      is_scaled_convolve_ ? kMaxBlockHeight * 2 : kMaxBlockHeight;
};

template <int bitdepth, typename Pixel>
void ConvolveTest<bitdepth, Pixel>::GetConvolveFuncs(
    const Dsp* const dsp, ConvolveFunc* func, ConvolveScaleFunc* scale_func) {
  if (is_scaled_convolve_) {
    *func = nullptr;
    *scale_func = dsp->convolve_scale[type_param_.is_compound];
  } else {
    *scale_func = nullptr;
    *func =
        dsp->convolve[type_param_.is_intra_block_copy][type_param_.is_compound]
                     [type_param_.has_vertical_filter]
                     [type_param_.has_horizontal_filter];
  }
}

template <int bitdepth, typename Pixel>
void ConvolveTest<bitdepth, Pixel>::SetInputData(bool use_fixed_values,
                                                 int value) {
  if (use_fixed_values) {
    std::fill(source_, source_ + source_height_ * source_stride_, value);
  } else {
    const int offset =
        kConvolveBorderLeftTop * source_stride_ + kConvolveBorderLeftTop;
    const int mask = (1 << bitdepth) - 1;
    libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed());
    const int height = is_scaled_convolve_ ? param_.height * 2 : param_.height;
    const int width = is_scaled_convolve_ ? param_.width * 2 : param_.width;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        source_[y * source_stride_ + x + offset] = rnd.Rand16() & mask;
      }
    }
    // Copy border pixels to the left and right borders.
    for (int y = 0; y < height; ++y) {
      Memset(&source_[(y + kConvolveBorderLeftTop) * source_stride_],
             source_[y * source_stride_ + offset], kConvolveBorderLeftTop);
      Memset(&source_[y * source_stride_ + offset + width],
             source_[y * source_stride_ + offset + width - 1],
             kConvolveBorderLeftTop);
    }
    // Copy border pixels to the top and bottom borders.
    for (int y = 0; y < kConvolveBorderLeftTop; ++y) {
      memcpy(&source_[y * source_stride_],
             &source_[kConvolveBorderLeftTop * source_stride_],
             source_stride_ * sizeof(Pixel));
      memcpy(&source_[(y + kConvolveBorderLeftTop + height) * source_stride_],
             &source_[(kConvolveBorderLeftTop + height - 1) * source_stride_],
             source_stride_ * sizeof(Pixel));
    }
  }
}

template <int bitdepth, typename Pixel>
void ConvolveTest<bitdepth, Pixel>::Check(bool use_fixed_values,
                                          const Pixel* src, const Pixel* dest,
                                          libvpx_test::MD5* md5_digest) {
  if (use_fixed_values) {
    // For fixed values, input and output are identical.
    const bool success =
        test_utils::CompareBlocks(src, dest, param_.width, param_.height,
                                  kMaxBlockWidth, kMaxBlockWidth, false, false);
    EXPECT_TRUE(success);
  } else {
    // For random input, compare md5.
    const int offset =
        kConvolveBorderLeftTop * kMaxBlockWidth + kConvolveBorderLeftTop;
    const size_t size = sizeof(dest_clipped_) - offset * sizeof(Pixel);
    md5_digest->Add(reinterpret_cast<const uint8_t*>(dest), size);
  }
}

template <int bitdepth, typename Pixel>
void ConvolveTest<bitdepth, Pixel>::Check16Bit(bool use_fixed_values,
                                               const uint16_t* src,
                                               const uint16_t* dest,
                                               libvpx_test::MD5* md5_digest) {
  if (use_fixed_values) {
    // For fixed values, input and output are identical.
    const bool success =
        test_utils::CompareBlocks(src, dest, param_.width, param_.height,
                                  kMaxBlockWidth, kMaxBlockWidth, false);
    EXPECT_TRUE(success);
  } else {
    // For random input, compare md5.
    const int offset =
        kConvolveBorderLeftTop * kMaxBlockWidth + kConvolveBorderLeftTop;
    const size_t size = sizeof(dest_16bit_) - offset * sizeof(uint16_t);
    md5_digest->Add(reinterpret_cast<const uint8_t*>(dest), size);
  }
}

template <int bitdepth, typename Pixel>
void ConvolveTest<bitdepth, Pixel>::Test(bool use_fixed_values, int value,
                                         int num_runs /*= 16 * 6*/) {
  // There's no meaning testing fixed input in compound convolve.
  if (type_param_.is_compound && use_fixed_values) GTEST_SKIP();

  // Scaled convolve does not behave differently under most params. Only need to
  // test the enabled compound implementation.
  if (is_scaled_convolve_ &&
      (type_param_.is_intra_block_copy || type_param_.has_vertical_filter ||
       type_param_.has_horizontal_filter)) {
    GTEST_SKIP();
  }

  // There should not be any function set for this combination.
  if (type_param_.is_intra_block_copy && type_param_.is_compound) {
    ASSERT_EQ(cur_convolve_func_, nullptr);
    return;
  }

  // Compound and intra block copy functions are only used for blocks 4x4 or
  // greater.
  if (type_param_.is_compound || type_param_.is_intra_block_copy) {
    if (param_.width < 4 || param_.height < 4) {
      GTEST_SKIP();
    }
  }

  // Skip unspecialized functions.
  if (cur_convolve_func_ == nullptr && cur_convolve_scale_func_ == nullptr) {
    GTEST_SKIP();
  }

  SetInputData(use_fixed_values, value);
  libvpx_test::ACMRandom rnd(libvpx_test::ACMRandom::DeterministicSeed() +
                             GetDigestId());
  // [1,2048] for |step_[xy]|. This covers a scaling range of 1/1024 to 2x.
  const int step_x = (rnd.Rand16() & ((1 << 11) - 1)) + 1;
  const int step_y = (rnd.Rand16() & ((1 << 11) - 1)) + 1;
  int subpixel_x = 0;
  int subpixel_y = 0;
  int vertical_index = 0;
  int horizontal_index = 0;
  const int offset =
      kConvolveBorderLeftTop * kMaxBlockWidth + kConvolveBorderLeftTop;
  const int offset_scale =
      kConvolveBorderLeftTop * source_stride_ + kConvolveBorderLeftTop;
  const Pixel* const src = source_ + offset;
  const Pixel* const src_scale = source_ + offset_scale;
  const ptrdiff_t src_stride = source_stride_ * sizeof(Pixel);
  const ptrdiff_t src_stride_16 = source_stride_;
  const ptrdiff_t dst_stride = kMaxBlockWidth * sizeof(Pixel);
  // Pack Compound output since we control the predictor buffer.
  const ptrdiff_t dst_stride_compound = param_.width;

  // Output is always 16 bits regardless of |bitdepth|.
  uint16_t* dst_16 = dest_16bit_ + offset;
  // Output depends on |bitdepth|.
  Pixel* dst_pixel = dest_clipped_ + offset;

  // Collect the first |kMinimumViableRuns| into one md5 buffer.
  libvpx_test::MD5 md5_digest;

  absl::Duration elapsed_time;
  for (int i = 0; i < num_runs; ++i) {
    // Test every filter.
    // Because of masking |subpixel_{x,y}| values roll over every 16 iterations.
    subpixel_x += 1 << 6;
    subpixel_y += 1 << 6;

    const int horizontal_filter_id = (subpixel_x >> 6) & 0xF;
    const int vertical_filter_id = (subpixel_y >> 6) & 0xF;

    // |filter_id| == 0 (copy) must be handled by the appropriate 1D or copy
    // function.
    if (horizontal_filter_id == 0 || vertical_filter_id == 0) {
      continue;
    }

    // For focused speed testing these can be set to the desired filter. Want
    // only 8 tap filters? Set |{vertical,horizontal}_index| to 2.
    vertical_index += static_cast<int>(i % 16 == 0);
    vertical_index %= 4;
    horizontal_index += static_cast<int>(i % 16 == 0);
    horizontal_index %= 4;

    if (is_scaled_convolve_) {
      ASSERT_EQ(cur_convolve_func_, nullptr);
      // Output type is uint16_t.
      const absl::Time start = absl::Now();
      if (type_param_.is_compound) {
        cur_convolve_scale_func_(
            source_, src_stride, horizontal_index, vertical_index, 0, 0, step_x,
            step_y, param_.width, param_.height, dst_16, dst_stride_compound);
      } else {
        cur_convolve_scale_func_(
            source_, src_stride, horizontal_index, vertical_index, 0, 0, step_x,
            step_y, param_.width, param_.height, dst_pixel, dst_stride);
      }
      elapsed_time += absl::Now() - start;
    } else if (type_param_.is_compound) {
      ASSERT_EQ(cur_convolve_scale_func_, nullptr);
      // Output type is uint16_t.
      const absl::Time start = absl::Now();
      cur_convolve_func_(src, src_stride, horizontal_index, vertical_index,
                         horizontal_filter_id, vertical_filter_id, param_.width,
                         param_.height, dst_16, dst_stride_compound);
      elapsed_time += absl::Now() - start;
    } else {
      ASSERT_EQ(cur_convolve_scale_func_, nullptr);
      // Output type is Pixel.
      const absl::Time start = absl::Now();
      cur_convolve_func_(src, src_stride, horizontal_index, vertical_index,
                         horizontal_filter_id, vertical_filter_id, param_.width,
                         param_.height, dst_pixel, dst_stride);
      elapsed_time += absl::Now() - start;
    }

    // Only check the output for the first set. After that it's just repeated
    // runs for speed timing.
    if (i >= kMinimumViableRuns) continue;

    if (is_scaled_convolve_) {
      // Convolve function does not clip the output. The clipping is applied
      // later. But libaom clips the output. So we apply clipping to match
      // libaom in tests.
      if (type_param_.is_compound) {
        const int single_round_offset = (1 << bitdepth) + (1 << (bitdepth - 1));
        Pixel* dest_row = dest_clipped_;
        for (int y = 0; y < kMaxBlockHeight; ++y) {
          for (int x = 0; x < kMaxBlockWidth; ++x) {
            dest_row[x] = static_cast<Pixel>(Clip3(
                dest_16bit_[y * dst_stride_compound + x] - single_round_offset,
                0, (1 << bitdepth) - 1));
          }
          dest_row += kMaxBlockWidth;
        }
      }

      if (type_param_.is_compound) {
        Check16Bit(use_fixed_values, source_16bit_ + offset_scale, dst_16,
                   &md5_digest);
      } else {
        Check(use_fixed_values, src_scale, dst_pixel, &md5_digest);
      }
    } else if (type_param_.is_compound) {
      // Need to copy source to a uint16_t buffer for comparison.
      Pixel* src_ptr = source_;
      uint16_t* src_ptr_16 = source_16bit_;
      for (int y = 0; y < kMaxBlockHeight; ++y) {
        for (int x = 0; x < kMaxBlockWidth; ++x) {
          src_ptr_16[x] = src_ptr[x];
        }
        src_ptr += src_stride_16;
        src_ptr_16 += src_stride_16;
      }

      Check16Bit(use_fixed_values, source_16bit_ + offset, dst_16, &md5_digest);
    } else {
      Check(use_fixed_values, src, dst_pixel, &md5_digest);
    }
  }

  if (!use_fixed_values) {
    // md5 sums are only calculated for random input.
    const char* ref_digest;
    if (bitdepth == 8) {
      ref_digest = GetDigest8bpp(GetDigestId());
    } else {
#if LIBGAV1_MAX_BITDEPTH >= 10
      ref_digest = GetDigest10bpp(GetDigestId());
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
    }
    const char* direction;
    if (is_scaled_convolve_ || (type_param_.has_vertical_filter &&
                                type_param_.has_horizontal_filter)) {
      direction = "2D";
    } else if (type_param_.has_vertical_filter) {
      direction = "Vertical";
    } else if (type_param_.has_horizontal_filter) {
      direction = "Horizontal";
    } else {
      direction = "Copy";
    }
    const auto elapsed_time_us =
        static_cast<int>(absl::ToInt64Microseconds(elapsed_time));
    printf("Mode Convolve%s%s%s%s[%25s]: %5d us MD5: %s\n",
           type_param_.is_compound ? "Compound" : "",
           type_param_.is_intra_block_copy ? "IntraBlockCopy" : "",
           is_scaled_convolve_ ? "Scale" : "", direction,
           absl::StrFormat("%dx%d", param_.width, param_.height).c_str(),
           elapsed_time_us, md5_digest.Get());
    EXPECT_STREQ(ref_digest, md5_digest.Get());
  }
}

void ApplyFilterToSignedInput(const int min_input, const int max_input,
                              const int8_t filter[kSubPixelTaps],
                              int* min_output, int* max_output) {
  int min = 0, max = 0;
  for (int i = 0; i < kSubPixelTaps; ++i) {
    const int tap = filter[i];
    if (tap > 0) {
      max += max_input * tap;
      min += min_input * tap;
    } else {
      min += max_input * tap;
      max += min_input * tap;
    }
  }
  *min_output = min;
  *max_output = max;
}

void ApplyFilterToUnsignedInput(const int max_input,
                                const int8_t filter[kSubPixelTaps],
                                int* min_output, int* max_output) {
  ApplyFilterToSignedInput(0, max_input, filter, min_output, max_output);
}

// Validate the maximum ranges for different parts of the Convolve process.
template <int bitdepth>
void ShowRange() {
  // Subtract one from the shift bits because the filter is pre-shifted by 1.
  constexpr int horizontal_bits = (bitdepth == kBitdepth12)
                                      ? kInterRoundBitsHorizontal12bpp - 1
                                      : kInterRoundBitsHorizontal - 1;
  constexpr int vertical_bits = (bitdepth == kBitdepth12)
                                    ? kInterRoundBitsVertical12bpp - 1
                                    : kInterRoundBitsVertical - 1;
  constexpr int compound_vertical_bits = kInterRoundBitsCompoundVertical - 1;

  constexpr int compound_offset = (bitdepth == 8) ? 0 : kCompoundOffset;

  constexpr int max_input = (1 << bitdepth) - 1;

  const int8_t* worst_convolve_filter = kHalfSubPixelFilters[2][8];

  // First pass.
  printf("Bitdepth: %2d Input range:            [%8d, %8d]\n", bitdepth, 0,
         max_input);

  int min, max;
  ApplyFilterToUnsignedInput(max_input, worst_convolve_filter, &min, &max);

  if (bitdepth == 8) {
    // 8bpp can use int16_t for sums.
    assert(min > INT16_MIN);
    assert(max < INT16_MAX);
  } else {
    // 10bpp and 12bpp require int32_t.
    assert(min > INT32_MIN);
    assert(max > INT16_MAX && max < INT32_MAX);
  }

  printf("  intermediate range:                [%8d, %8d]\n", min, max);

  const int first_pass_min = RightShiftWithRounding(min, horizontal_bits);
  const int first_pass_max = RightShiftWithRounding(max, horizontal_bits);

  // All bitdepths can use int16_t for first pass output.
  assert(first_pass_min > INT16_MIN);
  assert(first_pass_max < INT16_MAX);

  printf("  first pass output range:           [%8d, %8d]\n", first_pass_min,
         first_pass_max);

  // Second pass.
  ApplyFilterToSignedInput(first_pass_min, first_pass_max,
                           worst_convolve_filter, &min, &max);

  // All bitdepths require int32_t for second pass sums.
  assert(min < INT16_MIN && min > INT32_MIN);
  assert(max > INT16_MAX && max < INT32_MAX);

  printf("  intermediate range:                [%8d, %8d]\n", min, max);

  // Second pass non-compound output is clipped to Pixel values.
  const int second_pass_min =
      Clip3(RightShiftWithRounding(min, vertical_bits), 0, max_input);
  const int second_pass_max =
      Clip3(RightShiftWithRounding(max, vertical_bits), 0, max_input);
  printf("  second pass output range:          [%8d, %8d]\n", second_pass_min,
         second_pass_max);

  // Output is Pixel so matches Pixel values.
  assert(second_pass_min == 0);
  assert(second_pass_max == max_input);

  const int compound_second_pass_min =
      RightShiftWithRounding(min, compound_vertical_bits) + compound_offset;
  const int compound_second_pass_max =
      RightShiftWithRounding(max, compound_vertical_bits) + compound_offset;

  printf("  compound second pass output range: [%8d, %8d]\n",
         compound_second_pass_min, compound_second_pass_max);

  if (bitdepth == 8) {
    // 8bpp output is int16_t without an offset.
    assert(compound_second_pass_min > INT16_MIN);
    assert(compound_second_pass_max < INT16_MAX);
  } else {
    // 10bpp and 12bpp use the offset to fit inside uint16_t.
    assert(compound_second_pass_min > 0);
    assert(compound_second_pass_max < UINT16_MAX);
  }

  printf("\n");
}

TEST(ConvolveTest, ShowRange) {
  ShowRange<kBitdepth8>();
  ShowRange<kBitdepth10>();
  ShowRange<kBitdepth12>();
}

using ConvolveTest8bpp = ConvolveTest<8, uint8_t>;

TEST_P(ConvolveTest8bpp, FixedValues) {
  Test(true, 0);
  Test(true, 1);
  Test(true, 128);
  Test(true, 255);
}

TEST_P(ConvolveTest8bpp, RandomValues) { Test(false, 0); }

TEST_P(ConvolveTest8bpp, DISABLED_Speed) {
  const int num_runs = static_cast<int>(1.0e7 / (param_.width * param_.height));
  Test(false, 0, num_runs);
}

const ConvolveTestParam kConvolveParam[] = {
    ConvolveTestParam(2, 2),    ConvolveTestParam(2, 4),
    ConvolveTestParam(4, 2),    ConvolveTestParam(4, 4),
    ConvolveTestParam(4, 8),    ConvolveTestParam(8, 2),
    ConvolveTestParam(8, 4),    ConvolveTestParam(8, 8),
    ConvolveTestParam(8, 16),   ConvolveTestParam(16, 8),
    ConvolveTestParam(16, 16),  ConvolveTestParam(16, 32),
    ConvolveTestParam(32, 16),  ConvolveTestParam(32, 32),
    ConvolveTestParam(32, 64),  ConvolveTestParam(64, 32),
    ConvolveTestParam(64, 64),  ConvolveTestParam(64, 128),
    ConvolveTestParam(128, 64), ConvolveTestParam(128, 128),
};

const ConvolveTypeParam kConvolveTypeParam[] = {
    ConvolveTypeParam(false, false, false, false),
    ConvolveTypeParam(false, false, false, true),
    ConvolveTypeParam(false, false, true, false),
    ConvolveTypeParam(false, false, true, true),
    ConvolveTypeParam(false, true, false, false),
    ConvolveTypeParam(false, true, false, true),
    ConvolveTypeParam(false, true, true, false),
    ConvolveTypeParam(false, true, true, true),
    ConvolveTypeParam(true, false, false, false),
    ConvolveTypeParam(true, false, false, true),
    ConvolveTypeParam(true, false, true, false),
    ConvolveTypeParam(true, false, true, true),
    ConvolveTypeParam(true, true, false, false),
    ConvolveTypeParam(true, true, false, true),
    ConvolveTypeParam(true, true, true, false),
    ConvolveTypeParam(true, true, true, true),
};

INSTANTIATE_TEST_SUITE_P(
    C, ConvolveTest8bpp,
    ::testing::Combine(::testing::ValuesIn(kConvolveParam),
                       ::testing::ValuesIn(kConvolveTypeParam),
                       ::testing::Bool()));

#if LIBGAV1_ENABLE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, ConvolveTest8bpp,
    ::testing::Combine(::testing::ValuesIn(kConvolveParam),
                       ::testing::ValuesIn(kConvolveTypeParam),
                       ::testing::Bool()));
#endif  // LIBGAV1_ENABLE_NEON

#if LIBGAV1_ENABLE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE41, ConvolveTest8bpp,
    ::testing::Combine(::testing::ValuesIn(kConvolveParam),
                       ::testing::ValuesIn(kConvolveTypeParam),
                       ::testing::Bool()));
#endif  // LIBGAV1_ENABLE_SSE4_1

#if LIBGAV1_ENABLE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, ConvolveTest8bpp,
    ::testing::Combine(::testing::ValuesIn(kConvolveParam),
                       ::testing::ValuesIn(kConvolveTypeParam),
                       ::testing::Bool()));
#endif  // LIBGAV1_ENABLE_AVX2

#if LIBGAV1_MAX_BITDEPTH >= 10
using ConvolveTest10bpp = ConvolveTest<10, uint16_t>;

TEST_P(ConvolveTest10bpp, FixedValues) {
  Test(true, 0);
  Test(true, 1);
  Test(true, 128);
  Test(true, (1 << 10) - 1);
}

TEST_P(ConvolveTest10bpp, RandomValues) { Test(false, 0); }

TEST_P(ConvolveTest10bpp, DISABLED_Speed) {
  const int num_runs = static_cast<int>(1.0e7 / (param_.width * param_.height));
  Test(false, 0, num_runs);
}

INSTANTIATE_TEST_SUITE_P(
    C, ConvolveTest10bpp,
    ::testing::Combine(::testing::ValuesIn(kConvolveParam),
                       ::testing::ValuesIn(kConvolveTypeParam),
                       ::testing::Bool()));
#endif  // LIBGAV1_MAX_BITDEPTH >= 10

}  // namespace
}  // namespace dsp
}  // namespace libgav1
