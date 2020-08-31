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
  $CC -std=c99 -Wall -Werror gif.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// !! wuffs mimic cflags: -DWUFFS_MIMIC -lgif

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
#define WUFFS_CONFIG__MODULE__GIF
#define WUFFS_CONFIG__MODULE__LZW

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/gif.c"
#endif

// ---------------- Basic Tests

const char* test_basic_bad_receiver() {
  CHECK_FOCUS(__func__);
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  const char* status = wuffs_gif__decoder__decode_image_config(NULL, &ic, NULL);
  if (status != wuffs_base__error__bad_receiver) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_base__error__bad_receiver);
  }
  return NULL;
}

const char* test_basic_bad_sizeof_receiver() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  const char* status = wuffs_gif__decoder__initialize(
      &dec, 0, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status != wuffs_base__error__bad_sizeof_receiver) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_base__error__bad_sizeof_receiver);
  }
  return NULL;
}

const char* test_basic_bad_wuffs_version() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  const char* status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION ^ 0x123456789ABC,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status != wuffs_base__error__bad_wuffs_version) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_base__error__bad_wuffs_version);
  }
  return NULL;
}

const char* test_basic_initialize_not_called() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec = ((wuffs_gif__decoder){});
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  const char* status = wuffs_gif__decoder__decode_image_config(&dec, &ic, NULL);
  if (status != wuffs_base__error__initialize_not_called) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_base__error__initialize_not_called);
  }
  return NULL;
}

const char* test_basic_status_is_error() {
  CHECK_FOCUS(__func__);
  if (wuffs_base__status__is_error(NULL)) {
    RETURN_FAIL("is_error(NULL) returned true");
  }
  if (!wuffs_base__status__is_error(wuffs_base__error__bad_wuffs_version)) {
    RETURN_FAIL("is_error(BAD_WUFFS_VERSION) returned false");
  }
  if (wuffs_base__status__is_error(wuffs_base__suspension__short_write)) {
    RETURN_FAIL("is_error(SHORT_WRITE) returned true");
  }
  if (!wuffs_base__status__is_error(wuffs_gif__error__bad_header)) {
    RETURN_FAIL("is_error(BAD_HEADER) returned false");
  }
  return NULL;
}

const char* test_basic_status_strings() {
  CHECK_FOCUS(__func__);
  const char* s1 = wuffs_base__error__bad_wuffs_version;
  const char* t1 = "#base: bad wuffs version";
  if (strcmp(s1, t1)) {
    RETURN_FAIL("got \"%s\", want \"%s\"", s1, t1);
  }
  const char* s2 = wuffs_base__suspension__short_write;
  const char* t2 = "$base: short write";
  if (strcmp(s2, t2)) {
    RETURN_FAIL("got \"%s\", want \"%s\"", s2, t2);
  }
  const char* s3 = wuffs_gif__error__bad_header;
  const char* t3 = "#gif: bad header";
  if (strcmp(s3, t3)) {
    RETURN_FAIL("got \"%s\", want \"%s\"", s3, t3);
  }
  return NULL;
}

const char* test_basic_status_used_package() {
  CHECK_FOCUS(__func__);
  // The function call here is from "std/gif" but the argument is from
  // "std/lzw". The former package depends on the latter.
  const char* s0 = wuffs_lzw__error__bad_code;
  const char* t0 = "#lzw: bad code";
  if (strcmp(s0, t0)) {
    RETURN_FAIL("got \"%s\", want \"%s\"", s0, t0);
  }
  return NULL;
}

const char* test_basic_sub_struct_initializer() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  const char* status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  if (dec.private_impl.magic != WUFFS_BASE__MAGIC) {
    RETURN_FAIL("outer magic: got %" PRIu32 ", want %" PRIu32 "",
                dec.private_impl.magic, WUFFS_BASE__MAGIC);
  }
  if (dec.private_data.f_lzw.private_impl.magic != WUFFS_BASE__MAGIC) {
    RETURN_FAIL("inner magic: got %" PRIu32 ", want %" PRIu32,
                dec.private_data.f_lzw.private_impl.magic, WUFFS_BASE__MAGIC);
  }
  return NULL;
}

// ---------------- GIF Tests

const char* wuffs_gif_decode(wuffs_base__io_buffer* dst,
                             uint32_t wuffs_initialize_flags,
                             wuffs_base__pixel_format pixfmt,
                             wuffs_base__io_buffer* src) {
  wuffs_gif__decoder dec;
  const char* status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION, wuffs_initialize_flags);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }

  wuffs_base__pixel_config__set(&ic.pixcfg, pixfmt,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
                                wuffs_base__pixel_config__width(&ic.pixcfg),
                                wuffs_base__pixel_config__height(&ic.pixcfg));

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }

  while (true) {
    status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, src);
    if (status == wuffs_base__warning__end_of_data) {
      break;
    } else if (status) {
      RETURN_FAIL("decode_frame_config: \"%s\"", status);
    }

    status = wuffs_gif__decoder__decode_frame(&dec, &pb, src, global_work_slice,
                                              NULL);
    if (status) {
      RETURN_FAIL("decode_frame: \"%s\"", status);
    }

    status = copy_to_io_buffer_from_pixel_buffer(
        dst, &pb, wuffs_base__frame_config__bounds(&fc));
    if (status) {
      RETURN_FAIL("copy_to_io_buffer_from_pixel_buffer: \"%s\"", status);
    }
  }
  return NULL;
}

