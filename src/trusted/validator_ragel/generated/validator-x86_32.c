
#line 1 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

#if defined(_MSC_VER)
#define inline __inline
#endif

#define check_jump_dest \
    if ((jump_dest & bundle_mask) != bundle_mask) { \
      if (jump_dest >= size) { \
        printf("direct jump out of range: %"NACL_PRIxS"\n", jump_dest); \
        result = 1; \
        goto error_detected; \
      } else { \
        BitmapSetBit(jump_dests, jump_dest + 1); \
      } \
    }


#line 92 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"



#line 39 "src/trusted/validator_ragel/generated/validator-x86_32.c"
static const int x86_64_decoder_start = 235;
static const int x86_64_decoder_first_final = 235;
static const int x86_64_decoder_error = 0;

static const int x86_64_decoder_en_main = 235;


#line 95 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"

/* Ignore this information for now.  */
#define data16_prefix if (0) result
#define lock_prefix if (0) result
#define repz_prefix if (0) result
#define repnz_prefix if (0) result
#define branch_not_taken if (0) result
#define branch_taken if (0) result
#define vex_prefix3 if (0) result
#define disp if (0) p
#define disp_type if (0) result

enum disp_mode {
  DISPNONE,
  DISP8,
  DISP16,
  DISP32
};

static const int kBitsPerByte = 8;

static inline uint8_t *BitmapAllocate(size_t indexes) {
  size_t byte_count = (indexes + kBitsPerByte - 1) / kBitsPerByte;
  uint8_t *bitmap = malloc(byte_count);
  if (bitmap != NULL) {
    memset(bitmap, 0, byte_count);
  }
  return bitmap;
}

static inline int BitmapIsBitSet(uint8_t *bitmap, size_t index) {
  return (bitmap[index / kBitsPerByte] & (1 << (index % kBitsPerByte))) != 0;
}

static inline void BitmapSetBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] |= 1 << (index % kBitsPerByte);
}

static inline void BitmapClearBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] &= ~(1 << (index % kBitsPerByte));
}

static int CheckJumpTargets(uint8_t *valid_targets, uint8_t *jump_dests,
                            size_t size) {
  size_t i;
  for (i = 0; i < size / 32; i++) {
    uint32_t jump_dest_mask = ((uint32_t *) jump_dests)[i];
    uint32_t valid_target_mask = ((uint32_t *) valid_targets)[i];
    if ((jump_dest_mask & ~valid_target_mask) != 0) {
      printf("bad jump to around %x\n", (unsigned)(i * 32));
      return 1;
    }
  }
  return 0;
}

int ValidateChunkIA32(const uint8_t *data, size_t size,
                      process_error_func process_error, void *userdata) {
  const size_t bundle_size = 32;
  const size_t bundle_mask = bundle_size - 1;

  uint8_t *valid_targets = BitmapAllocate(size);
  uint8_t *jump_dests = BitmapAllocate(size);

  const uint8_t *p = data;
  const uint8_t *begin = p;  /* Start of the instruction being processed.  */

  int result = 0;

  assert(size % bundle_size == 0);

  while (p < data + size) {
    const uint8_t *pe = p + bundle_size;
    const uint8_t *eof = pe;
    int cs;

    
#line 125 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	{
	cs = x86_64_decoder_start;
	}

#line 172 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
    
#line 132 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr0:
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr9:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr10:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr11:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr15:
#line 55 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr58:
#line 43 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    int32_t offset =
           (uint32_t) (p[-3] + 256U * (p[-2] + 256U * (p[-1] + 256U * (p[0]))));
    size_t jump_dest = offset + (p - data);
    check_jump_dest;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr61:
#line 35 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    int8_t offset = (uint8_t) (p[0]);
    size_t jump_dest = offset + (p - data);
    check_jump_dest;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr78:
#line 54 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr104:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr224:
#line 52 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr263:
#line 39 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = FALSE;
  }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr267:
#line 73 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ BitmapClearBit(valid_targets, (p - data) - 1);
    }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
tr281:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st235;
st235:
	if ( ++p == pe )
		goto _test_eof235;
case 235:
#line 306 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr313;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr81:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st1;
tr130:
#line 36 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = FALSE;
  }
	goto st1;
tr132:
#line 39 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = FALSE;
  }
	goto st1;
tr275:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st1;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
#line 477 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st3;
	} else if ( (*p) >= 64u )
		goto st7;
	goto tr0;
tr119:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
#line 528 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 5u: goto st3;
		case 13u: goto st3;
		case 21u: goto st3;
		case 29u: goto st3;
		case 37u: goto st3;
		case 45u: goto st3;
		case 53u: goto st3;
		case 61u: goto st3;
		case 69u: goto st3;
		case 77u: goto st3;
		case 85u: goto st3;
		case 93u: goto st3;
		case 101u: goto st3;
		case 109u: goto st3;
		case 117u: goto st3;
		case 125u: goto st3;
		case 133u: goto st3;
		case 141u: goto st3;
		case 149u: goto st3;
		case 157u: goto st3;
		case 165u: goto st3;
		case 173u: goto st3;
		case 181u: goto st3;
		case 189u: goto st3;
		case 197u: goto st3;
		case 205u: goto st3;
		case 213u: goto st3;
		case 221u: goto st3;
		case 229u: goto st3;
		case 237u: goto st3;
		case 245u: goto st3;
		case 253u: goto st3;
	}
	goto tr0;
tr120:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st3;
tr290:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 581 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st4;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	goto st5;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	goto st6;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	goto tr9;
tr121:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st7;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
#line 608 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto tr10;
tr122:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st8;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
#line 620 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st7;
tr123:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 632 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st3;
tr53:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st10;
tr54:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st10;
tr91:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st10;
tr248:
#line 58 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
	goto st10;
tr260:
#line 57 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
	goto st10;
tr276:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st10;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
#line 673 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto tr11;
tr142:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st11;
tr143:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st11;
tr277:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st11;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
#line 700 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st12;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	goto st13;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	goto st14;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	goto tr15;
tr19:
#line 86 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        process_error(begin, userdata);
        result = 1;
        goto error_detected;
    }
	goto st0;
#line 725 "src/trusted/validator_ragel/generated/validator-x86_32.c"
st0:
cs = 0;
	goto _out;
tr278:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st15;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
#line 740 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 0u: goto st16;
		case 1u: goto st17;
		case 11u: goto tr0;
		case 13u: goto st18;
		case 14u: goto tr0;
		case 15u: goto st19;
		case 19u: goto st29;
		case 23u: goto st29;
		case 24u: goto st30;
		case 31u: goto st31;
		case 43u: goto st29;
		case 49u: goto tr0;
		case 80u: goto st32;
		case 112u: goto st33;
		case 115u: goto st43;
		case 119u: goto tr0;
		case 162u: goto tr0;
		case 164u: goto st33;
		case 172u: goto st33;
		case 174u: goto st48;
		case 179u: goto tr19;
		case 195u: goto st29;
		case 197u: goto st49;
		case 199u: goto st50;
		case 208u: goto tr19;
		case 214u: goto tr19;
		case 215u: goto st32;
		case 240u: goto tr19;
		case 247u: goto st32;
		case 255u: goto tr19;
	}
	if ( (*p) < 128u ) {
		if ( (*p) < 48u ) {
			if ( (*p) > 12u ) {
				if ( 25u <= (*p) && (*p) <= 39u )
					goto tr19;
			} else if ( (*p) >= 4u )
				goto tr19;
		} else if ( (*p) > 63u ) {
			if ( (*p) < 113u ) {
				if ( 108u <= (*p) && (*p) <= 109u )
					goto tr19;
			} else if ( (*p) > 114u ) {
				if ( 120u <= (*p) && (*p) <= 125u )
					goto tr19;
			} else
				goto st42;
		} else
			goto tr19;
	} else if ( (*p) > 143u ) {
		if ( (*p) < 184u ) {
			if ( (*p) < 166u ) {
				if ( 160u <= (*p) && (*p) <= 163u )
					goto tr19;
			} else if ( (*p) > 171u ) {
				if ( 178u <= (*p) && (*p) <= 181u )
					goto st29;
			} else
				goto tr19;
		} else if ( (*p) > 187u ) {
			if ( (*p) < 200u ) {
				if ( 194u <= (*p) && (*p) <= 198u )
					goto st33;
			} else if ( (*p) > 207u ) {
				if ( 230u <= (*p) && (*p) <= 231u )
					goto tr19;
			} else
				goto tr0;
		} else
			goto tr19;
	} else
		goto st44;
	goto st1;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	switch( (*p) ) {
		case 12u: goto st2;
		case 13u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 76u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 140u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
	}
	if ( (*p) < 88u ) {
		if ( (*p) < 24u ) {
			if ( 8u <= (*p) && (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 47u ) {
			if ( 72u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 111u ) {
		if ( (*p) < 152u ) {
			if ( 136u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 175u ) {
			if ( (*p) > 207u ) {
				if ( 216u <= (*p) && (*p) <= 239u )
					goto tr0;
			} else if ( (*p) >= 200u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	switch( (*p) ) {
		case 36u: goto st2;
		case 37u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 100u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 164u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 160u ) {
		if ( (*p) < 48u ) {
			if ( 32u <= (*p) && (*p) <= 39u )
				goto tr0;
		} else if ( (*p) > 63u ) {
			if ( (*p) > 103u ) {
				if ( 112u <= (*p) && (*p) <= 127u )
					goto st7;
			} else if ( (*p) >= 96u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 167u ) {
		if ( (*p) < 208u ) {
			if ( (*p) > 191u ) {
				if ( 200u <= (*p) && (*p) <= 201u )
					goto tr0;
			} else if ( (*p) >= 176u )
				goto st3;
		} else if ( (*p) > 209u ) {
			if ( (*p) > 231u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 224u )
				goto tr0;
		} else
			goto tr0;
	} else
		goto st3;
	goto tr19;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 15u )
			goto tr0;
	} else if ( (*p) > 79u ) {
		if ( 128u <= (*p) && (*p) <= 143u )
			goto st3;
	} else
		goto st7;
	goto tr19;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	switch( (*p) ) {
		case 4u: goto st21;
		case 5u: goto st22;
		case 12u: goto st21;
		case 13u: goto st22;
		case 20u: goto st21;
		case 21u: goto st22;
		case 28u: goto st21;
		case 29u: goto st22;
		case 36u: goto st21;
		case 37u: goto st22;
		case 44u: goto st21;
		case 45u: goto st22;
		case 52u: goto st21;
		case 53u: goto st22;
		case 60u: goto st21;
		case 61u: goto st22;
		case 68u: goto st27;
		case 76u: goto st27;
		case 84u: goto st27;
		case 92u: goto st27;
		case 100u: goto st27;
		case 108u: goto st27;
		case 116u: goto st27;
		case 124u: goto st27;
		case 132u: goto st28;
		case 140u: goto st28;
		case 148u: goto st28;
		case 156u: goto st28;
		case 164u: goto st28;
		case 172u: goto st28;
		case 180u: goto st28;
		case 188u: goto st28;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st22;
	} else if ( (*p) >= 64u )
		goto st26;
	goto st20;
tr42:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st20;
tr43:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st20;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
#line 994 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 138u: goto tr0;
		case 142u: goto tr0;
		case 144u: goto tr0;
		case 148u: goto tr0;
		case 154u: goto tr0;
		case 158u: goto tr0;
		case 160u: goto tr0;
		case 164u: goto tr0;
		case 170u: goto tr0;
		case 174u: goto tr0;
		case 176u: goto tr0;
		case 180u: goto tr0;
		case 187u: goto tr0;
		case 191u: goto tr0;
	}
	if ( (*p) < 150u ) {
		if ( (*p) > 13u ) {
			if ( 28u <= (*p) && (*p) <= 29u )
				goto tr0;
		} else if ( (*p) >= 12u )
			goto tr0;
	} else if ( (*p) > 151u ) {
		if ( (*p) > 167u ) {
			if ( 182u <= (*p) && (*p) <= 183u )
				goto tr0;
		} else if ( (*p) >= 166u )
			goto tr0;
	} else
		goto tr0;
	goto tr19;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	switch( (*p) ) {
		case 5u: goto st22;
		case 13u: goto st22;
		case 21u: goto st22;
		case 29u: goto st22;
		case 37u: goto st22;
		case 45u: goto st22;
		case 53u: goto st22;
		case 61u: goto st22;
		case 69u: goto st22;
		case 77u: goto st22;
		case 85u: goto st22;
		case 93u: goto st22;
		case 101u: goto st22;
		case 109u: goto st22;
		case 117u: goto st22;
		case 125u: goto st22;
		case 133u: goto st22;
		case 141u: goto st22;
		case 149u: goto st22;
		case 157u: goto st22;
		case 165u: goto st22;
		case 173u: goto st22;
		case 181u: goto st22;
		case 189u: goto st22;
		case 197u: goto st22;
		case 205u: goto st22;
		case 213u: goto st22;
		case 221u: goto st22;
		case 229u: goto st22;
		case 237u: goto st22;
		case 245u: goto st22;
		case 253u: goto st22;
	}
	goto st20;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	goto st23;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	goto st24;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	goto st25;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	goto tr42;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	goto tr43;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	goto st26;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
	goto st22;
tr82:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st29;
tr254:
#line 36 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = FALSE;
  }
	goto st29;
tr264:
#line 39 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = FALSE;
  }
	goto st29;
tr288:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st29;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
#line 1129 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 63u )
			goto tr0;
	} else if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st3;
	} else
		goto st7;
	goto tr19;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 31u )
			goto tr0;
	} else if ( (*p) > 95u ) {
		if ( 128u <= (*p) && (*p) <= 159u )
			goto st3;
	} else
		goto st7;
	goto tr19;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto tr0;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto tr0;
		} else if ( (*p) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr85:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st32;
tr258:
#line 36 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = FALSE;
  }
	goto st32;
tr266:
#line 39 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = FALSE;
  }
	goto st32;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
#line 1248 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 192u <= (*p) )
		goto tr0;
	goto tr19;
tr86:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st33;
tr256:
#line 36 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = FALSE;
  }
	goto st33;
tr265:
#line 39 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = FALSE;
  }
	goto st33;
tr284:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st33;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
#line 1281 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 12u: goto st34;
		case 13u: goto st35;
		case 20u: goto st34;
		case 21u: goto st35;
		case 28u: goto st34;
		case 29u: goto st35;
		case 36u: goto st34;
		case 37u: goto st35;
		case 44u: goto st34;
		case 45u: goto st35;
		case 52u: goto st34;
		case 53u: goto st35;
		case 60u: goto st34;
		case 61u: goto st35;
		case 68u: goto st40;
		case 76u: goto st40;
		case 84u: goto st40;
		case 92u: goto st40;
		case 100u: goto st40;
		case 108u: goto st40;
		case 116u: goto st40;
		case 124u: goto st40;
		case 132u: goto st41;
		case 140u: goto st41;
		case 148u: goto st41;
		case 156u: goto st41;
		case 164u: goto st41;
		case 172u: goto st41;
		case 180u: goto st41;
		case 188u: goto st41;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st35;
	} else if ( (*p) >= 64u )
		goto st39;
	goto st10;
tr92:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st34;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
#line 1332 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 5u: goto st35;
		case 13u: goto st35;
		case 21u: goto st35;
		case 29u: goto st35;
		case 37u: goto st35;
		case 45u: goto st35;
		case 53u: goto st35;
		case 61u: goto st35;
		case 69u: goto st35;
		case 77u: goto st35;
		case 85u: goto st35;
		case 93u: goto st35;
		case 101u: goto st35;
		case 109u: goto st35;
		case 117u: goto st35;
		case 125u: goto st35;
		case 133u: goto st35;
		case 141u: goto st35;
		case 149u: goto st35;
		case 157u: goto st35;
		case 165u: goto st35;
		case 173u: goto st35;
		case 181u: goto st35;
		case 189u: goto st35;
		case 197u: goto st35;
		case 205u: goto st35;
		case 213u: goto st35;
		case 221u: goto st35;
		case 229u: goto st35;
		case 237u: goto st35;
		case 245u: goto st35;
		case 253u: goto st35;
	}
	goto st10;
