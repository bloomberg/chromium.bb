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
  $CC -std=c99 -Wall -Werror zlib.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ZLIB

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/deflate-gzip-zlib.c"
#endif

// ---------------- Golden Tests

golden_test zlib_midsummer_gt = {
    .want_filename = "test/data/midsummer.txt",      //
    .src_filename = "test/data/midsummer.txt.zlib",  //
};

golden_test zlib_pi_gt = {
    .want_filename = "test/data/pi.txt",      //
    .src_filename = "test/data/pi.txt.zlib",  //
};

// This dictionary-using zlib-encoded data comes from
// https://play.golang.org/p/Jh9Wyp6PLID, also mentioned in the RAC spec.
const char* zlib_sheep_src_ptr =
    "\x78\xf9\x0b\xe0\x02\x6e\x0a\x29\xcf\x87\x31\x01\x01\x00\x00\xff\xff\x18"
    "\x0c\x03\xa8";
const size_t zlib_sheep_src_len = 21;
const char* zlib_sheep_dict_ptr = " sheep.\n";
const size_t zlib_sheep_dict_len = 8;
const char* zlib_sheep_want_ptr = "Two sheep.\n";
const size_t zlib_sheep_want_len = 11;

// ---------------- Zlib Tests

const char* wuffs_zlib_decode(wuffs_base__io_buffer* dst,
                              wuffs_base__io_buffer* src,
                              uint32_t wuffs_initialize_flags,
                              uint64_t wlimit,
                              uint64_t rlimit) {
  wuffs_zlib__decoder dec;
  const char* status = wuffs_zlib__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION, wuffs_initialize_flags);
  if (status) {
    return status;
  }

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    status = wuffs_zlib__decoder__decode_io_writer(
        &dec, &limited_dst, &limited_src, global_work_slice);

    dst->meta.wi += limited_dst.meta.wi;
    src->meta.ri += limited_src.meta.ri;

    if (((wlimit < UINT64_MAX) &&
         (status == wuffs_base__suspension__short_write)) ||
        ((rlimit < UINT64_MAX) &&
         (status == wuffs_base__suspension__short_read))) {
      continue;
    }
    return status;
  }
}

const char* do_test_wuffs_zlib_checksum(bool ignore_checksum,
                                        uint32_t bad_checksum) {
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, zlib_midsummer_gt.src_filename);
  if (status) {
    return status;
  }
  // Flip a bit in the zlib checksum, which is in the last 4 bytes of the file.
  if (src.meta.wi < 4) {
    RETURN_FAIL("source file was too short");
  }
  if (bad_checksum) {
    src.data.ptr[src.meta.wi - 1 - (bad_checksum & 3)] ^= 1;
  }

  int end_limit;  // The rlimit, relative to the end of the data.
  for (end_limit = 0; end_limit < 10; end_limit++) {
    wuffs_zlib__decoder dec;
    status = wuffs_zlib__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("initialize: \"%s\"", status);
    }
    wuffs_zlib__decoder__set_ignore_checksum(&dec, ignore_checksum);
    got.meta.wi = 0;
    src.meta.ri = 0;

    // Decode the src data in 1 or 2 chunks, depending on whether end_limit is
    // or isn't zero.
    int i;
    for (i = 0; i < 2; i++) {
      uint64_t rlimit = UINT64_MAX;
      const char* want_z = NULL;
      if (i == 0) {
        if (end_limit == 0) {
          continue;
        }
        if (src.meta.wi < end_limit) {
          RETURN_FAIL("end_limit=%d: not enough source data", end_limit);
        }
        rlimit = src.meta.wi - (uint64_t)(end_limit);
        want_z = wuffs_base__suspension__short_read;
      } else {
        want_z = (bad_checksum && !ignore_checksum)
                     ? wuffs_zlib__error__bad_checksum
                     : NULL;
      }

      wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);

      const char* got_z = wuffs_zlib__decoder__decode_io_writer(
          &dec, &got, &limited_src, global_work_slice);
      src.meta.ri += limited_src.meta.ri;
      if (got_z != want_z) {
        RETURN_FAIL("end_limit=%d: got \"%s\", want \"%s\"", end_limit, got_z,
                    want_z);
      }
    }
  }
  return NULL;
}

const char* test_wuffs_zlib_checksum_ignore() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(true, 4 | 0);
}

const char* test_wuffs_zlib_checksum_verify_bad0() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 4 | 0);
}

const char* test_wuffs_zlib_checksum_verify_bad3() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 4 | 3);
}

const char* test_wuffs_zlib_checksum_verify_good() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 0);
}