const char* do_test_wuffs_gif_decode(const char* filename,
                                     const char* palette_filename,
                                     const char* indexes_filename,
                                     uint64_t rlimit,
                                     wuffs_base__pixel_format dst_pixfmt) {
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

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});

  {
    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("decode_image_config: got \"%s\"", status);
    }
    if (wuffs_base__pixel_config__pixel_format(&ic.pixcfg) !=
        WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
      RETURN_FAIL("pixel_format: got 0x%08" PRIX32 ", want 0x%08" PRIX32,
                  wuffs_base__pixel_config__pixel_format(&ic.pixcfg),
                  WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
    }

    // bricks-dither.gif is a 160 × 120, opaque, still (not animated) GIF.
    if (wuffs_base__pixel_config__width(&ic.pixcfg) != 160) {
      RETURN_FAIL("width: got %" PRIu32 ", want 160",
                  wuffs_base__pixel_config__width(&ic.pixcfg));
    }
    if (wuffs_base__pixel_config__height(&ic.pixcfg) != 120) {
      RETURN_FAIL("height: got %" PRIu32 ", want 120",
                  wuffs_base__pixel_config__height(&ic.pixcfg));
    }
    if (!wuffs_base__image_config__first_frame_is_opaque(&ic)) {
      RETURN_FAIL("first_frame_is_opaque: got false, want true");
    }

    wuffs_base__pixel_config__set(
        &ic.pixcfg, dst_pixfmt, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, 160, 120);

    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      global_pixel_slice);
    if (status) {
      RETURN_FAIL("set_from_slice: \"%s\"", status);
    }

    uint32_t got = wuffs_gif__decoder__num_animation_loops(&dec);
    if (got != 1) {
      RETURN_FAIL("num_animation_loops: got %" PRIu32 ", want 1", got);
    }
  }

  if (wuffs_gif__decoder__workbuf_len(&dec).min_incl != 1) {
    RETURN_FAIL("workbuf_len: got %" PRIu64 ", want 1",
                wuffs_gif__decoder__workbuf_len(&dec).min_incl);
  }

  int num_iters = 0;
  while (true) {
    num_iters++;
    wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
    size_t old_ri = src.meta.ri;

    status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &limited_src);
    src.meta.ri += limited_src.meta.ri;

    if (!status) {
      break;
    }
    if (status != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame_config: got \"%s\", want \"%s\"", status,
                  wuffs_base__suspension__short_read);
    }

    if (src.meta.ri < old_ri) {
      RETURN_FAIL("read index src.meta.ri went backwards");
    }
    if (src.meta.ri == old_ri) {
      RETURN_FAIL("no progress was made");
    }
  }

  while (true) {
    num_iters++;
    wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
    size_t old_ri = src.meta.ri;

    const char* status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &limited_src, global_work_slice, NULL);
    src.meta.ri += limited_src.meta.ri;

    if (!status) {
      break;
    }
    if (status != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame: got \"%s\", want \"%s\"", status,
                  wuffs_base__suspension__short_read);
    }

    if (src.meta.ri < old_ri) {
      RETURN_FAIL("read index src.meta.ri went backwards");
    }
    if (src.meta.ri == old_ri) {
      RETURN_FAIL("no progress was made");
    }
  }

  status = copy_to_io_buffer_from_pixel_buffer(
      &got, &pb, wuffs_base__frame_config__bounds(&fc));
  if (status) {
    return status;
  }

  if (rlimit < UINT64_MAX) {
    if (num_iters <= 2) {
      RETURN_FAIL("num_iters: got %d, want > 2", num_iters);
    }
  } else {
    if (num_iters != 2) {
      RETURN_FAIL("num_iters: got %d, want 2", num_iters);
    }
  }

  uint8_t pal_want_array[1024];
  wuffs_base__io_buffer pal_want = ((wuffs_base__io_buffer){
      .data = ((wuffs_base__slice_u8){
          .ptr = pal_want_array,
          .len = 1024,
      }),
  });
  status = read_file(&pal_want, palette_filename);
  if (status) {
    return status;
  }
  if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    wuffs_base__io_buffer pal_got = ((wuffs_base__io_buffer){
        .data = wuffs_base__pixel_buffer__palette(&pb),
    });
    pal_got.meta.wi = pal_got.data.len;
    status = check_io_buffers_equal("palette ", &pal_got, &pal_want);
    if (status) {
      return status;
    }
  }

  wuffs_base__io_buffer ind_want = ((wuffs_base__io_buffer){
      .data = global_want_slice,
  });
  status = read_file(&ind_want, indexes_filename);
  if (status) {
    return status;
  }
  if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    status = check_io_buffers_equal("indexes ", &got, &ind_want);
    if (status) {
      return status;
    }
  } else {
    wuffs_base__io_buffer expanded_want = ((wuffs_base__io_buffer){
        .data = global_work_slice,
    });
    if (ind_want.meta.wi > (expanded_want.data.len / 4)) {
      RETURN_FAIL("indexes are too long to expand into the work buffer");
    }

    if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__BGR) {
      size_t i;
      for (i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[3 * i + 0] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[3 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[3 * i + 2] = pal_want_array[4 * index + 2];
      }
      expanded_want.meta.wi = 3 * ind_want.meta.wi;
    } else if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL) {
      size_t i;
      for (i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[4 * i + 0] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[4 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[4 * i + 2] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[4 * i + 3] = pal_want_array[4 * index + 3];
      }
      expanded_want.meta.wi = 4 * ind_want.meta.wi;
    } else if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__RGB) {
      size_t i;
      for (i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[3 * i + 0] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[3 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[3 * i + 2] = pal_want_array[4 * index + 0];
      }
      expanded_want.meta.wi = 3 * ind_want.meta.wi;
    } else if (dst_pixfmt == WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL) {
      size_t i;
      for (i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[4 * i + 0] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[4 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[4 * i + 2] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[4 * i + 3] = pal_want_array[4 * index + 3];
      }
      expanded_want.meta.wi = 4 * ind_want.meta.wi;
    } else {
      return "unsupported pixel format";
    }

    status = check_io_buffers_equal("pixels ", &got, &expanded_want);
    if (status) {
      return status;
    }
  }

  {
    if (src.meta.ri == src.meta.wi) {
      RETURN_FAIL("decode_frame returned \"ok\" but src was exhausted");
    }
    status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                              global_work_slice, NULL);
    if (status != wuffs_base__warning__end_of_data) {
      RETURN_FAIL("decode_frame: got \"%s\", want \"%s\"", status,
                  wuffs_base__warning__end_of_data);
    }
    if (src.meta.ri != src.meta.wi) {
      RETURN_FAIL(
          "decode_frame returned \"end of data\" but src was not exhausted");
    }
  }

  return NULL;
}

const char* do_test_wuffs_gif_decode_expecting(wuffs_base__io_buffer src,
                                               uint32_t quirk,
                                               const char* want_status,
                                               bool want_dirty_rect_is_empty) {
  wuffs_gif__decoder dec;
  const char* status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  if (quirk) {
    wuffs_gif__decoder__set_quirk_enabled(&dec, quirk, true);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: got \"%s\"", status);
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }

  status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src, global_work_slice,
                                            NULL);
  if (status != want_status) {
    RETURN_FAIL("decode_frame: got \"%s\", want \"%s\"", status, want_status);
  }

  wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
  bool dirty_rect_is_empty = wuffs_base__rect_ie_u32__is_empty(&r);
  if (dirty_rect_is_empty != want_dirty_rect_is_empty) {
    RETURN_FAIL("dirty_rect.is_empty: got %s, want %s",
                dirty_rect_is_empty ? "true" : "false",
                want_dirty_rect_is_empty ? "true" : "false");
  }
  return NULL;
}

const char* test_wuffs_gif_call_interleaved() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/bricks-dither.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  {
    wuffs_base__io_buffer limited_src = make_limited_reader(src, 700);
    status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &limited_src);
    src.meta.ri += limited_src.meta.ri;
    if (status != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame_config: got \"%s\", want \"%s\"", status,
                  wuffs_base__suspension__short_read);
    }
  }

  // Calling another coroutine (decode_image_config) while suspended inside an
  // active coroutine (decode_frame_config) should be an error:
  // wuffs_base__error__interleaved_coroutine_calls.

  {
    status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
    if (status != wuffs_base__error__interleaved_coroutine_calls) {
      RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                  wuffs_base__error__interleaved_coroutine_calls);
    }
  }

  return NULL;
}

const char* test_wuffs_gif_call_sequence() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/bricks-dither.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: got \"%s\"", status);
  }

  status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status != wuffs_base__error__bad_call_sequence) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_base__error__bad_call_sequence);
  }
  return NULL;
}

const char* do_test_wuffs_gif_decode_animated(
    const char* filename,
    uint32_t want_num_loops,
    uint32_t want_num_frames,
    wuffs_base__rect_ie_u32* want_frame_config_bounds) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, filename);
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: got \"%s\"", status);
  }

  uint32_t got_num_loops = wuffs_gif__decoder__num_animation_loops(&dec);
  if (got_num_loops != want_num_loops) {
    RETURN_FAIL("num_loops: got %" PRIu32 ", want %" PRIu32, got_num_loops,
                want_num_loops);
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }

  uint32_t i;
  for (i = 0; i < want_num_frames; i++) {
    wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
    status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
    if (status) {
      RETURN_FAIL("decode_frame_config #%" PRIu32 ": got \"%s\"", i, status);
    }

    if (want_frame_config_bounds) {
      wuffs_base__rect_ie_u32 got = wuffs_base__frame_config__bounds(&fc);
      wuffs_base__rect_ie_u32 want = want_frame_config_bounds[i];
      if (!wuffs_base__rect_ie_u32__equals(&got, want)) {
        RETURN_FAIL("decode_frame_config #%" PRIu32 ": bounds: got (%" PRIu32
                    ", %" PRIu32 ")-(%" PRIu32 ", %" PRIu32 "), want (%" PRIu32
                    ", %" PRIu32 ")-(%" PRIu32 ", %" PRIu32 ")",
                    i, got.min_incl_x, got.min_incl_y, got.max_excl_x,
                    got.max_excl_y, want.min_incl_x, want.min_incl_y,
                    want.max_excl_x, want.max_excl_y);
      }
    }

    status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                              global_work_slice, NULL);
    if (status) {
      RETURN_FAIL("decode_frame #%" PRIu32 ": got \"%s\"", i, status);
    }

    wuffs_base__rect_ie_u32 frame_rect = wuffs_base__frame_config__bounds(&fc);
    wuffs_base__rect_ie_u32 dirty_rect =
        wuffs_gif__decoder__frame_dirty_rect(&dec);
    if (!wuffs_base__rect_ie_u32__contains_rect(&frame_rect, dirty_rect)) {
      RETURN_FAIL("internal error: frame_rect does not contain dirty_rect");
    }
  }

  // There should be no more frames, no matter how many times we call
  // decode_frame.
  for (i = 0; i < 3; i++) {
    status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                              global_work_slice, NULL);
    if (status != wuffs_base__warning__end_of_data) {
      RETURN_FAIL("decode_frame: got \"%s\", want \"%s\"", status,
                  wuffs_base__warning__end_of_data);
    }
  }

  uint64_t got_num_frames = wuffs_gif__decoder__num_decoded_frames(&dec);
  if (got_num_frames != want_num_frames) {
    RETURN_FAIL("frame_count: got %" PRIu64 ", want %" PRIu32, got_num_frames,
                want_num_frames);
  }

  return NULL;
}

