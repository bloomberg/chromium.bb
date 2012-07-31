/* native_client/src/trusted/validator_ragel/gen/validator_x86_32.c
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for ia32 mode.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

/* Ignore this information: it's not used by security model in IA32 mode.  */
#undef GET_VEX_PREFIX3
#define GET_VEX_PREFIX3 0
#undef SET_VEX_PREFIX3
#define SET_VEX_PREFIX3(P)





static const int x86_64_decoder_start = 244;
static const int x86_64_decoder_first_final = 244;
static const int x86_64_decoder_error = 0;

static const int x86_64_decoder_en_main = 244;




int ValidateChunkIA32(const uint8_t *data, size_t size,
                      const NaClCPUFeaturesX86 *cpu_features,
                      process_validation_error_func process_error,
                      void *userdata) {
  uint8_t *valid_targets = BitmapAllocate(size);
  uint8_t *jump_dests = BitmapAllocate(size);

  const uint8_t *current_position = data;

  int result = 0;

  size_t i;

  uint32_t errors_detected = 0;

  assert(size % kBundleSize == 0);

  if (!valid_targets || !jump_dests) goto error_detected;

  while (current_position < data + size) {
    /* Start of the instruction being processed.  */
    const uint8_t *instruction_start = current_position;
    const uint8_t *end_of_bundle = current_position + kBundleSize;
    int current_state;

    
	{
	( current_state) = x86_64_decoder_start;
	}

    
	{
	if ( ( current_position) == ( end_of_bundle) )
		goto _test_eof;
	switch ( ( current_state) )
	{
tr0:
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr9:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr10:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr11:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr15:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr19:
	{
    SET_CPU_FEATURE(CPUFeature_3DNOW);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr27:
	{
    SET_CPU_FEATURE(CPUFeature_TSC);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr34:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr48:
	{
    SET_CPU_FEATURE(CPUFeature_MON);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr49:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr50:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr62:
	{
    SET_CPU_FEATURE(CPUFeature_E3DNOW);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr68:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr89:
	{
    rel32_operand(current_position + 1, data, jump_dests, size,
                  &errors_detected);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr90:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr96:
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr97:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr104:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr112:
	{
    rel8_operand(current_position + 1, data, jump_dests, size,
                 &errors_detected);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr133:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr178:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr252:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr259:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr293:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr320:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr346:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr372:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr378:
	{
    SET_CPU_FEATURE(CPUFeature_CMOVx87);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr398:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr406:
	{
      BitmapClearBit(valid_targets, (current_position - data) - 1);
    }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr420:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
tr429:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st244;
st244:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof244;
case 244:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr453;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr21:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr26:
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr28:
	{
    SET_CPU_FEATURE(CPUFeature_CMOV);
  }
	goto st1;
tr30:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	goto st1;
tr45:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st1;
tr134:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr143:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr146:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st1;
tr147:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st1;
tr149:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr150:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st1;
tr200:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr202:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_POPCNT);
  }
	goto st1;
tr203:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_TZCNT);
  }
	goto st1;
tr204:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_LZCNT);
  }
	goto st1;
tr251:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st1;
tr318:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st1;
tr290:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st1;
tr326:
	{
    SET_CPU_FEATURE(CPUFeature_FMA);
  }
	goto st1;
tr327:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st1;
tr328:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st1;
tr384:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr385:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr391:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr399:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr400:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr402:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr414:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st1;
st1:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof1;
case 1:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st3;
	} else if ( (*( current_position)) >= 64u )
		goto st7;
	goto tr0;
tr69:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st2;
tr51:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st2;
tr91:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st2;
tr105:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st2;
tr253:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st2;
tr321:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st2;
tr294:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st2;
tr373:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st2;
st2:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof2;
case 2:
	switch( (*( current_position)) ) {
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
tr70:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st3;
tr52:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st3;
tr92:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st3;
tr106:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st3;
tr254:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st3;
tr322:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st3;
tr295:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st3;
tr374:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st3;
tr430:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st3;
st3:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof3;
case 3:
	goto st4;
st4:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof4;
case 4:
	goto st5;
st5:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof5;
case 5:
	goto st6;
st6:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof6;
case 6:
	goto tr9;
tr71:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st7;
tr53:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st7;
tr93:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st7;
tr107:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st7;
tr255:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st7;
tr323:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st7;
tr296:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st7;
tr375:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st7;
st7:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof7;
case 7:
	goto tr10;
tr72:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st8;
tr54:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st8;
tr94:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st8;
tr108:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st8;
tr256:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st8;
tr324:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st8;
tr297:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st8;
tr376:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st8;
st8:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof8;
case 8:
	goto st7;
tr73:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st9;
tr55:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st9;
tr95:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st9;
tr109:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st9;
tr257:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st9;
tr325:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st9;
tr298:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st9;
tr377:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st9;
st9:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof9;
case 9:
	goto st3;
tr85:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	goto st10;
tr98:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st10;
tr83:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st10;
tr84:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st10;
tr164:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st10;
tr158:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st10;
tr165:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st10;
tr301:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st10;
tr371:
	{ }
	goto st10;
tr395:
	{ }
	goto st10;
tr415:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st10;
st10:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof10;
case 10:
	goto tr11;
tr214:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st11;
tr215:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st11;
tr263:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st11;
tr416:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st11;
st11:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof11;
case 11:
	goto st12;
st12:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof12;
case 12:
	goto st13;
st13:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof13;
case 13:
	goto st14;
st14:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof14;
case 14:
	goto tr15;
tr16:
	{
        process_error(instruction_start, UNRECOGNIZED_INSTRUCTION, userdata);
        result = 1;
        goto error_detected;
    }
	goto st0;
st0:
( current_state) = 0;
	goto _out;
tr417:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st15;
st15:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof15;
case 15:
	switch( (*( current_position)) ) {
		case 1u: goto st16;
		case 11u: goto tr0;
		case 13u: goto st17;
		case 14u: goto tr19;
		case 15u: goto st18;
		case 18u: goto st28;
		case 19u: goto tr23;
		case 22u: goto st28;
		case 23u: goto tr23;
		case 24u: goto st30;
		case 31u: goto st31;
		case 43u: goto tr23;
		case 49u: goto tr27;
		case 80u: goto tr29;
		case 112u: goto tr31;
		case 115u: goto st43;
		case 119u: goto tr34;
		case 162u: goto tr0;
		case 164u: goto st33;
		case 165u: goto st1;
		case 172u: goto st33;
		case 174u: goto st48;
		case 195u: goto tr40;
		case 196u: goto st49;
		case 197u: goto tr42;
		case 199u: goto st51;
		case 212u: goto tr26;
		case 215u: goto tr44;
		case 218u: goto tr45;
		case 222u: goto tr45;
		case 224u: goto tr45;
		case 229u: goto tr30;
		case 231u: goto tr46;
		case 234u: goto tr45;
		case 238u: goto tr45;
		case 244u: goto tr26;
		case 246u: goto tr45;
		case 247u: goto tr47;
		case 251u: goto tr26;
	}
	if ( (*( current_position)) < 126u ) {
		if ( (*( current_position)) < 81u ) {
			if ( (*( current_position)) < 42u ) {
				if ( (*( current_position)) > 21u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 41u )
						goto tr21;
				} else if ( (*( current_position)) >= 16u )
					goto tr21;
			} else if ( (*( current_position)) > 45u ) {
				if ( (*( current_position)) > 47u ) {
					if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
						goto tr28;
				} else if ( (*( current_position)) >= 46u )
					goto tr21;
			} else
				goto tr26;
		} else if ( (*( current_position)) > 89u ) {
			if ( (*( current_position)) < 96u ) {
				if ( (*( current_position)) > 91u ) {
					if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
						goto tr21;
				} else if ( (*( current_position)) >= 90u )
					goto tr26;
			} else if ( (*( current_position)) > 107u ) {
				if ( (*( current_position)) < 113u ) {
					if ( 110u <= (*( current_position)) && (*( current_position)) <= 111u )
						goto tr30;
				} else if ( (*( current_position)) > 114u ) {
					if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
						goto tr30;
				} else
					goto st42;
			} else
				goto tr30;
		} else
			goto tr21;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 173u ) {
				if ( (*( current_position)) > 143u ) {
					if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr28;
				} else if ( (*( current_position)) >= 128u )
					goto st44;
			} else if ( (*( current_position)) > 177u ) {
				if ( (*( current_position)) > 183u ) {
					if ( 188u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st1;
				} else if ( (*( current_position)) >= 182u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 198u ) {
			if ( (*( current_position)) < 216u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 209u <= (*( current_position)) && (*( current_position)) <= 213u )
						goto tr30;
				} else if ( (*( current_position)) >= 200u )
					goto tr0;
			} else if ( (*( current_position)) > 226u ) {
				if ( (*( current_position)) < 232u ) {
					if ( 227u <= (*( current_position)) && (*( current_position)) <= 228u )
						goto tr45;
				} else if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto tr30;
				} else
					goto tr30;
			} else
				goto tr30;
		} else
			goto tr39;
	} else
		goto tr30;
	goto tr16;
st16:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof16;
case 16:
	if ( (*( current_position)) == 208u )
		goto tr49;
	if ( 200u <= (*( current_position)) && (*( current_position)) <= 201u )
		goto tr48;
	goto tr16;
st17:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof17;
case 17:
	switch( (*( current_position)) ) {
		case 4u: goto tr51;
		case 5u: goto tr52;
		case 12u: goto tr51;
		case 13u: goto tr52;
		case 68u: goto tr54;
		case 76u: goto tr54;
		case 132u: goto tr55;
		case 140u: goto tr55;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr50;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr52;
	} else
		goto tr53;
	goto tr16;
st18:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof18;
case 18:
	switch( (*( current_position)) ) {
		case 4u: goto st20;
		case 5u: goto st21;
		case 12u: goto st20;
		case 13u: goto st21;
		case 20u: goto st20;
		case 21u: goto st21;
		case 28u: goto st20;
		case 29u: goto st21;
		case 36u: goto st20;
		case 37u: goto st21;
		case 44u: goto st20;
		case 45u: goto st21;
		case 52u: goto st20;
		case 53u: goto st21;
		case 60u: goto st20;
		case 61u: goto st21;
		case 68u: goto st26;
		case 76u: goto st26;
		case 84u: goto st26;
		case 92u: goto st26;
		case 100u: goto st26;
		case 108u: goto st26;
		case 116u: goto st26;
		case 124u: goto st26;
		case 132u: goto st27;
		case 140u: goto st27;
		case 148u: goto st27;
		case 156u: goto st27;
		case 164u: goto st27;
		case 172u: goto st27;
		case 180u: goto st27;
		case 188u: goto st27;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st21;
	} else if ( (*( current_position)) >= 64u )
		goto st25;
	goto st19;
tr66:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st19;
tr67:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st19;
st19:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof19;
case 19:
	switch( (*( current_position)) ) {
		case 12u: goto tr62;
		case 13u: goto tr19;
		case 28u: goto tr62;
		case 29u: goto tr19;
		case 138u: goto tr62;
		case 142u: goto tr62;
		case 144u: goto tr19;
		case 148u: goto tr19;
		case 154u: goto tr19;
		case 158u: goto tr19;
		case 160u: goto tr19;
		case 164u: goto tr19;
		case 170u: goto tr19;
		case 174u: goto tr19;
		case 176u: goto tr19;
		case 180u: goto tr19;
		case 187u: goto tr62;
		case 191u: goto tr19;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 151u )
			goto tr19;
	} else if ( (*( current_position)) > 167u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto tr19;
	} else
		goto tr19;
	goto tr16;
st20:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof20;
case 20:
	switch( (*( current_position)) ) {
		case 5u: goto st21;
		case 13u: goto st21;
		case 21u: goto st21;
		case 29u: goto st21;
		case 37u: goto st21;
		case 45u: goto st21;
		case 53u: goto st21;
		case 61u: goto st21;
		case 69u: goto st21;
		case 77u: goto st21;
		case 85u: goto st21;
		case 93u: goto st21;
		case 101u: goto st21;
		case 109u: goto st21;
		case 117u: goto st21;
		case 125u: goto st21;
		case 133u: goto st21;
		case 141u: goto st21;
		case 149u: goto st21;
		case 157u: goto st21;
		case 165u: goto st21;
		case 173u: goto st21;
		case 181u: goto st21;
		case 189u: goto st21;
		case 197u: goto st21;
		case 205u: goto st21;
		case 213u: goto st21;
		case 221u: goto st21;
		case 229u: goto st21;
		case 237u: goto st21;
		case 245u: goto st21;
		case 253u: goto st21;
	}
	goto st19;
st21:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof21;
case 21:
	goto st22;
st22:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof22;
case 22:
	goto st23;
st23:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof23;
case 23:
	goto st24;
st24:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof24;
case 24:
	goto tr66;
st25:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof25;
case 25:
	goto tr67;
st26:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof26;
case 26:
	goto st25;
st27:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof27;
case 27:
	goto st21;
st28:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof28;
case 28:
	switch( (*( current_position)) ) {
		case 4u: goto tr69;
		case 5u: goto tr70;
		case 12u: goto tr69;
		case 13u: goto tr70;
		case 20u: goto tr69;
		case 21u: goto tr70;
		case 28u: goto tr69;
		case 29u: goto tr70;
		case 36u: goto tr69;
		case 37u: goto tr70;
		case 44u: goto tr69;
		case 45u: goto tr70;
		case 52u: goto tr69;
		case 53u: goto tr70;
		case 60u: goto tr69;
		case 61u: goto tr70;
		case 68u: goto tr72;
		case 76u: goto tr72;
		case 84u: goto tr72;
		case 92u: goto tr72;
		case 100u: goto tr72;
		case 108u: goto tr72;
		case 116u: goto tr72;
		case 124u: goto tr72;
		case 132u: goto tr73;
		case 140u: goto tr73;
		case 148u: goto tr73;
		case 156u: goto tr73;
		case 164u: goto tr73;
		case 172u: goto tr73;
		case 180u: goto tr73;
		case 188u: goto tr73;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr70;
	} else if ( (*( current_position)) >= 64u )
		goto tr71;
	goto tr68;
tr23:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st29;
tr40:
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st29;
tr46:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st29;
tr135:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st29;
tr148:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st29;
tr291:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st29;
tr393:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st29;
tr386:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st29;
tr401:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st29;
tr427:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st29;
st29:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof29;
case 29:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 63u )
			goto tr0;
	} else if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st30:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof30;