tr93:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st35;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
#line 1378 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st36;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	goto st37;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	goto st38;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	goto tr53;
tr94:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st39;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
#line 1405 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto tr54;
tr95:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st40;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
#line 1417 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st39;
tr96:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st41;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
#line 1429 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st35;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
	if ( (*p) < 224u ) {
		if ( 208u <= (*p) && (*p) <= 215u )
			goto st10;
	} else if ( (*p) > 231u ) {
		if ( 240u <= (*p) && (*p) <= 247u )
			goto st10;
	} else
		goto st10;
	goto tr19;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
	if ( (*p) > 215u ) {
		if ( 240u <= (*p) && (*p) <= 247u )
			goto st10;
	} else if ( (*p) >= 208u )
		goto st10;
	goto tr19;
tr306:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st44;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
#line 1465 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st45;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	goto st46;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	goto st47;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	goto tr58;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
	switch( (*p) ) {
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
		case 232u: goto tr0;
		case 240u: goto tr0;
		case 248u: goto tr0;
	}
	if ( (*p) < 80u ) {
		if ( 16u <= (*p) && (*p) <= 63u )
			goto tr0;
	} else if ( (*p) > 127u ) {
		if ( 144u <= (*p) && (*p) <= 191u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr90:
#line 25 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = FALSE;
  }
	goto st49;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
#line 1534 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 192u <= (*p) )
		goto st10;
	goto tr19;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
	switch( (*p) ) {
		case 12u: goto st2;
		case 13u: goto st3;
		case 76u: goto st8;
		case 140u: goto st9;
	}
	if ( (*p) < 72u ) {
		if ( 8u <= (*p) && (*p) <= 15u )
			goto tr0;
	} else if ( (*p) > 79u ) {
		if ( 136u <= (*p) && (*p) <= 143u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr279:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 4 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    branch_not_taken = TRUE;
  }
	goto st51;
tr280:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 7 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    branch_taken = TRUE;
  }
	goto st51;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
#line 1583 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) == 15u )
		goto st52;
	if ( 112u <= (*p) && (*p) <= 127u )
		goto st53;
	goto tr19;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
	if ( 128u <= (*p) && (*p) <= 143u )
		goto st44;
	goto tr19;
tr285:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st53;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
#line 1607 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto tr61;
tr282:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 10 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = TRUE;
  }
	goto st54;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
#line 1624 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st55;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st55;
		case 15u: goto st57;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st55;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st55;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st55;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st55;
		case 46u: goto st65;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st55;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st55;
		case 102u: goto st73;
		case 104u: goto st55;
		case 105u: goto st78;
		case 107u: goto st33;
		case 129u: goto st78;
		case 131u: goto st33;
		case 133u: goto st1;
		case 135u: goto st1;
		case 137u: goto st1;
		case 139u: goto st1;
		case 140u: goto st87;
		case 141u: goto st29;
		case 143u: goto st88;
		case 161u: goto st3;
		case 163u: goto st3;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 169u: goto st55;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st89;
		case 199u: goto st90;
		case 209u: goto st91;
		case 211u: goto st91;
		case 240u: goto tr72;
		case 242u: goto tr73;
		case 243u: goto tr74;
		case 247u: goto st102;
		case 255u: goto st103;
	}
	if ( (*p) < 144u ) {
		if ( 64u <= (*p) && (*p) <= 79u )
			goto tr0;
	} else if ( (*p) > 153u ) {
		if ( (*p) > 157u ) {
			if ( 184u <= (*p) && (*p) <= 191u )
				goto st55;
		} else if ( (*p) >= 156u )
			goto tr0;
	} else
		goto tr0;
	goto tr19;
tr117:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st55;
tr118:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st55;
tr297:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st55;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
#line 1720 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st56;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	goto tr78;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
	switch( (*p) ) {
		case 0u: goto st58;
		case 1u: goto st59;
		case 31u: goto st31;
		case 43u: goto tr82;
		case 56u: goto st60;
		case 58u: goto st61;
		case 80u: goto tr85;
		case 81u: goto tr81;
		case 112u: goto tr86;
		case 115u: goto st64;
		case 121u: goto tr85;
		case 164u: goto st33;
		case 165u: goto st1;
		case 172u: goto st33;
		case 173u: goto st1;
		case 175u: goto st1;
		case 177u: goto st1;
		case 178u: goto st29;
		case 182u: goto st1;
		case 193u: goto st1;
		case 194u: goto tr86;
		case 196u: goto st62;
		case 197u: goto tr90;
		case 198u: goto tr86;
		case 215u: goto tr85;
		case 231u: goto tr82;
		case 247u: goto tr85;
	}
	if ( (*p) < 84u ) {
		if ( (*p) < 20u ) {
			if ( (*p) < 16u ) {
				if ( 2u <= (*p) && (*p) <= 3u )
					goto st1;
			} else if ( (*p) > 17u ) {
				if ( 18u <= (*p) && (*p) <= 19u )
					goto tr82;
			} else
				goto tr81;
		} else if ( (*p) > 21u ) {
			if ( (*p) < 40u ) {
				if ( 22u <= (*p) && (*p) <= 23u )
					goto tr82;
			} else if ( (*p) > 47u ) {
				if ( 64u <= (*p) && (*p) <= 79u )
					goto st1;
			} else
				goto tr81;
		} else
			goto tr81;
	} else if ( (*p) > 111u ) {
		if ( (*p) < 180u ) {
			if ( (*p) < 116u ) {
				if ( 113u <= (*p) && (*p) <= 114u )
					goto st63;
			} else if ( (*p) > 118u ) {
				if ( 124u <= (*p) && (*p) <= 127u )
					goto tr81;
			} else
				goto tr81;
		} else if ( (*p) > 181u ) {
			if ( (*p) < 208u ) {
				if ( 188u <= (*p) && (*p) <= 190u )
					goto st1;
			} else if ( (*p) > 239u ) {
				if ( 241u <= (*p) && (*p) <= 254u )
					goto tr81;
			} else
				goto tr81;
		} else
			goto st29;
	} else
		goto tr81;
	goto tr19;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	if ( 200u <= (*p) && (*p) <= 207u )
		goto tr0;
	goto tr19;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	if ( 224u <= (*p) && (*p) <= 231u )
		goto tr0;
	goto tr19;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	switch( (*p) ) {
		case 16u: goto tr81;
		case 23u: goto tr81;
		case 42u: goto tr82;
	}
	if ( (*p) < 32u ) {
		if ( (*p) < 20u ) {
			if ( (*p) <= 11u )
				goto tr81;
		} else if ( (*p) > 21u ) {
			if ( 28u <= (*p) && (*p) <= 30u )
				goto tr81;
		} else
			goto tr81;
	} else if ( (*p) > 37u ) {
		if ( (*p) < 48u ) {
			if ( 40u <= (*p) && (*p) <= 43u )
				goto tr81;
		} else if ( (*p) > 53u ) {
			if ( (*p) > 65u ) {
				if ( 219u <= (*p) && (*p) <= 223u )
					goto tr81;
			} else if ( (*p) >= 55u )
				goto tr81;
		} else
			goto tr81;
	} else
		goto tr81;
	goto tr19;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	switch( (*p) ) {
		case 23u: goto tr86;
		case 32u: goto st62;
		case 68u: goto tr86;
		case 223u: goto tr86;
	}
	if ( (*p) < 33u ) {
		if ( (*p) > 15u ) {
			if ( 20u <= (*p) && (*p) <= 22u )
				goto st62;
		} else if ( (*p) >= 8u )
			goto tr86;
	} else if ( (*p) > 34u ) {
		if ( (*p) > 66u ) {
			if ( 96u <= (*p) && (*p) <= 99u )
				goto tr86;
		} else if ( (*p) >= 64u )
			goto tr86;
	} else
		goto tr86;
	goto tr19;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	switch( (*p) ) {
		case 4u: goto tr92;
		case 5u: goto tr93;
		case 12u: goto tr92;
		case 13u: goto tr93;
		case 20u: goto tr92;
		case 21u: goto tr93;
		case 28u: goto tr92;
		case 29u: goto tr93;
		case 36u: goto tr92;
		case 37u: goto tr93;
		case 44u: goto tr92;
		case 45u: goto tr93;
		case 52u: goto tr92;
		case 53u: goto tr93;
		case 60u: goto tr92;
		case 61u: goto tr93;
		case 68u: goto tr95;
		case 76u: goto tr95;
		case 84u: goto tr95;
		case 92u: goto tr95;
		case 100u: goto tr95;
		case 108u: goto tr95;
		case 116u: goto tr95;
		case 124u: goto tr95;
		case 132u: goto tr96;
		case 140u: goto tr96;
		case 148u: goto tr96;
		case 156u: goto tr96;
		case 164u: goto tr96;
		case 172u: goto tr96;
		case 180u: goto tr96;
		case 188u: goto tr96;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto tr93;
	} else if ( (*p) >= 64u )
		goto tr94;
	goto tr91;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	if ( (*p) < 224u ) {
		if ( 208u <= (*p) && (*p) <= 215u )
			goto tr91;
	} else if ( (*p) > 231u ) {
		if ( 240u <= (*p) && (*p) <= 247u )
			goto tr91;
	} else
		goto tr91;
	goto tr19;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	if ( (*p) > 223u ) {
		if ( 240u <= (*p) )
			goto tr91;
	} else if ( (*p) >= 208u )
		goto tr91;
	goto tr19;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	if ( (*p) == 15u )
		goto st66;
	goto tr19;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
	if ( (*p) == 31u )
		goto st67;
	goto tr19;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	if ( (*p) == 132u )
		goto st68;
	goto tr19;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	if ( (*p) == 0u )
		goto st69;
	goto tr19;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	if ( (*p) == 0u )
		goto st70;
	goto tr19;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	if ( (*p) == 0u )
		goto st71;
	goto tr19;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
	if ( (*p) == 0u )
		goto st72;
	goto tr19;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
	if ( (*p) == 0u )
		goto tr104;
	goto tr19;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	switch( (*p) ) {
		case 46u: goto st65;
		case 102u: goto st74;
	}
	goto tr19;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
	switch( (*p) ) {
		case 46u: goto st65;
		case 102u: goto st75;
	}
	goto tr19;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
	switch( (*p) ) {
		case 46u: goto st65;
		case 102u: goto st76;
	}
	goto tr19;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
	switch( (*p) ) {
		case 46u: goto st65;
		case 102u: goto st77;
	}
	goto tr19;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	if ( (*p) == 46u )
		goto st65;
	goto tr19;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
	switch( (*p) ) {
		case 4u: goto st79;
		case 5u: goto st80;
		case 12u: goto st79;
		case 13u: goto st80;
		case 20u: goto st79;
		case 21u: goto st80;
		case 28u: goto st79;
		case 29u: goto st80;
		case 36u: goto st79;
		case 37u: goto st80;
		case 44u: goto st79;
		case 45u: goto st80;
		case 52u: goto st79;
		case 53u: goto st80;
		case 60u: goto st79;
		case 61u: goto st80;
		case 68u: goto st85;
		case 76u: goto st85;
		case 84u: goto st85;
		case 92u: goto st85;
		case 100u: goto st85;
		case 108u: goto st85;
		case 116u: goto st85;
		case 124u: goto st85;
		case 132u: goto st86;
		case 140u: goto st86;
		case 148u: goto st86;
		case 156u: goto st86;
		case 164u: goto st86;
		case 172u: goto st86;
		case 180u: goto st86;
		case 188u: goto st86;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st80;
	} else if ( (*p) >= 64u )
		goto st84;
	goto st55;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
	switch( (*p) ) {
		case 5u: goto st80;
		case 13u: goto st80;
		case 21u: goto st80;
		case 29u: goto st80;
		case 37u: goto st80;
		case 45u: goto st80;
		case 53u: goto st80;
		case 61u: goto st80;
		case 69u: goto st80;
		case 77u: goto st80;
		case 85u: goto st80;
		case 93u: goto st80;
		case 101u: goto st80;
		case 109u: goto st80;
		case 117u: goto st80;
		case 125u: goto st80;
		case 133u: goto st80;
		case 141u: goto st80;
		case 149u: goto st80;
		case 157u: goto st80;
		case 165u: goto st80;
		case 173u: goto st80;
		case 181u: goto st80;
		case 189u: goto st80;
		case 197u: goto st80;
		case 205u: goto st80;
		case 213u: goto st80;
		case 221u: goto st80;
		case 229u: goto st80;
		case 237u: goto st80;
		case 245u: goto st80;
		case 253u: goto st80;
	}
	goto st55;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
	goto st81;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
	goto st82;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
	goto st83;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
	goto tr117;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
	goto tr118;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
	goto st84;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
	goto st80;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	if ( 192u <= (*p) && (*p) <= 239u )
		goto tr0;
	goto tr19;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
	switch( (*p) ) {
		case 4u: goto tr119;
		case 5u: goto tr120;
		case 68u: goto tr122;
		case 132u: goto tr123;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto tr104;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto tr104;
		} else if ( (*p) >= 128u )
			goto tr120;
	} else
		goto tr121;
	goto tr19;
tr291:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st89;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
#line 2201 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 12u: goto st34;
		case 13u: goto st35;
		case 20u: goto st34;
		case 21u: goto st35;
		case 28u: goto st34;
		case 29u: goto st35;
		case 36u: goto st34;
		case 37u: goto st35;
		case 44u: goto st34;
		case 45u: goto st35;
		case 60u: goto st34;
		case 61u: goto st35;
		case 68u: goto st40;
		case 76u: goto st40;
		case 84u: goto st40;
		case 92u: goto st40;
		case 100u: goto st40;
		case 108u: goto st40;
		case 124u: goto st40;
		case 132u: goto st41;
		case 140u: goto st41;
		case 148u: goto st41;
		case 156u: goto st41;
		case 164u: goto st41;
		case 172u: goto st41;
		case 188u: goto st41;
	}
	if ( (*p) < 120u ) {
		if ( (*p) < 64u ) {
			if ( 48u <= (*p) && (*p) <= 55u )
				goto tr19;
		} else if ( (*p) > 111u ) {
			if ( 112u <= (*p) && (*p) <= 119u )
				goto tr19;
		} else
			goto st39;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 175u )
				goto st35;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 191u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr19;
			} else if ( (*p) >= 184u )
				goto st35;
		} else
			goto tr19;
	} else
		goto st39;
	goto st10;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
	switch( (*p) ) {
		case 4u: goto st79;
		case 5u: goto st80;
		case 68u: goto st85;
		case 132u: goto st86;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto st55;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto st55;
		} else if ( (*p) >= 128u )
			goto st80;
	} else
		goto st84;
	goto tr19;
tr298:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st91;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
#line 2289 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 120u ) {
		if ( (*p) < 64u ) {
			if ( 48u <= (*p) && (*p) <= 55u )
				goto tr19;
		} else if ( (*p) > 111u ) {
			if ( 112u <= (*p) && (*p) <= 119u )
				goto tr19;
		} else
			goto st7;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 175u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 191u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr19;
			} else if ( (*p) >= 184u )
				goto st3;
		} else
			goto tr19;
	} else
		goto st7;
	goto tr0;
tr72:
#line 13 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    lock_prefix = TRUE;
  }
	goto st92;
tr250:
#line 10 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = TRUE;
  }
	goto st92;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
