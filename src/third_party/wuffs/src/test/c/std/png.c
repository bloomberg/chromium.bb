// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------

/*
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror png.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// Libpng requires -lpng (and nothing else). Libspng (note the 's') requires
// -lm and -lz (and nothing else). It's easiest to just link with the union of
// all of these libraries.
//
// !! wuffs mimic cflags: -DWUFFS_MIMIC -lm -lpng -lz

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/png.c"
#endif

// ---------------- PNG Tests

const char*  //
wuffs_png_decode(uint64_t* n_bytes_out,
                 wuffs_base__io_buffer* dst,
                 uint32_t wuffs_initialize_flags,
                 wuffs_base__pixel_format pixfmt,
                 uint32_t* quirks_ptr,
                 size_t quirks_len,
                 wuffs_base__io_buffer* src) {
  wuffs_png__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_png__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                              wuffs_initialize_flags));
  return do_run__wuffs_base__image_decoder(
      wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      n_bytes_out, dst, pixfmt, quirks_ptr, quirks_len, src);
}

const char*  //
do_test_xxxxx_png_decode_bad_crc32_checksum_critical(
    const char* (*decode_func)(uint64_t* n_bytes_out,
                               wuffs_base__io_buffer* dst,
                               uint32_t wuffs_initialize_flags,
                               wuffs_base__pixel_format pixfmt,
                               uint32_t* quirks_ptr,
                               size_t quirks_len,
                               wuffs_base__io_buffer* src)) {
  const char* test_cases[] = {
      // Change a byte in the IHDR CRC-32 checksum.
      "@001F=8A=00;test/data/hippopotamus.regular.png",
      // Change a byte in a PLTE CRC-32 checksum.
      "@0372=52=00;test/data/bricks-dither.png",
      // Change a byte in a non-final IDAT CRC-32 checksum.
      "@2029=B7=00;test/data/bricks-color.png",
#ifndef WUFFS_MIMICLIB_PNG_DOES_NOT_VERIFY_FINAL_IDAT_CHECKSUMS
      // Change a byte in a final IDAT Adler-32 checksum.
      "@084E=26=00;test/data/hippopotamus.regular.png",
      // Change a byte in a final IDAT CRC-32 checksum.
      "@084F=F4=00;test/data/hippopotamus.regular.png",
#endif
  };

  int tc;
  for (tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, test_cases[tc]));

    wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
        .data = g_have_slice_u8,
    });
    if (NULL == (*decode_func)(NULL, &have, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
                               wuffs_base__make_pixel_format(
                                   WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
                               NULL, 0, &src)) {
      RETURN_FAIL("tc=%d (filename=\"%s\"): bad checksum not rejected", tc,
                  test_cases[tc]);
    }
  }
  return NULL;
}

const char*  //
do_wuffs_png_swizzle(uint32_t width,
                     uint32_t height,
                     uint8_t filter_distance,
                     wuffs_base__slice_u8 dst,
                     wuffs_base__slice_u8 workbuf) {
  wuffs_png__decoder dec;
  CHECK_STATUS("initialize", wuffs_png__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  dec.private_impl.f_width = width;
  dec.private_impl.f_height = height;
  dec.private_impl.f_pass_bytes_per_row = width;
  dec.private_impl.f_filter_distance = filter_distance;
  wuffs_png__decoder__choose_filter_implementations(&dec);

  CHECK_STATUS("prepare",
               wuffs_base__pixel_swizzler__prepare(
                   &dec.private_impl.f_swizzler,
                   wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y),
                   wuffs_base__empty_slice_u8(),
                   wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y),
                   wuffs_base__empty_slice_u8(), WUFFS_BASE__PIXEL_BLEND__SRC));

  wuffs_base__pixel_config pc = ((wuffs_base__pixel_config){});
  wuffs_base__pixel_config__set(&pc, WUFFS_BASE__PIXEL_FORMAT__Y,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width,
                                height);
  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});

  CHECK_STATUS("set_from_slice",
               wuffs_base__pixel_buffer__set_from_slice(&pb, &pc, dst));
  CHECK_STATUS("filter_and_swizzle",
               wuffs_png__decoder__filter_and_swizzle(&dec, &pb, workbuf));
  return NULL;
}

// --------

const char*  //
test_wuffs_png_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_png__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_png__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__image_decoder(
      wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      "test/data/bricks-gray.png", 0, SIZE_MAX, 160, 120, 0xFF060606);
}

const char*  //
test_wuffs_png_decode_bad_crc32_checksum_critical() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_png_decode_bad_crc32_checksum_critical(
      &wuffs_png_decode);
}

const char*  //
test_wuffs_png_decode_filters_golden() {
  CHECK_FOCUS(__func__);

  uint8_t src_rows[2][12] = {
      // "WhatsInAName".
      {0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65},
      // "SmellAsSweet".
      {0x53, 0x6D, 0x65, 0x6C, 0x6C, 0x41, 0x73, 0x53, 0x77, 0x65, 0x65, 0x74},
  };

  uint8_t want_rows[4 * 4 * 2][12] = {
      // Sub:1.
      {0x57, 0xBF, 0x20, 0x94, 0x07, 0x50, 0xBE, 0xFF, 0x4D, 0xAE, 0x1B, 0x80},
      {0x53, 0xC0, 0x25, 0x91, 0xFD, 0x3E, 0xB1, 0x04, 0x7B, 0xE0, 0x45, 0xB9},
      // Sub:2.
      {0x57, 0x68, 0xB8, 0xDC, 0x2B, 0x25, 0x99, 0x66, 0xE7, 0xC7, 0x54, 0x2C},
      {0x53, 0x6D, 0xB8, 0xD9, 0x24, 0x1A, 0x97, 0x6D, 0x0E, 0xD2, 0x73, 0x46},
      // Sub:3.
      {0x57, 0x68, 0x61, 0xCB, 0xDB, 0xAA, 0x39, 0x1C, 0xF8, 0x9A, 0x89, 0x5D},
      {0x53, 0x6D, 0x65, 0xBF, 0xD9, 0xA6, 0x32, 0x2C, 0x1D, 0x97, 0x91, 0x91},
      // Sub:4.
      {0x57, 0x68, 0x61, 0x74, 0xCA, 0xB1, 0xCF, 0xB5, 0x18, 0x12, 0x3C, 0x1A},
      {0x53, 0x6D, 0x65, 0x6C, 0xBF, 0xAE, 0xD8, 0xBF, 0x36, 0x13, 0x3D, 0x33},
      // Up:1.
      {0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65},
      {0xAA, 0xD5, 0xC6, 0xE0, 0xDF, 0x8A, 0xE1, 0x94, 0xC5, 0xC6, 0xD2, 0xD9},
      // Up:2.
      {0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65},
      {0xAA, 0xD5, 0xC6, 0xE0, 0xDF, 0x8A, 0xE1, 0x94, 0xC5, 0xC6, 0xD2, 0xD9},
      // Up:3.
      {0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65},
      {0xAA, 0xD5, 0xC6, 0xE0, 0xDF, 0x8A, 0xE1, 0x94, 0xC5, 0xC6, 0xD2, 0xD9},
      // Up:4.
      {0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65},
      {0xAA, 0xD5, 0xC6, 0xE0, 0xDF, 0x8A, 0xE1, 0x94, 0xC5, 0xC6, 0xD2, 0xD9},
      // Average:1.
      {0x57, 0x93, 0xAA, 0xC9, 0xD7, 0xB4, 0xC8, 0xA5, 0xA0, 0xB1, 0xC5, 0xC7},
      {0x7E, 0xF5, 0x34, 0xEA, 0x4C, 0xC1, 0x37, 0xC1, 0x27, 0xD1, 0x30, 0xEF},
      // Average:2.
      {0x57, 0x68, 0x8C, 0xA8, 0xB9, 0x9D, 0xCA, 0x8F, 0xB3, 0xA8, 0xC6, 0xB9},
      {0x7E, 0xA1, 0xEA, 0x10, 0x3D, 0x97, 0xF6, 0xE6, 0x4B, 0x2C, 0xED, 0xE6},
      // Average:3.
      {0x57, 0x68, 0x61, 0x9F, 0xA7, 0x79, 0xBD, 0x94, 0x8A, 0xBF, 0xB7, 0xAA},
      {0x7E, 0xA1, 0x95, 0xFA, 0x10, 0xC8, 0x4E, 0xA5, 0x20, 0xEB, 0x13, 0xD9},
      // Average:4.
      {0x57, 0x68, 0x61, 0x74, 0x9E, 0x7D, 0x9E, 0x7B, 0x9D, 0x9F, 0xBC, 0xA2},
      {0x7E, 0xA1, 0x95, 0xA6, 0xFA, 0xD0, 0x0C, 0xE3, 0x42, 0x1C, 0xC9, 0x36},
      // Paeth:1.
      {0x57, 0xBF, 0x20, 0x94, 0x07, 0x50, 0xBE, 0xFF, 0x4D, 0xAE, 0x1B, 0x80},
      {0xAA, 0x2C, 0x85, 0x00, 0x6C, 0xAD, 0x31, 0x84, 0xC4, 0x29, 0x80, 0xF4},
      // Paeth:2.
      {0x57, 0x68, 0xB8, 0xDC, 0x2B, 0x25, 0x99, 0x66, 0xE7, 0xC7, 0x54, 0x2C},
      {0xAA, 0xD5, 0x1D, 0x48, 0x89, 0x66, 0x0C, 0xB9, 0x10, 0x2C, 0x75, 0xA0},
      // Paeth:3.
      {0x57, 0x68, 0x61, 0xCB, 0xDB, 0xAA, 0x39, 0x1C, 0xF8, 0x9A, 0x89, 0x5D},
      {0xAA, 0xD5, 0xC6, 0x37, 0x47, 0x07, 0xAA, 0x6F, 0x7E, 0x0F, 0xEE, 0xD1},
      // Paeth:4.
      {0x57, 0x68, 0x61, 0x74, 0xCA, 0xB1, 0xCF, 0xB5, 0x18, 0x12, 0x3C, 0x1A},
      {0xAA, 0xD5, 0xC6, 0xE0, 0x36, 0x16, 0x42, 0x33, 0x8F, 0x77, 0xA1, 0x8E},
  };

  int filter;
  for (filter = 1; filter <= 4; filter++) {
    int filter_distance;
    for (filter_distance = 1; filter_distance <= 4; filter_distance++) {
      // For the top row, the Paeth filter (4) is equivalent to the Sub filter
      // (1), but the Paeth implementation is simpler if it can assume that
      // there is a previous row.
      uint8_t top_row_filter = (filter != 4) ? filter : 1;

      g_work_slice_u8.ptr[13 * 0] = top_row_filter;
      memcpy(g_work_slice_u8.ptr + (13 * 0) + 1, src_rows[0], 12);
      g_work_slice_u8.ptr[13 * 1] = filter;
      memcpy(g_work_slice_u8.ptr + (13 * 1) + 1, src_rows[1], 12);

      CHECK_STRING(do_wuffs_png_swizzle(
          12, 2, filter_distance, g_have_slice_u8,
          wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 13 * 2)));

      wuffs_base__io_buffer have =
          wuffs_base__ptr_u8__reader(g_have_slice_u8.ptr, 12 * 2, true);
      have.meta.ri = have.meta.wi;

      int index = (8 * (filter - 1)) + (2 * (filter_distance - 1));
      memcpy(g_want_slice_u8.ptr + (12 * 0), want_rows[index + 0], 12);
      memcpy(g_want_slice_u8.ptr + (12 * 1), want_rows[index + 1], 12);

      wuffs_base__io_buffer want =
          wuffs_base__ptr_u8__reader(g_want_slice_u8.ptr, 12 * 2, true);
      want.meta.ri = want.meta.wi;

      char prefix_buf[256];
      sprintf(prefix_buf, "filter=%d, filter_distance=%d ", filter,
              filter_distance);
      CHECK_STRING(check_io_buffers_equal(prefix_buf, &have, &want));
    }
  }

  return NULL;
}

int32_t  //
apply_png_abs(int32_t x) {
  return (x < 0) ? -x : +x;
}

const char*  //
apply_png_encode_filters(wuffs_base__slice_u8 dst_rows,
                         size_t width,
                         size_t height,
                         size_t filter_distance,
                         wuffs_base__slice_u8 src_rows) {
  if ((((width + 1) * height) != dst_rows.len) ||
      (((width + 1) * height) != src_rows.len)) {
    return "apply_png_encode_filters: unexpected rows.len";
  }
  size_t y;
  uint8_t* src_prev = NULL;
  for (y = 0; y < height; y++) {
    uint8_t filter = src_rows.ptr[(width + 1) * y];
    dst_rows.ptr[(width + 1) * y] = filter;
    uint8_t* dst_curr = &dst_rows.ptr[((width + 1) * y) + 1];
    uint8_t* src_curr = &src_rows.ptr[((width + 1) * y) + 1];

    size_t x;
    for (x = 0; x < width; x++) {
      int32_t fa = 0;
      int32_t fb = 0;
      int32_t fc = 0;
      if (x >= filter_distance) {
        fa = src_curr[x - filter_distance];
        if (src_prev) {
          fc = src_prev[x - filter_distance];
        }
      }
      if (src_prev) {
        fb = src_prev[x];
      }

      uint8_t prediction = 0;
      switch (filter) {
        case 1:
          prediction = fa;
          break;
        case 2:
          prediction = fb;
          break;
        case 3:
          prediction = (uint8_t)((fa + fb) / 2);
          break;
        case 4: {
          int32_t p = fa + fb - fc;
          int32_t pa = apply_png_abs(p - fa);
          int32_t pb = apply_png_abs(p - fb);
          int32_t pc = apply_png_abs(p - fc);
          if ((pa <= pb) && (pa <= pc)) {
            prediction = fa;
          } else if (pb <= pc) {
            prediction = fb;
          } else {
            prediction = fc;
          }
          break;
        }
      }
      dst_curr[x] = src_curr[x] - prediction;
    }
    src_prev = src_curr;
  }
  return NULL;
}

const char*  //
test_wuffs_png_decode_filters_round_trip() {
  CHECK_FOCUS(__func__);

  uint8_t src_rows[2][96] = {
      // "ThoughYouMightHearLaughingSpinningSwingingMadlyA"
      // "crossTheSun/ItsNotAimedAtAnyone/ItsJustEscapingO"
      {0x54, 0x68, 0x6F, 0x75, 0x67, 0x68, 0x59, 0x6F, 0x75, 0x4D, 0x69, 0x67,
       0x68, 0x74, 0x48, 0x65, 0x61, 0x72, 0x4C, 0x61, 0x75, 0x67, 0x68, 0x69,
       0x6E, 0x67, 0x53, 0x70, 0x69, 0x6E, 0x6E, 0x69, 0x6E, 0x67, 0x53, 0x77,
       0x69, 0x6E, 0x67, 0x69, 0x6E, 0x67, 0x4D, 0x61, 0x64, 0x6C, 0x79, 0x41,
       0x63, 0x72, 0x6F, 0x73, 0x73, 0x54, 0x68, 0x65, 0x53, 0x75, 0x6E, 0x2F,
       0x49, 0x74, 0x73, 0x4E, 0x6F, 0x74, 0x41, 0x69, 0x6D, 0x65, 0x64, 0x41,
       0x74, 0x41, 0x6E, 0x79, 0x6F, 0x6E, 0x65, 0x2F, 0x49, 0x74, 0x73, 0x4A,
       0x75, 0x73, 0x74, 0x45, 0x73, 0x63, 0x61, 0x70, 0x69, 0x6E, 0x67, 0x4F},
      // "YesToDanceBeneathTheDiamondSky/WithOneHandWaving"
      // "Free/SilhouettedByTheSea/CircledByTheCircusSands"
      {0x59, 0x65, 0x73, 0x54, 0x6F, 0x44, 0x61, 0x6E, 0x63, 0x65, 0x42, 0x65,
       0x6E, 0x65, 0x61, 0x74, 0x68, 0x54, 0x68, 0x65, 0x44, 0x69, 0x61, 0x6D,
       0x6F, 0x6E, 0x64, 0x53, 0x6B, 0x79, 0x2F, 0x57, 0x69, 0x74, 0x68, 0x4F,
       0x6E, 0x65, 0x48, 0x61, 0x6E, 0x64, 0x57, 0x61, 0x76, 0x69, 0x6E, 0x67,
       0x46, 0x72, 0x65, 0x65, 0x2F, 0x53, 0x69, 0x6C, 0x68, 0x6F, 0x75, 0x65,
       0x74, 0x74, 0x65, 0x64, 0x42, 0x79, 0x54, 0x68, 0x65, 0x53, 0x65, 0x61,
       0x2F, 0x43, 0x69, 0x72, 0x63, 0x6C, 0x65, 0x64, 0x42, 0x79, 0x54, 0x68,
       0x65, 0x43, 0x69, 0x72, 0x63, 0x75, 0x73, 0x53, 0x61, 0x6E, 0x64, 0x73},
  };

  memcpy(g_src_slice_u8.ptr + (97 * 0) + 1, src_rows[0], 96);
  memcpy(g_src_slice_u8.ptr + (97 * 1) + 1, src_rows[1], 96);

  int filter;
  for (filter = 1; filter <= 4; filter++) {
    int filter_distance;
    for (filter_distance = 1; filter_distance <= 8; filter_distance++) {
      if ((filter_distance == 5) || (filter_distance == 7)) {
        continue;
      }
      // For the top row, the Paeth filter (4) is equivalent to the Sub filter
      // (1), but the Paeth implementation is simpler if it can assume that
      // there is a previous row.
      uint8_t top_row_filter = (filter != 4) ? filter : 1;

      g_src_slice_u8.ptr[97 * 0] = top_row_filter;
      g_src_slice_u8.ptr[97 * 1] = filter;

      CHECK_STRING(apply_png_encode_filters(
          wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 97 * 2), 96, 2,
          filter_distance,
          wuffs_base__make_slice_u8(g_src_slice_u8.ptr, 97 * 2)));

      CHECK_STRING(do_wuffs_png_swizzle(
          96, 2, filter_distance, g_have_slice_u8,
          wuffs_base__make_slice_u8(g_work_slice_u8.ptr, 97 * 2)));

      wuffs_base__io_buffer have =
          wuffs_base__ptr_u8__reader(g_have_slice_u8.ptr, 96 * 2, true);
      have.meta.ri = have.meta.wi;

      memcpy(g_want_slice_u8.ptr + (96 * 0), src_rows[0], 96);
      memcpy(g_want_slice_u8.ptr + (96 * 1), src_rows[1], 96);

      wuffs_base__io_buffer want =
          wuffs_base__ptr_u8__reader(g_want_slice_u8.ptr, 96 * 2, true);
      want.meta.ri = want.meta.wi;

      char prefix_buf[256];
      sprintf(prefix_buf, "filter=%d, filter_distance=%d ", filter,
              filter_distance);
      CHECK_STRING(check_io_buffers_equal(prefix_buf, &have, &want));
    }
  }

  return NULL;
}

const char*  //
test_wuffs_png_decode_frame_config() {
  CHECK_FOCUS(__func__);
  wuffs_png__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_png__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/hibiscus.regular.png"));
  CHECK_STATUS("decode_frame_config #0",
               wuffs_png__decoder__decode_frame_config(&dec, &fc, &src));

  wuffs_base__status status =
      wuffs_png__decoder__decode_frame_config(&dec, &fc, &src);
  if (status.repr != wuffs_base__note__end_of_data) {
    RETURN_FAIL("decode_frame_config #1: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__note__end_of_data);
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
do_test_mimic_png_decode(const char* filename) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, filename));

  src.meta.ri = 0;
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(wuffs_png_decode(
      NULL, &have, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, &src));

  src.meta.ri = 0;
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(mimic_png_decode(
      NULL, &want, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, &src));

  return check_io_buffers_equal("", &have, &want);
}

const char*  //
test_mimic_png_decode_image_19k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_png_decode("test/data/bricks-gray.no-ancillary.png");
}

const char*  //
test_mimic_png_decode_image_40k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_png_decode("test/data/hat.png");
}

const char*  //
test_mimic_png_decode_image_77k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_png_decode("test/data/bricks-dither.png");
}

const char*  //
test_mimic_png_decode_image_552k_32bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_png_decode("test/data/hibiscus.primitive.png");
}

const char*  //
test_mimic_png_decode_image_4002k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_png_decode("test/data/harvesters.png");
}

const char*  //
test_mimic_png_decode_bad_crc32_checksum_ancillary() {
  CHECK_FOCUS(__func__);
  // libpng automatically applies the "gAMA" chunk (with no matching "sRGB"
  // chunk) but Wuffs does not. To make the comparison more like-for-like,
  // especially in emitting identical BGRA pixels, patch the source file by
  // replacing the "gAMA" with the nonsense "hAMA". ASCII 'g' is 0x67.
  //
  // This makes the "hAMA" CRC-32 checksum no longer verify, since the checksum
  // input includes the chunk type. By default, libpng "warns and discards"
  // when seeing ancillary chunk checksum failures (as opposed to critical
  // chunk checksum failures) but it still continues to decode the image.
  // Wuffs' decoder likewise ignores the bad ancillary chunk checksum.
  return do_test_mimic_png_decode("@25=67=68;test/data/bricks-gray.png");
}

const char*  //
test_mimic_png_decode_bad_crc32_checksum_critical() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_png_decode_bad_crc32_checksum_critical(
      &mimic_png_decode);
}

#endif  // WUFFS_MIMIC

// ---------------- PNG Benches

const char*  //
bench_wuffs_png_decode_image_19k_8bpp() {
  CHECK_FOCUS(__func__);
  // libpng automatically applies the "gAMA" chunk (with no matching "sRGB"
  // chunk) but Wuffs does not. To make the comparison more like-for-like,
  // especially in emitting identical BGRA pixels, patch the source file by
  // replacing the "gAMA" with the nonsense "hAMA". ASCII 'g' is 0x67.
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y), NULL, 0,
      "test/data/bricks-gray.no-ancillary.png", 0, SIZE_MAX, 50);
}

const char*  //
bench_wuffs_png_decode_image_40k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hat.png", 0, SIZE_MAX, 30);
}

const char*  //
bench_wuffs_png_decode_image_77k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/bricks-dither.png", 0, SIZE_MAX, 50);
}

const char*  //
bench_wuffs_png_decode_image_552k_32bpp_ignore_checksum() {
  uint32_t q = WUFFS_BASE__QUIRK_IGNORE_CHECKSUM;
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      &q, 1, "test/data/hibiscus.primitive.png", 0, SIZE_MAX, 4);
}

const char*  //
bench_wuffs_png_decode_image_552k_32bpp_verify_checksum() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.primitive.png", 0, SIZE_MAX, 4);
}

const char*  //
bench_wuffs_png_decode_image_4002k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/harvesters.png", 0, SIZE_MAX, 1);
}

const char*  //
do_bench_wuffs_png_decode_filter(uint8_t filter,
                                 uint8_t filter_distance,
                                 uint64_t iters_unscaled) {
  const uint32_t width = 160;
  const uint32_t height = 120;
  const uint32_t bytes_per_row = 160 * 4;
  const size_t n = (1 + bytes_per_row) * height;

  wuffs_base__io_buffer workbuf = wuffs_base__slice_u8__writer(g_work_slice_u8);
  CHECK_STRING(read_file(&workbuf, "test/data/pi.txt"));
  if (workbuf.meta.wi < n) {
    return "source data is too short";
  }

  uint32_t y;
  for (y = 0; y < height; y++) {
    workbuf.data.ptr[(1 + bytes_per_row) * y] = filter;
  }

  // For the top row, the Paeth filter (4) is equivalent to the Sub filter
  // (1), but the Paeth implementation is simpler if it can assume that
  // there is a previous row.
  if (workbuf.data.ptr[0] == 4) {
    workbuf.data.ptr[0] = 1;
  }

  wuffs_png__decoder dec;
  CHECK_STATUS("initialize", wuffs_png__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  dec.private_impl.f_width = width;
  dec.private_impl.f_height = height;
  dec.private_impl.f_pass_bytes_per_row = bytes_per_row;
  dec.private_impl.f_filter_distance = filter_distance;
  wuffs_png__decoder__choose_filter_implementations(&dec);

  CHECK_STATUS("prepare",
               wuffs_base__pixel_swizzler__prepare(
                   &dec.private_impl.f_swizzler,
                   wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y),
                   wuffs_base__empty_slice_u8(),
                   wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y),
                   wuffs_base__empty_slice_u8(), WUFFS_BASE__PIXEL_BLEND__SRC));

  wuffs_base__pixel_config pc = ((wuffs_base__pixel_config){});
  wuffs_base__pixel_config__set(&pc, WUFFS_BASE__PIXEL_FORMAT__Y,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width,
                                height);
  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});

  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &pc, g_pixel_slice_u8));

  bench_start();
  uint64_t n_bytes = 0;
  uint64_t i;
  uint64_t iters = iters_unscaled * g_flags.iterscale;
  for (i = 0; i < iters; i++) {
    CHECK_STATUS(
        "filter_and_swizzle",
        wuffs_png__decoder__filter_and_swizzle(
            &dec, &pb, wuffs_base__make_slice_u8(workbuf.data.ptr, n)));
    n_bytes += n;
  }
  bench_finish(iters, n_bytes);
  return NULL;
}

const char*  //
bench_wuffs_png_decode_filt_1_dist_3() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(1, 3, 200);
}

const char*  //
bench_wuffs_png_decode_filt_1_dist_4() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(1, 4, 200);
}

const char*  //
bench_wuffs_png_decode_filt_2_dist_3() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(2, 3, 1000);
}

const char*  //
bench_wuffs_png_decode_filt_2_dist_4() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(2, 4, 1000);
}

const char*  //
bench_wuffs_png_decode_filt_3_dist_3() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(3, 3, 100);
}

const char*  //
bench_wuffs_png_decode_filt_3_dist_4() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(3, 4, 100);
}

const char*  //
bench_wuffs_png_decode_filt_4_dist_3() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(4, 3, 20);
}

const char*  //
bench_wuffs_png_decode_filt_4_dist_4() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_png_decode_filter(4, 4, 20);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_png_decode_image_19k_8bpp() {
  CHECK_FOCUS(__func__);
  // libpng automatically applies the "gAMA" chunk (with no matching "sRGB"
  // chunk) but Wuffs does not. To make the comparison more like-for-like,
  // especially in emitting identical BGRA pixels, patch the source file by
  // replacing the "gAMA" with the nonsense "hAMA". ASCII 'g' is 0x67.
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y), NULL, 0,
      "test/data/bricks-gray.no-ancillary.png", 0, SIZE_MAX, 50);
}

const char*  //
bench_mimic_png_decode_image_40k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hat.png", 0, SIZE_MAX, 30);
}

const char*  //
bench_mimic_png_decode_image_77k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/bricks-dither.png", 0, SIZE_MAX, 50);
}

const char*  //
bench_mimic_png_decode_image_552k_32bpp_ignore_checksum() {
  uint32_t q = WUFFS_BASE__QUIRK_IGNORE_CHECKSUM;
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      &q, 1, "test/data/hibiscus.primitive.png", 0, SIZE_MAX, 4);
}

const char*  //
bench_mimic_png_decode_image_552k_32bpp_verify_checksum() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.primitive.png", 0, SIZE_MAX, 4);
}

const char*  //
bench_mimic_png_decode_image_4002k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_png_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/harvesters.png", 0, SIZE_MAX, 1);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_png_decode_bad_crc32_checksum_critical,
    test_wuffs_png_decode_filters_golden,
    test_wuffs_png_decode_filters_round_trip,
    test_wuffs_png_decode_frame_config,
    test_wuffs_png_decode_interface,

#ifdef WUFFS_MIMIC

    test_mimic_png_decode_bad_crc32_checksum_ancillary,
    test_mimic_png_decode_bad_crc32_checksum_critical,
    test_mimic_png_decode_image_19k_8bpp,
    test_mimic_png_decode_image_40k_24bpp,
    test_mimic_png_decode_image_77k_8bpp,
    test_mimic_png_decode_image_552k_32bpp,
    test_mimic_png_decode_image_4002k_24bpp,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_png_decode_filt_1_dist_3,
    bench_wuffs_png_decode_filt_1_dist_4,
    bench_wuffs_png_decode_filt_2_dist_3,
    bench_wuffs_png_decode_filt_2_dist_4,
    bench_wuffs_png_decode_filt_3_dist_3,
    bench_wuffs_png_decode_filt_3_dist_4,
    bench_wuffs_png_decode_filt_4_dist_3,
    bench_wuffs_png_decode_filt_4_dist_4,
    bench_wuffs_png_decode_image_19k_8bpp,
    bench_wuffs_png_decode_image_40k_24bpp,
    bench_wuffs_png_decode_image_77k_8bpp,
    bench_wuffs_png_decode_image_552k_32bpp_ignore_checksum,
    bench_wuffs_png_decode_image_552k_32bpp_verify_checksum,
    bench_wuffs_png_decode_image_4002k_24bpp,

#ifdef WUFFS_MIMIC

    bench_mimic_png_decode_image_19k_8bpp,
    bench_mimic_png_decode_image_40k_24bpp,
    bench_mimic_png_decode_image_77k_8bpp,
#ifndef WUFFS_MIMICLIB_PNG_DOES_NOT_SUPPORT_QUIRK_IGNORE_CHECKSUM
    bench_mimic_png_decode_image_552k_32bpp_ignore_checksum,
#endif
    bench_mimic_png_decode_image_552k_32bpp_verify_checksum,
    bench_mimic_png_decode_image_4002k_24bpp,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/png";
  return test_main(argc, argv, g_tests, g_benches);
}