case 30:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 31u )
			goto tr0;
	} else if ( (*( current_position)) > 95u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st31:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof31;
case 31:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto tr0;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
tr29:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st32;
tr44:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st32;
tr47:
	{
    SET_CPU_FEATURE(CPUFeature_EMMX);
  }
	goto st32;
tr138:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st32;
tr142:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st32;
tr302:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st32;
tr392:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st32;
tr390:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st32;
tr405:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st32;
st32:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof32;
case 32:
	if ( 192u <= (*( current_position)) )
		goto tr0;
	goto tr16;
tr39:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st33;
tr31:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st33;
tr139:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
tr152:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st33;
tr151:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st33;
tr156:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st33;
tr157:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st33;
tr155:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_CLMUL);
  }
	goto st33;
tr233:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st33;
tr292:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st33;
tr339:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st33;
tr353:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st33;
tr335:
	{
    SET_CPU_FEATURE(CPUFeature_CLMULAVX);
  }
	goto st33;
tr388:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
tr404:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st33;
tr403:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
tr424:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st33;
st33:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof33;
case 33:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st35;
	} else if ( (*( current_position)) >= 64u )
		goto st39;
	goto st10;
tr99:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st34;
tr159:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st34;
tr166:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st34;
tr306:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st34;
st34:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof34;
case 34:
	switch( (*( current_position)) ) {
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
tr100:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st35;
tr160:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st35;
tr167:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st35;
tr307:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st35;
st35:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof35;
case 35:
	goto st36;
st36:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof36;
case 36:
	goto st37;
st37:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof37;
case 37:
	goto st38;
st38:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof38;
case 38:
	goto tr83;
tr101:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st39;
tr161:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st39;
tr168:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st39;
tr308:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st39;
st39:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof39;
case 39:
	goto tr84;
tr102:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st40;
tr162:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st40;
tr169:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st40;
tr309:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st40;
st40:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof40;
case 40:
	goto st39;
tr103:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st41;
tr163:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st41;
tr170:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st41;
tr310:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st41;
st41:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof41;
case 41:
	goto st35;
st42:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof42;
case 42:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr85;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr85;
	} else
		goto tr85;
	goto tr16;
st43:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof43;
case 43:
	if ( (*( current_position)) > 215u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr85;
	} else if ( (*( current_position)) >= 208u )
		goto tr85;
	goto tr16;
tr446:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st44;
st44:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof44;
case 44:
	goto st45;
st45:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof45;
case 45:
	goto st46;
st46:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof46;
case 46:
	goto st47;
st47:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof47;
case 47:
	goto tr89;
st48:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof48;
case 48:
	switch( (*( current_position)) ) {
		case 20u: goto tr69;
		case 21u: goto tr70;
		case 28u: goto tr69;
		case 29u: goto tr70;
		case 60u: goto tr91;
		case 61u: goto tr92;
		case 84u: goto tr72;
		case 92u: goto tr72;
		case 124u: goto tr94;
		case 148u: goto tr73;
		case 156u: goto tr73;
		case 188u: goto tr95;
		case 232u: goto tr96;
		case 240u: goto tr96;
		case 248u: goto tr97;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) > 31u ) {
			if ( 56u <= (*( current_position)) && (*( current_position)) <= 63u )
				goto tr90;
		} else if ( (*( current_position)) >= 16u )
			goto tr68;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr93;
		} else if ( (*( current_position)) > 159u ) {
			if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr92;
		} else
			goto tr70;
	} else
		goto tr71;
	goto tr16;
st49:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof49;
case 49:
	switch( (*( current_position)) ) {
		case 4u: goto tr99;
		case 5u: goto tr100;
		case 12u: goto tr99;
		case 13u: goto tr100;
		case 20u: goto tr99;
		case 21u: goto tr100;
		case 28u: goto tr99;
		case 29u: goto tr100;
		case 36u: goto tr99;
		case 37u: goto tr100;
		case 44u: goto tr99;
		case 45u: goto tr100;
		case 52u: goto tr99;
		case 53u: goto tr100;
		case 60u: goto tr99;
		case 61u: goto tr100;
		case 68u: goto tr102;
		case 76u: goto tr102;
		case 84u: goto tr102;
		case 92u: goto tr102;
		case 100u: goto tr102;
		case 108u: goto tr102;
		case 116u: goto tr102;
		case 124u: goto tr102;
		case 132u: goto tr103;
		case 140u: goto tr103;
		case 148u: goto tr103;
		case 156u: goto tr103;
		case 164u: goto tr103;
		case 172u: goto tr103;
		case 180u: goto tr103;
		case 188u: goto tr103;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr100;
	} else if ( (*( current_position)) >= 64u )
		goto tr101;
	goto tr98;
tr42:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st50;
tr145:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st50;
tr305:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st50;
st50:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof50;
case 50:
	if ( 192u <= (*( current_position)) )
		goto st10;
	goto tr16;
st51:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof51;
case 51:
	switch( (*( current_position)) ) {
		case 12u: goto tr105;
		case 13u: goto tr106;
		case 76u: goto tr108;
		case 140u: goto tr109;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
			goto tr104;
	} else if ( (*( current_position)) > 79u ) {
		if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr106;
	} else
		goto tr107;
	goto tr16;
tr418:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_NOT_TAKEN(TRUE);
  }
	goto st52;
tr419:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_TAKEN(TRUE);
  }
	goto st52;
st52:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof52;
case 52:
	if ( (*( current_position)) == 15u )
		goto st53;
	if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
		goto st54;
	goto tr16;
st53:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof53;
case 53:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
		goto st44;
	goto tr16;
tr425:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st54;
st54:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof54;
case 54:
	goto tr112;
tr421:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st55;
st55:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof55;
case 55:
	switch( (*( current_position)) ) {
		case 139u: goto st56;
		case 161u: goto st57;
	}
	goto tr16;
st56:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof56;
case 56:
	switch( (*( current_position)) ) {
		case 5u: goto st57;
		case 13u: goto st57;
		case 21u: goto st57;
		case 29u: goto st57;
		case 37u: goto st57;
		case 45u: goto st57;
		case 53u: goto st57;
		case 61u: goto st57;
	}
	goto tr16;
st57:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof57;
case 57:
	switch( (*( current_position)) ) {
		case 0u: goto st58;
		case 4u: goto st58;
	}
	goto tr16;
st58:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof58;
case 58:
	if ( (*( current_position)) == 0u )
		goto st59;
	goto tr16;
st59:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof59;
case 59:
	if ( (*( current_position)) == 0u )
		goto st60;
	goto tr16;
st60:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof60;
case 60:
	if ( (*( current_position)) == 0u )
		goto tr0;
	goto tr16;
tr422:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st61;
st61:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof61;
case 61:
	switch( (*( current_position)) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st62;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st62;
		case 15u: goto st64;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st62;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st62;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st62;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st62;
		case 46u: goto st72;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st62;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st62;
		case 102u: goto st80;
		case 104u: goto st62;
		case 105u: goto st85;
		case 107u: goto st33;
		case 129u: goto st85;
		case 131u: goto st33;
		case 133u: goto st1;
		case 135u: goto st1;
		case 137u: goto st1;
		case 139u: goto st1;
		case 141u: goto st29;
		case 143u: goto st31;
		case 161u: goto st3;
		case 163u: goto st3;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 169u: goto st62;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st94;
		case 199u: goto st95;
		case 209u: goto st96;
		case 211u: goto st96;
		case 240u: goto tr127;
		case 242u: goto tr128;
		case 243u: goto tr129;
		case 247u: goto st108;
		case 255u: goto st109;
	}
	if ( (*( current_position)) < 144u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr0;
	} else if ( (*( current_position)) > 153u ) {
		if ( (*( current_position)) > 157u ) {
			if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto st62;
		} else if ( (*( current_position)) >= 156u )
			goto tr0;
	} else
		goto tr0;
	goto tr16;
tr191:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st62;
tr192:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st62;
st62:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof62;
case 62:
	goto st63;
st63:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof63;
case 63:
	goto tr133;
st64:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof64;
case 64:
	switch( (*( current_position)) ) {
		case 31u: goto st31;
		case 43u: goto tr135;
		case 56u: goto st65;
		case 58u: goto st66;
		case 80u: goto tr138;
		case 81u: goto tr134;
		case 112u: goto tr139;
		case 115u: goto st70;
		case 121u: goto tr142;
		case 164u: goto st33;
		case 165u: goto st1;
		case 172u: goto st33;
		case 173u: goto st1;
		case 175u: goto st1;
		case 177u: goto st1;
		case 193u: goto st1;
		case 194u: goto tr139;
		case 196u: goto st71;
		case 197u: goto tr145;
		case 198u: goto tr139;
		case 215u: goto tr138;
		case 231u: goto tr135;
		case 247u: goto tr138;
	}
	if ( (*( current_position)) < 113u ) {
		if ( (*( current_position)) < 22u ) {
			if ( (*( current_position)) < 18u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
					goto tr134;
			} else if ( (*( current_position)) > 19u ) {
				if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
					goto tr134;
			} else
				goto tr135;
		} else if ( (*( current_position)) > 23u ) {
			if ( (*( current_position)) < 64u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr134;
			} else if ( (*( current_position)) > 79u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr134;
			} else
				goto tr28;
		} else
			goto tr135;
	} else if ( (*( current_position)) > 114u ) {
		if ( (*( current_position)) < 182u ) {
			if ( (*( current_position)) < 124u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr134;
			} else if ( (*( current_position)) > 125u ) {
				if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr134;
			} else
				goto tr143;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( 188u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto st1;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr134;
			} else
				goto tr134;
		} else
			goto st1;
	} else
		goto st69;
	goto tr16;
st65:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof65;
case 65:
	switch( (*( current_position)) ) {
		case 16u: goto tr147;
		case 23u: goto tr147;
		case 42u: goto tr148;
		case 55u: goto tr149;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) <= 11u )
				goto tr146;
		} else if ( (*( current_position)) > 21u ) {
			if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
				goto tr146;
		} else
			goto tr147;
	} else if ( (*( current_position)) > 37u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 43u )
				goto tr147;
		} else if ( (*( current_position)) > 53u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr150;
			} else if ( (*( current_position)) >= 56u )
				goto tr147;
		} else
			goto tr147;
	} else
		goto tr147;
	goto tr16;
st66:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof66;
case 66:
	switch( (*( current_position)) ) {
		case 15u: goto tr152;
		case 22u: goto st68;
		case 23u: goto tr151;
		case 32u: goto st67;
		case 68u: goto tr155;
		case 223u: goto tr157;
	}
	if ( (*( current_position)) < 33u ) {
		if ( (*( current_position)) > 14u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto st67;
		} else if ( (*( current_position)) >= 8u )
			goto tr151;
	} else if ( (*( current_position)) > 34u ) {
		if ( (*( current_position)) > 66u ) {
			if ( 96u <= (*( current_position)) && (*( current_position)) <= 99u )
				goto tr156;
		} else if ( (*( current_position)) >= 64u )
			goto tr151;
	} else
		goto tr151;
	goto tr16;
st67:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof67;
case 67:
	switch( (*( current_position)) ) {
		case 4u: goto tr159;
		case 5u: goto tr160;
		case 12u: goto tr159;
		case 13u: goto tr160;
		case 20u: goto tr159;
		case 21u: goto tr160;
		case 28u: goto tr159;
		case 29u: goto tr160;
		case 36u: goto tr159;
		case 37u: goto tr160;
		case 44u: goto tr159;
		case 45u: goto tr160;
		case 52u: goto tr159;
		case 53u: goto tr160;
		case 60u: goto tr159;
		case 61u: goto tr160;
		case 68u: goto tr162;
		case 76u: goto tr162;
		case 84u: goto tr162;
		case 92u: goto tr162;
		case 100u: goto tr162;
		case 108u: goto tr162;
		case 116u: goto tr162;
		case 124u: goto tr162;
		case 132u: goto tr163;
		case 140u: goto tr163;
		case 148u: goto tr163;
		case 156u: goto tr163;
		case 164u: goto tr163;
		case 172u: goto tr163;
		case 180u: goto tr163;
		case 188u: goto tr163;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr160;
	} else if ( (*( current_position)) >= 64u )
		goto tr161;
	goto tr158;
st68:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof68;
case 68:
	if ( 192u <= (*( current_position)) )
		goto tr158;
	goto tr16;
st69:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof69;
case 69:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr164;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr164;
	} else
		goto tr164;
	goto tr16;
st70:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof70;
case 70:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr164;
	} else if ( (*( current_position)) >= 208u )
		goto tr164;
	goto tr16;
st71:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof71;
case 71:
	switch( (*( current_position)) ) {
		case 4u: goto tr166;
		case 5u: goto tr167;
		case 12u: goto tr166;
		case 13u: goto tr167;
		case 20u: goto tr166;
		case 21u: goto tr167;
		case 28u: goto tr166;
		case 29u: goto tr167;
		case 36u: goto tr166;
		case 37u: goto tr167;
		case 44u: goto tr166;
		case 45u: goto tr167;
		case 52u: goto tr166;
		case 53u: goto tr167;
		case 60u: goto tr166;
		case 61u: goto tr167;
		case 68u: goto tr169;
		case 76u: goto tr169;
		case 84u: goto tr169;
		case 92u: goto tr169;
		case 100u: goto tr169;
		case 108u: goto tr169;
		case 116u: goto tr169;
		case 124u: goto tr169;
		case 132u: goto tr170;
		case 140u: goto tr170;
		case 148u: goto tr170;
		case 156u: goto tr170;
		case 164u: goto tr170;
		case 172u: goto tr170;
		case 180u: goto tr170;
		case 188u: goto tr170;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr167;
	} else if ( (*( current_position)) >= 64u )
		goto tr168;
	goto tr165;