#line 2360 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 1u: goto st29;
		case 3u: goto st29;
		case 9u: goto st29;
		case 11u: goto st29;
		case 15u: goto st93;
		case 17u: goto st29;
		case 19u: goto st29;
		case 25u: goto st29;
		case 27u: goto st29;
		case 33u: goto st29;
		case 35u: goto st29;
		case 41u: goto st29;
		case 43u: goto st29;
		case 49u: goto st29;
		case 51u: goto st29;
		case 129u: goto st94;
		case 131u: goto st95;
		case 135u: goto st29;
		case 247u: goto st96;
		case 255u: goto st18;
	}
	goto tr19;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
	switch( (*p) ) {
		case 177u: goto st29;
		case 193u: goto st29;
	}
	goto tr19;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
	switch( (*p) ) {
		case 4u: goto st79;
		case 5u: goto st80;
		case 12u: goto st79;
		case 13u: goto st80;
		case 20u: goto st79;
		case 21u: goto st80;
		case 28u: goto st79;
		case 29u: goto st80;
		case 36u: goto st79;
		case 37u: goto st80;
		case 44u: goto st79;
		case 45u: goto st80;
		case 52u: goto st79;
		case 53u: goto st80;
		case 68u: goto st85;
		case 76u: goto st85;
		case 84u: goto st85;
		case 92u: goto st85;
		case 100u: goto st85;
		case 108u: goto st85;
		case 116u: goto st85;
		case 132u: goto st86;
		case 140u: goto st86;
		case 148u: goto st86;
		case 156u: goto st86;
		case 164u: goto st86;
		case 172u: goto st86;
		case 180u: goto st86;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 55u )
			goto st55;
	} else if ( (*p) > 119u ) {
		if ( 128u <= (*p) && (*p) <= 183u )
			goto st80;
	} else
		goto st84;
	goto tr19;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 12u: goto st34;
		case 13u: goto st35;
		case 20u: goto st34;
		case 21u: goto st35;
		case 28u: goto st34;
		case 29u: goto st35;
		case 36u: goto st34;
		case 37u: goto st35;
		case 44u: goto st34;
		case 45u: goto st35;
		case 52u: goto st34;
		case 53u: goto st35;
		case 68u: goto st40;
		case 76u: goto st40;
		case 84u: goto st40;
		case 92u: goto st40;
		case 100u: goto st40;
		case 108u: goto st40;
		case 116u: goto st40;
		case 132u: goto st41;
		case 140u: goto st41;
		case 148u: goto st41;
		case 156u: goto st41;
		case 164u: goto st41;
		case 172u: goto st41;
		case 180u: goto st41;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 55u )
			goto st10;
	} else if ( (*p) > 119u ) {
		if ( 128u <= (*p) && (*p) <= 183u )
			goto st35;
	} else
		goto st39;
	goto tr19;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	switch( (*p) ) {
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 84u: goto st8;
		case 92u: goto st8;
		case 148u: goto st9;
		case 156u: goto st9;
	}
	if ( (*p) < 80u ) {
		if ( 16u <= (*p) && (*p) <= 31u )
			goto tr0;
	} else if ( (*p) > 95u ) {
		if ( 144u <= (*p) && (*p) <= 159u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr73:
#line 22 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = TRUE;
  }
	goto st97;
tr253:
#line 10 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = TRUE;
  }
	goto st97;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
#line 2518 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 15u: goto st98;
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr19;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
	if ( (*p) == 56u )
		goto st99;
	goto tr19;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
	if ( (*p) == 241u )
		goto tr130;
	goto tr19;
tr74:
#line 19 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = TRUE;
  }
#line 16 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = TRUE;
  }
	goto st100;
tr262:
#line 10 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    data16_prefix = TRUE;
  }
	goto st100;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
#line 2559 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 15u: goto st101;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr19;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	if ( (*p) == 184u )
		goto tr132;
	if ( 188u <= (*p) && (*p) <= 189u )
		goto tr132;
	goto tr19;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
	switch( (*p) ) {
		case 4u: goto st79;
		case 5u: goto st80;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st85;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st86;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 80u ) {
		if ( (*p) < 8u ) {
			if ( (*p) <= 7u )
				goto st55;
		} else if ( (*p) > 15u ) {
			if ( (*p) > 71u ) {
				if ( 72u <= (*p) && (*p) <= 79u )
					goto tr19;
			} else if ( (*p) >= 64u )
				goto st84;
		} else
			goto tr19;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 144u ) {
			if ( (*p) > 135u ) {
				if ( 136u <= (*p) && (*p) <= 143u )
					goto tr19;
			} else if ( (*p) >= 128u )
				goto st80;
		} else if ( (*p) > 191u ) {
			if ( (*p) > 199u ) {
				if ( 200u <= (*p) && (*p) <= 207u )
					goto tr19;
			} else if ( (*p) >= 192u )
				goto st55;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto tr119;
		case 53u: goto tr120;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto tr122;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto tr123;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr104;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr104;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto tr120;
	} else
		goto tr121;
	goto tr19;
tr283:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st104;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
#line 2695 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st105;
		case 5u: goto st106;
		case 12u: goto st105;
		case 13u: goto st106;
		case 20u: goto st105;
		case 21u: goto st106;
		case 28u: goto st105;
		case 29u: goto st106;
		case 36u: goto st105;
		case 37u: goto st106;
		case 44u: goto st105;
		case 45u: goto st106;
		case 52u: goto st105;
		case 53u: goto st106;
		case 60u: goto st105;
		case 61u: goto st106;
		case 68u: goto st111;
		case 76u: goto st111;
		case 84u: goto st111;
		case 92u: goto st111;
		case 100u: goto st111;
		case 108u: goto st111;
		case 116u: goto st111;
		case 124u: goto st111;
		case 132u: goto st112;
		case 140u: goto st112;
		case 148u: goto st112;
		case 156u: goto st112;
		case 164u: goto st112;
		case 172u: goto st112;
		case 180u: goto st112;
		case 188u: goto st112;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st106;
	} else if ( (*p) >= 64u )
		goto st110;
	goto st11;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	switch( (*p) ) {
		case 5u: goto st106;
		case 13u: goto st106;
		case 21u: goto st106;
		case 29u: goto st106;
		case 37u: goto st106;
		case 45u: goto st106;
		case 53u: goto st106;
		case 61u: goto st106;
		case 69u: goto st106;
		case 77u: goto st106;
		case 85u: goto st106;
		case 93u: goto st106;
		case 101u: goto st106;
		case 109u: goto st106;
		case 117u: goto st106;
		case 125u: goto st106;
		case 133u: goto st106;
		case 141u: goto st106;
		case 149u: goto st106;
		case 157u: goto st106;
		case 165u: goto st106;
		case 173u: goto st106;
		case 181u: goto st106;
		case 189u: goto st106;
		case 197u: goto st106;
		case 205u: goto st106;
		case 213u: goto st106;
		case 221u: goto st106;
		case 229u: goto st106;
		case 237u: goto st106;
		case 245u: goto st106;
		case 253u: goto st106;
	}
	goto st11;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
	goto st107;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
	goto st108;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
	goto st109;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
	goto tr142;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
	goto tr143;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
	goto st110;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
	goto st106;
tr286:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st113;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
#line 2821 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 12u: goto st34;
		case 13u: goto st35;
		case 20u: goto st34;
		case 21u: goto st35;
		case 28u: goto st34;
		case 29u: goto st35;
		case 36u: goto st34;
		case 37u: goto st35;
		case 44u: goto st34;
		case 45u: goto st35;
		case 52u: goto st34;
		case 53u: goto st35;
		case 60u: goto st34;
		case 61u: goto st35;
		case 68u: goto st40;
		case 76u: goto st40;
		case 84u: goto st40;
		case 92u: goto st40;
		case 100u: goto st40;
		case 108u: goto st40;
		case 116u: goto st40;
		case 124u: goto st40;
		case 132u: goto st41;
		case 140u: goto st41;
		case 148u: goto st41;
		case 156u: goto st41;
		case 164u: goto st41;
		case 172u: goto st41;
		case 180u: goto st41;
		case 188u: goto st41;
		case 224u: goto st114;
		case 225u: goto st220;
		case 226u: goto st222;
		case 227u: goto st224;
		case 228u: goto st226;
		case 229u: goto st228;
		case 230u: goto st230;
		case 231u: goto st232;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st35;
	} else if ( (*p) >= 64u )
		goto st39;
	goto st10;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
	if ( (*p) == 224u )
		goto tr152;
	goto tr11;
tr152:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st236;
st236:
	if ( ++p == pe )
		goto _test_eof236;
case 236:
#line 2892 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr314;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr287:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st115;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
#line 3045 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
	}
	if ( (*p) < 112u ) {
		if ( (*p) > 63u ) {
			if ( 64u <= (*p) && (*p) <= 111u )
				goto st7;
		} else if ( (*p) >= 48u )
			goto tr19;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 175u )
				goto st3;
		} else if ( (*p) > 191u ) {
			if ( 240u <= (*p) )
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr19;
	goto tr0;
tr289:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st116;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
#line 3101 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 232u: goto st117;
		case 233u: goto st132;
		case 234u: goto st140;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto tr0;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto tr0;
		} else if ( (*p) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr19;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
	switch( (*p) ) {
		case 64u: goto tr156;
		case 68u: goto tr157;
		case 72u: goto tr156;
		case 76u: goto tr157;
		case 80u: goto tr156;
		case 84u: goto tr157;
		case 88u: goto tr156;
		case 92u: goto tr157;
		case 96u: goto tr156;
		case 100u: goto tr157;
		case 104u: goto tr156;
		case 108u: goto tr157;
		case 112u: goto tr156;
		case 116u: goto tr157;
		case 120u: goto tr158;
		case 124u: goto tr157;
		case 192u: goto tr159;
		case 196u: goto tr157;
		case 200u: goto tr159;
		case 204u: goto tr157;
		case 208u: goto tr159;
		case 212u: goto tr157;
		case 216u: goto tr159;
		case 220u: goto tr157;
		case 224u: goto tr159;
		case 228u: goto tr157;
		case 232u: goto tr159;
		case 236u: goto tr157;
		case 240u: goto tr159;
		case 244u: goto tr157;
		case 248u: goto tr159;
		case 252u: goto tr157;
	}
	goto tr19;
tr156:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st118;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
#line 3172 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 166u: goto st119;
		case 182u: goto st119;
	}
	if ( (*p) < 158u ) {
		if ( (*p) < 142u ) {
			if ( 133u <= (*p) && (*p) <= 135u )
				goto st119;
		} else if ( (*p) > 143u ) {
			if ( 149u <= (*p) && (*p) <= 151u )
				goto st119;
		} else
			goto st119;
	} else if ( (*p) > 159u ) {
		if ( (*p) < 204u ) {
			if ( 162u <= (*p) && (*p) <= 163u )
				goto st119;
		} else if ( (*p) > 207u ) {
			if ( 236u <= (*p) && (*p) <= 239u )
				goto st33;
		} else
			goto st33;
	} else
		goto st119;
	goto tr19;
st119:
	if ( ++p == pe )
		goto _test_eof119;
case 119:
	switch( (*p) ) {
		case 4u: goto st121;
		case 5u: goto st122;
		case 12u: goto st121;
		case 13u: goto st122;
		case 20u: goto st121;
		case 21u: goto st122;
		case 28u: goto st121;
		case 29u: goto st122;
		case 36u: goto st121;
		case 37u: goto st122;
		case 44u: goto st121;
		case 45u: goto st122;
		case 52u: goto st121;
		case 53u: goto st122;
		case 60u: goto st121;
		case 61u: goto st122;
		case 68u: goto st127;
		case 76u: goto st127;
		case 84u: goto st127;
		case 92u: goto st127;
		case 100u: goto st127;
		case 108u: goto st127;
		case 116u: goto st127;
		case 124u: goto st127;
		case 132u: goto st128;
		case 140u: goto st128;
		case 148u: goto st128;
		case 156u: goto st128;
		case 164u: goto st128;
		case 172u: goto st128;
		case 180u: goto st128;
		case 188u: goto st128;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st122;
	} else if ( (*p) >= 64u )
		goto st126;
	goto st120;
tr170:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st120;
tr171:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st120;
st120:
	if ( ++p == pe )
		goto _test_eof120;
case 120:
#line 3260 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 0u: goto tr0;
		case 16u: goto tr0;
		case 32u: goto tr0;
		case 48u: goto tr0;
		case 64u: goto tr0;
		case 80u: goto tr0;
		case 96u: goto tr0;
		case 112u: goto tr0;
	}
	goto tr19;
st121:
	if ( ++p == pe )
		goto _test_eof121;
case 121:
	switch( (*p) ) {
		case 5u: goto st122;
		case 13u: goto st122;
		case 21u: goto st122;
		case 29u: goto st122;
		case 37u: goto st122;
		case 45u: goto st122;
		case 53u: goto st122;
		case 61u: goto st122;
		case 69u: goto st122;
		case 77u: goto st122;
		case 85u: goto st122;
		case 93u: goto st122;
		case 101u: goto st122;
		case 109u: goto st122;
		case 117u: goto st122;
		case 125u: goto st122;
		case 133u: goto st122;
		case 141u: goto st122;
		case 149u: goto st122;
		case 157u: goto st122;
		case 165u: goto st122;
		case 173u: goto st122;
		case 181u: goto st122;
		case 189u: goto st122;
		case 197u: goto st122;
		case 205u: goto st122;
		case 213u: goto st122;
		case 221u: goto st122;
		case 229u: goto st122;
		case 237u: goto st122;
		case 245u: goto st122;
		case 253u: goto st122;
	}
	goto st120;
st122:
	if ( ++p == pe )
		goto _test_eof122;
case 122:
	goto st123;
st123:
	if ( ++p == pe )
		goto _test_eof123;
case 123:
	goto st124;
st124:
	if ( ++p == pe )
		goto _test_eof124;
case 124:
	goto st125;
st125:
	if ( ++p == pe )
		goto _test_eof125;
case 125:
	goto tr170;
st126:
	if ( ++p == pe )
		goto _test_eof126;
case 126:
	goto tr171;
st127:
	if ( ++p == pe )
		goto _test_eof127;
case 127:
	goto st126;
st128:
	if ( ++p == pe )
		goto _test_eof128;
case 128:
	goto st122;
tr157:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st129;
st129:
	if ( ++p == pe )
		goto _test_eof129;
case 129:
#line 3356 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) == 162u )
		goto st119;
	goto tr19;
tr158:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st130;
st130:
	if ( ++p == pe )
		goto _test_eof130;
case 130:
#line 3370 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 166u: goto st119;
		case 182u: goto st119;
	}
	if ( (*p) < 158u ) {
		if ( (*p) < 142u ) {
			if ( 133u <= (*p) && (*p) <= 135u )
				goto st119;
		} else if ( (*p) > 143u ) {
			if ( 149u <= (*p) && (*p) <= 151u )
				goto st119;
		} else
			goto st119;
	} else if ( (*p) > 159u ) {
		if ( (*p) < 192u ) {
			if ( 162u <= (*p) && (*p) <= 163u )
				goto st119;
		} else if ( (*p) > 195u ) {
			if ( (*p) > 207u ) {
				if ( 236u <= (*p) && (*p) <= 239u )
					goto st33;
			} else if ( (*p) >= 204u )
				goto st33;
		} else
			goto st33;
	} else
		goto st119;
	goto tr19;
tr159:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st131;
st131:
	if ( ++p == pe )
		goto _test_eof131;
case 131:
#line 3409 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 162u <= (*p) && (*p) <= 163u )
		goto st119;
	goto tr19;
st132:
	if ( ++p == pe )
		goto _test_eof132;
case 132:
	switch( (*p) ) {
		case 64u: goto tr172;
		case 72u: goto tr172;
		case 80u: goto tr172;
		case 88u: goto tr172;
		case 96u: goto tr172;
		case 104u: goto tr172;
		case 112u: goto tr172;
		case 120u: goto tr173;
		case 124u: goto tr174;
		case 192u: goto tr175;
		case 200u: goto tr175;
		case 208u: goto tr175;
		case 216u: goto tr175;
		case 224u: goto tr175;
		case 232u: goto tr175;
		case 240u: goto tr175;
		case 248u: goto tr175;
	}
	goto tr19;