const char* test_wuffs_gif_decode_animated_big() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_animated("test/data/gifplayer-muybridge.gif",
                                           0, 380, NULL);
}

const char* test_wuffs_gif_decode_animated_medium() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_animated("test/data/muybridge.gif", 0, 15,
                                           NULL);
}

const char* test_wuffs_gif_decode_animated_small() {
  CHECK_FOCUS(__func__);
  // animated-red-blue.gif's num_loops should be 3. The value explicitly in the
  // wire format is 0x0002, but that value means "repeat 2 times after the
  // first play", so the total number of loops is 3.
  const uint32_t want_num_loops = 3;
  wuffs_base__rect_ie_u32 want_frame_config_bounds[4] = {
      make_rect_ie_u32(0, 0, 64, 48),
      make_rect_ie_u32(15, 31, 52, 40),
      make_rect_ie_u32(15, 0, 64, 40),
      make_rect_ie_u32(15, 0, 64, 40),
  };
  return do_test_wuffs_gif_decode_animated(
      "test/data/animated-red-blue.gif", want_num_loops,
      WUFFS_TESTLIB_ARRAY_SIZE(want_frame_config_bounds),
      want_frame_config_bounds);
}

const char* test_wuffs_gif_decode_delay_num_frames_decoded() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status = read_file(&src, "test/data/animated-red-blue.gif");
  if (status) {
    return status;
  }
  if (src.meta.wi < 1) {
    return "src file is too short";
  }

  // A GIF image should end with the 0x3B Trailer byte.
  if (src.data.ptr[src.meta.wi - 1] != 0x3B) {
    RETURN_FAIL("final byte: got 0x%02X, want 0x%02X",
                src.data.ptr[src.meta.wi - 1], 0x3B);
  }
  // Replace that final byte with something invalid: neither 0x21 (Extension
  // Introducer), 0x2C (Image Separator) or 0x3B (Trailer).
  src.data.ptr[src.meta.wi - 1] = 0x99;

  int q;
  for (q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }
    wuffs_gif__decoder__set_quirk_enabled(
        &dec, WUFFS_GIF__QUIRK_DELAY_NUM_DECODED_FRAMES, q);

    while (true) {
      status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
      if (status) {
        break;
      }
    }

    uint64_t got = wuffs_gif__decoder__num_decoded_frames(&dec);
    uint64_t want = q ? 3 : 4;
    if (got != want) {
      RETURN_FAIL("q=%d: num_decoded_frames: got %" PRIu64 ", want %" PRIu64, q,
                  got, want);
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_empty_palette() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-empty-palette.gif");
  if (status) {
    return status;
  }
  int q;
  for (q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }
    wuffs_gif__decoder__set_quirk_enabled(
        &dec, WUFFS_GIF__QUIRK_REJECT_EMPTY_PALETTE, q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status);
    }

    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      global_pixel_slice);
    if (status) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status);
    }

    int i;
    for (i = 0; i < 2; i++) {
      status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                global_work_slice, NULL);
      if ((q == 1) && (i == 1)) {
        if (status != wuffs_gif__error__bad_palette) {
          RETURN_FAIL("q=%d: i=%d: decode_frame: got \"%s\", want \"%s\"", q, i,
                      status, wuffs_gif__error__bad_palette);
        }
        break;
      }
      if (status) {
        RETURN_FAIL("q=%d: i=%d: decode_frame: \"%s\"", q, i, status);
      }

      wuffs_base__slice_u8 pal = wuffs_base__pixel_buffer__palette(&pb);
      if (pal.len < 4) {
        RETURN_FAIL("q=%d: i=%d: palette length: got %d, want >= 4", q, i,
                    (int)(pal.len));
      }
      uint8_t* p = pal.ptr;
      int blue = i ? 0x00 : 0xFF;
      if ((p[0] != blue) || (p[1] != 0x00) || (p[2] != 0x00) ||
          (p[3] != 0xFF)) {
        RETURN_FAIL(
            "q=%d: i=%d: palette[0] BGRA: got (0x%02X, 0x%02X, 0x%02X, "
            "0x%02X), want (0x%02X, 0x00, 0x00, 0xFF)",
            q, i, (int)(p[0]), (int)(p[1]), (int)(p[2]), (int)(p[3]), blue);
      }
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_background_color() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-background-color.gif");
  if (status) {
    return status;
  }
  int q;
  for (q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }
    wuffs_gif__decoder__set_quirk_enabled(
        &dec, WUFFS_GIF__QUIRK_HONOR_BACKGROUND_COLOR, q);

    wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
    status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
    if (status) {
      RETURN_FAIL("q=%d: decode_frame_config: \"%s\"", q, status);
    }

    wuffs_base__color_u32_argb_premul got =
        wuffs_base__frame_config__background_color(&fc);
    wuffs_base__color_u32_argb_premul want = q ? 0xFF80C3C3 : 0x00000000;

    if (got != want) {
      RETURN_FAIL("q=%d: got 0x%08" PRIX32 ", want 0x%08" PRIX32, q, got, want);
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_first_frame_is_opaque() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-frame-out-of-bounds.gif");
  if (status) {
    return status;
  }
  int q;
  for (q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }
    wuffs_gif__decoder__set_quirk_enabled(
        &dec, WUFFS_GIF__QUIRK_HONOR_BACKGROUND_COLOR, q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status);
    }

    bool got = wuffs_base__image_config__first_frame_is_opaque(&ic);
    bool want = q;

    if (got != want) {
      RETURN_FAIL("q=%d: got %s, want %s", q, got ? "true" : "false",
                  want ? "true" : "false");
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_frame_out_of_bounds() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-frame-out-of-bounds.gif");
  if (status) {
    return status;
  }

  int q;
  for (q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }
    wuffs_gif__decoder__set_quirk_enabled(
        &dec, WUFFS_GIF__QUIRK_IMAGE_BOUNDS_ARE_STRICT, q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status);
    }

    // The nominal width and height for the overall image is 2×2, but its first
    // frame extends those bounds to 4×2. See
    // test/data/artificial/gif-frame-out-of-bounds.gif.make-artificial.txt for
    // more discussion.

    const uint32_t width = q ? 2 : 4;
    const uint32_t height = 2;

    if (wuffs_base__pixel_config__width(&ic.pixcfg) != width) {
      RETURN_FAIL("q=%d: width: got %" PRIu32 ", want %" PRIu32, q,
                  wuffs_base__pixel_config__width(&ic.pixcfg), width);
    }
    if (wuffs_base__pixel_config__height(&ic.pixcfg) != height) {
      RETURN_FAIL("q=%d: height: got %" PRIu32 ", want %" PRIu32, q,
                  wuffs_base__pixel_config__height(&ic.pixcfg), height);
    }

    wuffs_base__pixel_config five_by_five = ((wuffs_base__pixel_config){});
    wuffs_base__pixel_config__set(
        &five_by_five, wuffs_base__pixel_config__pixel_format(&ic.pixcfg),
        wuffs_base__pixel_config__pixel_subsampling(&ic.pixcfg), 5, 5);
    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &five_by_five,
                                                      global_pixel_slice);
    if (status) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status);
    }

    // See test/data/artificial/gif-frame-out-of-bounds.gif.make-artificial.txt
    // for the want_frame_config_bounds and want_pixel_indexes source.

    wuffs_base__rect_ie_u32 want_frame_config_bounds[4] = {
        q ? make_rect_ie_u32(1, 0, 2, 1) : make_rect_ie_u32(1, 0, 4, 1),
        q ? make_rect_ie_u32(0, 1, 2, 2) : make_rect_ie_u32(0, 1, 2, 2),
        q ? make_rect_ie_u32(0, 2, 1, 2) : make_rect_ie_u32(0, 2, 1, 2),
        q ? make_rect_ie_u32(2, 0, 2, 2) : make_rect_ie_u32(2, 0, 4, 2),
    };

    const char* want_pixel_indexes[4] = {
        q ? ".1.." : ".123....",
        q ? "..89" : "....89..",
        q ? "...." : "........",
        q ? "...." : "..45..89",
    };

    uint32_t i;
    for (i = 0; true; i++) {
      wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
      {
        status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
        if (i == WUFFS_TESTLIB_ARRAY_SIZE(want_frame_config_bounds)) {
          if (status != wuffs_base__warning__end_of_data) {
            RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32 ": got \"%s\"", q,
                        i, status);
          }
          break;
        }
        if (status) {
          RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32 ": got \"%s\"", q,
                      i, status);
        }

        wuffs_base__rect_ie_u32 got = wuffs_base__frame_config__bounds(&fc);
        wuffs_base__rect_ie_u32 want = want_frame_config_bounds[i];
        if (!wuffs_base__rect_ie_u32__equals(&got, want)) {
          RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32
                      ": bounds: got (%" PRIu32 ", %" PRIu32 ")-(%" PRIu32
                      ", %" PRIu32 "), want (%" PRIu32 ", %" PRIu32
                      ")-(%" PRIu32 ", %" PRIu32 ")",
                      q, i, got.min_incl_x, got.min_incl_y, got.max_excl_x,
                      got.max_excl_y, want.min_incl_x, want.min_incl_y,
                      want.max_excl_x, want.max_excl_y);
        }
      }

      {
        wuffs_base__table_u8 p = wuffs_base__pixel_buffer__plane(&pb, 0);
        uint32_t y, x;
        for (y = 0; y < 5; y++) {
          for (x = 0; x < 5; x++) {
            p.ptr[(y * p.stride) + x] = 0xEE;
          }
        }
        for (y = 0; y < height; y++) {
          for (x = 0; x < width; x++) {
            p.ptr[(y * p.stride) + x] = 0;
          }
        }

        status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                  global_work_slice, NULL);
        if (status) {
          RETURN_FAIL("q=%d: decode_frame #%" PRIu32 ": got \"%s\"", q, i,
                      status);
        }

        for (y = 0; y < 5; y++) {
          for (x = 0; x < 5; x++) {
            if ((x < width) && (y < height)) {
              continue;
            }
            if (p.ptr[(y * p.stride) + x] != 0xEE) {
              RETURN_FAIL("q=%d: decode_frame #%" PRIu32
                          ": dirty pixel (%" PRIu32 ", %" PRIu32
                          ") outside of image bounds",
                          q, i, x, y);
            }
          }
        }

        wuffs_base__rect_ie_u32 frame_rect =
            wuffs_base__frame_config__bounds(&fc);
        wuffs_base__rect_ie_u32 dirty_rect =
            wuffs_gif__decoder__frame_dirty_rect(&dec);
        if (!wuffs_base__rect_ie_u32__contains_rect(&frame_rect, dirty_rect)) {
          RETURN_FAIL(
              "q=%d: internal error: frame_rect does not contain dirty_rect",
              q);
        }

        char got[(width * height) + 1];
        for (y = 0; y < height; y++) {
          for (x = 0; x < width; x++) {
            uint8_t index = 0x0F & p.ptr[(y * p.stride) + x];
            got[(y * width) + x] = index ? ('0' + index) : '.';
          }
        }
        got[width * height] = 0;

        const char* want = want_pixel_indexes[i];
        if (memcmp(got, want, width * height)) {
          RETURN_FAIL("q=%d: decode_frame #%" PRIu32
                      ": got \"%s\", want \"%s\"",
                      q, i, got, want);
        }
      }
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_zero_width_frame() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-zero-width-frame.gif");
  if (status) {
    return status;
  }
  int q;
  for (q = 0; q < 3; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    status = wuffs_gif__decoder__initialize(
        &dec, sizeof dec, WUFFS_VERSION,
        WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
    if (status) {
      RETURN_FAIL("q=%d: initialize: \"%s\"", q, status);
    }

    const char* want = NULL;
    switch (q) {
      case 0:
        want = wuffs_base__error__too_much_data;
        break;
      case 1:
        want = NULL;
        wuffs_gif__decoder__set_quirk_enabled(
            &dec, WUFFS_GIF__QUIRK_IGNORE_TOO_MUCH_PIXEL_DATA, true);
        break;
      case 2:
        want = wuffs_gif__error__bad_frame_size;
        wuffs_gif__decoder__set_quirk_enabled(
            &dec, WUFFS_GIF__QUIRK_REJECT_EMPTY_FRAME, true);
        break;
    }

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status);
    }

    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      global_pixel_slice);
    if (status) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status);
    }

    const char* got = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                       global_work_slice, NULL);
    if (got != want) {
      RETURN_FAIL("q=%d: decode_frame: got \"%s\", want \"%s\"", q, got, want);
    }
  }
  return NULL;
}

