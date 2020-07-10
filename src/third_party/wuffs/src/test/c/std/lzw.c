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
  $CC -std=c99 -Wall -Werror lzw.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// !! wuffs mimic cflags: -DWUFFS_MIMIC

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
#define WUFFS_CONFIG__MODULE__LZW

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"

// ---------------- LZW Tests

const char* do_test_wuffs_lzw_decode(const char* src_filename,
                                     uint64_t src_size,
                                     const char* want_filename,
                                     uint64_t want_size,
                                     uint64_t wlimit,
                                     uint64_t rlimit) {
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = global_want_slice,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, src_filename);
  if (status) {
    return status;
  }
  if (src.meta.wi != src_size) {
    RETURN_FAIL("src size: got %d, want %d", (int)(src.meta.wi),
                (int)(src_size));
  }
  // The first byte in that file, the LZW literal width, should be 0x08.
  uint8_t literal_width = src.data.ptr[0];
  if (literal_width != 0x08) {
    RETURN_FAIL("LZW literal width: got %d, want %d", (int)(src.data.ptr[0]),
                0x08);
  }
  src.meta.ri++;

  status = read_file(&want, want_filename);
  if (status) {
    return status;
  }
  if (want.meta.wi != want_size) {
    RETURN_FAIL("want size: got %d, want %d", (int)(want.meta.wi),
                (int)(want_size));
  }

  wuffs_lzw__decoder dec;
  status = wuffs_lzw__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_lzw__decoder__set_literal_width(&dec, literal_width);
  int num_iters = 0;
  while (true) {
    num_iters++;
    wuffs_base__io_buffer limited_got = make_limited_writer(got, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
    size_t old_wi = got.meta.wi;
    size_t old_ri = src.meta.ri;

    status = wuffs_lzw__decoder__decode_io_writer(
        &dec, &limited_got, &limited_src, global_work_slice);
    got.meta.wi += limited_got.meta.wi;
    src.meta.ri += limited_src.meta.ri;
    if (!status) {
      if (src.meta.ri != src.meta.wi) {
        RETURN_FAIL("decode returned \"ok\" but src was not exhausted");
      }
      break;
    }
    if ((status != wuffs_base__suspension__short_read) &&
        (status != wuffs_base__suspension__short_write)) {
      RETURN_FAIL("decode: got \"%s\", want \"%s\" or \"%s\"", status,
                  wuffs_base__suspension__short_read,
                  wuffs_base__suspension__short_write);
    }

    if (got.meta.wi < old_wi) {
      RETURN_FAIL("write index got.wi went backwards");
    }
    if (src.meta.ri < old_ri) {
      RETURN_FAIL("read index src.ri went backwards");
    }
    if ((got.meta.wi == old_wi) && (src.meta.ri == old_ri)) {
      RETURN_FAIL("no progress was made");
    }
  }

  if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX)) {
    if (num_iters <= 1) {
      RETURN_FAIL("num_iters: got %d, want > 1", num_iters);
    }
  } else {
    if (num_iters != 1) {
      RETURN_FAIL("num_iters: got %d, want 1", num_iters);
    }
  }

  return check_io_buffers_equal("", &got, &want);
}

const char* test_wuffs_lzw_decode_bricks_dither() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_lzw_decode("test/data/bricks-dither.indexes.giflzw",
                                  14923, "test/data/bricks-dither.indexes",
                                  19200, UINT64_MAX, UINT64_MAX);
}

const char* test_wuffs_lzw_decode_bricks_nodither() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_lzw_decode("test/data/bricks-nodither.indexes.giflzw",
                                  13382, "test/data/bricks-nodither.indexes",
                                  19200, UINT64_MAX, UINT64_MAX);
}

const char* test_wuffs_lzw_decode_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_lzw_decode("test/data/bricks-gray.indexes.giflzw", 14731,
                                  "test/data/bricks-gray.indexes", 19200,
                                  UINT64_MAX, 4096);
}

const char* test_wuffs_lzw_decode_many_small_writes_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_lzw_decode("test/data/bricks-gray.indexes.giflzw", 14731,
                                  "test/data/bricks-gray.indexes", 19200, 41,
                                  43);
}