tr172:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st133;
st133:
	if ( ++p == pe )
		goto _test_eof133;
case 133:
#line 3447 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 1u: goto st134;
		case 2u: goto st135;
	}
	if ( 144u <= (*p) && (*p) <= 155u )
		goto st1;
	goto tr19;
st134:
	if ( ++p == pe )
		goto _test_eof134;
case 134:
	switch( (*p) ) {
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 72u ) {
		if ( (*p) > 7u ) {
			if ( 64u <= (*p) && (*p) <= 71u )
				goto tr19;
		} else
			goto tr19;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 136u ) {
			if ( 128u <= (*p) && (*p) <= 135u )
				goto tr19;
		} else if ( (*p) > 191u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto tr19;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st135:
	if ( ++p == pe )
		goto _test_eof135;
case 135:
	switch( (*p) ) {
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 76u: goto st8;
		case 116u: goto st8;
		case 140u: goto st9;
		case 180u: goto st9;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( 8u <= (*p) && (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 72u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 136u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 200u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
tr173:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st136;
st136:
	if ( ++p == pe )
		goto _test_eof136;
case 136:
#line 3555 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 1u: goto st134;
		case 2u: goto st135;
		case 18u: goto st137;
		case 203u: goto st1;
		case 219u: goto st1;
	}
	if ( (*p) < 198u ) {
		if ( (*p) < 144u ) {
			if ( 128u <= (*p) && (*p) <= 131u )
				goto st1;
		} else if ( (*p) > 155u ) {
			if ( 193u <= (*p) && (*p) <= 195u )
				goto st1;
		} else
			goto st1;
	} else if ( (*p) > 199u ) {
		if ( (*p) < 214u ) {
			if ( 209u <= (*p) && (*p) <= 211u )
				goto st1;
		} else if ( (*p) > 215u ) {
			if ( 225u <= (*p) && (*p) <= 227u )
				goto st1;
		} else
			goto st1;
	} else
		goto st1;
	goto tr19;
st137:
	if ( ++p == pe )
		goto _test_eof137;
case 137:
	if ( 192u <= (*p) && (*p) <= 207u )
		goto tr0;
	goto tr19;
tr174:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st138;
st138:
	if ( ++p == pe )
		goto _test_eof138;
case 138:
#line 3601 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 128u <= (*p) && (*p) <= 129u )
		goto st1;
	goto tr19;
tr175:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st139;
st139:
	if ( ++p == pe )
		goto _test_eof139;
case 139:
#line 3615 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 144u <= (*p) && (*p) <= 155u )
		goto st1;
	goto tr19;
st140:
	if ( ++p == pe )
		goto _test_eof140;
case 140:
	switch( (*p) ) {
		case 64u: goto tr179;
		case 72u: goto tr179;
		case 80u: goto tr179;
		case 88u: goto tr179;
		case 96u: goto tr179;
		case 104u: goto tr179;
		case 112u: goto tr179;
		case 120u: goto tr180;
	}
	goto tr19;
tr179:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st141;
st141:
	if ( ++p == pe )
		goto _test_eof141;
case 141:
#line 3644 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) == 18u )
		goto st142;
	goto tr19;
st142:
	if ( ++p == pe )
		goto _test_eof142;
case 142:
	switch( (*p) ) {
		case 4u: goto st105;
		case 5u: goto st106;
		case 12u: goto st105;
		case 13u: goto st106;
		case 68u: goto st111;
		case 76u: goto st111;
		case 132u: goto st112;
		case 140u: goto st112;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 15u )
			goto st11;
	} else if ( (*p) > 79u ) {
		if ( (*p) > 143u ) {
			if ( 192u <= (*p) && (*p) <= 207u )
				goto st11;
		} else if ( (*p) >= 128u )
			goto st106;
	} else
		goto st110;
	goto tr19;
tr180:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st143;
st143:
	if ( ++p == pe )
		goto _test_eof143;
case 143:
#line 3684 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 16u: goto st104;
		case 18u: goto st142;
	}
	goto tr19;
tr292:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st144;
st144:
	if ( ++p == pe )
		goto _test_eof144;
case 144:
#line 3701 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 225u: goto st145;
		case 226u: goto st163;
		case 227u: goto st172;
	}
	goto tr19;
st145:
	if ( ++p == pe )
		goto _test_eof145;
case 145:
	switch( (*p) ) {
		case 65u: goto tr187;
		case 66u: goto tr188;
		case 67u: goto tr189;
		case 68u: goto tr190;
		case 69u: goto tr191;
		case 70u: goto tr192;
		case 71u: goto tr193;
		case 73u: goto tr187;
		case 74u: goto tr188;
		case 75u: goto tr189;
		case 76u: goto tr190;
		case 77u: goto tr191;
		case 78u: goto tr192;
		case 79u: goto tr193;
		case 81u: goto tr187;
		case 82u: goto tr188;
		case 83u: goto tr189;
		case 84u: goto tr190;
		case 85u: goto tr191;
		case 86u: goto tr192;
		case 87u: goto tr193;
		case 89u: goto tr187;
		case 90u: goto tr188;
		case 91u: goto tr189;
		case 92u: goto tr190;
		case 93u: goto tr191;
		case 94u: goto tr192;
		case 95u: goto tr193;
		case 97u: goto tr187;
		case 98u: goto tr188;
		case 99u: goto tr189;
		case 100u: goto tr190;
		case 101u: goto tr191;
		case 102u: goto tr192;
		case 103u: goto tr193;
		case 105u: goto tr187;
		case 106u: goto tr188;
		case 107u: goto tr189;
		case 108u: goto tr190;
		case 109u: goto tr191;
		case 110u: goto tr192;
		case 111u: goto tr193;
		case 113u: goto tr187;
		case 114u: goto tr188;
		case 115u: goto tr189;
		case 116u: goto tr190;
		case 117u: goto tr191;
		case 118u: goto tr192;
		case 119u: goto tr193;
		case 120u: goto tr194;
		case 121u: goto tr195;
		case 122u: goto tr196;
		case 123u: goto tr197;
		case 124u: goto tr198;
		case 125u: goto tr199;
		case 126u: goto tr200;
		case 127u: goto tr201;
	}
	if ( 64u <= (*p) && (*p) <= 112u )
		goto tr186;
	goto tr19;
tr186:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st146;
tr231:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st146;
st146:
	if ( ++p == pe )
		goto _test_eof146;