const char* test_wuffs_gif_decode_pixfmt_bgr() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode("test/data/bricks-dither.gif",
                                  "test/data/bricks-dither.palette",
                                  "test/data/bricks-dither.indexes", UINT64_MAX,
                                  WUFFS_BASE__PIXEL_FORMAT__BGR);
}

const char* test_wuffs_gif_decode_pixfmt_bgra_nonpremul() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode("test/data/bricks-dither.gif",
                                  "test/data/bricks-dither.palette",
                                  "test/data/bricks-dither.indexes", UINT64_MAX,
                                  WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL);
}

const char* test_wuffs_gif_decode_pixfmt_rgb() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode("test/data/bricks-dither.gif",
                                  "test/data/bricks-dither.palette",
                                  "test/data/bricks-dither.indexes", UINT64_MAX,
                                  WUFFS_BASE__PIXEL_FORMAT__RGB);
}

const char* test_wuffs_gif_decode_pixfmt_rgba_nonpremul() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode("test/data/bricks-dither.gif",
                                  "test/data/bricks-dither.palette",
                                  "test/data/bricks-dither.indexes", UINT64_MAX,
                                  WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL);
}

const char* test_wuffs_gif_decode_input_is_a_gif_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
}

const char* test_wuffs_gif_decode_input_is_a_gif_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 4096,
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
}

const char* test_wuffs_gif_decode_input_is_a_gif_many_medium_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 787,
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
  // The magic 787 tickles being in the middle of a decode_extension skip
  // call.
  //
  // TODO: has 787 changed since we decode the image_config separately?
}

const char* test_wuffs_gif_decode_input_is_a_gif_many_small_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 13,
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
}

const char* test_wuffs_gif_decode_input_is_a_png() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/bricks-dither.png");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status != wuffs_gif__error__bad_header) {
    RETURN_FAIL("decode_image_config: got \"%s\", want \"%s\"", status,
                wuffs_gif__error__bad_header);
  }
  return NULL;
}