st72:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof72;
case 72:
	if ( (*( current_position)) == 15u )
		goto st73;
	goto tr16;
st73:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof73;
case 73:
	if ( (*( current_position)) == 31u )
		goto st74;
	goto tr16;
st74:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof74;
case 74:
	if ( (*( current_position)) == 132u )
		goto st75;
	goto tr16;
st75:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof75;
case 75:
	if ( (*( current_position)) == 0u )
		goto st76;
	goto tr16;
st76:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof76;
case 76:
	if ( (*( current_position)) == 0u )
		goto st77;
	goto tr16;
st77:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof77;
case 77:
	if ( (*( current_position)) == 0u )
		goto st78;
	goto tr16;
st78:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof78;
case 78:
	if ( (*( current_position)) == 0u )
		goto st79;
	goto tr16;
st79:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof79;
case 79:
	if ( (*( current_position)) == 0u )
		goto tr178;
	goto tr16;
st80:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof80;
case 80:
	switch( (*( current_position)) ) {
		case 46u: goto st72;
		case 102u: goto st81;
	}
	goto tr16;
st81:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof81;
case 81:
	switch( (*( current_position)) ) {
		case 46u: goto st72;
		case 102u: goto st82;
	}
	goto tr16;
st82:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof82;
case 82:
	switch( (*( current_position)) ) {
		case 46u: goto st72;
		case 102u: goto st83;
	}
	goto tr16;
st83:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof83;
case 83:
	switch( (*( current_position)) ) {
		case 46u: goto st72;
		case 102u: goto st84;
	}
	goto tr16;
st84:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof84;
case 84:
	if ( (*( current_position)) == 46u )
		goto st72;
	goto tr16;
st85:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof85;
case 85:
	switch( (*( current_position)) ) {
		case 4u: goto st86;
		case 5u: goto st87;
		case 12u: goto st86;
		case 13u: goto st87;
		case 20u: goto st86;
		case 21u: goto st87;
		case 28u: goto st86;
		case 29u: goto st87;
		case 36u: goto st86;
		case 37u: goto st87;
		case 44u: goto st86;
		case 45u: goto st87;
		case 52u: goto st86;
		case 53u: goto st87;
		case 60u: goto st86;
		case 61u: goto st87;
		case 68u: goto st92;
		case 76u: goto st92;
		case 84u: goto st92;
		case 92u: goto st92;
		case 100u: goto st92;
		case 108u: goto st92;
		case 116u: goto st92;
		case 124u: goto st92;
		case 132u: goto st93;
		case 140u: goto st93;
		case 148u: goto st93;
		case 156u: goto st93;
		case 164u: goto st93;
		case 172u: goto st93;
		case 180u: goto st93;
		case 188u: goto st93;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st87;
	} else if ( (*( current_position)) >= 64u )
		goto st91;
	goto st62;
st86:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof86;
case 86:
	switch( (*( current_position)) ) {
		case 5u: goto st87;
		case 13u: goto st87;
		case 21u: goto st87;
		case 29u: goto st87;
		case 37u: goto st87;
		case 45u: goto st87;
		case 53u: goto st87;
		case 61u: goto st87;
		case 69u: goto st87;
		case 77u: goto st87;
		case 85u: goto st87;
		case 93u: goto st87;
		case 101u: goto st87;
		case 109u: goto st87;
		case 117u: goto st87;
		case 125u: goto st87;
		case 133u: goto st87;
		case 141u: goto st87;
		case 149u: goto st87;
		case 157u: goto st87;
		case 165u: goto st87;
		case 173u: goto st87;
		case 181u: goto st87;
		case 189u: goto st87;
		case 197u: goto st87;
		case 205u: goto st87;
		case 213u: goto st87;
		case 221u: goto st87;
		case 229u: goto st87;
		case 237u: goto st87;
		case 245u: goto st87;
		case 253u: goto st87;
	}
	goto st62;
st87:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof87;
case 87:
	goto st88;
st88:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof88;
case 88:
	goto st89;
st89:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof89;
case 89:
	goto st90;
st90:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof90;
case 90:
	goto tr191;
st91:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof91;
case 91:
	goto tr192;
st92:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof92;
case 92:
	goto st91;
st93:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof93;
case 93:
	goto st87;
tr431:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st94;
st94:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof94;
case 94:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st39;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st35;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st35;
		} else
			goto tr16;
	} else
		goto st39;
	goto st10;
st95:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof95;
case 95:
	switch( (*( current_position)) ) {
		case 4u: goto st86;
		case 5u: goto st87;
		case 68u: goto st92;
		case 132u: goto st93;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st62;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st62;
		} else if ( (*( current_position)) >= 128u )
			goto st87;
	} else
		goto st91;
	goto tr16;
tr437:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st96;
st96:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof96;
case 96:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st7;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st3;
		} else
			goto tr16;
	} else
		goto st7;
	goto tr0;
tr127:
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st97;
tr380:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st97;
st97:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof97;
case 97:
	switch( (*( current_position)) ) {
		case 1u: goto st29;
		case 9u: goto st29;
		case 15u: goto st98;
		case 17u: goto st29;
		case 25u: goto st29;
		case 33u: goto st29;
		case 41u: goto st29;
		case 49u: goto st29;
		case 129u: goto st99;
		case 131u: goto st100;
		case 135u: goto st29;
		case 247u: goto st101;
		case 255u: goto st102;
	}
	goto tr16;
st98:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof98;
case 98:
	switch( (*( current_position)) ) {
		case 177u: goto st29;
		case 193u: goto st29;
	}
	goto tr16;
st99:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof99;
case 99:
	switch( (*( current_position)) ) {
		case 4u: goto st86;
		case 5u: goto st87;
		case 12u: goto st86;
		case 13u: goto st87;
		case 20u: goto st86;
		case 21u: goto st87;
		case 28u: goto st86;
		case 29u: goto st87;
		case 36u: goto st86;
		case 37u: goto st87;
		case 44u: goto st86;
		case 45u: goto st87;
		case 52u: goto st86;
		case 53u: goto st87;
		case 68u: goto st92;
		case 76u: goto st92;
		case 84u: goto st92;
		case 92u: goto st92;
		case 100u: goto st92;
		case 108u: goto st92;
		case 116u: goto st92;
		case 132u: goto st93;
		case 140u: goto st93;
		case 148u: goto st93;
		case 156u: goto st93;
		case 164u: goto st93;
		case 172u: goto st93;
		case 180u: goto st93;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st62;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st87;
	} else
		goto st91;
	goto tr16;
st100:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof100;
case 100:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st10;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st35;
	} else
		goto st39;
	goto tr16;
st101:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof101;
case 101:
	switch( (*( current_position)) ) {
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 84u: goto st8;
		case 92u: goto st8;
		case 148u: goto st9;
		case 156u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr0;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st102:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof102;
case 102:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr0;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto st3;
	} else
		goto st7;
	goto tr16;
tr128:
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st103;
tr383:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st103;
st103:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof103;
case 103:
	switch( (*( current_position)) ) {
		case 15u: goto st104;
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st104:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof104;
case 104:
	if ( (*( current_position)) == 56u )
		goto st105;
	goto tr16;
st105:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof105;
case 105:
	if ( (*( current_position)) == 241u )
		goto tr200;
	goto tr16;
tr129:
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st106;
tr397:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st106;
st106:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof106;
case 106:
	switch( (*( current_position)) ) {
		case 15u: goto st107;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st107:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof107;
case 107:
	switch( (*( current_position)) ) {
		case 184u: goto tr202;
		case 188u: goto tr203;
		case 189u: goto tr204;
	}
	goto tr16;
st108:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof108;
case 108:
	switch( (*( current_position)) ) {
		case 4u: goto st86;
		case 5u: goto st87;
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
		case 68u: goto st92;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st93;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st62;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st91;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st87;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st62;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr453:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st109;
st109:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof109;
case 109:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
tr269:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st110;
tr423:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st110;
st110:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof110;
case 110:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 12u: goto st111;
		case 13u: goto st112;
		case 20u: goto st111;
		case 21u: goto st112;
		case 28u: goto st111;
		case 29u: goto st112;
		case 36u: goto st111;
		case 37u: goto st112;
		case 44u: goto st111;
		case 45u: goto st112;
		case 52u: goto st111;
		case 53u: goto st112;
		case 60u: goto st111;
		case 61u: goto st112;
		case 68u: goto st117;
		case 76u: goto st117;
		case 84u: goto st117;
		case 92u: goto st117;
		case 100u: goto st117;
		case 108u: goto st117;
		case 116u: goto st117;
		case 124u: goto st117;
		case 132u: goto st118;
		case 140u: goto st118;
		case 148u: goto st118;
		case 156u: goto st118;
		case 164u: goto st118;
		case 172u: goto st118;
		case 180u: goto st118;
		case 188u: goto st118;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st112;
	} else if ( (*( current_position)) >= 64u )
		goto st116;
	goto st11;
tr264:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st111;
st111:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof111;
case 111:
	switch( (*( current_position)) ) {
		case 5u: goto st112;
		case 13u: goto st112;
		case 21u: goto st112;
		case 29u: goto st112;
		case 37u: goto st112;
		case 45u: goto st112;
		case 53u: goto st112;
		case 61u: goto st112;
		case 69u: goto st112;
		case 77u: goto st112;
		case 85u: goto st112;
		case 93u: goto st112;
		case 101u: goto st112;
		case 109u: goto st112;
		case 117u: goto st112;
		case 125u: goto st112;
		case 133u: goto st112;
		case 141u: goto st112;
		case 149u: goto st112;
		case 157u: goto st112;
		case 165u: goto st112;
		case 173u: goto st112;
		case 181u: goto st112;
		case 189u: goto st112;
		case 197u: goto st112;
		case 205u: goto st112;
		case 213u: goto st112;
		case 221u: goto st112;
		case 229u: goto st112;
		case 237u: goto st112;
		case 245u: goto st112;
		case 253u: goto st112;
	}
	goto st11;
tr265:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st112;
st112:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof112;
case 112:
	goto st113;
st113:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof113;
case 113:
	goto st114;
st114:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof114;
case 114:
	goto st115;
st115:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof115;
case 115:
	goto tr214;
tr266:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st116;
st116:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof116;
case 116:
	goto tr215;
tr267:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st117;
st117:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof117;
case 117:
	goto st116;
tr268:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st118;
st118:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof118;
case 118:
	goto st112;
tr426:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st119;
st119:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof119;
case 119:
	switch( (*( current_position)) ) {
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
		case 224u: goto st120;
		case 225u: goto st230;
		case 226u: goto st232;
		case 227u: goto st234;
		case 228u: goto st236;
		case 229u: goto st238;
		case 230u: goto st240;
		case 231u: goto st242;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st35;
	} else if ( (*( current_position)) >= 64u )
		goto st39;
	goto st10;
st120:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof120;
case 120:
	if ( (*( current_position)) == 224u )
		goto tr224;
	goto tr11;
tr224:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st245;
st245:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof245;
case 245:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr454;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr428:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st121;
st121:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof121;
case 121:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 232u: goto st122;
		case 233u: goto st137;
		case 234u: goto st145;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto tr0;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st122:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof122;
case 122:
	switch( (*( current_position)) ) {
		case 64u: goto tr228;
		case 68u: goto tr229;
		case 72u: goto tr228;
		case 76u: goto tr229;
		case 80u: goto tr228;
		case 84u: goto tr229;
		case 88u: goto tr228;
		case 92u: goto tr229;
		case 96u: goto tr228;
		case 100u: goto tr229;
		case 104u: goto tr228;
		case 108u: goto tr229;
		case 112u: goto tr228;
		case 116u: goto tr229;
		case 120u: goto tr230;
		case 124u: goto tr229;
		case 192u: goto tr231;
		case 196u: goto tr229;
		case 200u: goto tr231;
		case 204u: goto tr229;
		case 208u: goto tr231;
		case 212u: goto tr229;
		case 216u: goto tr231;
		case 220u: goto tr229;
		case 224u: goto tr231;
		case 228u: goto tr229;
		case 232u: goto tr231;
		case 236u: goto tr229;
		case 240u: goto tr231;
		case 244u: goto tr229;
		case 248u: goto tr231;
		case 252u: goto tr229;
	}
	goto tr16;
tr228:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st123;
st123:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof123;
case 123:
	switch( (*( current_position)) ) {
		case 166u: goto tr232;
		case 182u: goto tr232;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr232;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr232;
		} else
			goto tr232;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 204u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr232;
		} else if ( (*( current_position)) > 207u ) {
			if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
				goto tr233;
		} else
			goto tr233;
	} else
		goto tr232;
	goto tr16;
tr232:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st124;
tr337:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st124;
tr338:
	{
    SET_CPU_FEATURE(CPUFeature_FMA4);
  }
	goto st124;
st124:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof124;
case 124:
	switch( (*( current_position)) ) {
		case 4u: goto st126;
		case 5u: goto st127;
		case 12u: goto st126;
		case 13u: goto st127;
		case 20u: goto st126;
		case 21u: goto st127;
		case 28u: goto st126;
		case 29u: goto st127;
		case 36u: goto st126;
		case 37u: goto st127;
		case 44u: goto st126;
		case 45u: goto st127;
		case 52u: goto st126;
		case 53u: goto st127;
		case 60u: goto st126;
		case 61u: goto st127;
		case 68u: goto st132;
		case 76u: goto st132;
		case 84u: goto st132;
		case 92u: goto st132;
		case 100u: goto st132;
		case 108u: goto st132;
		case 116u: goto st132;
		case 124u: goto st132;
		case 132u: goto st133;
		case 140u: goto st133;
		case 148u: goto st133;
		case 156u: goto st133;
		case 164u: goto st133;
		case 172u: goto st133;
		case 180u: goto st133;
		case 188u: goto st133;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st127;
	} else if ( (*( current_position)) >= 64u )
		goto st131;
	goto st125;
