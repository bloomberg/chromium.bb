// Copyright 2017 The Wuffs Authors.
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
  $CC -std=c99 -Wall -Werror adler32.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// !! wuffs mimic cflags: -DWUFFS_MIMIC -lz

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c whitelist which parts of Wuffs to build. That file contains
// the entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/deflate-gzip-zlib.c"
#endif

// ---------------- Golden Tests

golden_test adler32_midsummer_gt = {
    .src_filename = "test/data/midsummer.txt",  //
};

golden_test adler32_pi_gt = {
    .src_filename = "test/data/pi.txt",  //
};

// ---------------- Adler32 Tests

const char* test_wuffs_adler32_golden() {
  CHECK_FOCUS(__func__);

  struct {
    const char* filename;
    // The want values are determined by script/checksum.go.
    uint32_t want;
  } test_cases[] = {
      {
          .filename = "test/data/hat.bmp",
          .want = 0x3D26D034,
      },
      {
          .filename = "test/data/hat.gif",
          .want = 0x2A5EB144,
      },
      {
          .filename = "test/data/hat.jpeg",
          .want = 0x3A503B1A,
      },
      {
          .filename = "test/data/hat.lossless.webp",
          .want = 0xD059D427,
      },
      {
          .filename = "test/data/hat.lossy.webp",
          .want = 0xF1BB258D,
      },
      {
          .filename = "test/data/hat.png",
          .want = 0xDFC6C9C6,
      },
      {
          .filename = "test/data/hat.tiff",
          .want = 0xBDC011E9,
      },
  };

  int i;
  for (i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); i++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = global_src_slice,
    });
    const char* status = read_file(&src, test_cases[i].filename);
    if (status) {
      return status;
    }

    int j;
    for (j = 0; j < 2; j++) {
      wuffs_adler32__hasher checksum;
      status = wuffs_adler32__hasher__initialize(
          &checksum, sizeof checksum, WUFFS_VERSION,
          WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
      if (status) {
        RETURN_FAIL("initialize: \"%s\"", status);
      }

      uint32_t got = 0;
      size_t num_fragments = 0;
      size_t num_bytes = 0;
      do {
        wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
            .ptr = src.data.ptr + num_bytes,
            .len = src.meta.wi - num_bytes,
        });
        size_t limit = 101 + 103 * num_fragments;
        if ((j > 0) && (data.len > limit)) {
          data.len = limit;
        }
        got = wuffs_adler32__hasher__update(&checksum, data);
        num_fragments++;
        num_bytes += data.len;
      } while (num_bytes < src.meta.wi);

      if (got != test_cases[i].want) {
        RETURN_FAIL("i=%d, j=%d, filename=\"%s\": got 0x%08" PRIX32
                    ", want 0x%08" PRIX32 "\n",
                    i, j, test_cases[i].filename, got, test_cases[i].want);
      }
    }
  }
  return NULL;
}