const char* test_wuffs_gif_decode_interlaced_truncated() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/hippopotamus.interlaced.truncated.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }
  if (wuffs_base__pixel_config__width(&ic.pixcfg) != 36) {
    RETURN_FAIL("width: got %" PRIu32 ", want 36",
                wuffs_base__pixel_config__width(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__height(&ic.pixcfg) != 28) {
    RETURN_FAIL("height: got %" PRIu32 ", want 28",
                wuffs_base__pixel_config__height(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__pixel_format(&ic.pixcfg) !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    RETURN_FAIL("pixel_format: got 0x%08" PRIX32 ", want 0x%08" PRIX32,
                wuffs_base__pixel_config__pixel_format(&ic.pixcfg),
                WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
  }
  const int num_pixel_indexes = 36 * 28 * 1;

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }
  uint8_t* pixel_ptr = wuffs_base__pixel_buffer__plane(&pb, 0).ptr;
  memset(pixel_ptr, 0xEE, num_pixel_indexes);

  if (pixel_ptr[num_pixel_indexes - 1] != 0xEE) {
    RETURN_FAIL("final pixel index, before: got 0x%02X, want 0x%02X",
                pixel_ptr[num_pixel_indexes - 1], 0xEE);
  }

  status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src, global_work_slice,
                                            NULL);
  if (status != wuffs_base__suspension__short_read) {
    RETURN_FAIL("decode_frame: got \"%s\", want \"%s\"", status,
                wuffs_base__suspension__short_read);
  }

  // Even though the source GIF data was truncated, replicating the interlaced
  // rows should have set the bottom right pixel.
  if (pixel_ptr[num_pixel_indexes - 1] == 0xEE) {
    RETURN_FAIL("final pixel index, after: got 0x%02X, want not 0x%02X",
                pixel_ptr[num_pixel_indexes - 1], 0xEE);
  }

  wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
  if (r.max_excl_y != 28) {
    RETURN_FAIL("frame_dirty_rect max_excl_y: got %" PRIu32 ", want 28",
                r.max_excl_y);
  }
  return NULL;
}

const char* do_test_wuffs_gif_decode_metadata(bool full) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, full ? "test/data/artificial/gif-metadata-full.gif"
                           : "test/data/artificial/gif-metadata-empty.gif");
  if (status) {
    return status;
  }

  int iccp;
  for (iccp = 0; iccp < 2; iccp++) {
    int xmp;
    for (xmp = 0; xmp < 2; xmp++) {
      bool seen_iccp = false;
      bool seen_xmp = false;

      wuffs_gif__decoder dec;
      status = wuffs_gif__decoder__initialize(
          &dec, sizeof dec, WUFFS_VERSION,
          WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
      if (status) {
        RETURN_FAIL("initialize: \"%s\"", status);
      }

      if (iccp) {
        wuffs_gif__decoder__set_report_metadata(&dec, WUFFS_BASE__FOURCC__ICCP,
                                                true);
      }
      if (xmp) {
        wuffs_gif__decoder__set_report_metadata(&dec, WUFFS_BASE__FOURCC__XMP,
                                                true);
      }

      wuffs_base__image_config ic = ((wuffs_base__image_config){});
      src.meta.ri = 0;

      while (true) {
        status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
        if (!status) {
          break;
        } else if (status != wuffs_base__warning__metadata_reported) {
          RETURN_FAIL(
              "decode_image_config (iccp=%d, xmp=%d): got \"%s\", want \"%s\"",
              iccp, xmp, status, wuffs_base__warning__metadata_reported);
        }

        const char* want = "";
        char got_buffer[100];
        int got_length = 0;
        uint32_t got_fourcc = wuffs_gif__decoder__metadata_fourcc(&dec);

        switch (wuffs_gif__decoder__metadata_fourcc(&dec)) {
          case WUFFS_BASE__FOURCC__ICCP:
            want = full ? "\x16\x26\x36\x46\x56\x76\x86\x96" : "";
            seen_iccp = true;
            break;
          case WUFFS_BASE__FOURCC__XMP:
            want = full ? "\x05\x17\x27\x37\x47\x57\x03\x77\x87\x97" : "";
            seen_xmp = true;
            break;
          default:
            RETURN_FAIL(
                "metadata_fourcc (iccp=%d, xmp=%d): unexpected FourCC "
                "0x%08" PRIX32,
                iccp, xmp, got_fourcc);
        }

        while (true) {
          uint64_t n = wuffs_gif__decoder__metadata_chunk_length(&dec);
          if ((n > 100) || (n + got_length > 100)) {
            RETURN_FAIL(
                "metadata_chunk_length (iccp=%d, xmp=%d): too much "
                "metadata (vs buffer size)",
                iccp, xmp);
          }
          if (n > wuffs_base__io_buffer__reader_available(&src)) {
            RETURN_FAIL(
                "metadata_chunk_length (iccp=%d, xmp=%d): too much "
                "metadata (vs available)",
                iccp, xmp);
          }
          memcpy(got_buffer + got_length, src.data.ptr + src.meta.ri, n);
          got_length += n;
          src.meta.ri += n;

          status = wuffs_gif__decoder__ack_metadata_chunk(&dec, &src);
          if (!status) {
            break;
          } else if (status != wuffs_base__warning__metadata_reported) {
            RETURN_FAIL(
                "ack_metadata_chunk (iccp=%d, xmp=%d): got \"%s\", want \"%s\"",
                iccp, xmp, status, wuffs_base__warning__metadata_reported);
          }
        }

        int want_length = strlen(want);
        if ((got_length != want_length) ||
            strncmp(got_buffer, want, want_length)) {
          RETURN_FAIL("metadata (iccp=%d, xmp=%d): fourcc=0x%08" PRIX32
                      ": values differed",
                      iccp, xmp, got_fourcc);
        }
      }

      if (iccp != seen_iccp) {
        RETURN_FAIL("seen_iccp (iccp=%d, xmp=%d): got %d, want %d", iccp, xmp,
                    seen_iccp, iccp);
      }

      if (xmp != seen_xmp) {
        RETURN_FAIL("seen_xmp (iccp=%d, xmp=%d): got %d, want %d", iccp, xmp,
                    seen_xmp, xmp);
      }

      {
        uint64_t got = wuffs_base__image_config__first_frame_io_position(&ic);
        uint64_t want = 25;
        if (got != want) {
          RETURN_FAIL("first_frame_io_position: got %" PRIu64 ", want %" PRIu64,
                      got, want);
        }
      }

      {
        uint32_t got = wuffs_gif__decoder__num_animation_loops(&dec);
        uint32_t want = 2001;
        if (got != want) {
          RETURN_FAIL("num_animation_loops: got %" PRIu32 ", want %" PRIu32,
                      got, want);
        }
      }

      {
        wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
        status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
        if (status) {
          RETURN_FAIL("decode_frame_config (iccp=%d, xmp=%d): %s", iccp, xmp,
                      status);
        }
        uint32_t got = wuffs_base__frame_config__width(&fc);
        uint32_t want = 1;
        if (got != want) {
          RETURN_FAIL(
              "decode_frame_config (iccp=%d, xmp=%d): width: got %" PRIu32
              ", want %" PRIu32,
              iccp, xmp, got, want);
        }
      }
    }
  }

  return NULL;
}

const char* test_wuffs_gif_decode_metadata_empty() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_metadata(false);
}

const char* test_wuffs_gif_decode_metadata_full() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_metadata(true);
}

const char* test_wuffs_gif_decode_missing_two_src_bytes() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/pjw-thumbnail.gif");
  if (status) {
    return status;
  }

  // Trim the final two bytes: the 0x00 end-of-block and the 0x3B trailer.
  if (src.meta.wi < 2) {
    return "src file is too short";
  }
  src.meta.wi -= 2;

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__suspension__short_read, false);
}