tr243:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st125;
tr244:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st125;
st125:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof125;
case 125:
	switch( (*( current_position)) ) {
		case 0u: goto tr0;
		case 16u: goto tr0;
		case 32u: goto tr0;
		case 48u: goto tr0;
		case 64u: goto tr0;
		case 80u: goto tr0;
		case 96u: goto tr0;
		case 112u: goto tr0;
	}
	goto tr16;
st126:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof126;
case 126:
	switch( (*( current_position)) ) {
		case 5u: goto st127;
		case 13u: goto st127;
		case 21u: goto st127;
		case 29u: goto st127;
		case 37u: goto st127;
		case 45u: goto st127;
		case 53u: goto st127;
		case 61u: goto st127;
		case 69u: goto st127;
		case 77u: goto st127;
		case 85u: goto st127;
		case 93u: goto st127;
		case 101u: goto st127;
		case 109u: goto st127;
		case 117u: goto st127;
		case 125u: goto st127;
		case 133u: goto st127;
		case 141u: goto st127;
		case 149u: goto st127;
		case 157u: goto st127;
		case 165u: goto st127;
		case 173u: goto st127;
		case 181u: goto st127;
		case 189u: goto st127;
		case 197u: goto st127;
		case 205u: goto st127;
		case 213u: goto st127;
		case 221u: goto st127;
		case 229u: goto st127;
		case 237u: goto st127;
		case 245u: goto st127;
		case 253u: goto st127;
	}
	goto st125;
st127:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof127;
case 127:
	goto st128;
st128:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof128;
case 128:
	goto st129;
st129:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof129;
case 129:
	goto st130;
st130:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof130;
case 130:
	goto tr243;
st131:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof131;
case 131:
	goto tr244;
st132:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof132;
case 132:
	goto st131;
st133:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof133;
case 133:
	goto st127;
tr229:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st134;
st134:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof134;
case 134:
	if ( (*( current_position)) == 162u )
		goto tr232;
	goto tr16;
tr230:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st135;
st135:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof135;
case 135:
	switch( (*( current_position)) ) {
		case 166u: goto tr232;
		case 182u: goto tr232;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr232;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr232;
		} else
			goto tr232;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 192u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr232;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr233;
			} else if ( (*( current_position)) >= 204u )
				goto tr233;
		} else
			goto tr233;
	} else
		goto tr232;
	goto tr16;
tr231:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st136;
st136:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof136;
case 136:
	if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
		goto tr232;
	goto tr16;
st137:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof137;
case 137:
	switch( (*( current_position)) ) {
		case 64u: goto tr245;
		case 72u: goto tr245;
		case 80u: goto tr245;
		case 88u: goto tr245;
		case 96u: goto tr245;
		case 104u: goto tr245;
		case 112u: goto tr245;
		case 120u: goto tr246;
		case 124u: goto tr247;
		case 192u: goto tr248;
		case 200u: goto tr248;
		case 208u: goto tr248;
		case 216u: goto tr248;
		case 224u: goto tr248;
		case 232u: goto tr248;
		case 240u: goto tr248;
		case 248u: goto tr248;
	}
	goto tr16;
tr245:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st138;
st138:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof138;
case 138:
	switch( (*( current_position)) ) {
		case 1u: goto st139;
		case 2u: goto st140;
	}
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr251;
	goto tr16;
st139:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof139;
case 139:
	switch( (*( current_position)) ) {
		case 12u: goto tr253;
		case 13u: goto tr254;
		case 20u: goto tr253;
		case 21u: goto tr254;
		case 28u: goto tr253;
		case 29u: goto tr254;
		case 36u: goto tr253;
		case 37u: goto tr254;
		case 44u: goto tr253;
		case 45u: goto tr254;
		case 52u: goto tr253;
		case 53u: goto tr254;
		case 60u: goto tr253;
		case 61u: goto tr254;
		case 76u: goto tr256;
		case 84u: goto tr256;
		case 92u: goto tr256;
		case 100u: goto tr256;
		case 108u: goto tr256;
		case 116u: goto tr256;
		case 124u: goto tr256;
		case 140u: goto tr257;
		case 148u: goto tr257;
		case 156u: goto tr257;
		case 164u: goto tr257;
		case 172u: goto tr257;
		case 180u: goto tr257;
		case 188u: goto tr257;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) > 7u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 71u )
				goto tr16;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 136u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr16;
		} else if ( (*( current_position)) > 191u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr16;
		} else
			goto tr254;
	} else
		goto tr255;
	goto tr252;
st140:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof140;
case 140:
	switch( (*( current_position)) ) {
		case 12u: goto tr253;
		case 13u: goto tr254;
		case 52u: goto tr253;
		case 53u: goto tr254;
		case 76u: goto tr256;
		case 116u: goto tr256;
		case 140u: goto tr257;
		case 180u: goto tr257;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr252;
		} else if ( (*( current_position)) > 55u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto tr255;
		} else
			goto tr252;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto tr254;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr252;
			} else if ( (*( current_position)) >= 200u )
				goto tr252;
		} else
			goto tr254;
	} else
		goto tr255;
	goto tr16;
tr246:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st141;
st141:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof141;
case 141:
	switch( (*( current_position)) ) {
		case 1u: goto st139;
		case 2u: goto st140;
		case 18u: goto st142;
		case 203u: goto tr251;
		case 219u: goto tr251;
	}
	if ( (*( current_position)) < 198u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 131u )
				goto tr251;
		} else if ( (*( current_position)) > 155u ) {
			if ( 193u <= (*( current_position)) && (*( current_position)) <= 195u )
				goto tr251;
		} else
			goto tr251;
	} else if ( (*( current_position)) > 199u ) {
		if ( (*( current_position)) < 214u ) {
			if ( 209u <= (*( current_position)) && (*( current_position)) <= 211u )
				goto tr251;
		} else if ( (*( current_position)) > 215u ) {
			if ( 225u <= (*( current_position)) && (*( current_position)) <= 227u )
				goto tr251;
		} else
			goto tr251;
	} else
		goto tr251;
	goto tr16;
st142:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof142;
case 142:
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr259;
	goto tr16;
tr247:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st143;
st143:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof143;
case 143:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 129u )
		goto tr251;
	goto tr16;
tr248:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st144;
st144:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof144;
case 144:
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr251;
	goto tr16;
st145:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof145;
case 145:
	switch( (*( current_position)) ) {
		case 64u: goto tr260;
		case 72u: goto tr260;
		case 80u: goto tr260;
		case 88u: goto tr260;
		case 96u: goto tr260;
		case 104u: goto tr260;
		case 112u: goto tr260;
		case 120u: goto tr261;
	}
	goto tr16;
tr260:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st146;
st146:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof146;
case 146:
	if ( (*( current_position)) == 18u )
		goto st147;
	goto tr16;
st147:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof147;
case 147:
	switch( (*( current_position)) ) {
		case 4u: goto tr264;
		case 5u: goto tr265;
		case 12u: goto tr264;
		case 13u: goto tr265;
		case 68u: goto tr267;
		case 76u: goto tr267;
		case 132u: goto tr268;
		case 140u: goto tr268;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr263;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr263;
		} else if ( (*( current_position)) >= 128u )
			goto tr265;
	} else
		goto tr266;
	goto tr16;
tr261:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st148;
st148:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof148;
case 148:
	switch( (*( current_position)) ) {
		case 16u: goto tr269;
		case 18u: goto st147;
	}
	goto tr16;
tr432:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st149;
st149:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof149;
case 149:
	switch( (*( current_position)) ) {
		case 225u: goto st150;
		case 226u: goto st172;
		case 227u: goto st181;
	}
	goto tr16;
st150:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof150;
case 150:
	switch( (*( current_position)) ) {
		case 65u: goto tr274;
		case 66u: goto tr275;
		case 67u: goto tr276;
		case 68u: goto tr277;
		case 69u: goto tr278;
		case 70u: goto tr279;
		case 71u: goto tr280;
		case 73u: goto tr274;
		case 74u: goto tr275;
		case 75u: goto tr276;
		case 76u: goto tr277;
		case 77u: goto tr278;
		case 78u: goto tr279;
		case 79u: goto tr280;
		case 81u: goto tr274;
		case 82u: goto tr275;
		case 83u: goto tr276;
		case 84u: goto tr277;
		case 85u: goto tr278;
		case 86u: goto tr279;
		case 87u: goto tr280;
		case 89u: goto tr274;
		case 90u: goto tr275;
		case 91u: goto tr276;
		case 92u: goto tr277;
		case 93u: goto tr278;
		case 94u: goto tr279;
		case 95u: goto tr280;
		case 97u: goto tr274;
		case 98u: goto tr275;
		case 99u: goto tr276;
		case 100u: goto tr277;
		case 101u: goto tr278;
		case 102u: goto tr279;
		case 103u: goto tr280;
		case 105u: goto tr274;
		case 106u: goto tr275;
		case 107u: goto tr276;
		case 108u: goto tr277;
		case 109u: goto tr278;
		case 110u: goto tr279;
		case 111u: goto tr280;
		case 113u: goto tr274;
		case 114u: goto tr275;
		case 115u: goto tr276;
		case 116u: goto tr277;
		case 117u: goto tr278;
		case 118u: goto tr279;
		case 119u: goto tr280;
		case 120u: goto tr281;
		case 121u: goto tr282;
		case 122u: goto tr283;
		case 123u: goto tr284;
		case 124u: goto tr285;
		case 125u: goto tr286;
		case 126u: goto tr287;
		case 127u: goto tr288;
	}
	if ( 64u <= (*( current_position)) && (*( current_position)) <= 112u )
		goto tr273;
	goto tr16;
tr273:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st151;
tr354:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st151;
st151:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof151;
case 151:
	switch( (*( current_position)) ) {
		case 18u: goto st152;
		case 22u: goto st152;
		case 23u: goto tr291;
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 198u: goto tr292;
	}
	if ( (*( current_position)) < 46u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr290;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) > 89u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr290;
		} else if ( (*( current_position)) >= 84u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
st152:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof152;
case 152:
	switch( (*( current_position)) ) {
		case 4u: goto tr294;
		case 5u: goto tr295;
		case 12u: goto tr294;
		case 13u: goto tr295;
		case 20u: goto tr294;
		case 21u: goto tr295;
		case 28u: goto tr294;
		case 29u: goto tr295;
		case 36u: goto tr294;
		case 37u: goto tr295;
		case 44u: goto tr294;
		case 45u: goto tr295;
		case 52u: goto tr294;
		case 53u: goto tr295;
		case 60u: goto tr294;
		case 61u: goto tr295;
		case 68u: goto tr297;
		case 76u: goto tr297;
		case 84u: goto tr297;
		case 92u: goto tr297;
		case 100u: goto tr297;
		case 108u: goto tr297;
		case 116u: goto tr297;
		case 124u: goto tr297;
		case 132u: goto tr298;
		case 140u: goto tr298;
		case 148u: goto tr298;
		case 156u: goto tr298;
		case 164u: goto tr298;
		case 172u: goto tr298;
		case 180u: goto tr298;
		case 188u: goto tr298;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr295;
	} else if ( (*( current_position)) >= 64u )
		goto tr296;
	goto tr293;
tr274:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st153;
tr355:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st153;
st153:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof153;
case 153:
	switch( (*( current_position)) ) {
		case 18u: goto tr291;
		case 81u: goto tr290;
		case 115u: goto st155;
		case 194u: goto tr292;
		case 198u: goto tr292;
	}
	if ( (*( current_position)) < 116u ) {
		if ( (*( current_position)) < 46u ) {
			if ( (*( current_position)) > 21u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr291;
			} else if ( (*( current_position)) >= 20u )
				goto tr290;
		} else if ( (*( current_position)) > 47u ) {
			if ( (*( current_position)) < 92u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 89u )
					goto tr290;
			} else if ( (*( current_position)) > 109u ) {
				if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
					goto st154;
			} else
				goto tr290;
		} else
			goto tr290;
	} else if ( (*( current_position)) > 118u ) {
		if ( (*( current_position)) < 216u ) {
			if ( (*( current_position)) > 125u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr290;
			} else if ( (*( current_position)) >= 124u )
				goto tr290;
		} else if ( (*( current_position)) > 229u ) {
			if ( (*( current_position)) < 241u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr290;
			} else if ( (*( current_position)) > 246u ) {
				if ( 248u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr290;
			} else
				goto tr290;
		} else
			goto tr290;
	} else
		goto tr290;
	goto tr16;
st154:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof154;
case 154:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr301;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr301;
	} else
		goto tr301;
	goto tr16;
st155:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof155;
case 155:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr301;
	} else if ( (*( current_position)) >= 208u )
		goto tr301;
	goto tr16;
tr275:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st156;
st156:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof156;
case 156:
	switch( (*( current_position)) ) {
		case 42u: goto tr290;
		case 81u: goto tr290;
		case 83u: goto tr290;
		case 194u: goto tr292;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr290;
	} else if ( (*( current_position)) >= 88u )
		goto tr290;
	goto tr16;