const char* test_wuffs_adler32_pi() {
  CHECK_FOCUS(__func__);

  const char* digits =
      "3."
      "141592653589793238462643383279502884197169399375105820974944592307816406"
      "2862089986280348253421170";
  if (strlen(digits) != 99) {
    RETURN_FAIL("strlen(digits): got %d, want 99", (int)(strlen(digits)));
  }

  // The want values are determined by script/checksum.go.
  //
  // wants[i] is the checksum of the first i bytes of the digits string.
  uint32_t wants[100] = {
      0x00000001, 0x00340034, 0x00960062, 0x01290093, 0x01F000C7, 0x02E800F8,
      0x0415012D, 0x057B0166, 0x07130198, 0x08E101CE, 0x0AE40203, 0x0D1A0236,
      0x0F85026B, 0x122802A3, 0x150402DC, 0x18170313, 0x1B63034C, 0x1EE2037F,
      0x229303B1, 0x267703E4, 0x2A93041C, 0x2EE30450, 0x33690486, 0x382104B8,
      0x3D0F04EE, 0x42310522, 0x47860555, 0x4D0E0588, 0x52CE05C0, 0x58C105F3,
      0x5EE60625, 0x6542065C, 0x6BD70695, 0x72A106CA, 0x799B06FA, 0x80C7072C,
      0x882B0764, 0x8FC7079C, 0x979707D0, 0x9F980801, 0xA7D2083A, 0xB0430871,
      0xB8E508A2, 0xC1BD08D8, 0xCACE0911, 0xD4120944, 0xDD8F097D, 0xE74509B6,
      0xF12E09E9, 0xFB4E0A20, 0x05B20A55, 0x10380A86, 0x1AEE0AB6, 0x25D90AEB,
      0x30FC0B23, 0x3C510B55, 0x47D60B85, 0x53940BBE, 0x5F890BF5, 0x6BB20C29,
      0x78140C62, 0x84AA0C96, 0x91740CCA, 0x9E730CFF, 0xABAB0D38, 0xB9150D6A,
      0xC6B20D9D, 0xD47F0DCD, 0xE2830E04, 0xF0BF0E3C, 0xFF2C0E6D, 0x0DDE0EA3,
      0x1CB50ED7, 0x2BBC0F07, 0x3AF90F3D, 0x4A680F6F, 0x5A0F0FA7, 0x69EC0FDD,
      0x79FB100F, 0x8A3A103F, 0x9AB11077, 0xAB6110B0, 0xBC4A10E9, 0xCD6B1121,
      0xDEC21157, 0xF04B1189, 0x021B11C1, 0x140C11F1, 0x26301224, 0x38881258,
      0x4B181290, 0x5DDA12C2, 0x70D112F7, 0x83FB132A, 0x9759135E, 0xAAE91390,
      0xBEAA13C1, 0xD29C13F2, 0xE6C51429, 0xFB1E1459,
  };

  int i;
  for (i = 0; i < 100; i++) {
    wuffs_adler32__hasher checksum;
    const char* status = wuffs_adler32__hasher__initialize(
        &checksum, sizeof checksum, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("initialize: \"%s\"", status);
    }
    uint32_t got =
        wuffs_adler32__hasher__update(&checksum, ((wuffs_base__slice_u8){
                                                     .ptr = (uint8_t*)(digits),
                                                     .len = i,
                                                 }));
    if (got != wants[i]) {
      RETURN_FAIL("i=%d: got 0x%08" PRIX32 ", want 0x%08" PRIX32, i, got,
                  wants[i]);
    }
  }
  return NULL;
}

// ---------------- Adler32 Benches

uint32_t global_wuffs_adler32_unused_u32;

const char* wuffs_bench_adler32(wuffs_base__io_buffer* dst,
                                wuffs_base__io_buffer* src,
                                uint32_t wuffs_initialize_flags,
                                uint64_t wlimit,
                                uint64_t rlimit) {
  uint64_t len = src->meta.wi - src->meta.ri;
  if (rlimit) {
    len = wuffs_base__u64__min(len, rlimit);
  }
  wuffs_adler32__hasher checksum;
  const char* status = wuffs_adler32__hasher__initialize(
      &checksum, sizeof checksum, WUFFS_VERSION, wuffs_initialize_flags);
  if (status) {
    return status;
  }
  global_wuffs_adler32_unused_u32 = wuffs_adler32__hasher__update(
      &checksum, ((wuffs_base__slice_u8){
                     .ptr = src->data.ptr + src->meta.ri,
                     .len = len,
                 }));
  src->meta.ri += len;
  return NULL;
}

const char* bench_wuffs_adler32_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_adler32,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tc_src,
      &adler32_midsummer_gt, UINT64_MAX, UINT64_MAX, 1500);
}

const char* bench_wuffs_adler32_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_adler32,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tc_src,
      &adler32_pi_gt, UINT64_MAX, UINT64_MAX, 150);
}

  // ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char* bench_mimic_adler32_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_adler32, 0, tc_src,
                             &adler32_midsummer_gt, UINT64_MAX, UINT64_MAX,
                             1500);
}

const char* bench_mimic_adler32_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_adler32, 0, tc_src, &adler32_pi_gt,
                             UINT64_MAX, UINT64_MAX, 150);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

// Note that the adler32 mimic tests and benches don't work with
// WUFFS_MIMICLIB_USE_MINIZ_INSTEAD_OF_ZLIB.

// The empty comments forces clang-format to place one element per line.
proc tests[] = {

    test_wuffs_adler32_golden,  //
    test_wuffs_adler32_pi,      //

    NULL,
};

// The empty comments forces clang-format to place one element per line.
proc benches[] = {

    bench_wuffs_adler32_10k,   //
    bench_wuffs_adler32_100k,  //

#ifdef WUFFS_MIMIC

    bench_mimic_adler32_10k,   //
    bench_mimic_adler32_100k,  //

#endif  // WUFFS_MIMIC

    NULL,
};

int main(int argc, char** argv) {
  proc_package_name = "std/adler32";
  return test_main(argc, argv, tests, benches);
}