case 146:
#line 3791 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 23u: goto st29;
		case 81u: goto st1;
		case 194u: goto st33;
		case 198u: goto st33;
	}
	if ( (*p) < 46u ) {
		if ( 20u <= (*p) && (*p) <= 22u )
			goto st1;
	} else if ( (*p) > 47u ) {
		if ( (*p) > 89u ) {
			if ( 92u <= (*p) && (*p) <= 95u )
				goto st1;
		} else if ( (*p) >= 84u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr187:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st147;
tr232:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st147;
st147:
	if ( ++p == pe )
		goto _test_eof147;
case 147:
#line 3828 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st29;
		case 81u: goto st1;
		case 115u: goto st148;
		case 194u: goto st33;
		case 198u: goto st33;
	}
	if ( (*p) < 116u ) {
		if ( (*p) < 46u ) {
			if ( (*p) > 21u ) {
				if ( 22u <= (*p) && (*p) <= 23u )
					goto st29;
			} else if ( (*p) >= 20u )
				goto st1;
		} else if ( (*p) > 47u ) {
			if ( (*p) < 92u ) {
				if ( 84u <= (*p) && (*p) <= 89u )
					goto st1;
			} else if ( (*p) > 109u ) {
				if ( 113u <= (*p) && (*p) <= 114u )
					goto st42;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*p) > 118u ) {
		if ( (*p) < 216u ) {
			if ( (*p) > 125u ) {
				if ( 208u <= (*p) && (*p) <= 213u )
					goto st1;
			} else if ( (*p) >= 124u )
				goto st1;
		} else if ( (*p) > 229u ) {
			if ( (*p) < 241u ) {
				if ( 232u <= (*p) && (*p) <= 239u )
					goto st1;
			} else if ( (*p) > 246u ) {
				if ( 248u <= (*p) && (*p) <= 254u )
					goto st1;
			} else
				goto st1;
		} else
			goto st1;
	} else
		goto st1;
	goto tr19;
st148:
	if ( ++p == pe )
		goto _test_eof148;
case 148:
	if ( (*p) > 223u ) {
		if ( 240u <= (*p) )
			goto st10;
	} else if ( (*p) >= 208u )
		goto st10;
	goto tr19;
tr188:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st149;
st149:
	if ( ++p == pe )
		goto _test_eof149;
case 149:
#line 3895 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 42u: goto st1;
		case 81u: goto st1;
		case 83u: goto st1;
		case 194u: goto st33;
	}
	if ( (*p) > 90u ) {
		if ( 92u <= (*p) && (*p) <= 95u )
			goto st1;
	} else if ( (*p) >= 88u )
		goto st1;
	goto tr19;
tr189:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st150;
st150:
	if ( ++p == pe )
		goto _test_eof150;
case 150:
#line 3918 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 42u: goto st1;
		case 81u: goto st1;
		case 194u: goto st33;
		case 208u: goto st1;
	}
	if ( (*p) < 92u ) {
		if ( 88u <= (*p) && (*p) <= 90u )
			goto st1;
	} else if ( (*p) > 95u ) {
		if ( 124u <= (*p) && (*p) <= 125u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr190:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st151;
tr235:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st151;
st151:
	if ( ++p == pe )
		goto _test_eof151;
case 151:
#line 3951 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 81u: goto st1;
		case 194u: goto st33;
		case 198u: goto st33;
	}
	if ( (*p) < 84u ) {
		if ( 20u <= (*p) && (*p) <= 21u )
			goto st1;
	} else if ( (*p) > 89u ) {
		if ( 92u <= (*p) && (*p) <= 95u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr191:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st152;
tr236:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st152;
st152:
	if ( ++p == pe )
		goto _test_eof152;
case 152:
#line 3983 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 81u: goto st1;
		case 194u: goto st33;
		case 198u: goto st33;
		case 208u: goto st1;
	}
	if ( (*p) < 84u ) {
		if ( 20u <= (*p) && (*p) <= 21u )
			goto st1;
	} else if ( (*p) > 89u ) {
		if ( (*p) > 95u ) {
			if ( 124u <= (*p) && (*p) <= 125u )
				goto st1;
		} else if ( (*p) >= 92u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr192:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st153;
tr237:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st153;
st153:
	if ( ++p == pe )
		goto _test_eof153;
case 153:
#line 4019 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 16u <= (*p) && (*p) <= 17u )
		goto st32;
	goto tr19;
tr193:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st154;
tr238:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st154;
st154:
	if ( ++p == pe )
		goto _test_eof154;
case 154:
#line 4040 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) == 208u )
		goto st1;
	if ( (*p) > 17u ) {
		if ( 124u <= (*p) && (*p) <= 125u )
			goto st1;
	} else if ( (*p) >= 16u )
		goto st32;
	goto tr19;
tr194:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st155;
tr239:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st155;
st155:
	if ( ++p == pe )
		goto _test_eof155;
case 155:
#line 4066 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 19u: goto st29;
		case 23u: goto st29;
		case 43u: goto st29;
		case 80u: goto st32;
		case 119u: goto tr0;
		case 174u: goto st96;
		case 194u: goto st33;
		case 198u: goto st33;
	}
	if ( (*p) < 40u ) {
		if ( 16u <= (*p) && (*p) <= 22u )
			goto st1;
	} else if ( (*p) > 41u ) {
		if ( (*p) > 47u ) {
			if ( 81u <= (*p) && (*p) <= 95u )
				goto st1;
		} else if ( (*p) >= 46u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr195:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st156;
tr240:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st156;
st156:
	if ( ++p == pe )
		goto _test_eof156;
case 156:
#line 4106 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 43u: goto st29;
		case 80u: goto st32;
		case 81u: goto st1;
		case 112u: goto st33;
		case 115u: goto st148;
		case 127u: goto st1;
		case 194u: goto st33;
		case 197u: goto st49;
		case 215u: goto st32;
		case 231u: goto st29;
		case 247u: goto st32;
	}
	if ( (*p) < 84u ) {
		if ( (*p) < 20u ) {
			if ( (*p) > 17u ) {
				if ( 18u <= (*p) && (*p) <= 19u )
					goto st29;
			} else if ( (*p) >= 16u )
				goto st1;
		} else if ( (*p) > 21u ) {
			if ( (*p) < 40u ) {
				if ( 22u <= (*p) && (*p) <= 23u )
					goto st29;
			} else if ( (*p) > 41u ) {
				if ( 46u <= (*p) && (*p) <= 47u )
					goto st1;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*p) > 111u ) {
		if ( (*p) < 196u ) {
			if ( (*p) < 116u ) {
				if ( 113u <= (*p) && (*p) <= 114u )
					goto st42;
			} else if ( (*p) > 118u ) {
				if ( 124u <= (*p) && (*p) <= 125u )
					goto st1;
			} else
				goto st1;
		} else if ( (*p) > 198u ) {
			if ( (*p) < 216u ) {
				if ( 208u <= (*p) && (*p) <= 213u )
					goto st1;
			} else if ( (*p) > 239u ) {
				if ( 241u <= (*p) && (*p) <= 254u )
					goto st1;
			} else
				goto st1;
		} else
			goto st33;
	} else
		goto st1;
	goto tr19;
tr196:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st157;
st157:
	if ( ++p == pe )
		goto _test_eof157;
case 157:
#line 4172 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 22u: goto st1;
		case 42u: goto st1;
		case 111u: goto st1;
		case 112u: goto st33;
		case 194u: goto st33;
		case 230u: goto st1;
	}
	if ( (*p) < 81u ) {
		if ( (*p) > 17u ) {
			if ( 44u <= (*p) && (*p) <= 45u )
				goto st1;
		} else if ( (*p) >= 16u )
			goto st29;
	} else if ( (*p) > 83u ) {
		if ( (*p) > 95u ) {
			if ( 126u <= (*p) && (*p) <= 127u )
				goto st1;
		} else if ( (*p) >= 88u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr197:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st158;
st158:
	if ( ++p == pe )
		goto _test_eof158;
case 158:
#line 4207 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 42u: goto st1;
		case 81u: goto st1;
		case 112u: goto st33;
		case 194u: goto st33;
		case 208u: goto st1;
		case 230u: goto st1;
		case 240u: goto st29;
	}
	if ( (*p) < 88u ) {
		if ( (*p) > 17u ) {
			if ( 44u <= (*p) && (*p) <= 45u )
				goto st1;
		} else if ( (*p) >= 16u )
			goto st29;
	} else if ( (*p) > 90u ) {
		if ( (*p) > 95u ) {
			if ( 124u <= (*p) && (*p) <= 125u )
				goto st1;
		} else if ( (*p) >= 92u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr198:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st159;
tr243:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st159;
st159:
	if ( ++p == pe )
		goto _test_eof159;
case 159:
#line 4250 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 43u: goto st29;
		case 80u: goto st32;
		case 119u: goto tr0;
		case 194u: goto st33;
		case 198u: goto st33;
	}
	if ( (*p) < 20u ) {
		if ( 16u <= (*p) && (*p) <= 17u )
			goto st1;
	} else if ( (*p) > 21u ) {
		if ( (*p) > 41u ) {
			if ( 81u <= (*p) && (*p) <= 95u )
				goto st1;
		} else if ( (*p) >= 40u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr199:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st160;
tr244:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st160;
st160:
	if ( ++p == pe )
		goto _test_eof160;
case 160:
#line 4287 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 43u: goto st29;
		case 80u: goto st32;
		case 81u: goto st1;
		case 194u: goto st33;
		case 198u: goto st33;
		case 208u: goto st1;
		case 214u: goto st1;
		case 230u: goto st1;
		case 231u: goto st29;
	}
	if ( (*p) < 40u ) {
		if ( (*p) > 17u ) {
			if ( 20u <= (*p) && (*p) <= 21u )
				goto st1;
		} else if ( (*p) >= 16u )
			goto st1;
	} else if ( (*p) > 41u ) {
		if ( (*p) > 95u ) {
			if ( 124u <= (*p) && (*p) <= 127u )
				goto st1;
		} else if ( (*p) >= 84u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr200:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st161;
tr245:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st161;
st161:
	if ( ++p == pe )
		goto _test_eof161;
case 161:
#line 4331 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 22u: goto st1;
		case 91u: goto st1;
		case 127u: goto st1;
		case 230u: goto st1;
	}
	if ( 16u <= (*p) && (*p) <= 17u )
		goto st32;
	goto tr19;
tr201:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st162;
tr246:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st162;
st162:
	if ( ++p == pe )
		goto _test_eof162;
case 162:
#line 4359 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 208u: goto st1;
		case 230u: goto st1;
		case 240u: goto st29;
	}
	if ( (*p) > 17u ) {
		if ( 124u <= (*p) && (*p) <= 125u )
			goto st1;
	} else if ( (*p) >= 16u )
		goto st32;
	goto tr19;
st163:
	if ( ++p == pe )
		goto _test_eof163;
case 163:
	switch( (*p) ) {
		case 64u: goto tr203;
		case 65u: goto tr204;
		case 69u: goto tr205;
		case 72u: goto tr203;
		case 73u: goto tr204;
		case 77u: goto tr205;
		case 80u: goto tr203;
		case 81u: goto tr204;
		case 85u: goto tr205;
		case 88u: goto tr203;
		case 89u: goto tr204;
		case 93u: goto tr205;
		case 96u: goto tr203;
		case 97u: goto tr204;
		case 101u: goto tr205;
		case 104u: goto tr203;
		case 105u: goto tr204;
		case 109u: goto tr205;
		case 112u: goto tr203;
		case 113u: goto tr204;
		case 117u: goto tr205;
		case 120u: goto tr203;
		case 121u: goto tr206;
		case 125u: goto tr207;
		case 193u: goto tr208;
		case 197u: goto tr209;
		case 201u: goto tr208;
		case 205u: goto tr209;
		case 209u: goto tr208;
		case 213u: goto tr209;
		case 217u: goto tr208;
		case 221u: goto tr209;
		case 225u: goto tr208;
		case 229u: goto tr209;
		case 233u: goto tr208;
		case 237u: goto tr209;
		case 241u: goto tr208;
		case 245u: goto tr209;
		case 249u: goto tr208;
		case 253u: goto tr209;
	}
	goto tr19;
tr203:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st164;
st164:
	if ( ++p == pe )
		goto _test_eof164;
case 164:
#line 4429 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 242u: goto st1;
		case 243u: goto st165;
		case 247u: goto st1;
	}
	goto tr19;
st165:
	if ( ++p == pe )
		goto _test_eof165;
case 165:
	switch( (*p) ) {
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
	}
	if ( (*p) < 72u ) {
		if ( 8u <= (*p) && (*p) <= 31u )
			goto tr0;
	} else if ( (*p) > 95u ) {
		if ( (*p) > 159u ) {
			if ( 200u <= (*p) && (*p) <= 223u )
				goto tr0;
		} else if ( (*p) >= 136u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr204:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st166;
st166:
	if ( ++p == pe )
		goto _test_eof166;
case 166:
#line 4476 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) == 43u )
		goto st1;
	if ( (*p) < 55u ) {
		if ( (*p) < 40u ) {
			if ( (*p) <= 13u )
				goto st1;
		} else if ( (*p) > 41u ) {
			if ( 44u <= (*p) && (*p) <= 47u )
				goto st29;
		} else
			goto st1;
	} else if ( (*p) > 64u ) {
		if ( (*p) < 166u ) {
			if ( 150u <= (*p) && (*p) <= 159u )
				goto st1;
		} else if ( (*p) > 175u ) {
			if ( (*p) > 191u ) {
				if ( 219u <= (*p) && (*p) <= 223u )
					goto st1;
			} else if ( (*p) >= 182u )
				goto st1;
		} else
			goto st1;
	} else
		goto st1;
	goto tr19;
tr205:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st167;
st167:
	if ( ++p == pe )
		goto _test_eof167;
case 167:
#line 4513 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 154u: goto st1;
		case 156u: goto st1;
		case 158u: goto st1;
		case 170u: goto st1;
		case 172u: goto st1;
		case 174u: goto st1;
		case 186u: goto st1;
		case 188u: goto st1;
		case 190u: goto st1;
	}
	if ( (*p) < 150u ) {
		if ( (*p) > 13u ) {
			if ( 44u <= (*p) && (*p) <= 47u )
				goto st29;
		} else if ( (*p) >= 12u )
			goto st1;
	} else if ( (*p) > 152u ) {
		if ( (*p) > 168u ) {
			if ( 182u <= (*p) && (*p) <= 184u )
				goto st1;
		} else if ( (*p) >= 166u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr206:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st168;
st168:
	if ( ++p == pe )
		goto _test_eof168;
case 168:
#line 4550 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 19u: goto st1;
		case 23u: goto st1;
		case 24u: goto st29;
		case 42u: goto st29;
	}
	if ( (*p) < 48u ) {
		if ( (*p) < 32u ) {
			if ( (*p) > 15u ) {
				if ( 28u <= (*p) && (*p) <= 30u )
					goto st1;
			} else
				goto st1;
		} else if ( (*p) > 37u ) {
			if ( (*p) > 43u ) {
				if ( 44u <= (*p) && (*p) <= 47u )
					goto st29;
			} else if ( (*p) >= 40u )
				goto st1;
		} else
			goto st1;
	} else if ( (*p) > 53u ) {
		if ( (*p) < 166u ) {
			if ( (*p) > 65u ) {
				if ( 150u <= (*p) && (*p) <= 159u )
					goto st1;
			} else if ( (*p) >= 55u )
				goto st1;
		} else if ( (*p) > 175u ) {
			if ( (*p) > 191u ) {
				if ( 219u <= (*p) && (*p) <= 223u )
					goto st1;
			} else if ( (*p) >= 182u )
				goto st1;
		} else
			goto st1;
	} else
		goto st1;
	goto tr19;
tr207:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st169;
st169:
	if ( ++p == pe )
		goto _test_eof169;
case 169:
#line 4600 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 19u: goto st1;
		case 23u: goto st1;
		case 154u: goto st1;
		case 156u: goto st1;
		case 158u: goto st1;
		case 170u: goto st1;
		case 172u: goto st1;
		case 174u: goto st1;
		case 186u: goto st1;
		case 188u: goto st1;
		case 190u: goto st1;
	}
	if ( (*p) < 44u ) {
		if ( (*p) > 15u ) {
			if ( 24u <= (*p) && (*p) <= 26u )
				goto st29;
		} else if ( (*p) >= 12u )
			goto st1;
	} else if ( (*p) > 47u ) {
		if ( (*p) < 166u ) {
			if ( 150u <= (*p) && (*p) <= 152u )
				goto st1;
		} else if ( (*p) > 168u ) {
			if ( 182u <= (*p) && (*p) <= 184u )
				goto st1;
		} else
			goto st1;
	} else
		goto st29;
	goto tr19;
tr208:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st170;
st170:
	if ( ++p == pe )
		goto _test_eof170;
case 170:
#line 4642 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) < 166u ) {
		if ( 150u <= (*p) && (*p) <= 159u )
			goto st1;
	} else if ( (*p) > 175u ) {
		if ( 182u <= (*p) && (*p) <= 191u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr209:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st171;
st171:
	if ( ++p == pe )
		goto _test_eof171;
case 171:
#line 4662 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 154u: goto st1;
		case 156u: goto st1;
		case 158u: goto st1;
		case 170u: goto st1;
		case 172u: goto st1;
		case 174u: goto st1;
		case 186u: goto st1;
		case 188u: goto st1;
		case 190u: goto st1;
	}
	if ( (*p) < 166u ) {
		if ( 150u <= (*p) && (*p) <= 152u )
			goto st1;
	} else if ( (*p) > 168u ) {
		if ( 182u <= (*p) && (*p) <= 184u )
			goto st1;
	} else
		goto st1;
	goto tr19;
st172:
	if ( ++p == pe )
		goto _test_eof172;
case 172:
	switch( (*p) ) {
		case 65u: goto tr211;
		case 69u: goto tr212;
		case 73u: goto tr211;
		case 77u: goto tr212;
		case 81u: goto tr211;
		case 85u: goto tr212;
		case 89u: goto tr211;
		case 93u: goto tr212;
		case 97u: goto tr211;
		case 101u: goto tr212;
		case 105u: goto tr211;
		case 109u: goto tr212;
		case 113u: goto tr211;
		case 117u: goto tr212;
		case 121u: goto tr213;
		case 125u: goto tr214;
		case 193u: goto tr215;
		case 197u: goto tr216;
		case 201u: goto tr215;
		case 205u: goto tr216;
		case 209u: goto tr215;
		case 213u: goto tr216;
		case 217u: goto tr215;
		case 221u: goto tr216;
		case 225u: goto tr215;
		case 229u: goto tr216;
		case 233u: goto tr215;
		case 237u: goto tr216;
		case 241u: goto tr215;
		case 245u: goto tr216;
		case 249u: goto tr215;
		case 253u: goto tr216;
	}
	goto tr19;
tr211:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st173;
st173:
	if ( ++p == pe )
		goto _test_eof173;
case 173:
#line 4732 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 33u: goto st33;
		case 68u: goto st33;
		case 223u: goto st33;
	}
	if ( (*p) < 74u ) {
		if ( (*p) < 64u ) {
			if ( 8u <= (*p) && (*p) <= 15u )
				goto st33;
		} else if ( (*p) > 66u ) {
			if ( 72u <= (*p) && (*p) <= 73u )
				goto st174;
		} else
			goto st33;
	} else if ( (*p) > 76u ) {
		if ( (*p) < 104u ) {
			if ( 92u <= (*p) && (*p) <= 95u )
				goto st119;
		} else if ( (*p) > 111u ) {
			if ( 120u <= (*p) && (*p) <= 127u )
				goto st119;
		} else
			goto st119;
	} else
		goto st119;
	goto tr19;
st174:
	if ( ++p == pe )
		goto _test_eof174;
case 174:
	switch( (*p) ) {
		case 4u: goto st176;
		case 5u: goto st177;
		case 12u: goto st176;
		case 13u: goto st177;
		case 20u: goto st176;
		case 21u: goto st177;
		case 28u: goto st176;
		case 29u: goto st177;
		case 36u: goto st176;
		case 37u: goto st177;
		case 44u: goto st176;
		case 45u: goto st177;
		case 52u: goto st176;
		case 53u: goto st177;
		case 60u: goto st176;
		case 61u: goto st177;
		case 68u: goto st182;
		case 76u: goto st182;
		case 84u: goto st182;
		case 92u: goto st182;
		case 100u: goto st182;
		case 108u: goto st182;
		case 116u: goto st182;
		case 124u: goto st182;
		case 132u: goto st183;
		case 140u: goto st183;
		case 148u: goto st183;
		case 156u: goto st183;
		case 164u: goto st183;
		case 172u: goto st183;
		case 180u: goto st183;
		case 188u: goto st183;
	}
	if ( (*p) > 127u ) {
		if ( 128u <= (*p) && (*p) <= 191u )
			goto st177;
	} else if ( (*p) >= 64u )
		goto st181;
	goto st175;
tr228:
#line 46 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP32;
    disp = p - 3;
  }
	goto st175;
tr229:
#line 42 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    disp_type = DISP8;
    disp = p;
  }
	goto st175;
st175:
	if ( ++p == pe )
		goto _test_eof175;
case 175:
#line 4821 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) < 48u ) {
		if ( (*p) < 16u ) {
			if ( (*p) <= 3u )
				goto tr224;
		} else if ( (*p) > 19u ) {
			if ( 32u <= (*p) && (*p) <= 35u )
				goto tr224;
		} else
			goto tr224;
	} else if ( (*p) > 51u ) {
		if ( (*p) < 80u ) {
			if ( 64u <= (*p) && (*p) <= 67u )
				goto tr224;
		} else if ( (*p) > 83u ) {
			if ( (*p) > 99u ) {
				if ( 112u <= (*p) && (*p) <= 115u )
					goto tr224;
			} else if ( (*p) >= 96u )
				goto tr224;
		} else
			goto tr224;
	} else
		goto tr224;
	goto tr19;
st176:
	if ( ++p == pe )
		goto _test_eof176;
case 176:
	switch( (*p) ) {
		case 5u: goto st177;
		case 13u: goto st177;
		case 21u: goto st177;
		case 29u: goto st177;
		case 37u: goto st177;
		case 45u: goto st177;
		case 53u: goto st177;
		case 61u: goto st177;
		case 69u: goto st177;
		case 77u: goto st177;
		case 85u: goto st177;
		case 93u: goto st177;
		case 101u: goto st177;
		case 109u: goto st177;
		case 117u: goto st177;
		case 125u: goto st177;
		case 133u: goto st177;
		case 141u: goto st177;
		case 149u: goto st177;
		case 157u: goto st177;
		case 165u: goto st177;
		case 173u: goto st177;
		case 181u: goto st177;
		case 189u: goto st177;
		case 197u: goto st177;
		case 205u: goto st177;
		case 213u: goto st177;
		case 221u: goto st177;
		case 229u: goto st177;
		case 237u: goto st177;
		case 245u: goto st177;
		case 253u: goto st177;
	}
	goto st175;
st177:
	if ( ++p == pe )
		goto _test_eof177;
case 177:
	goto st178;
st178:
	if ( ++p == pe )
		goto _test_eof178;
case 178:
	goto st179;
st179:
	if ( ++p == pe )
		goto _test_eof179;
case 179:
	goto st180;
st180:
	if ( ++p == pe )
		goto _test_eof180;
case 180:
	goto tr228;
st181:
	if ( ++p == pe )
		goto _test_eof181;
case 181:
	goto tr229;
st182:
	if ( ++p == pe )
		goto _test_eof182;
case 182:
	goto st181;
st183:
	if ( ++p == pe )
		goto _test_eof183;
case 183:
	goto st177;
tr212:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st184;
st184:
	if ( ++p == pe )
		goto _test_eof184;
case 184:
#line 4930 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 6u: goto st33;
		case 64u: goto st33;
	}
	if ( (*p) < 92u ) {
		if ( (*p) < 12u ) {
			if ( 8u <= (*p) && (*p) <= 9u )
				goto st33;
		} else if ( (*p) > 13u ) {
			if ( (*p) > 73u ) {
				if ( 74u <= (*p) && (*p) <= 75u )
					goto st119;
			} else if ( (*p) >= 72u )
				goto st174;
		} else
			goto st33;
	} else if ( (*p) > 95u ) {
		if ( (*p) < 108u ) {
			if ( 104u <= (*p) && (*p) <= 105u )
				goto st119;
		} else if ( (*p) > 109u ) {
			if ( (*p) > 121u ) {
				if ( 124u <= (*p) && (*p) <= 125u )
					goto st119;
			} else if ( (*p) >= 120u )
				goto st119;
		} else
			goto st119;
	} else
		goto st119;
	goto tr19;
tr213:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st185;
st185:
	if ( ++p == pe )
		goto _test_eof185;
case 185:
#line 4972 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 23u: goto st186;
		case 29u: goto st33;
		case 68u: goto st33;
		case 223u: goto st33;
	}
	if ( (*p) < 72u ) {
		if ( (*p) < 20u ) {
			if ( (*p) > 5u ) {
				if ( 8u <= (*p) && (*p) <= 15u )
					goto st33;
			} else if ( (*p) >= 4u )
				goto st33;
		} else if ( (*p) > 22u ) {
			if ( (*p) > 34u ) {
				if ( 64u <= (*p) && (*p) <= 66u )
					goto st33;
			} else if ( (*p) >= 32u )
				goto st33;
		} else
			goto st33;
	} else if ( (*p) > 73u ) {
		if ( (*p) < 96u ) {
			if ( (*p) > 76u ) {
				if ( 92u <= (*p) && (*p) <= 95u )
					goto st119;
			} else if ( (*p) >= 74u )
				goto st119;
		} else if ( (*p) > 99u ) {
			if ( (*p) > 111u ) {
				if ( 120u <= (*p) && (*p) <= 127u )
					goto st119;
			} else if ( (*p) >= 104u )
				goto st119;
		} else
			goto st33;
	} else
		goto st174;
	goto tr19;
st186:
	if ( ++p == pe )
		goto _test_eof186;
case 186:
	switch( (*p) ) {
		case 4u: goto st34;
		case 12u: goto st34;
		case 20u: goto st34;
		case 28u: goto st34;
		case 36u: goto st34;
		case 44u: goto st34;
		case 52u: goto st34;
		case 60u: goto st34;
		case 68u: goto st40;
		case 76u: goto st40;
		case 84u: goto st40;
		case 92u: goto st40;
		case 100u: goto st40;
		case 108u: goto st40;
		case 116u: goto st40;
		case 124u: goto st40;
		case 132u: goto st41;
		case 140u: goto st41;
		case 148u: goto st41;
		case 156u: goto st41;
		case 164u: goto st41;
		case 172u: goto st41;
		case 180u: goto st41;
		case 188u: goto st41;
	}
	if ( (*p) < 38u ) {
		if ( (*p) < 14u ) {
			if ( (*p) > 3u ) {
				if ( 6u <= (*p) && (*p) <= 11u )
					goto st10;
			} else
				goto st10;
		} else if ( (*p) > 19u ) {
			if ( (*p) > 27u ) {
				if ( 30u <= (*p) && (*p) <= 35u )
					goto st10;
			} else if ( (*p) >= 22u )
				goto st10;
		} else
			goto st10;
	} else if ( (*p) > 43u ) {
		if ( (*p) < 62u ) {
			if ( (*p) > 51u ) {
				if ( 54u <= (*p) && (*p) <= 59u )
					goto st10;
			} else if ( (*p) >= 46u )
				goto st10;
		} else if ( (*p) > 63u ) {
			if ( (*p) > 127u ) {
				if ( 192u <= (*p) )
					goto tr19;
			} else if ( (*p) >= 64u )
				goto st39;
		} else
			goto st10;
	} else
		goto st10;
	goto st35;
tr214:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st187;
st187:
	if ( ++p == pe )
		goto _test_eof187;
case 187:
#line 5085 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 29u: goto st33;
		case 64u: goto st33;
	}
	if ( (*p) < 74u ) {
		if ( (*p) < 12u ) {
			if ( (*p) > 6u ) {
				if ( 8u <= (*p) && (*p) <= 9u )
					goto st33;
			} else if ( (*p) >= 4u )
				goto st33;
		} else if ( (*p) > 13u ) {
			if ( (*p) > 25u ) {
				if ( 72u <= (*p) && (*p) <= 73u )
					goto st174;
			} else if ( (*p) >= 24u )
				goto st33;
		} else
			goto st33;
	} else if ( (*p) > 75u ) {
		if ( (*p) < 108u ) {
			if ( (*p) > 95u ) {
				if ( 104u <= (*p) && (*p) <= 105u )
					goto st119;
			} else if ( (*p) >= 92u )
				goto st119;
		} else if ( (*p) > 109u ) {
			if ( (*p) > 121u ) {
				if ( 124u <= (*p) && (*p) <= 125u )
					goto st119;
			} else if ( (*p) >= 120u )
				goto st119;
		} else
			goto st119;
	} else
		goto st119;
	goto tr19;