tr276:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st157;
st157:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof157;
case 157:
	switch( (*( current_position)) ) {
		case 42u: goto tr290;
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 208u: goto tr290;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr290;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr277:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st158;
tr358:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st158;
st158:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof158;
case 158:
	switch( (*( current_position)) ) {
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 198u: goto tr292;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr290;
	} else if ( (*( current_position)) > 89u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr278:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st159;
tr359:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st159;
st159:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof159;
case 159:
	switch( (*( current_position)) ) {
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 198u: goto tr292;
		case 208u: goto tr290;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr290;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr290;
		} else if ( (*( current_position)) >= 92u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr279:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st160;
tr360:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st160;
st160:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof160;
case 160:
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr302;
	goto tr16;
tr280:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st161;
tr361:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st161;
st161:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof161;
case 161:
	if ( (*( current_position)) == 208u )
		goto tr290;
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr290;
	} else if ( (*( current_position)) >= 16u )
		goto tr302;
	goto tr16;
tr281:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st162;
tr362:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st162;
st162:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof162;
case 162:
	switch( (*( current_position)) ) {
		case 18u: goto st152;
		case 19u: goto tr291;
		case 22u: goto st152;
		case 23u: goto tr291;
		case 43u: goto tr291;
		case 80u: goto tr302;
		case 119u: goto tr293;
		case 174u: goto st163;
		case 194u: goto tr292;
		case 198u: goto tr292;
	}
	if ( (*( current_position)) < 40u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr290;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 47u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr290;
		} else if ( (*( current_position)) >= 46u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
st163:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof163;
case 163:
	switch( (*( current_position)) ) {
		case 20u: goto tr294;
		case 21u: goto tr295;
		case 28u: goto tr294;
		case 29u: goto tr295;
		case 84u: goto tr297;
		case 92u: goto tr297;
		case 148u: goto tr298;
		case 156u: goto tr298;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr293;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr295;
	} else
		goto tr296;
	goto tr16;
tr282:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st164;
tr363:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st164;
st164:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof164;
case 164:
	switch( (*( current_position)) ) {
		case 43u: goto tr291;
		case 80u: goto tr302;
		case 81u: goto tr290;
		case 112u: goto tr292;
		case 115u: goto st155;
		case 127u: goto tr290;
		case 194u: goto tr292;
		case 196u: goto st165;
		case 197u: goto tr305;
		case 198u: goto tr292;
		case 215u: goto tr302;
		case 231u: goto tr291;
		case 247u: goto tr302;
	}
	if ( (*( current_position)) < 84u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 18u <= (*( current_position)) && (*( current_position)) <= 19u )
					goto tr291;
			} else if ( (*( current_position)) >= 16u )
				goto tr290;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) < 40u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr291;
			} else if ( (*( current_position)) > 41u ) {
				if ( 46u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr290;
			} else
				goto tr290;
		} else
			goto tr290;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 124u ) {
			if ( (*( current_position)) > 114u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr290;
			} else if ( (*( current_position)) >= 113u )
				goto st154;
		} else if ( (*( current_position)) > 125u ) {
			if ( (*( current_position)) < 216u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr290;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr290;
			} else
				goto tr290;
		} else
			goto tr290;
	} else
		goto tr290;
	goto tr16;
st165:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof165;
case 165:
	switch( (*( current_position)) ) {
		case 4u: goto tr306;
		case 5u: goto tr307;
		case 12u: goto tr306;
		case 13u: goto tr307;
		case 20u: goto tr306;
		case 21u: goto tr307;
		case 28u: goto tr306;
		case 29u: goto tr307;
		case 36u: goto tr306;
		case 37u: goto tr307;
		case 44u: goto tr306;
		case 45u: goto tr307;
		case 52u: goto tr306;
		case 53u: goto tr307;
		case 60u: goto tr306;
		case 61u: goto tr307;
		case 68u: goto tr309;
		case 76u: goto tr309;
		case 84u: goto tr309;
		case 92u: goto tr309;
		case 100u: goto tr309;
		case 108u: goto tr309;
		case 116u: goto tr309;
		case 124u: goto tr309;
		case 132u: goto tr310;
		case 140u: goto tr310;
		case 148u: goto tr310;
		case 156u: goto tr310;
		case 164u: goto tr310;
		case 172u: goto tr310;
		case 180u: goto tr310;
		case 188u: goto tr310;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr307;
	} else if ( (*( current_position)) >= 64u )
		goto tr308;
	goto tr301;
tr283:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st166;
st166:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof166;
case 166:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 22u: goto tr290;
		case 42u: goto tr290;
		case 111u: goto tr290;
		case 112u: goto tr292;
		case 194u: goto tr292;
		case 230u: goto tr290;
	}
	if ( (*( current_position)) < 81u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr290;
		} else if ( (*( current_position)) >= 16u )
			goto tr291;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr290;
		} else if ( (*( current_position)) >= 88u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr284:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st167;
st167:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof167;
case 167:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 42u: goto tr290;
		case 81u: goto tr290;
		case 112u: goto tr292;
		case 194u: goto tr292;
		case 208u: goto tr290;
		case 230u: goto tr290;
		case 240u: goto tr291;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr290;
		} else if ( (*( current_position)) >= 16u )
			goto tr291;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr290;
		} else if ( (*( current_position)) >= 92u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr285:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st168;
tr366:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st168;
st168:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof168;
case 168:
	switch( (*( current_position)) ) {
		case 43u: goto tr291;
		case 80u: goto tr302;
		case 119u: goto tr293;
		case 194u: goto tr292;
		case 198u: goto tr292;
	}
	if ( (*( current_position)) < 20u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr290;
	} else if ( (*( current_position)) > 21u ) {
		if ( (*( current_position)) > 41u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr290;
		} else if ( (*( current_position)) >= 40u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr286:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st169;
tr367:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st169;
st169:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof169;
case 169:
	switch( (*( current_position)) ) {
		case 43u: goto tr291;
		case 80u: goto tr302;
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 198u: goto tr292;
		case 208u: goto tr290;
		case 214u: goto tr290;
		case 230u: goto tr290;
		case 231u: goto tr291;
	}
	if ( (*( current_position)) < 40u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto tr290;
		} else if ( (*( current_position)) >= 16u )
			goto tr290;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr290;
		} else if ( (*( current_position)) >= 84u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr287:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st170;
tr368:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st170;
st170:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof170;
case 170:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 22u: goto tr290;
		case 91u: goto tr290;
		case 127u: goto tr290;
		case 230u: goto tr290;
	}
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr302;
	goto tr16;
tr288:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st171;
tr369:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st171;
st171:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof171;
case 171:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 208u: goto tr290;
		case 230u: goto tr290;
		case 240u: goto tr291;
	}
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr290;
	} else if ( (*( current_position)) >= 16u )
		goto tr302;
	goto tr16;
st172:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof172;
case 172:
	switch( (*( current_position)) ) {
		case 64u: goto tr311;
		case 65u: goto tr312;
		case 69u: goto tr313;
		case 72u: goto tr311;
		case 73u: goto tr312;
		case 77u: goto tr313;
		case 80u: goto tr311;
		case 81u: goto tr312;
		case 85u: goto tr313;
		case 88u: goto tr311;
		case 89u: goto tr312;
		case 93u: goto tr313;
		case 96u: goto tr311;
		case 97u: goto tr312;
		case 101u: goto tr313;
		case 104u: goto tr311;
		case 105u: goto tr312;
		case 109u: goto tr313;
		case 112u: goto tr311;
		case 113u: goto tr312;
		case 117u: goto tr313;
		case 120u: goto tr311;
		case 121u: goto tr314;
		case 125u: goto tr315;
		case 193u: goto tr316;
		case 197u: goto tr317;
		case 201u: goto tr316;
		case 205u: goto tr317;
		case 209u: goto tr316;
		case 213u: goto tr317;
		case 217u: goto tr316;
		case 221u: goto tr317;
		case 225u: goto tr316;
		case 229u: goto tr317;
		case 233u: goto tr316;
		case 237u: goto tr317;
		case 241u: goto tr316;
		case 245u: goto tr317;
		case 249u: goto tr316;
		case 253u: goto tr317;
	}
	goto tr16;
tr311:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st173;
st173:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof173;
case 173:
	switch( (*( current_position)) ) {
		case 242u: goto tr318;
		case 243u: goto st174;
		case 247u: goto tr318;
	}
	goto tr16;
st174:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof174;
case 174:
	switch( (*( current_position)) ) {
		case 12u: goto tr321;
		case 13u: goto tr322;
		case 20u: goto tr321;
		case 21u: goto tr322;
		case 28u: goto tr321;
		case 29u: goto tr322;
		case 76u: goto tr324;
		case 84u: goto tr324;
		case 92u: goto tr324;
		case 140u: goto tr325;
		case 148u: goto tr325;
		case 156u: goto tr325;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr320;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 159u ) {
			if ( 200u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr320;
		} else if ( (*( current_position)) >= 136u )
			goto tr322;
	} else
		goto tr323;
	goto tr16;
tr312:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st175;
st175:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof175;
case 175:
	if ( (*( current_position)) == 43u )
		goto tr290;
	if ( (*( current_position)) < 55u ) {
		if ( (*( current_position)) < 40u ) {
			if ( (*( current_position)) <= 13u )
				goto tr290;
		} else if ( (*( current_position)) > 41u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr291;
		} else
			goto tr290;
	} else if ( (*( current_position)) > 64u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
				goto tr326;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr327;
			} else if ( (*( current_position)) >= 182u )
				goto tr326;
		} else
			goto tr326;
	} else
		goto tr290;
	goto tr16;
tr313:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st176;
st176:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof176;
case 176:
	switch( (*( current_position)) ) {
		case 154u: goto tr326;
		case 156u: goto tr326;
		case 158u: goto tr326;
		case 170u: goto tr326;
		case 172u: goto tr326;
		case 174u: goto tr326;
		case 186u: goto tr326;
		case 188u: goto tr326;
		case 190u: goto tr326;
	}
	if ( (*( current_position)) < 150u ) {
		if ( (*( current_position)) > 13u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr291;
		} else if ( (*( current_position)) >= 12u )
			goto tr290;
	} else if ( (*( current_position)) > 152u ) {
		if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr326;
		} else if ( (*( current_position)) >= 166u )
			goto tr326;
	} else
		goto tr326;
	goto tr16;
tr314:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st177;
st177:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof177;
case 177:
	switch( (*( current_position)) ) {
		case 19u: goto tr328;
		case 23u: goto tr290;
		case 24u: goto tr291;
		case 42u: goto tr291;
	}
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
					goto tr290;
			} else
				goto tr290;
		} else if ( (*( current_position)) > 37u ) {
			if ( (*( current_position)) > 43u ) {
				if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr291;
			} else if ( (*( current_position)) >= 40u )
				goto tr290;
		} else
			goto tr290;
	} else if ( (*( current_position)) > 53u ) {
		if ( (*( current_position)) < 166u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr326;
			} else if ( (*( current_position)) >= 55u )
				goto tr290;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr327;
			} else if ( (*( current_position)) >= 182u )
				goto tr326;
		} else
			goto tr326;
	} else
		goto tr290;
	goto tr16;
tr315:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st178;
st178:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof178;
case 178:
	switch( (*( current_position)) ) {
		case 19u: goto tr328;
		case 23u: goto tr290;
		case 154u: goto tr326;
		case 156u: goto tr326;
		case 158u: goto tr326;
		case 170u: goto tr326;
		case 172u: goto tr326;
		case 174u: goto tr326;
		case 186u: goto tr326;
		case 188u: goto tr326;
		case 190u: goto tr326;
	}
	if ( (*( current_position)) < 44u ) {
		if ( (*( current_position)) > 15u ) {
			if ( 24u <= (*( current_position)) && (*( current_position)) <= 26u )
				goto tr291;
		} else if ( (*( current_position)) >= 12u )
			goto tr290;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
				goto tr326;
		} else if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr326;
		} else
			goto tr326;
	} else
		goto tr291;
	goto tr16;
tr316:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st179;
st179:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof179;
case 179:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr326;
	} else if ( (*( current_position)) > 175u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr326;
	} else
		goto tr326;
	goto tr16;
tr317:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st180;
st180:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof180;
case 180:
	switch( (*( current_position)) ) {
		case 154u: goto tr326;
		case 156u: goto tr326;
		case 158u: goto tr326;
		case 170u: goto tr326;
		case 172u: goto tr326;
		case 174u: goto tr326;
		case 186u: goto tr326;
		case 188u: goto tr326;
		case 190u: goto tr326;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto tr326;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto tr326;
	} else
		goto tr326;
	goto tr16;
st181:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof181;
case 181:
	switch( (*( current_position)) ) {
		case 65u: goto tr329;
		case 69u: goto tr330;
		case 73u: goto tr329;
		case 77u: goto tr330;
		case 81u: goto tr329;
		case 85u: goto tr330;
		case 89u: goto tr329;
		case 93u: goto tr330;
		case 97u: goto tr329;
		case 101u: goto tr330;
		case 105u: goto tr329;
		case 109u: goto tr330;
		case 113u: goto tr329;
		case 117u: goto tr330;
		case 121u: goto tr331;
		case 125u: goto tr332;
		case 193u: goto tr333;
		case 197u: goto tr334;
		case 201u: goto tr333;
		case 205u: goto tr334;
		case 209u: goto tr333;
		case 213u: goto tr334;
		case 217u: goto tr333;
		case 221u: goto tr334;
		case 225u: goto tr333;
		case 229u: goto tr334;
		case 233u: goto tr333;
		case 237u: goto tr334;
		case 241u: goto tr333;
		case 245u: goto tr334;
		case 249u: goto tr333;
		case 253u: goto tr334;
	}
	goto tr16;
tr329:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st182;
st182:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof182;
case 182:
	switch( (*( current_position)) ) {
		case 33u: goto tr292;
		case 68u: goto tr335;
		case 223u: goto tr339;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr292;
		} else if ( (*( current_position)) > 66u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
				goto tr336;
		} else
			goto tr292;
	} else if ( (*( current_position)) > 76u ) {
		if ( (*( current_position)) < 104u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr338;
		} else if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr338;
		} else
			goto tr338;
	} else
		goto tr337;
	goto tr16;
tr336:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st183;
st183:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof183;
case 183:
	switch( (*( current_position)) ) {
		case 4u: goto st185;
		case 5u: goto st186;
		case 12u: goto st185;
		case 13u: goto st186;
		case 20u: goto st185;
		case 21u: goto st186;
		case 28u: goto st185;
		case 29u: goto st186;
		case 36u: goto st185;
		case 37u: goto st186;
		case 44u: goto st185;
		case 45u: goto st186;
		case 52u: goto st185;
		case 53u: goto st186;
		case 60u: goto st185;
		case 61u: goto st186;
		case 68u: goto st191;
		case 76u: goto st191;
		case 84u: goto st191;
		case 92u: goto st191;
		case 100u: goto st191;
		case 108u: goto st191;
		case 116u: goto st191;
		case 124u: goto st191;
		case 132u: goto st192;
		case 140u: goto st192;
		case 148u: goto st192;
		case 156u: goto st192;
		case 164u: goto st192;
		case 172u: goto st192;
		case 180u: goto st192;
		case 188u: goto st192;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st186;
	} else if ( (*( current_position)) >= 64u )
		goto st190;
	goto st184;