const char* test_wuffs_gif_decode_multiple_graphic_controls() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-multiple-graphic-controls.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
  if (status) {
    RETURN_FAIL("decode_frame_config: \"%s\"", status);
  }

  int got = wuffs_base__frame_config__duration(&fc) /
            WUFFS_BASE__FLICKS_PER_MILLISECOND;
  int want = 300;
  if (got != want) {
    RETURN_FAIL("duration: got %d, want %d", got, want);
  }
  return NULL;
}

const char* test_wuffs_gif_decode_multiple_loop_counts() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status =
      read_file(&src, "test/data/artificial/gif-multiple-loop-counts.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }

  // See test/data/artificial/gif-multiple-loop-counts.gif.make-artificial.txt
  // for the want_num_animation_loops source. Note that the GIF wire format's
  // loop counts are the number of animation loops *after* the first play
  // through, and Wuffs' are the number *including* the first play through.

  uint32_t want_num_animation_loops[4] = {1, 1, 51, 31};

  uint32_t i;
  for (i = 0; true; i++) {
    {
      status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
      if (i == WUFFS_TESTLIB_ARRAY_SIZE(want_num_animation_loops)) {
        if (status != wuffs_base__warning__end_of_data) {
          RETURN_FAIL("decode_frame_config #%" PRIu32 ": got \"%s\"", i,
                      status);
        }
        break;
      }
      if (status) {
        RETURN_FAIL("decode_frame_config #%" PRIu32 ": got \"%s\"", i, status);
      }
    }

    uint32_t got = wuffs_gif__decoder__num_animation_loops(&dec);
    uint32_t want = want_num_animation_loops[i];
    if (got != want) {
      RETURN_FAIL("num_animation_loops #%" PRIu32 ": got %" PRIu32
                  ", want %" PRIu32,
                  i, got, want);
    }
  }

  uint32_t got = wuffs_gif__decoder__num_animation_loops(&dec);
  uint32_t want = 41;
  if (got != want) {
    RETURN_FAIL("num_animation_loops #%" PRIu32 ": got %" PRIu32
                ", want %" PRIu32,
                i, got, want);
  }
  return NULL;
}

const char* test_wuffs_gif_decode_pixel_data_none() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/artificial/gif-pixel-data-none.gif");
  if (status) {
    return status;
  }

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__not_enough_data, true);
}

const char* test_wuffs_gif_decode_pixel_data_not_enough() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/artificial/gif-pixel-data-not-enough.gif");
  if (status) {
    return status;
  }

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__not_enough_data, false);
}

const char* test_wuffs_gif_decode_pixel_data_too_much_sans_quirk() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/artificial/gif-pixel-data-too-much.gif");
  if (status) {
    return status;
  }

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__too_much_data, false);
}

const char* test_wuffs_gif_decode_pixel_data_too_much_with_quirk() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/artificial/gif-pixel-data-too-much.gif");
  if (status) {
    return status;
  }

  return do_test_wuffs_gif_decode_expecting(
      src, WUFFS_GIF__QUIRK_IGNORE_TOO_MUCH_PIXEL_DATA, NULL, false);
}

const char* test_wuffs_gif_frame_dirty_rect() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/hippopotamus.interlaced.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }

  // The hippopotamus.interlaced.gif image is 28 pixels high. As we decode rows
  // of pixels, interlacing means that we decode rows 0, 8, 16, 24, 4, 12, 20,
  // 2, 6, 10, ..., 22, 26, 1, 3, 5, ..., 25, 27. As we progress, the dirty
  // rect's max_excl_y should be one more than the highest decoded row so far,
  // until the row is complete, when it is replicated (to a multiple of 8). If
  // we haven't decoded any rows yet, max_excl_y should be zero.
  uint32_t wants[9] = {0, 1, 8, 9, 16, 17, 24, 25, 28};
  int i = 0;

  while (true) {
    wuffs_base__io_buffer limited_src = make_limited_reader(src, 1);

    status = wuffs_gif__decoder__decode_frame(&dec, &pb, &limited_src,
                                              global_work_slice, NULL);
    src.meta.ri += limited_src.meta.ri;

    wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
    if ((i < WUFFS_TESTLIB_ARRAY_SIZE(wants)) && (wants[i] == r.max_excl_y)) {
      i++;
    }

    if (!status) {
      break;
    } else if (status != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame: \"%s\"", status);
    }
  }

  if (i != WUFFS_TESTLIB_ARRAY_SIZE(wants)) {
    RETURN_FAIL("i: got %d, want %d", i,
                (int)(WUFFS_TESTLIB_ARRAY_SIZE(wants)));
  }
  return NULL;
}

const char* do_test_wuffs_gif_num_decoded(bool frame_config) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status = read_file(&src, "test/data/animated-red-blue.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  if (!frame_config) {
    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (status) {
      RETURN_FAIL("decode_image_config: \"%s\"", status);
    }

    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      global_pixel_slice);
    if (status) {
      RETURN_FAIL("set_from_slice: \"%s\"", status);
    }
  }

  const char* method = frame_config ? "decode_frame_config" : "decode_frame";
  bool end_of_data = false;
  uint64_t want = 0;
  while (true) {
    uint64_t got =
        frame_config
            ? (uint64_t)(wuffs_gif__decoder__num_decoded_frame_configs(&dec))
            : (uint64_t)(wuffs_gif__decoder__num_decoded_frames(&dec));
    if (got != want) {
      RETURN_FAIL("num_%ss: got %" PRIu64 ", want %" PRIu64, method, got, want);
    }

    if (end_of_data) {
      break;
    }

    const char* status = NULL;
    if (frame_config) {
      status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
    } else {
      status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                global_work_slice, NULL);
    }

    if (!status) {
      want++;
    } else if (status == wuffs_base__warning__end_of_data) {
      end_of_data = true;
    } else {
      RETURN_FAIL("%s: \"%s\"", method, status);
    }
  }

  if (want != 4) {
    RETURN_FAIL("%s: got %" PRIu64 ", want 4", method, want);
  }
  return NULL;
}

const char* test_wuffs_gif_num_decoded_frame_configs() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_num_decoded(true);
}

const char* test_wuffs_gif_num_decoded_frames() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_num_decoded(false);
}