const char* test_wuffs_zlib_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_zlib_decode, &zlib_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char* test_wuffs_zlib_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_zlib_decode, &zlib_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char* test_wuffs_zlib_decode_sheep() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src =
      make_io_buffer_from_string(zlib_sheep_src_ptr, zlib_sheep_src_len);

  wuffs_zlib__decoder dec;
  const char* status = wuffs_zlib__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
  if (status) {
    RETURN_FAIL("initialize: %s", status);
  }

  int i;
  for (i = 0; i < 3; i++) {
    status = wuffs_zlib__decoder__decode_io_writer(&dec, &got, &src,
                                                   global_work_slice);

    if (status != wuffs_zlib__warning__dictionary_required) {
      RETURN_FAIL("decode_io_writer (before dict): got \"%s\", want \"%s\"",
                  status, wuffs_zlib__warning__dictionary_required);
    }

    uint32_t dict_id_got = wuffs_zlib__decoder__dictionary_id(&dec);
    uint32_t dict_id_want = 0x0BE0026E;
    if (dict_id_got != dict_id_want) {
      RETURN_FAIL("dictionary_id: got 0x%08" PRIX32 ", want 0x%08x" PRIX32,
                  dict_id_got, dict_id_want);
    }
  }

  wuffs_zlib__decoder__add_dictionary(
      &dec, ((wuffs_base__slice_u8){
                .ptr = ((uint8_t*)(zlib_sheep_dict_ptr)),
                .len = zlib_sheep_dict_len,
            }));

  status = wuffs_zlib__decoder__decode_io_writer(&dec, &got, &src,
                                                 global_work_slice);
  if (status) {
    RETURN_FAIL("decode_io_writer (after dict): %s", status);
  }

  wuffs_base__io_buffer want =
      make_io_buffer_from_string(zlib_sheep_want_ptr, zlib_sheep_want_len);
  return check_io_buffers_equal("", &got, &want);
}

  // ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char* test_mimic_zlib_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_zlib_decode, &zlib_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char* test_mimic_zlib_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_zlib_decode, &zlib_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char* test_mimic_zlib_decode_sheep() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src =
      make_io_buffer_from_string(zlib_sheep_src_ptr, zlib_sheep_src_len);
  wuffs_base__slice_u8 dict = ((wuffs_base__slice_u8){
      .ptr = ((uint8_t*)(zlib_sheep_dict_ptr)),
      .len = zlib_sheep_dict_len,
  });
  const char* status = mimic_zlib_decode_with_dictionary(&got, &src, dict);
  if (status) {
    return status;
  }
  wuffs_base__io_buffer want =
      make_io_buffer_from_string(zlib_sheep_want_ptr, zlib_sheep_want_len);
  return check_io_buffers_equal("", &got, &want);
}

#endif  // WUFFS_MIMIC

// ---------------- Zlib Benches

const char* bench_wuffs_zlib_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_zlib_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tc_dst, &zlib_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char* bench_wuffs_zlib_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_zlib_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tc_dst, &zlib_pi_gt, UINT64_MAX, UINT64_MAX, 30);
}

  // ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char* bench_mimic_zlib_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_zlib_decode, 0, tc_dst, &zlib_midsummer_gt,
                             UINT64_MAX, UINT64_MAX, 300);
}

const char* bench_mimic_zlib_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_zlib_decode, 0, tc_dst, &zlib_pi_gt,
                             UINT64_MAX, UINT64_MAX, 30);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

// The empty comments forces clang-format to place one element per line.
proc tests[] = {

    test_wuffs_zlib_checksum_ignore,       //
    test_wuffs_zlib_checksum_verify_bad0,  //
    test_wuffs_zlib_checksum_verify_bad3,  //
    test_wuffs_zlib_checksum_verify_good,  //
    test_wuffs_zlib_decode_midsummer,      //
    test_wuffs_zlib_decode_pi,             //
    test_wuffs_zlib_decode_sheep,          //

#ifdef WUFFS_MIMIC

    test_mimic_zlib_decode_midsummer,  //
    test_mimic_zlib_decode_pi,         //
    test_mimic_zlib_decode_sheep,      //

#endif  // WUFFS_MIMIC

    NULL,
};

// The empty comments forces clang-format to place one element per line.
proc benches[] = {

    bench_wuffs_zlib_decode_10k,   //
    bench_wuffs_zlib_decode_100k,  //

#ifdef WUFFS_MIMIC

    bench_mimic_zlib_decode_10k,   //
    bench_mimic_zlib_decode_100k,  //

#endif  // WUFFS_MIMIC

    NULL,
};

int main(int argc, char** argv) {
  proc_package_name = "std/zlib";
  return test_main(argc, argv, tests, benches);
}