tr350:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st184;
tr351:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st184;
st184:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof184;
case 184:
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 16u ) {
			if ( (*( current_position)) <= 3u )
				goto tr346;
		} else if ( (*( current_position)) > 19u ) {
			if ( 32u <= (*( current_position)) && (*( current_position)) <= 35u )
				goto tr346;
		} else
			goto tr346;
	} else if ( (*( current_position)) > 51u ) {
		if ( (*( current_position)) < 80u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 67u )
				goto tr346;
		} else if ( (*( current_position)) > 83u ) {
			if ( (*( current_position)) > 99u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 115u )
					goto tr346;
			} else if ( (*( current_position)) >= 96u )
				goto tr346;
		} else
			goto tr346;
	} else
		goto tr346;
	goto tr16;
st185:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof185;
case 185:
	switch( (*( current_position)) ) {
		case 5u: goto st186;
		case 13u: goto st186;
		case 21u: goto st186;
		case 29u: goto st186;
		case 37u: goto st186;
		case 45u: goto st186;
		case 53u: goto st186;
		case 61u: goto st186;
		case 69u: goto st186;
		case 77u: goto st186;
		case 85u: goto st186;
		case 93u: goto st186;
		case 101u: goto st186;
		case 109u: goto st186;
		case 117u: goto st186;
		case 125u: goto st186;
		case 133u: goto st186;
		case 141u: goto st186;
		case 149u: goto st186;
		case 157u: goto st186;
		case 165u: goto st186;
		case 173u: goto st186;
		case 181u: goto st186;
		case 189u: goto st186;
		case 197u: goto st186;
		case 205u: goto st186;
		case 213u: goto st186;
		case 221u: goto st186;
		case 229u: goto st186;
		case 237u: goto st186;
		case 245u: goto st186;
		case 253u: goto st186;
	}
	goto st184;
st186:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof186;
case 186:
	goto st187;
st187:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof187;
case 187:
	goto st188;
st188:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof188;
case 188:
	goto st189;
st189:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof189;
case 189:
	goto tr350;
st190:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof190;
case 190:
	goto tr351;
st191:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof191;
case 191:
	goto st190;
st192:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof192;
case 192:
	goto st186;
tr330:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st193;
st193:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof193;
case 193:
	switch( (*( current_position)) ) {
		case 6u: goto tr292;
		case 64u: goto tr292;
	}
	if ( (*( current_position)) < 92u ) {
		if ( (*( current_position)) < 12u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
				goto tr292;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 73u ) {
				if ( 74u <= (*( current_position)) && (*( current_position)) <= 75u )
					goto tr337;
			} else if ( (*( current_position)) >= 72u )
				goto tr336;
		} else
			goto tr292;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 108u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto tr338;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr338;
			} else if ( (*( current_position)) >= 120u )
				goto tr338;
		} else
			goto tr338;
	} else
		goto tr338;
	goto tr16;
tr331:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st194;
st194:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof194;
case 194:
	switch( (*( current_position)) ) {
		case 22u: goto tr292;
		case 23u: goto tr352;
		case 29u: goto tr353;
		case 32u: goto st165;
		case 68u: goto tr335;
		case 223u: goto tr339;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 5u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
					goto tr292;
			} else if ( (*( current_position)) >= 4u )
				goto tr292;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) > 34u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 66u )
					goto tr292;
			} else if ( (*( current_position)) >= 33u )
				goto tr292;
		} else
			goto st165;
	} else if ( (*( current_position)) > 73u ) {
		if ( (*( current_position)) < 96u ) {
			if ( (*( current_position)) > 76u ) {
				if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
					goto tr338;
			} else if ( (*( current_position)) >= 74u )
				goto tr337;
		} else if ( (*( current_position)) > 99u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr338;
			} else if ( (*( current_position)) >= 104u )
				goto tr338;
		} else
			goto tr292;
	} else
		goto tr336;
	goto tr16;
tr352:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st195;
st195:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof195;
case 195:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 63u )
			goto st10;
	} else if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st35;
	} else
		goto st39;
	goto tr16;
tr332:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st196;
st196:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof196;
case 196:
	switch( (*( current_position)) ) {
		case 29u: goto tr353;
		case 64u: goto tr292;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 12u ) {
			if ( (*( current_position)) > 6u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
					goto tr292;
			} else if ( (*( current_position)) >= 4u )
				goto tr292;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 25u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
					goto tr336;
			} else if ( (*( current_position)) >= 24u )
				goto tr292;
		} else
			goto tr292;
	} else if ( (*( current_position)) > 75u ) {
		if ( (*( current_position)) < 108u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
					goto tr338;
			} else if ( (*( current_position)) >= 92u )
				goto tr338;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr338;
			} else if ( (*( current_position)) >= 120u )
				goto tr338;
		} else
			goto tr338;
	} else
		goto tr337;
	goto tr16;
tr333:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st197;
st197:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof197;
case 197:
	if ( (*( current_position)) < 92u ) {
		if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
			goto tr336;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr338;
		} else if ( (*( current_position)) >= 104u )
			goto tr338;
	} else
		goto tr338;
	goto tr16;
tr334:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st198;
st198:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof198;
case 198:
	if ( (*( current_position)) < 104u ) {
		if ( (*( current_position)) > 73u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr338;
		} else if ( (*( current_position)) >= 72u )
			goto tr336;
	} else if ( (*( current_position)) > 105u ) {
		if ( (*( current_position)) < 120u ) {
			if ( 108u <= (*( current_position)) && (*( current_position)) <= 109u )
				goto tr338;
		} else if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr338;
		} else
			goto tr338;
	} else
		goto tr338;
	goto tr16;
tr433:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st199;
st199:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof199;
case 199:
	switch( (*( current_position)) ) {
		case 193u: goto tr355;
		case 194u: goto tr356;
		case 195u: goto tr357;
		case 196u: goto tr358;
		case 197u: goto tr359;
		case 198u: goto tr360;
		case 199u: goto tr361;
		case 201u: goto tr355;
		case 202u: goto tr356;
		case 203u: goto tr357;
		case 204u: goto tr358;
		case 205u: goto tr359;
		case 206u: goto tr360;
		case 207u: goto tr361;
		case 209u: goto tr355;
		case 210u: goto tr356;
		case 211u: goto tr357;
		case 212u: goto tr358;
		case 213u: goto tr359;
		case 214u: goto tr360;
		case 215u: goto tr361;
		case 217u: goto tr355;
		case 218u: goto tr356;
		case 219u: goto tr357;
		case 220u: goto tr358;
		case 221u: goto tr359;
		case 222u: goto tr360;
		case 223u: goto tr361;
		case 225u: goto tr355;
		case 226u: goto tr356;
		case 227u: goto tr357;
		case 228u: goto tr358;
		case 229u: goto tr359;
		case 230u: goto tr360;
		case 231u: goto tr361;
		case 233u: goto tr355;
		case 234u: goto tr356;
		case 235u: goto tr357;
		case 236u: goto tr358;
		case 237u: goto tr359;
		case 238u: goto tr360;
		case 239u: goto tr361;
		case 241u: goto tr355;
		case 242u: goto tr356;
		case 243u: goto tr357;
		case 244u: goto tr358;
		case 245u: goto tr359;
		case 246u: goto tr360;
		case 247u: goto tr361;
		case 248u: goto tr362;
		case 249u: goto tr363;
		case 250u: goto tr364;
		case 251u: goto tr365;
		case 252u: goto tr366;
		case 253u: goto tr367;
		case 254u: goto tr368;
		case 255u: goto tr369;
	}
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 240u )
		goto tr354;
	goto tr16;
tr356:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st200;
st200:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof200;
case 200:
	switch( (*( current_position)) ) {
		case 81u: goto tr290;
		case 83u: goto tr290;
		case 194u: goto tr292;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr290;
	} else if ( (*( current_position)) >= 88u )
		goto tr290;
	goto tr16;
tr357:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st201;
st201:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof201;
case 201:
	switch( (*( current_position)) ) {
		case 81u: goto tr290;
		case 194u: goto tr292;
		case 208u: goto tr290;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr290;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr364:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st202;
st202:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof202;
case 202:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 22u: goto tr290;
		case 111u: goto tr290;
		case 112u: goto tr292;
		case 194u: goto tr292;
		case 230u: goto tr290;
	}
	if ( (*( current_position)) < 81u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr291;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr290;
		} else if ( (*( current_position)) >= 88u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr365:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st203;
st203:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof203;
case 203:
	switch( (*( current_position)) ) {
		case 18u: goto tr290;
		case 81u: goto tr290;
		case 112u: goto tr292;
		case 194u: goto tr292;
		case 208u: goto tr290;
		case 230u: goto tr290;
		case 240u: goto tr291;
	}
	if ( (*( current_position)) < 88u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr291;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr290;
		} else if ( (*( current_position)) >= 92u )
			goto tr290;
	} else
		goto tr290;
	goto tr16;
tr434:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st204;
st204:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof204;
case 204:
	switch( (*( current_position)) ) {
		case 4u: goto st34;
		case 5u: goto st35;
		case 68u: goto st40;
		case 132u: goto st41;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st10;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st10;
		} else if ( (*( current_position)) >= 128u )
			goto st35;
	} else
		goto st39;
	goto tr16;
tr435:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st205;
st205:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof205;
case 205:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 68u: goto st117;
		case 132u: goto st118;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st11;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st11;
		} else if ( (*( current_position)) >= 128u )
			goto st112;
	} else
		goto st116;
	goto tr16;
tr436:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st206;
st206:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof206;
case 206:
	goto st207;
st207:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof207;
case 207:
	goto tr371;
tr438:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st208;
st208:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof208;
case 208:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 52u: goto tr373;
		case 53u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr374;
	} else if ( (*( current_position)) >= 64u )
		goto tr375;
	goto tr372;
tr439:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st209;
st209:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof209;
case 209:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto st9;
		case 188u: goto st9;
		case 239u: goto tr16;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 63u )
					goto tr0;
			} else if ( (*( current_position)) >= 8u )
				goto tr16;
		} else if ( (*( current_position)) > 71u ) {
			if ( (*( current_position)) < 80u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto st7;
			} else
				goto tr375;
		} else
			goto tr375;
	} else if ( (*( current_position)) > 135u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) > 143u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr374;
			} else if ( (*( current_position)) >= 136u )
				goto tr16;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 209u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr16;
			} else if ( (*( current_position)) > 227u ) {
				if ( 230u <= (*( current_position)) && (*( current_position)) <= 231u )
					goto tr16;
			} else
				goto tr16;
		} else
			goto st3;
	} else
		goto tr374;
	goto tr372;
tr440:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st210;
st210:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof210;
case 210:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 12u: goto tr373;
		case 20u: goto tr373;
		case 28u: goto tr373;
		case 36u: goto tr373;
		case 44u: goto tr373;
		case 52u: goto tr373;
		case 60u: goto tr373;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
		case 233u: goto tr372;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr372;
			} else
				goto tr372;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr372;
			} else if ( (*( current_position)) >= 22u )
				goto tr372;
		} else
			goto tr372;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr372;
			} else if ( (*( current_position)) >= 46u )
				goto tr372;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) < 192u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr375;
			} else if ( (*( current_position)) > 223u ) {
				if ( 224u <= (*( current_position)) )
					goto tr16;
			} else
				goto tr378;
		} else
			goto tr372;
	} else
		goto tr372;
	goto tr374;
tr441:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st211;
st211:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof211;
case 211:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 108u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 172u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 56u ) {
			if ( (*( current_position)) > 31u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr372;
			} else
				goto tr372;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr375;
			} else if ( (*( current_position)) >= 64u )
				goto tr375;
		} else
			goto tr372;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 184u ) {
			if ( (*( current_position)) > 159u ) {
				if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr374;
			} else if ( (*( current_position)) >= 128u )
				goto tr374;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 192u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr378;
			} else if ( (*( current_position)) > 227u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr372;
			} else
				goto tr372;
		} else
			goto tr374;
	} else
		goto tr375;
	goto tr16;
tr442:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st212;
st212:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof212;
case 212:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 52u: goto tr373;
		case 53u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr375;
	} else if ( (*( current_position)) > 191u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 223u )
			goto tr16;
	} else
		goto tr374;
	goto tr372;
tr443:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st213;
st213:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof213;
case 213:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 52u: goto tr373;
		case 53u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr16;
		} else if ( (*( current_position)) > 103u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr375;
			} else if ( (*( current_position)) >= 104u )
				goto tr16;
		} else
			goto tr375;
	} else if ( (*( current_position)) > 167u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto tr16;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 200u )
				goto tr16;
		} else
			goto tr374;
	} else
		goto tr374;
	goto tr372;