const char* do_test_wuffs_gif_io_position(bool chunked) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status = read_file(&src, "test/data/animated-red-blue.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  if (chunked) {
    if (src.meta.wi < 50) {
      RETURN_FAIL("src is too short");
    }
    size_t saved_wi = src.meta.wi;
    bool saved_closed = src.meta.closed;
    src.meta.wi = 30;
    src.meta.closed = false;

    status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
    if (status != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_image_config (chunked): \"%s\"", status);
    }

    src.meta.wi = saved_wi;
    src.meta.closed = saved_closed;

    if (src.meta.pos != 0) {
      RETURN_FAIL("src.meta.pos: got %" PRIu64 ", want zero", src.meta.pos);
    }
    wuffs_base__io_buffer__compact(&src);
    if (src.meta.pos == 0) {
      RETURN_FAIL("src.meta.pos: got %" PRIu64 ", want non-zero", src.meta.pos);
    }
  }

  status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }

  wuffs_base__frame_config fcs[4];
  uint32_t width_wants[4] = {64, 37, 49, 49};
  uint64_t pos_wants[4] = {781, 2126, 2187, 2542};
  int i;
  for (i = 0; i < 4; i++) {
    fcs[i] = ((wuffs_base__frame_config){});
    status = wuffs_gif__decoder__decode_frame_config(&dec, &fcs[i], &src);
    if (status) {
      RETURN_FAIL("decode_frame_config #%d: \"%s\"", i, status);
    }

    uint64_t index_got = wuffs_base__frame_config__index(&fcs[i]);
    uint64_t index_want = i;
    if (index_got != index_want) {
      RETURN_FAIL("index #%d: got %" PRIu64 ", want %" PRIu64, i, index_got,
                  index_want);
    }

    uint32_t width_got = wuffs_base__frame_config__width(&fcs[i]);
    uint32_t width_want = width_wants[i];
    if (width_got != width_want) {
      RETURN_FAIL("width #%d: got %" PRIu32 ", want %" PRIu32, i, width_got,
                  width_want);
    }

    uint64_t pos_got = wuffs_base__frame_config__io_position(&fcs[i]);
    uint64_t pos_want = pos_wants[i];
    if (pos_got != pos_want) {
      RETURN_FAIL("io_position #%d: got %" PRIu64 ", want %" PRIu64, i, pos_got,
                  pos_want);
    }

    // Look for the 0x21 byte that's a GIF's Extension Introducer. Not every
    // GIF's frame_config's I/O position will point to 0x21, as an 0x2C Image
    // Separator is also valid. But for animated-red-blue.gif, it'll be 0x21.
    if (pos_got < src.meta.pos) {
      RETURN_FAIL("io_position #%d: got %" PRIu64 ", was too small", i,
                  pos_got);
    }
    uint64_t src_ptr_offset = pos_got - src.meta.pos;
    if (src_ptr_offset >= src.meta.wi) {
      RETURN_FAIL("io_position #%d: got %" PRIu64 ", was too large", i,
                  pos_got);
    }
    uint8_t x = src.data.ptr[src_ptr_offset];
    if (x != 0x21) {
      RETURN_FAIL("Image Descriptor byte #%d: got 0x%02X, want 0x2C", i,
                  (int)x);
    }
  }

  status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
  if (status != wuffs_base__warning__end_of_data) {
    RETURN_FAIL("decode_frame_config EOD: \"%s\"", status);
  }

  // If we're chunked, we've discarded some source bytes due to an earlier
  // wuffs_base__io_buffer__compact call. We won't bother testing
  // wuffs_gif__decoder__restart_frame in that case.
  if (chunked) {
    return NULL;
  }

  for (i = 0; i < 4; i++) {
    src.meta.ri = pos_wants[i];

    status = wuffs_gif__decoder__restart_frame(
        &dec, i, wuffs_base__frame_config__io_position(&fcs[i]));
    if (status) {
      RETURN_FAIL("restart_frame #%d: \"%s\"", i, status);
    }

    int j;
    for (j = i; j < 4; j++) {
      wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
      status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
      if (status) {
        RETURN_FAIL("decode_frame_config #%d, #%d: \"%s\"", i, j, status);
      }

      uint64_t index_got = wuffs_base__frame_config__index(&fc);
      uint64_t index_want = j;
      if (index_got != index_want) {
        RETURN_FAIL("index #%d, #%d: got %" PRIu64 ", want %" PRIu64, i, j,
                    index_got, index_want);
      }

      uint32_t width_got = wuffs_base__frame_config__width(&fc);
      uint32_t width_want = width_wants[j];
      if (width_got != width_want) {
        RETURN_FAIL("width #%d, #%d: got %" PRIu32 ", want %" PRIu32, i, j,
                    width_got, width_want);
      }
    }

    status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
    if (status != wuffs_base__warning__end_of_data) {
      RETURN_FAIL("decode_frame_config EOD #%d: \"%s\"", i, status);
    }
  }

  return NULL;
}

const char* test_wuffs_gif_io_position_one_chunk() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_io_position(false);
}

const char* test_wuffs_gif_io_position_two_chunks() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_io_position(true);
}

const char* test_wuffs_gif_small_frame_interlaced() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });

  const char* status =
      read_file(&src, "test/data/artificial/gif-small-frame-interlaced.gif");
  if (status) {
    return status;
  }

  wuffs_gif__decoder dec;
  status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status) {
    RETURN_FAIL("initialize: \"%s\"", status);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  status = wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status) {
    RETURN_FAIL("decode_image_config: \"%s\"", status);
  }
  if (wuffs_base__pixel_config__width(&ic.pixcfg) != 5) {
    RETURN_FAIL("width: got %" PRIu32 ", want 5",
                wuffs_base__pixel_config__width(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__height(&ic.pixcfg) != 5) {
    RETURN_FAIL("height: got %" PRIu32 ", want 5",
                wuffs_base__pixel_config__height(&ic.pixcfg));
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                    global_pixel_slice);
  if (status) {
    RETURN_FAIL("set_from_slice: \"%s\"", status);
  }

  wuffs_base__frame_config fc;
  status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
  if (status) {
    RETURN_FAIL("decode_frame_config: \"%s\"", status);
  }

  wuffs_base__rect_ie_u32 fr = wuffs_base__frame_config__bounds(&fc);
  if (fr.max_excl_y != 3) {
    RETURN_FAIL("frame rect max_excl_y: got %" PRIu32 ", want 3",
                fr.max_excl_y);
  }

  status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src, global_work_slice,
                                            NULL);
  if (status) {
    RETURN_FAIL("decode_frame: \"%s\"", status);
  }

  wuffs_base__rect_ie_u32 dr = wuffs_gif__decoder__frame_dirty_rect(&dec);
  if (dr.max_excl_y != 3) {
    RETURN_FAIL("dirty rect max_excl_y: got %" PRIu32 ", want 3",
                dr.max_excl_y);
  }

  return NULL;
}

  // ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char* do_test_mimic_gif_decode(const char* filename) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = global_src_slice,
  });
  const char* status = read_file(&src, filename);
  if (status) {
    return status;
  }

  src.meta.ri = 0;
  wuffs_base__io_buffer got = ((wuffs_base__io_buffer){
      .data = global_got_slice,
  });
  status = wuffs_gif_decode(
      &got, 0, WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, &src);
  if (status) {
    return status;
  }

  src.meta.ri = 0;
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = global_want_slice,
  });
  status = mimic_gif_decode(
      &want, 0, WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, &src);
  if (status) {
    return status;
  }

  status = check_io_buffers_equal("", &got, &want);
  if (status) {
    return status;
  }

  // TODO: check the palette RGB values, not just the palette indexes
  // (pixels).

  return NULL;
}

const char* test_mimic_gif_decode_animated_small() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/animated-red-blue.gif");
}

const char* test_mimic_gif_decode_bricks_dither() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-dither.gif");
}

const char* test_mimic_gif_decode_bricks_gray() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-gray.gif");
}

const char* test_mimic_gif_decode_bricks_nodither() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-nodither.gif");
}

const char* test_mimic_gif_decode_gifplayer_muybridge() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/gifplayer-muybridge.gif");
}

const char* test_mimic_gif_decode_harvesters() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/harvesters.gif");
}

const char* test_mimic_gif_decode_hat() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hat.gif");
}

const char* test_mimic_gif_decode_hibiscus_primitive() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hibiscus.primitive.gif");
}

const char* test_mimic_gif_decode_hibiscus_regular() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hibiscus.regular.gif");
}

const char* test_mimic_gif_decode_hippopotamus_interlaced() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hippopotamus.interlaced.gif");
}

const char* test_mimic_gif_decode_hippopotamus_regular() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hippopotamus.regular.gif");
}

const char* test_mimic_gif_decode_muybridge() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/muybridge.gif");
}

const char* test_mimic_gif_decode_pjw_thumbnail() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/pjw-thumbnail.gif");
}

#endif  // WUFFS_MIMIC

// ---------------- GIF Benches

const char* do_bench_gif_decode(
    const char* (*decode_func)(wuffs_base__io_buffer*,
                               uint32_t wuffs_initialize_flags,
                               wuffs_base__pixel_format,
                               wuffs_base__io_buffer*),
    uint32_t wuffs_initialize_flags,
    const char* filename,
    wuffs_base__pixel_format pixfmt,
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

  bench_start();
  uint64_t n_bytes = 0;
  uint64_t i;
  uint64_t iters = iters_unscaled * iterscale;
  for (i = 0; i < iters; i++) {
    got.meta.wi = 0;
    src.meta.ri = 0;
    status = decode_func(&got, wuffs_initialize_flags, pixfmt, &src);
    if (status) {
      return status;
    }
    n_bytes += got.meta.wi;
  }
  bench_finish(iters, n_bytes);
  return NULL;
}