tr215:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st188;
st188:
	if ( ++p == pe )
		goto _test_eof188;
case 188:
#line 5133 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) < 92u ) {
		if ( 72u <= (*p) && (*p) <= 73u )
			goto st174;
	} else if ( (*p) > 95u ) {
		if ( (*p) > 111u ) {
			if ( 120u <= (*p) && (*p) <= 127u )
				goto st119;
		} else if ( (*p) >= 104u )
			goto st119;
	} else
		goto st119;
	goto tr19;
tr216:
#line 161 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    vex_prefix3 = *p;
  }
	goto st189;
st189:
	if ( ++p == pe )
		goto _test_eof189;
case 189:
#line 5156 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( (*p) < 104u ) {
		if ( (*p) > 73u ) {
			if ( 92u <= (*p) && (*p) <= 95u )
				goto st119;
		} else if ( (*p) >= 72u )
			goto st174;
	} else if ( (*p) > 105u ) {
		if ( (*p) < 120u ) {
			if ( 108u <= (*p) && (*p) <= 109u )
				goto st119;
		} else if ( (*p) > 121u ) {
			if ( 124u <= (*p) && (*p) <= 125u )
				goto st119;
		} else
			goto st119;
	} else
		goto st119;
	goto tr19;
tr293:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st190;
st190:
	if ( ++p == pe )
		goto _test_eof190;
case 190:
#line 5186 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 193u: goto tr232;
		case 194u: goto tr233;
		case 195u: goto tr234;
		case 196u: goto tr235;
		case 197u: goto tr236;
		case 198u: goto tr237;
		case 199u: goto tr238;
		case 201u: goto tr232;
		case 202u: goto tr233;
		case 203u: goto tr234;
		case 204u: goto tr235;
		case 205u: goto tr236;
		case 206u: goto tr237;
		case 207u: goto tr238;
		case 209u: goto tr232;
		case 210u: goto tr233;
		case 211u: goto tr234;
		case 212u: goto tr235;
		case 213u: goto tr236;
		case 214u: goto tr237;
		case 215u: goto tr238;
		case 217u: goto tr232;
		case 218u: goto tr233;
		case 219u: goto tr234;
		case 220u: goto tr235;
		case 221u: goto tr236;
		case 222u: goto tr237;
		case 223u: goto tr238;
		case 225u: goto tr232;
		case 226u: goto tr233;
		case 227u: goto tr234;
		case 228u: goto tr235;
		case 229u: goto tr236;
		case 230u: goto tr237;
		case 231u: goto tr238;
		case 233u: goto tr232;
		case 234u: goto tr233;
		case 235u: goto tr234;
		case 236u: goto tr235;
		case 237u: goto tr236;
		case 238u: goto tr237;
		case 239u: goto tr238;
		case 241u: goto tr232;
		case 242u: goto tr233;
		case 243u: goto tr234;
		case 244u: goto tr235;
		case 245u: goto tr236;
		case 246u: goto tr237;
		case 247u: goto tr238;
		case 248u: goto tr239;
		case 249u: goto tr240;
		case 250u: goto tr241;
		case 251u: goto tr242;
		case 252u: goto tr243;
		case 253u: goto tr244;
		case 254u: goto tr245;
		case 255u: goto tr246;
	}
	if ( 192u <= (*p) && (*p) <= 240u )
		goto tr231;
	goto tr19;
tr233:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st191;
st191:
	if ( ++p == pe )
		goto _test_eof191;
case 191:
#line 5260 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 81u: goto st1;
		case 83u: goto st1;
		case 194u: goto st33;
	}
	if ( (*p) > 90u ) {
		if ( 92u <= (*p) && (*p) <= 95u )
			goto st1;
	} else if ( (*p) >= 88u )
		goto st1;
	goto tr19;
tr234:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st192;
st192:
	if ( ++p == pe )
		goto _test_eof192;
case 192:
#line 5283 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 81u: goto st1;
		case 194u: goto st33;
		case 208u: goto st1;
	}
	if ( (*p) < 92u ) {
		if ( 88u <= (*p) && (*p) <= 90u )
			goto st1;
	} else if ( (*p) > 95u ) {
		if ( 124u <= (*p) && (*p) <= 125u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr241:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st193;
st193:
	if ( ++p == pe )
		goto _test_eof193;
case 193:
#line 5309 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 22u: goto st1;
		case 111u: goto st1;
		case 112u: goto st33;
		case 194u: goto st33;
		case 230u: goto st1;
	}
	if ( (*p) < 81u ) {
		if ( 16u <= (*p) && (*p) <= 17u )
			goto st29;
	} else if ( (*p) > 83u ) {
		if ( (*p) > 95u ) {
			if ( 126u <= (*p) && (*p) <= 127u )
				goto st1;
		} else if ( (*p) >= 88u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr242:
#line 165 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    /* VEX.R is not used in ia32 mode.  */
    vex_prefix3 = p[0] & 0x7f;
  }
	goto st194;
st194:
	if ( ++p == pe )
		goto _test_eof194;
case 194:
#line 5341 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 18u: goto st1;
		case 81u: goto st1;
		case 112u: goto st33;
		case 194u: goto st33;
		case 208u: goto st1;
		case 230u: goto st1;
		case 240u: goto st29;
	}
	if ( (*p) < 88u ) {
		if ( 16u <= (*p) && (*p) <= 17u )
			goto st29;
	} else if ( (*p) > 90u ) {
		if ( (*p) > 95u ) {
			if ( 124u <= (*p) && (*p) <= 125u )
				goto st1;
		} else if ( (*p) >= 92u )
			goto st1;
	} else
		goto st1;
	goto tr19;
tr294:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st195;
st195:
	if ( ++p == pe )
		goto _test_eof195;
case 195:
#line 5374 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 68u: goto st40;
		case 132u: goto st41;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto st10;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto st10;
		} else if ( (*p) >= 128u )
			goto st35;
	} else
		goto st39;
	goto tr19;
tr295:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st196;
st196:
	if ( ++p == pe )
		goto _test_eof196;
case 196:
#line 5404 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st105;
		case 5u: goto st106;
		case 68u: goto st111;
		case 132u: goto st112;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 7u )
			goto st11;
	} else if ( (*p) > 71u ) {
		if ( (*p) > 135u ) {
			if ( 192u <= (*p) && (*p) <= 199u )
				goto st11;
		} else if ( (*p) >= 128u )
			goto st106;
	} else
		goto st110;
	goto tr19;
tr296:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st197;
st197:
	if ( ++p == pe )
		goto _test_eof197;
case 197:
#line 5434 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	goto st198;
st198:
	if ( ++p == pe )
		goto _test_eof198;
case 198:
	goto tr248;
tr299:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st199;
st199:
	if ( ++p == pe )
		goto _test_eof199;
case 199:
#line 5452 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
		case 239u: goto tr19;
	}
	if ( (*p) < 128u ) {
		if ( (*p) < 64u ) {
			if ( 8u <= (*p) && (*p) <= 15u )
				goto tr19;
		} else if ( (*p) > 71u ) {
			if ( (*p) > 79u ) {
				if ( 80u <= (*p) && (*p) <= 127u )
					goto st7;
			} else if ( (*p) >= 72u )
				goto tr19;
		} else
			goto st7;
	} else if ( (*p) > 135u ) {
		if ( (*p) < 209u ) {
			if ( (*p) > 143u ) {
				if ( 144u <= (*p) && (*p) <= 191u )
					goto st3;
			} else if ( (*p) >= 136u )
				goto tr19;
		} else if ( (*p) > 223u ) {
			if ( (*p) > 227u ) {
				if ( 229u <= (*p) && (*p) <= 231u )
					goto tr19;
			} else if ( (*p) >= 226u )
				goto tr19;
		} else
			goto tr19;
	} else
		goto st3;
	goto tr0;
tr300:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st200;
st200:
	if ( ++p == pe )
		goto _test_eof200;
case 200:
#line 5525 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 128u ) {
		if ( 64u <= (*p) && (*p) <= 127u )
			goto st7;
	} else if ( (*p) > 191u ) {
		if ( (*p) > 232u ) {
			if ( 234u <= (*p) )
				goto tr19;
		} else if ( (*p) >= 224u )
			goto tr19;
	} else
		goto st3;
	goto tr0;
tr301:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st201;
st201:
	if ( ++p == pe )
		goto _test_eof201;
case 201:
#line 5583 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 108u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 172u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 128u ) {
		if ( (*p) < 96u ) {
			if ( (*p) < 48u ) {
				if ( 32u <= (*p) && (*p) <= 39u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( 64u <= (*p) && (*p) <= 95u )
					goto st7;
			} else
				goto tr19;
		} else if ( (*p) > 103u ) {
			if ( (*p) < 112u ) {
				if ( 104u <= (*p) && (*p) <= 111u )
					goto st7;
			} else if ( (*p) > 119u ) {
				if ( 120u <= (*p) && (*p) <= 127u )
					goto st7;
			} else
				goto tr19;
		} else
			goto tr19;
	} else if ( (*p) > 159u ) {
		if ( (*p) < 184u ) {
			if ( (*p) < 168u ) {
				if ( 160u <= (*p) && (*p) <= 167u )
					goto tr19;
			} else if ( (*p) > 175u ) {
				if ( 176u <= (*p) && (*p) <= 183u )
					goto tr19;
			} else
				goto st3;
		} else if ( (*p) > 191u ) {
			if ( (*p) < 228u ) {
				if ( 224u <= (*p) && (*p) <= 225u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( 248u <= (*p) )
					goto tr19;
			} else
				goto tr19;
		} else
			goto st3;
	} else
		goto st3;
	goto tr0;
tr302:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st202;
st202:
	if ( ++p == pe )
		goto _test_eof202;
case 202:
#line 5666 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 128u ) {
		if ( 64u <= (*p) && (*p) <= 127u )
			goto st7;
	} else if ( (*p) > 191u ) {
		if ( 208u <= (*p) && (*p) <= 223u )
			goto tr19;
	} else
		goto st3;
	goto tr0;
tr303:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st203;
st203:
	if ( ++p == pe )
		goto _test_eof203;
case 203:
#line 5721 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 128u ) {
		if ( (*p) < 64u ) {
			if ( 40u <= (*p) && (*p) <= 47u )
				goto tr19;
		} else if ( (*p) > 103u ) {
			if ( (*p) > 111u ) {
				if ( 112u <= (*p) && (*p) <= 127u )
					goto st7;
			} else if ( (*p) >= 104u )
				goto tr19;
		} else
			goto st7;
	} else if ( (*p) > 167u ) {
		if ( (*p) < 176u ) {
			if ( 168u <= (*p) && (*p) <= 175u )
				goto tr19;
		} else if ( (*p) > 191u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) )
					goto tr19;
			} else if ( (*p) >= 200u )
				goto tr19;
		} else
			goto st3;
	} else
		goto st3;
	goto tr0;
tr304:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st204;
st204:
	if ( ++p == pe )
		goto _test_eof204;
case 204:
#line 5790 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 128u ) {
		if ( 64u <= (*p) && (*p) <= 127u )
			goto st7;
	} else if ( (*p) > 191u ) {
		if ( (*p) > 216u ) {
			if ( 218u <= (*p) && (*p) <= 223u )
				goto tr19;
		} else if ( (*p) >= 208u )
			goto tr19;
	} else
		goto st3;
	goto tr0;
tr305:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st205;
st205:
	if ( ++p == pe )
		goto _test_eof205;
case 205:
#line 5848 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 192u ) {
		if ( (*p) > 127u ) {
			if ( 128u <= (*p) && (*p) <= 191u )
				goto st3;
		} else if ( (*p) >= 64u )
			goto st7;
	} else if ( (*p) > 223u ) {
		if ( (*p) > 231u ) {
			if ( 248u <= (*p) )
				goto tr19;
		} else if ( (*p) >= 225u )
			goto tr19;
	} else
		goto tr19;
	goto tr0;
tr307:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 13 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    lock_prefix = TRUE;
  }
	goto st206;