tr444:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st214;
st214:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof214;
case 214:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 52u: goto tr373;
		case 53u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr375;
	} else if ( (*( current_position)) > 191u ) {
		if ( (*( current_position)) > 216u ) {
			if ( 218u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr16;
		} else if ( (*( current_position)) >= 208u )
			goto tr16;
	} else
		goto tr374;
	goto tr372;
tr445:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st215;
st215:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof215;
case 215:
	switch( (*( current_position)) ) {
		case 4u: goto tr373;
		case 5u: goto tr374;
		case 12u: goto tr373;
		case 13u: goto tr374;
		case 20u: goto tr373;
		case 21u: goto tr374;
		case 28u: goto tr373;
		case 29u: goto tr374;
		case 36u: goto tr373;
		case 37u: goto tr374;
		case 44u: goto tr373;
		case 45u: goto tr374;
		case 52u: goto tr373;
		case 53u: goto tr374;
		case 60u: goto tr373;
		case 61u: goto tr374;
		case 68u: goto tr376;
		case 76u: goto tr376;
		case 84u: goto tr376;
		case 92u: goto tr376;
		case 100u: goto tr376;
		case 108u: goto tr376;
		case 116u: goto tr376;
		case 124u: goto tr376;
		case 132u: goto tr377;
		case 140u: goto tr377;
		case 148u: goto tr377;
		case 156u: goto tr377;
		case 164u: goto tr377;
		case 172u: goto tr377;
		case 180u: goto tr377;
		case 188u: goto tr377;
	}
	if ( (*( current_position)) < 192u ) {
		if ( (*( current_position)) > 127u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr374;
		} else if ( (*( current_position)) >= 64u )
			goto tr375;
	} else if ( (*( current_position)) > 223u ) {
		if ( (*( current_position)) > 231u ) {
			if ( 248u <= (*( current_position)) )
				goto tr16;
		} else if ( (*( current_position)) >= 225u )
			goto tr16;
	} else
		goto tr16;
	goto tr372;
tr447:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st216;
st216:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof216;
case 216:
	switch( (*( current_position)) ) {
		case 15u: goto st217;
		case 102u: goto tr380;
		case 128u: goto st100;
		case 129u: goto st218;
		case 131u: goto st100;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 1u )
				goto st29;
		} else if ( (*( current_position)) > 9u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 24u <= (*( current_position)) && (*( current_position)) <= 25u )
					goto st29;
			} else if ( (*( current_position)) >= 16u )
				goto st29;
		} else
			goto st29;
	} else if ( (*( current_position)) > 33u ) {
		if ( (*( current_position)) < 134u ) {
			if ( (*( current_position)) > 41u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 49u )
					goto st29;
			} else if ( (*( current_position)) >= 40u )
				goto st29;
		} else if ( (*( current_position)) > 135u ) {
			if ( (*( current_position)) > 247u ) {
				if ( 254u <= (*( current_position)) )
					goto st102;
			} else if ( (*( current_position)) >= 246u )
				goto st101;
		} else
			goto st29;
	} else
		goto st29;
	goto tr16;
st217:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof217;
case 217:
	if ( (*( current_position)) == 199u )
		goto st51;
	if ( (*( current_position)) > 177u ) {
		if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
			goto st29;
	} else if ( (*( current_position)) >= 176u )
		goto st29;
	goto tr16;
st218:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof218;
case 218:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 12u: goto st111;
		case 13u: goto st112;
		case 20u: goto st111;
		case 21u: goto st112;
		case 28u: goto st111;
		case 29u: goto st112;
		case 36u: goto st111;
		case 37u: goto st112;
		case 44u: goto st111;
		case 45u: goto st112;
		case 52u: goto st111;
		case 53u: goto st112;
		case 68u: goto st117;
		case 76u: goto st117;
		case 84u: goto st117;
		case 92u: goto st117;
		case 100u: goto st117;
		case 108u: goto st117;
		case 116u: goto st117;
		case 132u: goto st118;
		case 140u: goto st118;
		case 148u: goto st118;
		case 156u: goto st118;
		case 164u: goto st118;
		case 172u: goto st118;
		case 180u: goto st118;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st11;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st112;
	} else
		goto st116;
	goto tr16;
tr448:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st219;
st219:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof219;
case 219:
	switch( (*( current_position)) ) {
		case 15u: goto st220;
		case 102u: goto tr383;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 166u )
		goto tr0;
	goto tr16;
st220:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof220;
case 220:
	switch( (*( current_position)) ) {
		case 18u: goto tr385;
		case 43u: goto tr386;
		case 56u: goto st221;
		case 81u: goto tr384;
		case 112u: goto tr388;
		case 120u: goto tr389;
		case 121u: goto tr390;
		case 194u: goto tr388;
		case 208u: goto tr391;
		case 214u: goto tr392;
		case 230u: goto tr384;
		case 240u: goto tr393;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 42u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr384;
		} else if ( (*( current_position)) >= 16u )
			goto tr384;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr385;
		} else if ( (*( current_position)) >= 92u )
			goto tr384;
	} else
		goto tr384;
	goto tr16;
st221:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof221;
case 221:
	if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
		goto tr200;
	goto tr16;
tr389:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st222;
st222:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof222;
case 222:
	if ( 192u <= (*( current_position)) )
		goto st223;
	goto tr16;
st223:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof223;
case 223:
	goto tr395;
tr449:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st224;
st224:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof224;
case 224:
	switch( (*( current_position)) ) {
		case 15u: goto st225;
		case 102u: goto tr397;
		case 144u: goto tr398;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 170u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 164u )
		goto tr0;
	goto tr16;
st225:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof225;
case 225:
	switch( (*( current_position)) ) {
		case 18u: goto tr400;
		case 22u: goto tr400;
		case 43u: goto tr401;
		case 111u: goto tr402;
		case 112u: goto tr403;
		case 184u: goto tr202;
		case 188u: goto tr203;
		case 189u: goto tr204;
		case 194u: goto tr404;
		case 214u: goto tr405;
		case 230u: goto tr402;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 42u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
				goto tr399;
		} else if ( (*( current_position)) > 45u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 83u )
				goto tr399;
		} else
			goto tr399;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) < 92u ) {
			if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
				goto tr402;
		} else if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr402;
		} else
			goto tr399;
	} else
		goto tr399;
	goto tr16;
tr450:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st226;
st226:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof226;
case 226:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st10;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st39;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st35;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st10;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr451:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st227;
st227:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof227;
case 227:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
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
		case 68u: goto st117;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st118;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st11;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st116;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st112;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st11;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr452:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st228;