const char* bench_wuffs_gif_decode_1k_bw() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/pjw-thumbnail.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 2000);
}

const char* bench_wuffs_gif_decode_1k_color_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      "test/data/hippopotamus.regular.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1000);
}

const char* bench_wuffs_gif_decode_1k_color_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/hippopotamus.regular.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1000);
}

const char* bench_wuffs_gif_decode_10k_bgra() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/hat.gif", WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL, 100);
}

const char* bench_wuffs_gif_decode_10k_indexed() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/hat.gif", WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 100);
}

const char* bench_wuffs_gif_decode_20k() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/bricks-gray.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 50);
}

const char* bench_wuffs_gif_decode_100k_artificial() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/hibiscus.primitive.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 15);
}

const char* bench_wuffs_gif_decode_100k_realistic() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/hibiscus.regular.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 10);
}

const char* bench_wuffs_gif_decode_1000k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(wuffs_gif_decode,
                             WUFFS_INITIALIZE__DEFAULT_OPTIONS,
                             "test/data/harvesters.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1);
}

const char* bench_wuffs_gif_decode_1000k_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/harvesters.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1);
}

const char* bench_wuffs_gif_decode_anim_screencap() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      "test/data/gifplayer-muybridge.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1);
}

  // ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char* bench_mimic_gif_decode_1k_bw() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(mimic_gif_decode, 0, "test/data/pjw-thumbnail.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY,
                             2000);
}

const char* bench_mimic_gif_decode_1k_color() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      mimic_gif_decode, 0, "test/data/hippopotamus.regular.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1000);
}

const char* bench_mimic_gif_decode_10k_indexed() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(mimic_gif_decode, 0, "test/data/hat.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY,
                             100);
}

const char* bench_mimic_gif_decode_20k() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(mimic_gif_decode, 0, "test/data/bricks-gray.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY,
                             50);
}

const char* bench_mimic_gif_decode_100k_artificial() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      mimic_gif_decode, 0, "test/data/hibiscus.primitive.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 15);
}

const char* bench_mimic_gif_decode_100k_realistic() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(
      mimic_gif_decode, 0, "test/data/hibiscus.regular.gif",
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 10);
}

const char* bench_mimic_gif_decode_1000k() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(mimic_gif_decode, 0, "test/data/harvesters.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1);
}

const char* bench_mimic_gif_decode_anim_screencap() {
  CHECK_FOCUS(__func__);
  return do_bench_gif_decode(mimic_gif_decode, 0,
                             "test/data/gifplayer-muybridge.gif",
                             WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY, 1);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

// The empty comments forces clang-format to place one element per line.
proc tests[] = {

    // These basic tests are really testing the Wuffs compiler. They aren't
    // specific to the std/gif code, but putting them here is as good as any
    // other place.
    test_basic_bad_receiver,            //
    test_basic_bad_sizeof_receiver,     //
    test_basic_bad_wuffs_version,       //
    test_basic_initialize_not_called,   //
    test_basic_status_is_error,         //
    test_basic_status_strings,          //
    test_basic_status_used_package,     //
    test_basic_sub_struct_initializer,  //

    test_wuffs_gif_call_interleaved,                         //
    test_wuffs_gif_call_sequence,                            //
    test_wuffs_gif_decode_animated_big,                      //
    test_wuffs_gif_decode_animated_medium,                   //
    test_wuffs_gif_decode_animated_small,                    //
    test_wuffs_gif_decode_background_color,                  //
    test_wuffs_gif_decode_delay_num_frames_decoded,          //
    test_wuffs_gif_decode_empty_palette,                     //
    test_wuffs_gif_decode_first_frame_is_opaque,             //
    test_wuffs_gif_decode_frame_out_of_bounds,               //
    test_wuffs_gif_decode_input_is_a_gif_just_one_read,      //
    test_wuffs_gif_decode_input_is_a_gif_many_big_reads,     //
    test_wuffs_gif_decode_input_is_a_gif_many_medium_reads,  //
    test_wuffs_gif_decode_input_is_a_gif_many_small_reads,   //
    test_wuffs_gif_decode_input_is_a_png,                    //
    test_wuffs_gif_decode_interlaced_truncated,              //
    test_wuffs_gif_decode_metadata_empty,                    //
    test_wuffs_gif_decode_metadata_full,                     //
    test_wuffs_gif_decode_missing_two_src_bytes,             //
    test_wuffs_gif_decode_multiple_graphic_controls,         //
    test_wuffs_gif_decode_multiple_loop_counts,              //
    test_wuffs_gif_decode_pixel_data_none,                   //
    test_wuffs_gif_decode_pixel_data_not_enough,             //
    test_wuffs_gif_decode_pixel_data_too_much_sans_quirk,    //
    test_wuffs_gif_decode_pixel_data_too_much_with_quirk,    //
    test_wuffs_gif_decode_pixfmt_bgr,                        //
    test_wuffs_gif_decode_pixfmt_bgra_nonpremul,             //
    test_wuffs_gif_decode_pixfmt_rgb,                        //
    test_wuffs_gif_decode_pixfmt_rgba_nonpremul,             //
    test_wuffs_gif_decode_zero_width_frame,                  //
    test_wuffs_gif_frame_dirty_rect,                         //
    test_wuffs_gif_num_decoded_frame_configs,                //
    test_wuffs_gif_num_decoded_frames,                       //
    test_wuffs_gif_io_position_one_chunk,                    //
    test_wuffs_gif_io_position_two_chunks,                   //
    test_wuffs_gif_small_frame_interlaced,                   //

#ifdef WUFFS_MIMIC

    test_mimic_gif_decode_animated_small,           //
    test_mimic_gif_decode_bricks_dither,            //
    test_mimic_gif_decode_bricks_gray,              //
    test_mimic_gif_decode_bricks_nodither,          //
    test_mimic_gif_decode_gifplayer_muybridge,      //
    test_mimic_gif_decode_harvesters,               //
    test_mimic_gif_decode_hat,                      //
    test_mimic_gif_decode_hibiscus_primitive,       //
    test_mimic_gif_decode_hibiscus_regular,         //
    test_mimic_gif_decode_hippopotamus_interlaced,  //
    test_mimic_gif_decode_hippopotamus_regular,     //
    test_mimic_gif_decode_muybridge,                //
    test_mimic_gif_decode_pjw_thumbnail,            //

#endif  // WUFFS_MIMIC

    NULL,
};

// The empty comments forces clang-format to place one element per line.
proc benches[] = {

    bench_wuffs_gif_decode_1k_bw,               //
    bench_wuffs_gif_decode_1k_color_full_init,  //
    bench_wuffs_gif_decode_1k_color_part_init,  //
    bench_wuffs_gif_decode_10k_bgra,            //
    bench_wuffs_gif_decode_10k_indexed,         //
    bench_wuffs_gif_decode_20k,                 //
    bench_wuffs_gif_decode_100k_artificial,     //
    bench_wuffs_gif_decode_100k_realistic,      //
    bench_wuffs_gif_decode_1000k_full_init,     //
    bench_wuffs_gif_decode_1000k_part_init,     //
    bench_wuffs_gif_decode_anim_screencap,      //

#ifdef WUFFS_MIMIC

    bench_mimic_gif_decode_1k_bw,            //
    bench_mimic_gif_decode_1k_color,         //
    bench_mimic_gif_decode_10k_indexed,      //
    bench_mimic_gif_decode_20k,              //
    bench_mimic_gif_decode_100k_artificial,  //
    bench_mimic_gif_decode_100k_realistic,   //
    bench_mimic_gif_decode_1000k,            //
    bench_mimic_gif_decode_anim_screencap,   //

#endif  // WUFFS_MIMIC

    NULL,
};

int main(int argc, char** argv) {
  proc_package_name = "std/gif";
  return test_main(argc, argv, tests, benches);
}