const char* test_wuffs_lzw_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_lzw_decode("test/data/pi.txt.giflzw", 50550,
                                  "test/data/pi.txt", 100003, UINT64_MAX,
                                  UINT64_MAX);
}

const char* test_wuffs_lzw_decode_output_bad() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  // Set up src to be 20 bytes long, starting with three 8-bit literal codes
  // (0x41, 0x42, 0x43) then a bad 8-bit code 0xFF. Decoding should produce 3
  // bytes and consume 4 bytes.
  src.meta.wi = 20;
  int i;
  for (i = 0; i < src.meta.wi; i++) {
    src.data.ptr[i] = 0;
  }
  src.data.ptr[0] = 0x41;
  src.data.ptr[1] = 0x42;
  src.data.ptr[2] = 0x43;
  src.data.ptr[3] = 0xFF;

  wuffs_lzw__decoder dec;
  const char* status = wuffs_lzw__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_lzw__decoder__set_literal_width(&dec, 7);

  status =
      wuffs_lzw__decoder__decode_io_writer(&dec, &got, &src, global_work_slice);
  if (status != wuffs_lzw__error__bad_code) {
    RETURN_FAIL("decode: \"%s\"", status);
  }

  if (got.meta.wi != 3) {
    RETURN_FAIL("got.meta.wi: got %d, want 3", (int)(got.meta.wi));
  }
  if (src.meta.ri != 4) {
    RETURN_FAIL("src.meta.ri: got %d, want 4", (int)(src.meta.ri));
  }
  return NULL;
}

const char* test_wuffs_lzw_decode_output_empty() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  // Set up src to be 20 bytes long, starting with the 9-bit end code 0x101.
  // Decoding should produce 0 bytes and consume 2 bytes.
  src.meta.wi = 20;
  int i;
  for (i = 0; i < src.meta.wi; i++) {
    src.data.ptr[i] = 0;
  }
  src.data.ptr[0] = 0x01;
  src.data.ptr[1] = 0x01;

  wuffs_lzw__decoder dec;
  const char* status = wuffs_lzw__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_lzw__decoder__set_literal_width(&dec, 8);

  status =
      wuffs_lzw__decoder__decode_io_writer(&dec, &got, &src, global_work_slice);
  if (status) {
    RETURN_FAIL("decode: \"%s\"", status);
  }

  if (got.meta.wi != 0) {
    RETURN_FAIL("got.meta.wi: got %d, want 0", (int)(got.meta.wi));
  }
  if (src.meta.ri != 2) {
    RETURN_FAIL("src.meta.ri: got %d, want 2", (int)(src.meta.ri));
  }
  return NULL;
}

const char* do_test_wuffs_lzw_decode_width(uint32_t width,
                                           wuffs_base__io_buffer src,
                                           wuffs_base__io_buffer want) {
  wuffs_lzw__decoder dec;
  const char* status = wuffs_lzw__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_lzw__decoder__set_literal_width(&dec, width);

  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  status =
      wuffs_lzw__decoder__decode_io_writer(&dec, &got, &src, global_work_slice);
  if (status) {
    RETURN_FAIL("decode: \"%s\"", status);
  }

  return check_io_buffers_equal("", &got, &want);
}

// A zero literal width isn't very practical: the output bytes can only be 0x00
// and it isn't possible to encode the empty string, as the End code requires
// two bits but the first non-Clear code after a Clear code has only one bit,
// so it must be the literal 0x00. Nonetheless, the giflib C library accepts a
// zero literal width (it only rejects literal widths above 8), so we do too.
const char* test_wuffs_lzw_decode_width_0() {
  CHECK_FOCUS(__func__);

  // 0b...._...._...._...1  0x001 Clear code.
  // 0b...._...._...._..0.  0x000 Literal "0".
  // 0b...._...._...._11..  0x011 Back-ref "00"
  // 0b...._...._.100_....  0x100 Back-ref "000".
  // 0b...._..00_0..._....  0x000 Literal "0".
  // 0b...0_10.._...._....  0x010 End code.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  src.meta.wi = 2;
  src.data.ptr[0] = 0x4D;
  src.data.ptr[1] = 0x08;

  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = global_want_slice,
  });
  want.meta.wi = 7;
  want.data.ptr[0] = 0x00;
  want.data.ptr[1] = 0x00;
  want.data.ptr[2] = 0x00;
  want.data.ptr[3] = 0x00;
  want.data.ptr[4] = 0x00;
  want.data.ptr[5] = 0x00;
  want.data.ptr[6] = 0x00;

  return do_test_wuffs_lzw_decode_width(0, src, want);
}