st228:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof228;
case 228:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr0;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
tr454:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st229;
st229:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof229;
case 229:
	switch( (*( current_position)) ) {
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
		case 208u: goto tr406;
		case 224u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st230:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof230;
case 230:
	if ( (*( current_position)) == 224u )
		goto tr407;
	goto tr11;
tr407:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st246;
st246:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof246;
case 246:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr455;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr455:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st231;
st231:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof231;
case 231:
	switch( (*( current_position)) ) {
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
		case 209u: goto tr406;
		case 225u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st232:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof232;
case 232:
	if ( (*( current_position)) == 224u )
		goto tr408;
	goto tr11;
tr408:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st247;
st247:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof247;
case 247:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr456;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr456:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st233;
st233:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof233;
case 233:
	switch( (*( current_position)) ) {
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
		case 210u: goto tr406;
		case 226u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st234:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof234;
case 234:
	if ( (*( current_position)) == 224u )
		goto tr409;
	goto tr11;
tr409:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st248;
st248:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof248;
case 248:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr457;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr457:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st235;
st235:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof235;
case 235:
	switch( (*( current_position)) ) {
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
		case 211u: goto tr406;
		case 227u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st236:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof236;
case 236:
	if ( (*( current_position)) == 224u )
		goto tr410;
	goto tr11;
tr410:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st249;
st249:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof249;
case 249:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr458;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr458:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st237;
st237:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof237;
case 237:
	switch( (*( current_position)) ) {
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
		case 212u: goto tr406;
		case 228u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st238:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof238;
case 238:
	if ( (*( current_position)) == 224u )
		goto tr411;
	goto tr11;
tr411:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st250;
st250:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof250;
case 250:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr459;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr459:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st239;
st239:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof239;
case 239:
	switch( (*( current_position)) ) {
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
		case 213u: goto tr406;
		case 229u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st240:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof240;
case 240:
	if ( (*( current_position)) == 224u )
		goto tr412;
	goto tr11;
tr412:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st251;
st251:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof251;
case 251:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr460;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr460:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st241;
st241:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof241;
case 241:
	switch( (*( current_position)) ) {
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
		case 214u: goto tr406;
		case 230u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st242:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof242;
case 242:
	if ( (*( current_position)) == 224u )
		goto tr413;
	goto tr11;
tr413:
	{ }
	{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
     }
	goto st252;
st252:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof252;
case 252:
	switch( (*( current_position)) ) {
		case 4u: goto tr415;
		case 5u: goto tr416;
		case 12u: goto tr415;
		case 13u: goto tr416;
		case 14u: goto tr16;
		case 15u: goto tr417;
		case 20u: goto tr415;
		case 21u: goto tr416;
		case 28u: goto tr415;
		case 29u: goto tr416;
		case 36u: goto tr415;
		case 37u: goto tr416;
		case 44u: goto tr415;
		case 45u: goto tr416;
		case 46u: goto tr418;
		case 47u: goto tr16;
		case 52u: goto tr415;
		case 53u: goto tr416;
		case 60u: goto tr415;
		case 61u: goto tr416;
		case 62u: goto tr419;
		case 63u: goto tr16;
		case 101u: goto tr421;
		case 102u: goto tr422;
		case 104u: goto tr416;
		case 105u: goto tr423;
		case 106u: goto tr415;
		case 107u: goto tr424;
		case 128u: goto tr424;
		case 129u: goto tr423;
		case 130u: goto tr16;
		case 131u: goto tr426;
		case 141u: goto tr427;
		case 143u: goto tr428;
		case 155u: goto tr429;
		case 168u: goto tr415;
		case 169u: goto tr416;
		case 196u: goto tr432;
		case 197u: goto tr433;
		case 198u: goto tr434;
		case 199u: goto tr435;
		case 200u: goto tr436;
		case 216u: goto tr438;
		case 217u: goto tr439;
		case 218u: goto tr440;
		case 219u: goto tr441;
		case 220u: goto tr442;
		case 221u: goto tr443;
		case 222u: goto tr444;
		case 223u: goto tr445;
		case 235u: goto tr425;
		case 240u: goto tr447;
		case 242u: goto tr448;
		case 243u: goto tr449;
		case 246u: goto tr450;
		case 247u: goto tr451;
		case 254u: goto tr452;
		case 255u: goto tr461;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr414;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr414;
			} else
				goto tr414;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr414;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr414;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr425;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr414;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr16;
				} else if ( (*( current_position)) >= 140u )
					goto tr16;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr415;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr431;
				} else
					goto tr416;
			} else
				goto tr430;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr437;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr446;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr16;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr414;
	goto tr420;
tr461:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st243;
st243:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof243;
case 243:
	switch( (*( current_position)) ) {
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
		case 215u: goto tr406;
		case 231u: goto tr406;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
	}
	_test_eof244: ( current_state) = 244; goto _test_eof; 
	_test_eof1: ( current_state) = 1; goto _test_eof; 
	_test_eof2: ( current_state) = 2; goto _test_eof; 
	_test_eof3: ( current_state) = 3; goto _test_eof; 
	_test_eof4: ( current_state) = 4; goto _test_eof; 
	_test_eof5: ( current_state) = 5; goto _test_eof; 
	_test_eof6: ( current_state) = 6; goto _test_eof; 
	_test_eof7: ( current_state) = 7; goto _test_eof; 
	_test_eof8: ( current_state) = 8; goto _test_eof; 
	_test_eof9: ( current_state) = 9; goto _test_eof; 
	_test_eof10: ( current_state) = 10; goto _test_eof; 
	_test_eof11: ( current_state) = 11; goto _test_eof; 
	_test_eof12: ( current_state) = 12; goto _test_eof; 
	_test_eof13: ( current_state) = 13; goto _test_eof; 
	_test_eof14: ( current_state) = 14; goto _test_eof; 
	_test_eof15: ( current_state) = 15; goto _test_eof; 
	_test_eof16: ( current_state) = 16; goto _test_eof; 
	_test_eof17: ( current_state) = 17; goto _test_eof; 
	_test_eof18: ( current_state) = 18; goto _test_eof; 
	_test_eof19: ( current_state) = 19; goto _test_eof; 
	_test_eof20: ( current_state) = 20; goto _test_eof; 
	_test_eof21: ( current_state) = 21; goto _test_eof; 
	_test_eof22: ( current_state) = 22; goto _test_eof; 
	_test_eof23: ( current_state) = 23; goto _test_eof; 
	_test_eof24: ( current_state) = 24; goto _test_eof; 
	_test_eof25: ( current_state) = 25; goto _test_eof; 
	_test_eof26: ( current_state) = 26; goto _test_eof; 
	_test_eof27: ( current_state) = 27; goto _test_eof; 
	_test_eof28: ( current_state) = 28; goto _test_eof; 
	_test_eof29: ( current_state) = 29; goto _test_eof; 
	_test_eof30: ( current_state) = 30; goto _test_eof; 
	_test_eof31: ( current_state) = 31; goto _test_eof; 
	_test_eof32: ( current_state) = 32; goto _test_eof; 
	_test_eof33: ( current_state) = 33; goto _test_eof; 
	_test_eof34: ( current_state) = 34; goto _test_eof; 
	_test_eof35: ( current_state) = 35; goto _test_eof; 
	_test_eof36: ( current_state) = 36; goto _test_eof; 
	_test_eof37: ( current_state) = 37; goto _test_eof; 
	_test_eof38: ( current_state) = 38; goto _test_eof; 
	_test_eof39: ( current_state) = 39; goto _test_eof; 
	_test_eof40: ( current_state) = 40; goto _test_eof; 
	_test_eof41: ( current_state) = 41; goto _test_eof; 
	_test_eof42: ( current_state) = 42; goto _test_eof; 
	_test_eof43: ( current_state) = 43; goto _test_eof; 
	_test_eof44: ( current_state) = 44; goto _test_eof; 
	_test_eof45: ( current_state) = 45; goto _test_eof; 
	_test_eof46: ( current_state) = 46; goto _test_eof; 
	_test_eof47: ( current_state) = 47; goto _test_eof; 
	_test_eof48: ( current_state) = 48; goto _test_eof; 
	_test_eof49: ( current_state) = 49; goto _test_eof; 
	_test_eof50: ( current_state) = 50; goto _test_eof; 
	_test_eof51: ( current_state) = 51; goto _test_eof; 
	_test_eof52: ( current_state) = 52; goto _test_eof; 
	_test_eof53: ( current_state) = 53; goto _test_eof; 
	_test_eof54: ( current_state) = 54; goto _test_eof; 
	_test_eof55: ( current_state) = 55; goto _test_eof; 
	_test_eof56: ( current_state) = 56; goto _test_eof; 
	_test_eof57: ( current_state) = 57; goto _test_eof; 
	_test_eof58: ( current_state) = 58; goto _test_eof; 
	_test_eof59: ( current_state) = 59; goto _test_eof; 
	_test_eof60: ( current_state) = 60; goto _test_eof; 
	_test_eof61: ( current_state) = 61; goto _test_eof; 
	_test_eof62: ( current_state) = 62; goto _test_eof; 
	_test_eof63: ( current_state) = 63; goto _test_eof; 
	_test_eof64: ( current_state) = 64; goto _test_eof; 
	_test_eof65: ( current_state) = 65; goto _test_eof; 
	_test_eof66: ( current_state) = 66; goto _test_eof; 
	_test_eof67: ( current_state) = 67; goto _test_eof; 
	_test_eof68: ( current_state) = 68; goto _test_eof; 
	_test_eof69: ( current_state) = 69; goto _test_eof; 
	_test_eof70: ( current_state) = 70; goto _test_eof; 
	_test_eof71: ( current_state) = 71; goto _test_eof; 
	_test_eof72: ( current_state) = 72; goto _test_eof; 
	_test_eof73: ( current_state) = 73; goto _test_eof; 
	_test_eof74: ( current_state) = 74; goto _test_eof; 
	_test_eof75: ( current_state) = 75; goto _test_eof; 
	_test_eof76: ( current_state) = 76; goto _test_eof; 
	_test_eof77: ( current_state) = 77; goto _test_eof; 
	_test_eof78: ( current_state) = 78; goto _test_eof; 
	_test_eof79: ( current_state) = 79; goto _test_eof; 
	_test_eof80: ( current_state) = 80; goto _test_eof; 
	_test_eof81: ( current_state) = 81; goto _test_eof; 
	_test_eof82: ( current_state) = 82; goto _test_eof; 
	_test_eof83: ( current_state) = 83; goto _test_eof; 
	_test_eof84: ( current_state) = 84; goto _test_eof; 
	_test_eof85: ( current_state) = 85; goto _test_eof; 
	_test_eof86: ( current_state) = 86; goto _test_eof; 
	_test_eof87: ( current_state) = 87; goto _test_eof; 
	_test_eof88: ( current_state) = 88; goto _test_eof; 
	_test_eof89: ( current_state) = 89; goto _test_eof; 
	_test_eof90: ( current_state) = 90; goto _test_eof; 
	_test_eof91: ( current_state) = 91; goto _test_eof; 
	_test_eof92: ( current_state) = 92; goto _test_eof; 
	_test_eof93: ( current_state) = 93; goto _test_eof; 
	_test_eof94: ( current_state) = 94; goto _test_eof; 
	_test_eof95: ( current_state) = 95; goto _test_eof; 
	_test_eof96: ( current_state) = 96; goto _test_eof; 
	_test_eof97: ( current_state) = 97; goto _test_eof; 
	_test_eof98: ( current_state) = 98; goto _test_eof; 
	_test_eof99: ( current_state) = 99; goto _test_eof; 
	_test_eof100: ( current_state) = 100; goto _test_eof; 
	_test_eof101: ( current_state) = 101; goto _test_eof; 
	_test_eof102: ( current_state) = 102; goto _test_eof; 
	_test_eof103: ( current_state) = 103; goto _test_eof; 
	_test_eof104: ( current_state) = 104; goto _test_eof; 
	_test_eof105: ( current_state) = 105; goto _test_eof; 
	_test_eof106: ( current_state) = 106; goto _test_eof; 
	_test_eof107: ( current_state) = 107; goto _test_eof; 
	_test_eof108: ( current_state) = 108; goto _test_eof; 
	_test_eof109: ( current_state) = 109; goto _test_eof; 
	_test_eof110: ( current_state) = 110; goto _test_eof; 
	_test_eof111: ( current_state) = 111; goto _test_eof; 
	_test_eof112: ( current_state) = 112; goto _test_eof; 
	_test_eof113: ( current_state) = 113; goto _test_eof; 
	_test_eof114: ( current_state) = 114; goto _test_eof; 
	_test_eof115: ( current_state) = 115; goto _test_eof; 
	_test_eof116: ( current_state) = 116; goto _test_eof; 
	_test_eof117: ( current_state) = 117; goto _test_eof; 
	_test_eof118: ( current_state) = 118; goto _test_eof; 
	_test_eof119: ( current_state) = 119; goto _test_eof; 
	_test_eof120: ( current_state) = 120; goto _test_eof; 
	_test_eof245: ( current_state) = 245; goto _test_eof; 
	_test_eof121: ( current_state) = 121; goto _test_eof; 
	_test_eof122: ( current_state) = 122; goto _test_eof; 
	_test_eof123: ( current_state) = 123; goto _test_eof; 
	_test_eof124: ( current_state) = 124; goto _test_eof; 
	_test_eof125: ( current_state) = 125; goto _test_eof; 
	_test_eof126: ( current_state) = 126; goto _test_eof; 
	_test_eof127: ( current_state) = 127; goto _test_eof; 
	_test_eof128: ( current_state) = 128; goto _test_eof; 
	_test_eof129: ( current_state) = 129; goto _test_eof; 
	_test_eof130: ( current_state) = 130; goto _test_eof; 
	_test_eof131: ( current_state) = 131; goto _test_eof; 
	_test_eof132: ( current_state) = 132; goto _test_eof; 
	_test_eof133: ( current_state) = 133; goto _test_eof; 
	_test_eof134: ( current_state) = 134; goto _test_eof; 
	_test_eof135: ( current_state) = 135; goto _test_eof; 
	_test_eof136: ( current_state) = 136; goto _test_eof; 
	_test_eof137: ( current_state) = 137; goto _test_eof; 
	_test_eof138: ( current_state) = 138; goto _test_eof; 
	_test_eof139: ( current_state) = 139; goto _test_eof; 
	_test_eof140: ( current_state) = 140; goto _test_eof; 
	_test_eof141: ( current_state) = 141; goto _test_eof; 
	_test_eof142: ( current_state) = 142; goto _test_eof; 
	_test_eof143: ( current_state) = 143; goto _test_eof; 
	_test_eof144: ( current_state) = 144; goto _test_eof; 
	_test_eof145: ( current_state) = 145; goto _test_eof; 
	_test_eof146: ( current_state) = 146; goto _test_eof; 
	_test_eof147: ( current_state) = 147; goto _test_eof; 
	_test_eof148: ( current_state) = 148; goto _test_eof; 
	_test_eof149: ( current_state) = 149; goto _test_eof; 
	_test_eof150: ( current_state) = 150; goto _test_eof; 
	_test_eof151: ( current_state) = 151; goto _test_eof; 
	_test_eof152: ( current_state) = 152; goto _test_eof; 
	_test_eof153: ( current_state) = 153; goto _test_eof; 
	_test_eof154: ( current_state) = 154; goto _test_eof; 
	_test_eof155: ( current_state) = 155; goto _test_eof; 
	_test_eof156: ( current_state) = 156; goto _test_eof; 
	_test_eof157: ( current_state) = 157; goto _test_eof; 
	_test_eof158: ( current_state) = 158; goto _test_eof; 
	_test_eof159: ( current_state) = 159; goto _test_eof; 
	_test_eof160: ( current_state) = 160; goto _test_eof; 
	_test_eof161: ( current_state) = 161; goto _test_eof; 
	_test_eof162: ( current_state) = 162; goto _test_eof; 
	_test_eof163: ( current_state) = 163; goto _test_eof; 
	_test_eof164: ( current_state) = 164; goto _test_eof; 
	_test_eof165: ( current_state) = 165; goto _test_eof; 
	_test_eof166: ( current_state) = 166; goto _test_eof; 
	_test_eof167: ( current_state) = 167; goto _test_eof; 
	_test_eof168: ( current_state) = 168; goto _test_eof; 
	_test_eof169: ( current_state) = 169; goto _test_eof; 
	_test_eof170: ( current_state) = 170; goto _test_eof; 
	_test_eof171: ( current_state) = 171; goto _test_eof; 
	_test_eof172: ( current_state) = 172; goto _test_eof; 
	_test_eof173: ( current_state) = 173; goto _test_eof; 
	_test_eof174: ( current_state) = 174; goto _test_eof; 
	_test_eof175: ( current_state) = 175; goto _test_eof; 
	_test_eof176: ( current_state) = 176; goto _test_eof; 
	_test_eof177: ( current_state) = 177; goto _test_eof; 
	_test_eof178: ( current_state) = 178; goto _test_eof; 
	_test_eof179: ( current_state) = 179; goto _test_eof; 
	_test_eof180: ( current_state) = 180; goto _test_eof; 
	_test_eof181: ( current_state) = 181; goto _test_eof; 
	_test_eof182: ( current_state) = 182; goto _test_eof; 
	_test_eof183: ( current_state) = 183; goto _test_eof; 
	_test_eof184: ( current_state) = 184; goto _test_eof; 
	_test_eof185: ( current_state) = 185; goto _test_eof; 
	_test_eof186: ( current_state) = 186; goto _test_eof; 
	_test_eof187: ( current_state) = 187; goto _test_eof; 
	_test_eof188: ( current_state) = 188; goto _test_eof; 
	_test_eof189: ( current_state) = 189; goto _test_eof; 
	_test_eof190: ( current_state) = 190; goto _test_eof; 
	_test_eof191: ( current_state) = 191; goto _test_eof; 
	_test_eof192: ( current_state) = 192; goto _test_eof; 
	_test_eof193: ( current_state) = 193; goto _test_eof; 
	_test_eof194: ( current_state) = 194; goto _test_eof; 
	_test_eof195: ( current_state) = 195; goto _test_eof; 
	_test_eof196: ( current_state) = 196; goto _test_eof; 
	_test_eof197: ( current_state) = 197; goto _test_eof; 
	_test_eof198: ( current_state) = 198; goto _test_eof; 
	_test_eof199: ( current_state) = 199; goto _test_eof; 
	_test_eof200: ( current_state) = 200; goto _test_eof; 
	_test_eof201: ( current_state) = 201; goto _test_eof; 
	_test_eof202: ( current_state) = 202; goto _test_eof; 
	_test_eof203: ( current_state) = 203; goto _test_eof; 
	_test_eof204: ( current_state) = 204; goto _test_eof; 
	_test_eof205: ( current_state) = 205; goto _test_eof; 
	_test_eof206: ( current_state) = 206; goto _test_eof; 
	_test_eof207: ( current_state) = 207; goto _test_eof; 
	_test_eof208: ( current_state) = 208; goto _test_eof; 
	_test_eof209: ( current_state) = 209; goto _test_eof; 
	_test_eof210: ( current_state) = 210; goto _test_eof; 
	_test_eof211: ( current_state) = 211; goto _test_eof; 
	_test_eof212: ( current_state) = 212; goto _test_eof; 
	_test_eof213: ( current_state) = 213; goto _test_eof; 
	_test_eof214: ( current_state) = 214; goto _test_eof; 
	_test_eof215: ( current_state) = 215; goto _test_eof; 
	_test_eof216: ( current_state) = 216; goto _test_eof; 
	_test_eof217: ( current_state) = 217; goto _test_eof; 
	_test_eof218: ( current_state) = 218; goto _test_eof; 
	_test_eof219: ( current_state) = 219; goto _test_eof; 
	_test_eof220: ( current_state) = 220; goto _test_eof; 
	_test_eof221: ( current_state) = 221; goto _test_eof; 
	_test_eof222: ( current_state) = 222; goto _test_eof; 
	_test_eof223: ( current_state) = 223; goto _test_eof; 
	_test_eof224: ( current_state) = 224; goto _test_eof; 
	_test_eof225: ( current_state) = 225; goto _test_eof; 
	_test_eof226: ( current_state) = 226; goto _test_eof; 
	_test_eof227: ( current_state) = 227; goto _test_eof; 
	_test_eof228: ( current_state) = 228; goto _test_eof; 
	_test_eof229: ( current_state) = 229; goto _test_eof; 
	_test_eof230: ( current_state) = 230; goto _test_eof; 
	_test_eof246: ( current_state) = 246; goto _test_eof; 
	_test_eof231: ( current_state) = 231; goto _test_eof; 
	_test_eof232: ( current_state) = 232; goto _test_eof; 
	_test_eof247: ( current_state) = 247; goto _test_eof; 
	_test_eof233: ( current_state) = 233; goto _test_eof; 
	_test_eof234: ( current_state) = 234; goto _test_eof; 
	_test_eof248: ( current_state) = 248; goto _test_eof; 
	_test_eof235: ( current_state) = 235; goto _test_eof; 
	_test_eof236: ( current_state) = 236; goto _test_eof; 
	_test_eof249: ( current_state) = 249; goto _test_eof; 
	_test_eof237: ( current_state) = 237; goto _test_eof; 
	_test_eof238: ( current_state) = 238; goto _test_eof; 
	_test_eof250: ( current_state) = 250; goto _test_eof; 
	_test_eof239: ( current_state) = 239; goto _test_eof; 
	_test_eof240: ( current_state) = 240; goto _test_eof; 
	_test_eof251: ( current_state) = 251; goto _test_eof; 
	_test_eof241: ( current_state) = 241; goto _test_eof; 
	_test_eof242: ( current_state) = 242; goto _test_eof; 
	_test_eof252: ( current_state) = 252; goto _test_eof; 
	_test_eof243: ( current_state) = 243; goto _test_eof; 

	_test_eof: {}
	if ( ( current_position) == ( end_of_bundle) )
	{
	switch ( ( current_state) ) {
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
	case 235: 
	case 236: 
	case 237: 
	case 238: 
	case 239: 
	case 240: 
	case 241: 
	case 242: 
	case 243: 
	{
        process_error(instruction_start, UNRECOGNIZED_INSTRUCTION, userdata);
        result = 1;
        goto error_detected;
    }
	break;
	}
	}

	_out: {}
	}

  }

  for (i = 0; i < size / 32; i++) {
    uint32_t jump_dest_mask = ((uint32_t *) jump_dests)[i];
    uint32_t valid_target_mask = ((uint32_t *) valid_targets)[i];
    if ((jump_dest_mask & ~valid_target_mask) != 0) {
      process_error(data + i * 32, BAD_JUMP_TARGET, userdata);
      result = 1;
      break;
    }
  }

error_detected:
  free(jump_dests);
  free(valid_targets);
  return result;
}