st206:
	if ( ++p == pe )
		goto _test_eof206;
case 206:
#line 5913 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 15u: goto st207;
		case 102u: goto tr250;
		case 128u: goto st95;
		case 129u: goto st208;
		case 131u: goto st95;
	}
	if ( (*p) < 32u ) {
		if ( (*p) < 8u ) {
			if ( (*p) <= 3u )
				goto st29;
		} else if ( (*p) > 11u ) {
			if ( (*p) > 19u ) {
				if ( 24u <= (*p) && (*p) <= 27u )
					goto st29;
			} else if ( (*p) >= 16u )
				goto st29;
		} else
			goto st29;
	} else if ( (*p) > 35u ) {
		if ( (*p) < 134u ) {
			if ( (*p) > 43u ) {
				if ( 48u <= (*p) && (*p) <= 51u )
					goto st29;
			} else if ( (*p) >= 40u )
				goto st29;
		} else if ( (*p) > 135u ) {
			if ( (*p) > 247u ) {
				if ( 254u <= (*p) )
					goto st18;
			} else if ( (*p) >= 246u )
				goto st96;
		} else
			goto st29;
	} else
		goto st29;
	goto tr19;
st207:
	if ( ++p == pe )
		goto _test_eof207;
case 207:
	if ( (*p) == 199u )
		goto st50;
	if ( (*p) > 177u ) {
		if ( 192u <= (*p) && (*p) <= 193u )
			goto st29;
	} else if ( (*p) >= 176u )
		goto st29;
	goto tr19;
st208:
	if ( ++p == pe )
		goto _test_eof208;
case 208:
	switch( (*p) ) {
		case 4u: goto st105;
		case 5u: goto st106;
		case 12u: goto st105;
		case 13u: goto st106;
		case 20u: goto st105;
		case 21u: goto st106;
		case 28u: goto st105;
		case 29u: goto st106;
		case 36u: goto st105;
		case 37u: goto st106;
		case 44u: goto st105;
		case 45u: goto st106;
		case 52u: goto st105;
		case 53u: goto st106;
		case 68u: goto st111;
		case 76u: goto st111;
		case 84u: goto st111;
		case 92u: goto st111;
		case 100u: goto st111;
		case 108u: goto st111;
		case 116u: goto st111;
		case 132u: goto st112;
		case 140u: goto st112;
		case 148u: goto st112;
		case 156u: goto st112;
		case 164u: goto st112;
		case 172u: goto st112;
		case 180u: goto st112;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 55u )
			goto st11;
	} else if ( (*p) > 119u ) {
		if ( 128u <= (*p) && (*p) <= 183u )
			goto st106;
	} else
		goto st110;
	goto tr19;
tr308:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 22 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = TRUE;
  }
	goto st209;
st209:
	if ( ++p == pe )
		goto _test_eof209;
case 209:
#line 6021 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 15u: goto st210;
		case 102u: goto tr253;
	}
	if ( (*p) > 167u ) {
		if ( 174u <= (*p) && (*p) <= 175u )
			goto tr0;
	} else if ( (*p) >= 166u )
		goto tr0;
	goto tr19;
st210:
	if ( ++p == pe )
		goto _test_eof210;
case 210:
	switch( (*p) ) {
		case 43u: goto tr254;
		case 56u: goto st211;
		case 81u: goto tr130;
		case 112u: goto tr256;
		case 120u: goto tr257;
		case 121u: goto tr258;
		case 194u: goto tr256;
		case 208u: goto tr130;
		case 214u: goto tr258;
		case 230u: goto tr130;
		case 240u: goto tr254;
	}
	if ( (*p) < 88u ) {
		if ( (*p) > 18u ) {
			if ( 42u <= (*p) && (*p) <= 45u )
				goto tr130;
		} else if ( (*p) >= 16u )
			goto tr130;
	} else if ( (*p) > 90u ) {
		if ( (*p) > 95u ) {
			if ( 124u <= (*p) && (*p) <= 125u )
				goto tr130;
		} else if ( (*p) >= 92u )
			goto tr130;
	} else
		goto tr130;
	goto tr19;
st211:
	if ( ++p == pe )
		goto _test_eof211;
case 211:
	if ( 240u <= (*p) && (*p) <= 241u )
		goto tr130;
	goto tr19;
tr257:
#line 36 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repnz_prefix = FALSE;
  }
	goto st212;
st212:
	if ( ++p == pe )
		goto _test_eof212;
case 212:
#line 6081 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	if ( 192u <= (*p) )
		goto st213;
	goto tr19;
st213:
	if ( ++p == pe )
		goto _test_eof213;
case 213:
	goto tr260;
tr309:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
#line 19 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = TRUE;
  }
#line 16 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
    repz_prefix = TRUE;
  }
	goto st214;
st214:
	if ( ++p == pe )
		goto _test_eof214;
case 214:
#line 6109 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 15u: goto st215;
		case 102u: goto tr262;
		case 144u: goto tr263;
	}
	if ( (*p) > 167u ) {
		if ( 170u <= (*p) && (*p) <= 175u )
			goto tr0;
	} else if ( (*p) >= 164u )
		goto tr0;
	goto tr19;
st215:
	if ( ++p == pe )
		goto _test_eof215;
case 215:
	switch( (*p) ) {
		case 22u: goto tr132;
		case 43u: goto tr264;
		case 111u: goto tr132;
		case 112u: goto tr265;
		case 184u: goto tr132;
		case 194u: goto tr265;
		case 214u: goto tr266;
		case 230u: goto tr132;
	}
	if ( (*p) < 81u ) {
		if ( (*p) > 18u ) {
			if ( 42u <= (*p) && (*p) <= 45u )
				goto tr132;
		} else if ( (*p) >= 16u )
			goto tr132;
	} else if ( (*p) > 83u ) {
		if ( (*p) < 126u ) {
			if ( 88u <= (*p) && (*p) <= 95u )
				goto tr132;
		} else if ( (*p) > 127u ) {
			if ( 188u <= (*p) && (*p) <= 189u )
				goto tr132;
		} else
			goto tr132;
	} else
		goto tr132;
	goto tr19;
tr310:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st216;
st216:
	if ( ++p == pe )
		goto _test_eof216;