const char* test_wuffs_lzw_decode_width_1() {
  CHECK_FOCUS(__func__);

  // 0b...._...._...._..10  0x010 Clear code.
  // 0b...._...._...._00..  0x000 Literal "0".
  // 0b...._...._.001_....  0x001 Literal "1".
  // 0b...._..10_0..._....  0x100 Back-ref "01".
  // 0b...0_11.._...._....  0x011 End code.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  src.meta.wi = 2;
  src.data.ptr[0] = 0x12;
  src.data.ptr[1] = 0x0E;

  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = global_want_slice,
  });
  want.meta.wi = 4;
  want.data.ptr[0] = 0x00;
  want.data.ptr[1] = 0x01;
  want.data.ptr[2] = 0x00;
  want.data.ptr[3] = 0x01;

  return do_test_wuffs_lzw_decode_width(1, src, want);
}

// ---------------- LZW Benches

const char* do_bench_wuffs_lzw_decode(const char* filename,
                                      uint64_t iters_unscaled) {
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, filename);
  if (status) {
    return status;
  }
  if (src.meta.wi <= 0) {
    RETURN_FAIL("src size: got %d, want > 0", (int)(src.meta.wi));
  }
  uint8_t literal_width = src.data.ptr[0];
  if (literal_width != 0x08) {
    RETURN_FAIL("LZW literal width: got %d, want %d", (int)(src.data.ptr[0]),
                0x08);
  }

  bench_start();
  uint64_t n_bytes = 0;
  uint64_t i;
  uint64_t iters = iters_unscaled * iterscale;
  for (i = 0; i < iters; i++) {
    got.meta.wi = 0;
    src.meta.ri = 1;  // Skip the literal width.
    wuffs_lzw__decoder dec;
    status = wuffs_lzw__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("initialize: \"%s\"", status);
    }
    status = wuffs_lzw__decoder__decode_io_writer(&dec, &got, &src,
                                                  global_work_slice);
    if (status) {
      RETURN_FAIL("decode: \"%s\"", status);
    }
    n_bytes += got.meta.wi;
  }
  bench_finish(iters, n_bytes);
  return NULL;
}

const char* bench_wuffs_lzw_decode_20k() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_lzw_decode("test/data/bricks-gray.indexes.giflzw", 50);
}

const char* bench_wuffs_lzw_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_lzw_decode("test/data/pi.txt.giflzw", 10);
}

// ---------------- Manifest

// The empty comments forces clang-format to place one element per line.
proc tests[] = {

    test_wuffs_lzw_decode_bricks_dither,            //
    test_wuffs_lzw_decode_bricks_nodither,          //
    test_wuffs_lzw_decode_many_big_reads,           //
    test_wuffs_lzw_decode_many_small_writes_reads,  //
    test_wuffs_lzw_decode_output_bad,               //
    test_wuffs_lzw_decode_output_empty,             //
    test_wuffs_lzw_decode_pi,                       //
    test_wuffs_lzw_decode_width_0,                  //
    test_wuffs_lzw_decode_width_1,                  //

    NULL,
};

// The empty comments forces clang-format to place one element per line.
proc benches[] = {

    bench_wuffs_lzw_decode_20k,   //
    bench_wuffs_lzw_decode_100k,  //

    NULL,
};

int main(int argc, char** argv) {
  proc_package_name = "std/lzw";
  return test_main(argc, argv, tests, benches);
}