case 216:
#line 6164 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st40;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st41;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 80u ) {
		if ( (*p) < 8u ) {
			if ( (*p) <= 7u )
				goto st10;
		} else if ( (*p) > 15u ) {
			if ( (*p) > 71u ) {
				if ( 72u <= (*p) && (*p) <= 79u )
					goto tr19;
			} else if ( (*p) >= 64u )
				goto st39;
		} else
			goto tr19;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 144u ) {
			if ( (*p) > 135u ) {
				if ( 136u <= (*p) && (*p) <= 143u )
					goto tr19;
			} else if ( (*p) >= 128u )
				goto st35;
		} else if ( (*p) > 191u ) {
			if ( (*p) > 199u ) {
				if ( 200u <= (*p) && (*p) <= 207u )
					goto tr19;
			} else if ( (*p) >= 192u )
				goto st10;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr311:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st217;
st217:
	if ( ++p == pe )
		goto _test_eof217;
case 217:
#line 6236 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st105;
		case 5u: goto st106;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st111;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st112;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*p) < 80u ) {
		if ( (*p) < 8u ) {
			if ( (*p) <= 7u )
				goto st11;
		} else if ( (*p) > 15u ) {
			if ( (*p) > 71u ) {
				if ( 72u <= (*p) && (*p) <= 79u )
					goto tr19;
			} else if ( (*p) >= 64u )
				goto st110;
		} else
			goto tr19;
	} else if ( (*p) > 127u ) {
		if ( (*p) < 144u ) {
			if ( (*p) > 135u ) {
				if ( 136u <= (*p) && (*p) <= 143u )
					goto tr19;
			} else if ( (*p) >= 128u )
				goto st106;
		} else if ( (*p) > 191u ) {
			if ( (*p) > 199u ) {
				if ( 200u <= (*p) && (*p) <= 207u )
					goto tr19;
			} else if ( (*p) >= 192u )
				goto st11;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr312:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st218;
st218:
	if ( ++p == pe )
		goto _test_eof218;
case 218:
#line 6308 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*p) < 64u ) {
		if ( (*p) <= 15u )
			goto tr0;
	} else if ( (*p) > 79u ) {
		if ( (*p) > 143u ) {
			if ( 192u <= (*p) && (*p) <= 207u )
				goto tr0;
		} else if ( (*p) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr19;
tr314:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st219;
st219:
	if ( ++p == pe )
		goto _test_eof219;
case 219:
#line 6342 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 208u: goto tr267;
		case 224u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st220:
	if ( ++p == pe )
		goto _test_eof220;
case 220:
	if ( (*p) == 224u )
		goto tr268;
	goto tr11;
tr268:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st237;
st237:
	if ( ++p == pe )
		goto _test_eof237;
case 237:
#line 6405 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr315;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr315:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st221;
st221:
	if ( ++p == pe )
		goto _test_eof221;
case 221:
#line 6558 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 209u: goto tr267;
		case 225u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st222:
	if ( ++p == pe )
		goto _test_eof222;
case 222:
	if ( (*p) == 224u )
		goto tr269;
	goto tr11;
tr269:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st238;
st238:
	if ( ++p == pe )
		goto _test_eof238;
case 238:
#line 6621 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr316;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr316:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st223;
st223:
	if ( ++p == pe )
		goto _test_eof223;
case 223:
#line 6774 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 210u: goto tr267;
		case 226u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st224:
	if ( ++p == pe )
		goto _test_eof224;
case 224:
	if ( (*p) == 224u )
		goto tr270;
	goto tr11;
tr270:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st239;
st239:
	if ( ++p == pe )
		goto _test_eof239;
case 239:
#line 6837 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr317;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr317:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st225;
st225:
	if ( ++p == pe )
		goto _test_eof225;
case 225:
#line 6990 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 211u: goto tr267;
		case 227u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st226:
	if ( ++p == pe )
		goto _test_eof226;
case 226:
	if ( (*p) == 224u )
		goto tr271;
	goto tr11;
tr271:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st240;
st240:
	if ( ++p == pe )
		goto _test_eof240;
case 240:
#line 7053 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr318;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr318:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st227;
st227:
	if ( ++p == pe )
		goto _test_eof227;
case 227:
#line 7206 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 212u: goto tr267;
		case 228u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st228:
	if ( ++p == pe )
		goto _test_eof228;
case 228:
	if ( (*p) == 224u )
		goto tr272;
	goto tr11;
tr272:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st241;
st241:
	if ( ++p == pe )
		goto _test_eof241;
case 241:
#line 7269 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr319;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr319:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st229;
st229:
	if ( ++p == pe )
		goto _test_eof229;
case 229:
#line 7422 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 213u: goto tr267;
		case 229u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st230:
	if ( ++p == pe )
		goto _test_eof230;
case 230:
	if ( (*p) == 224u )
		goto tr273;
	goto tr11;
tr273:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st242;
st242:
	if ( ++p == pe )
		goto _test_eof242;
case 242:
#line 7485 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr320;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr320:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st231;
st231:
	if ( ++p == pe )
		goto _test_eof231;
case 231:
#line 7638 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 214u: goto tr267;
		case 230u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st232:
	if ( ++p == pe )
		goto _test_eof232;
case 232:
	if ( (*p) == 224u )
		goto tr274;
	goto tr11;
tr274:
#line 53 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{ }
#line 80 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
	goto st243;
st243:
	if ( ++p == pe )
		goto _test_eof243;
case 243:
#line 7701 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto tr276;
		case 5u: goto tr277;
		case 12u: goto tr276;
		case 13u: goto tr277;
		case 14u: goto tr19;
		case 15u: goto tr278;
		case 20u: goto tr276;
		case 21u: goto tr277;
		case 28u: goto tr276;
		case 29u: goto tr277;
		case 36u: goto tr276;
		case 37u: goto tr277;
		case 44u: goto tr276;
		case 45u: goto tr277;
		case 46u: goto tr279;
		case 47u: goto tr19;
		case 52u: goto tr276;
		case 53u: goto tr277;
		case 60u: goto tr276;
		case 61u: goto tr277;
		case 62u: goto tr280;
		case 63u: goto tr19;
		case 102u: goto tr282;
		case 104u: goto tr277;
		case 105u: goto tr283;
		case 106u: goto tr276;
		case 107u: goto tr284;
		case 128u: goto tr284;
		case 129u: goto tr283;
		case 130u: goto tr19;
		case 131u: goto tr286;
		case 141u: goto tr288;
		case 143u: goto tr289;
		case 154u: goto tr19;
		case 168u: goto tr276;
		case 169u: goto tr277;
		case 196u: goto tr292;
		case 197u: goto tr293;
		case 198u: goto tr294;
		case 199u: goto tr295;
		case 200u: goto tr296;
		case 202u: goto tr297;
		case 216u: goto tr275;
		case 217u: goto tr299;
		case 218u: goto tr300;
		case 219u: goto tr301;
		case 220u: goto tr302;
		case 221u: goto tr303;
		case 222u: goto tr304;
		case 223u: goto tr305;
		case 235u: goto tr285;
		case 240u: goto tr307;
		case 242u: goto tr308;
		case 243u: goto tr309;
		case 246u: goto tr310;
		case 247u: goto tr311;
		case 254u: goto tr312;
		case 255u: goto tr321;
	}
	if ( (*p) < 132u ) {
		if ( (*p) < 32u ) {
			if ( (*p) < 8u ) {
				if ( (*p) > 3u ) {
					if ( 6u <= (*p) && (*p) <= 7u )
						goto tr19;
				} else
					goto tr275;
			} else if ( (*p) > 19u ) {
				if ( (*p) < 24u ) {
					if ( 22u <= (*p) && (*p) <= 23u )
						goto tr19;
				} else if ( (*p) > 27u ) {
					if ( 30u <= (*p) && (*p) <= 31u )
						goto tr19;
				} else
					goto tr275;
			} else
				goto tr275;
		} else if ( (*p) > 35u ) {
			if ( (*p) < 54u ) {
				if ( (*p) > 39u ) {
					if ( 40u <= (*p) && (*p) <= 51u )
						goto tr275;
				} else if ( (*p) >= 38u )
					goto tr19;
			} else if ( (*p) > 55u ) {
				if ( (*p) < 96u ) {
					if ( 56u <= (*p) && (*p) <= 59u )
						goto tr275;
				} else if ( (*p) > 111u ) {
					if ( 112u <= (*p) && (*p) <= 127u )
						goto tr285;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr275;
	} else if ( (*p) > 139u ) {
		if ( (*p) < 194u ) {
			if ( (*p) < 160u ) {
				if ( (*p) > 142u ) {
					if ( 156u <= (*p) && (*p) <= 157u )
						goto tr19;
				} else if ( (*p) >= 140u )
					goto tr287;
			} else if ( (*p) > 163u ) {
				if ( (*p) < 184u ) {
					if ( 176u <= (*p) && (*p) <= 183u )
						goto tr276;
				} else if ( (*p) > 191u ) {
					if ( 192u <= (*p) && (*p) <= 193u )
						goto tr291;
				} else
					goto tr277;
			} else
				goto tr290;
		} else if ( (*p) > 195u ) {
			if ( (*p) < 212u ) {
				if ( (*p) > 207u ) {
					if ( 208u <= (*p) && (*p) <= 211u )
						goto tr298;
				} else if ( (*p) >= 204u )
					goto tr19;
			} else if ( (*p) > 231u ) {
				if ( (*p) < 234u ) {
					if ( 232u <= (*p) && (*p) <= 233u )
						goto tr306;
				} else if ( (*p) > 241u ) {
					if ( 250u <= (*p) && (*p) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr275;
	goto tr281;
tr321:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st233;
st233:
	if ( ++p == pe )
		goto _test_eof233;
case 233:
#line 7854 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 215u: goto tr267;
		case 231u: goto tr267;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
tr313:
#line 76 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
     }
	goto st234;
st234:
	if ( ++p == pe )
		goto _test_eof234;
case 234:
#line 7906 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	switch( (*p) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
	}
	if ( (*p) < 112u ) {
		if ( (*p) < 48u ) {
			if ( (*p) <= 15u )
				goto tr0;
		} else if ( (*p) > 55u ) {
			if ( 64u <= (*p) && (*p) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*p) > 119u ) {
		if ( (*p) < 176u ) {
			if ( 128u <= (*p) && (*p) <= 143u )
				goto st3;
		} else if ( (*p) > 183u ) {
			if ( (*p) > 207u ) {
				if ( 240u <= (*p) && (*p) <= 247u )
					goto tr0;
			} else if ( (*p) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
	}
	_test_eof235: cs = 235; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 
	_test_eof62: cs = 62; goto _test_eof; 
	_test_eof63: cs = 63; goto _test_eof; 
	_test_eof64: cs = 64; goto _test_eof; 
	_test_eof65: cs = 65; goto _test_eof; 
	_test_eof66: cs = 66; goto _test_eof; 
	_test_eof67: cs = 67; goto _test_eof; 
	_test_eof68: cs = 68; goto _test_eof; 
	_test_eof69: cs = 69; goto _test_eof; 
	_test_eof70: cs = 70; goto _test_eof; 
	_test_eof71: cs = 71; goto _test_eof; 
	_test_eof72: cs = 72; goto _test_eof; 
	_test_eof73: cs = 73; goto _test_eof; 
	_test_eof74: cs = 74; goto _test_eof; 
	_test_eof75: cs = 75; goto _test_eof; 
	_test_eof76: cs = 76; goto _test_eof; 
	_test_eof77: cs = 77; goto _test_eof; 
	_test_eof78: cs = 78; goto _test_eof; 
	_test_eof79: cs = 79; goto _test_eof; 
	_test_eof80: cs = 80; goto _test_eof; 
	_test_eof81: cs = 81; goto _test_eof; 
	_test_eof82: cs = 82; goto _test_eof; 
	_test_eof83: cs = 83; goto _test_eof; 
	_test_eof84: cs = 84; goto _test_eof; 
	_test_eof85: cs = 85; goto _test_eof; 
	_test_eof86: cs = 86; goto _test_eof; 
	_test_eof87: cs = 87; goto _test_eof; 
	_test_eof88: cs = 88; goto _test_eof; 
	_test_eof89: cs = 89; goto _test_eof; 
	_test_eof90: cs = 90; goto _test_eof; 
	_test_eof91: cs = 91; goto _test_eof; 
	_test_eof92: cs = 92; goto _test_eof; 
	_test_eof93: cs = 93; goto _test_eof; 
	_test_eof94: cs = 94; goto _test_eof; 
	_test_eof95: cs = 95; goto _test_eof; 
	_test_eof96: cs = 96; goto _test_eof; 
	_test_eof97: cs = 97; goto _test_eof; 
	_test_eof98: cs = 98; goto _test_eof; 
	_test_eof99: cs = 99; goto _test_eof; 
	_test_eof100: cs = 100; goto _test_eof; 
	_test_eof101: cs = 101; goto _test_eof; 
	_test_eof102: cs = 102; goto _test_eof; 
	_test_eof103: cs = 103; goto _test_eof; 
	_test_eof104: cs = 104; goto _test_eof; 
	_test_eof105: cs = 105; goto _test_eof; 
	_test_eof106: cs = 106; goto _test_eof; 
	_test_eof107: cs = 107; goto _test_eof; 
	_test_eof108: cs = 108; goto _test_eof; 
	_test_eof109: cs = 109; goto _test_eof; 
	_test_eof110: cs = 110; goto _test_eof; 
	_test_eof111: cs = 111; goto _test_eof; 
	_test_eof112: cs = 112; goto _test_eof; 
	_test_eof113: cs = 113; goto _test_eof; 
	_test_eof114: cs = 114; goto _test_eof; 
	_test_eof236: cs = 236; goto _test_eof; 
	_test_eof115: cs = 115; goto _test_eof; 
	_test_eof116: cs = 116; goto _test_eof; 
	_test_eof117: cs = 117; goto _test_eof; 
	_test_eof118: cs = 118; goto _test_eof; 
	_test_eof119: cs = 119; goto _test_eof; 
	_test_eof120: cs = 120; goto _test_eof; 
	_test_eof121: cs = 121; goto _test_eof; 
	_test_eof122: cs = 122; goto _test_eof; 
	_test_eof123: cs = 123; goto _test_eof; 
	_test_eof124: cs = 124; goto _test_eof; 
	_test_eof125: cs = 125; goto _test_eof; 
	_test_eof126: cs = 126; goto _test_eof; 
	_test_eof127: cs = 127; goto _test_eof; 
	_test_eof128: cs = 128; goto _test_eof; 
	_test_eof129: cs = 129; goto _test_eof; 
	_test_eof130: cs = 130; goto _test_eof; 
	_test_eof131: cs = 131; goto _test_eof; 
	_test_eof132: cs = 132; goto _test_eof; 
	_test_eof133: cs = 133; goto _test_eof; 
	_test_eof134: cs = 134; goto _test_eof; 
	_test_eof135: cs = 135; goto _test_eof; 
	_test_eof136: cs = 136; goto _test_eof; 
	_test_eof137: cs = 137; goto _test_eof; 
	_test_eof138: cs = 138; goto _test_eof; 
	_test_eof139: cs = 139; goto _test_eof; 
	_test_eof140: cs = 140; goto _test_eof; 
	_test_eof141: cs = 141; goto _test_eof; 
	_test_eof142: cs = 142; goto _test_eof; 
	_test_eof143: cs = 143; goto _test_eof; 
	_test_eof144: cs = 144; goto _test_eof; 
	_test_eof145: cs = 145; goto _test_eof; 
	_test_eof146: cs = 146; goto _test_eof; 
	_test_eof147: cs = 147; goto _test_eof; 
	_test_eof148: cs = 148; goto _test_eof; 
	_test_eof149: cs = 149; goto _test_eof; 
	_test_eof150: cs = 150; goto _test_eof; 
	_test_eof151: cs = 151; goto _test_eof; 
	_test_eof152: cs = 152; goto _test_eof; 
	_test_eof153: cs = 153; goto _test_eof; 
	_test_eof154: cs = 154; goto _test_eof; 
	_test_eof155: cs = 155; goto _test_eof; 
	_test_eof156: cs = 156; goto _test_eof; 
	_test_eof157: cs = 157; goto _test_eof; 
	_test_eof158: cs = 158; goto _test_eof; 
	_test_eof159: cs = 159; goto _test_eof; 
	_test_eof160: cs = 160; goto _test_eof; 
	_test_eof161: cs = 161; goto _test_eof; 
	_test_eof162: cs = 162; goto _test_eof; 
	_test_eof163: cs = 163; goto _test_eof; 
	_test_eof164: cs = 164; goto _test_eof; 
	_test_eof165: cs = 165; goto _test_eof; 
	_test_eof166: cs = 166; goto _test_eof; 
	_test_eof167: cs = 167; goto _test_eof; 
	_test_eof168: cs = 168; goto _test_eof; 
	_test_eof169: cs = 169; goto _test_eof; 
	_test_eof170: cs = 170; goto _test_eof; 
	_test_eof171: cs = 171; goto _test_eof; 
	_test_eof172: cs = 172; goto _test_eof; 
	_test_eof173: cs = 173; goto _test_eof; 
	_test_eof174: cs = 174; goto _test_eof; 
	_test_eof175: cs = 175; goto _test_eof; 
	_test_eof176: cs = 176; goto _test_eof; 
	_test_eof177: cs = 177; goto _test_eof; 
	_test_eof178: cs = 178; goto _test_eof; 
	_test_eof179: cs = 179; goto _test_eof; 
	_test_eof180: cs = 180; goto _test_eof; 
	_test_eof181: cs = 181; goto _test_eof; 
	_test_eof182: cs = 182; goto _test_eof; 
	_test_eof183: cs = 183; goto _test_eof; 
	_test_eof184: cs = 184; goto _test_eof; 
	_test_eof185: cs = 185; goto _test_eof; 
	_test_eof186: cs = 186; goto _test_eof; 
	_test_eof187: cs = 187; goto _test_eof; 
	_test_eof188: cs = 188; goto _test_eof; 
	_test_eof189: cs = 189; goto _test_eof; 
	_test_eof190: cs = 190; goto _test_eof; 
	_test_eof191: cs = 191; goto _test_eof; 
	_test_eof192: cs = 192; goto _test_eof; 
	_test_eof193: cs = 193; goto _test_eof; 
	_test_eof194: cs = 194; goto _test_eof; 
	_test_eof195: cs = 195; goto _test_eof; 
	_test_eof196: cs = 196; goto _test_eof; 
	_test_eof197: cs = 197; goto _test_eof; 
	_test_eof198: cs = 198; goto _test_eof; 
	_test_eof199: cs = 199; goto _test_eof; 
	_test_eof200: cs = 200; goto _test_eof; 
	_test_eof201: cs = 201; goto _test_eof; 
	_test_eof202: cs = 202; goto _test_eof; 
	_test_eof203: cs = 203; goto _test_eof; 
	_test_eof204: cs = 204; goto _test_eof; 
	_test_eof205: cs = 205; goto _test_eof; 
	_test_eof206: cs = 206; goto _test_eof; 
	_test_eof207: cs = 207; goto _test_eof; 
	_test_eof208: cs = 208; goto _test_eof; 
	_test_eof209: cs = 209; goto _test_eof; 
	_test_eof210: cs = 210; goto _test_eof; 
	_test_eof211: cs = 211; goto _test_eof; 
	_test_eof212: cs = 212; goto _test_eof; 
	_test_eof213: cs = 213; goto _test_eof; 
	_test_eof214: cs = 214; goto _test_eof; 
	_test_eof215: cs = 215; goto _test_eof; 
	_test_eof216: cs = 216; goto _test_eof; 
	_test_eof217: cs = 217; goto _test_eof; 
	_test_eof218: cs = 218; goto _test_eof; 
	_test_eof219: cs = 219; goto _test_eof; 
	_test_eof220: cs = 220; goto _test_eof; 
	_test_eof237: cs = 237; goto _test_eof; 
	_test_eof221: cs = 221; goto _test_eof; 
	_test_eof222: cs = 222; goto _test_eof; 
	_test_eof238: cs = 238; goto _test_eof; 
	_test_eof223: cs = 223; goto _test_eof; 
	_test_eof224: cs = 224; goto _test_eof; 
	_test_eof239: cs = 239; goto _test_eof; 
	_test_eof225: cs = 225; goto _test_eof; 
	_test_eof226: cs = 226; goto _test_eof; 
	_test_eof240: cs = 240; goto _test_eof; 
	_test_eof227: cs = 227; goto _test_eof; 
	_test_eof228: cs = 228; goto _test_eof; 
	_test_eof241: cs = 241; goto _test_eof; 
	_test_eof229: cs = 229; goto _test_eof; 
	_test_eof230: cs = 230; goto _test_eof; 
	_test_eof242: cs = 242; goto _test_eof; 
	_test_eof231: cs = 231; goto _test_eof; 
	_test_eof232: cs = 232; goto _test_eof; 
	_test_eof243: cs = 243; goto _test_eof; 
	_test_eof233: cs = 233; goto _test_eof; 
	_test_eof234: cs = 234; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 1: 
	case 2: 
	case 3: 
	case 4: 
	case 5: 
	case 6: 
	case 7: 
	case 8: 
	case 9: 
	case 10: 
	case 11: 
	case 12: 
	case 13: 
	case 14: 
	case 15: 
	case 16: 
	case 17: 
	case 18: 
	case 19: 
	case 20: 
	case 21: 
	case 22: 
	case 23: 
	case 24: 
	case 25: 
	case 26: 
	case 27: 
	case 28: 
	case 29: 
	case 30: 
	case 31: 
	case 32: 
	case 33: 
	case 34: 
	case 35: 
	case 36: 
	case 37: 
	case 38: 
	case 39: 
	case 40: 
	case 41: 
	case 42: 
	case 43: 
	case 44: 
	case 45: 
	case 46: 
	case 47: 
	case 48: 
	case 49: 
	case 50: 
	case 51: 
	case 52: 
	case 53: 
	case 54: 
	case 55: 
	case 56: 
	case 57: 
	case 58: 
	case 59: 
	case 60: 
	case 61: 
	case 62: 
	case 63: 
	case 64: 
	case 65: 
	case 66: 
	case 67: 
	case 68: 
	case 69: 
	case 70: 
	case 71: 
	case 72: 
	case 73: 
	case 74: 
	case 75: 
	case 76: 
	case 77: 
	case 78: 
	case 79: 
	case 80: 
	case 81: 
	case 82: 
	case 83: 
	case 84: 
	case 85: 
	case 86: 
	case 87: 
	case 88: 
	case 89: 
	case 90: 
	case 91: 
	case 92: 
	case 93: 
	case 94: 
	case 95: 
	case 96: 
	case 97: 
	case 98: 
	case 99: 
	case 100: 
	case 101: 
	case 102: 
	case 103: 
	case 104: 
	case 105: 
	case 106: 
	case 107: 
	case 108: 
	case 109: 
	case 110: 
	case 111: 
	case 112: 
	case 113: 
	case 114: 
	case 115: 
	case 116: 
	case 117: 
	case 118: 
	case 119: 
	case 120: 
	case 121: 
	case 122: 
	case 123: 
	case 124: 
	case 125: 
	case 126: 
	case 127: 
	case 128: 
	case 129: 
	case 130: 
	case 131: 
	case 132: 
	case 133: 
	case 134: 
	case 135: 
	case 136: 
	case 137: 
	case 138: 
	case 139: 
	case 140: 
	case 141: 
	case 142: 
	case 143: 
	case 144: 
	case 145: 
	case 146: 
	case 147: 
	case 148: 
	case 149: 
	case 150: 
	case 151: 
	case 152: 
	case 153: 
	case 154: 
	case 155: 
	case 156: 
	case 157: 
	case 158: 
	case 159: 
	case 160: 
	case 161: 
	case 162: 
	case 163: 
	case 164: 
	case 165: 
	case 166: 
	case 167: 
	case 168: 
	case 169: 
	case 170: 
	case 171: 
	case 172: 
	case 173: 
	case 174: 
	case 175: 
	case 176: 
	case 177: 
	case 178: 
	case 179: 
	case 180: 
	case 181: 
	case 182: 
	case 183: 
	case 184: 
	case 185: 
	case 186: 
	case 187: 
	case 188: 
	case 189: 
	case 190: 
	case 191: 
	case 192: 
	case 193: 
	case 194: 
	case 195: 
	case 196: 
	case 197: 
	case 198: 
	case 199: 
	case 200: 
	case 201: 
	case 202: 
	case 203: 
	case 204: 
	case 205: 
	case 206: 
	case 207: 
	case 208: 
	case 209: 
	case 210: 
	case 211: 
	case 212: 
	case 213: 
	case 214: 
	case 215: 
	case 216: 
	case 217: 
	case 218: 
	case 219: 
	case 220: 
	case 221: 
	case 222: 
	case 223: 
	case 224: 
	case 225: 
	case 226: 
	case 227: 
	case 228: 
	case 229: 
	case 230: 
	case 231: 
	case 232: 
	case 233: 
	case 234: 
#line 86 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
	{
        process_error(begin, userdata);
        result = 1;
        goto error_detected;
    }
	break;
#line 8435 "src/trusted/validator_ragel/generated/validator-x86_32.c"
	}
	}

	_out: {}
	}

#line 173 "src/trusted/validator_ragel/unreviewed/validator-x86_32.rl"
  }

  if (CheckJumpTargets(valid_targets, jump_dests, size)) {
    return 1;
  }

error_detected:
  return result;
}
