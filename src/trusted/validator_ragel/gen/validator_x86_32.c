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





static const int x86_32_validator_start = 255;
static const int x86_32_validator_first_final = 255;
static const int x86_32_validator_error = 0;

static const int x86_32_validator_en_main = 255;




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
	( current_state) = x86_32_validator_start;
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
	goto st255;
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
	goto st255;
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
	goto st255;
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
	goto st255;
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
	goto st255;
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
	goto st255;
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
	goto st255;
tr36:
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
	goto st255;
tr50:
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
	goto st255;
tr51:
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
	goto st255;
tr52:
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
	goto st255;
tr64:
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
	goto st255;
tr70:
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
	goto st255;
tr94:
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
	goto st255;
tr97:
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
	goto st255;
tr106:
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
	goto st255;
tr107:
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
	goto st255;
tr114:
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
	goto st255;
tr122:
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
	goto st255;
tr145:
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
	goto st255;
tr162:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
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
	goto st255;
tr167:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
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
	goto st255;
tr199:
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
	goto st255;
tr214:
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
	goto st255;
tr279:
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
	goto st255;
tr286:
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
	goto st255;
tr320:
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
	goto st255;
tr347:
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
	goto st255;
tr373:
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
	goto st255;
tr399:
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
	goto st255;
tr419:
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
	goto st255;
tr427:
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
	goto st255;
tr441:
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
	goto st255;
st255:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof255;
case 255:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr474;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
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
tr30:
	{
    SET_CPU_FEATURE(CPUFeature_CMOV);
  }
	goto st1;
tr32:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	goto st1;
tr47:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st1;
tr76:
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st1;
tr146:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr156:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr168:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st1;
tr169:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st1;
tr171:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr172:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st1;
tr227:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr229:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_POPCNT);
  }
	goto st1;
tr230:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_TZCNT);
  }
	goto st1;
tr231:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_LZCNT);
  }
	goto st1;
tr278:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st1;
tr345:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st1;
tr317:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st1;
tr353:
	{
    SET_CPU_FEATURE(CPUFeature_FMA);
  }
	goto st1;
tr354:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st1;
tr355:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st1;
tr405:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr406:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr412:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr420:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr421:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr423:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr435:
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
tr71:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st2;
tr53:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st2;
tr95:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st2;
tr98:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st2;
tr115:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st2;
tr215:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st2;
tr280:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st2;
tr348:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st2;
tr321:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
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
tr72:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st3;
tr54:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st3;
tr96:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st3;
tr99:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st3;
tr116:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st3;
tr216:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st3;
tr281:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st3;
tr349:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st3;
tr322:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st3;
tr451:
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
tr73:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st7;
tr55:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st7;
tr100:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st7;
tr102:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st7;
tr117:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st7;
tr217:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st7;
tr282:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st7;
tr350:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st7;
tr323:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st7;
st7:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof7;
case 7:
	goto tr10;
tr74:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st8;
tr56:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st8;
tr101:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st8;
tr103:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st8;
tr118:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st8;
tr218:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st8;
tr283:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st8;
tr351:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st8;
tr324:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st8;
st8:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof8;
case 8:
	goto st7;
tr75:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st9;
tr57:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st9;
tr104:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st9;
tr105:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st9;
tr119:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st9;
tr219:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st9;
tr284:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st9;
tr352:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st9;
tr325:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st9;
st9:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof9;
case 9:
	goto st3;
tr90:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	goto st10;
tr108:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st10;
tr88:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st10;
tr89:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st10;
tr185:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st10;
tr179:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st10;
tr186:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st10;
tr328:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st10;
tr398:
	{ }
	goto st10;
tr416:
	{ }
	goto st10;
tr436:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st10;
st10:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof10;
case 10:
	goto tr11;
tr241:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st11;
tr242:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st11;
tr290:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st11;
tr437:
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
tr438:
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
		case 56u: goto st32;
		case 58u: goto st33;
		case 80u: goto tr31;
		case 112u: goto tr33;
		case 115u: goto st45;
		case 119u: goto tr36;
		case 162u: goto tr0;
		case 164u: goto st34;
		case 165u: goto st1;
		case 172u: goto st34;
		case 174u: goto st50;
		case 195u: goto tr42;
		case 196u: goto st51;
		case 197u: goto tr44;
		case 199u: goto st53;
		case 212u: goto tr26;
		case 215u: goto tr46;
		case 218u: goto tr47;
		case 222u: goto tr47;
		case 224u: goto tr47;
		case 229u: goto tr32;
		case 231u: goto tr48;
		case 234u: goto tr47;
		case 238u: goto tr47;
		case 244u: goto tr26;
		case 246u: goto tr47;
		case 247u: goto tr49;
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
						goto tr30;
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
						goto tr32;
				} else if ( (*( current_position)) > 114u ) {
					if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
						goto tr32;
				} else
					goto st44;
			} else
				goto tr32;
		} else
			goto tr21;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 173u ) {
				if ( (*( current_position)) > 143u ) {
					if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr30;
				} else if ( (*( current_position)) >= 128u )
					goto st46;
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
						goto tr32;
				} else if ( (*( current_position)) >= 200u )
					goto tr0;
			} else if ( (*( current_position)) > 226u ) {
				if ( (*( current_position)) < 232u ) {
					if ( 227u <= (*( current_position)) && (*( current_position)) <= 228u )
						goto tr47;
				} else if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto tr32;
				} else
					goto tr32;
			} else
				goto tr32;
		} else
			goto tr41;
	} else
		goto tr32;
	goto tr16;
st16:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof16;
case 16:
	if ( (*( current_position)) == 208u )
		goto tr51;
	if ( 200u <= (*( current_position)) && (*( current_position)) <= 201u )
		goto tr50;
	goto tr16;
st17:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof17;
case 17:
	switch( (*( current_position)) ) {
		case 4u: goto tr53;
		case 5u: goto tr54;
		case 12u: goto tr53;
		case 13u: goto tr54;
		case 28u: goto tr53;
		case 29u: goto tr54;
		case 68u: goto tr56;
		case 76u: goto tr56;
		case 92u: goto tr56;
		case 132u: goto tr57;
		case 140u: goto tr57;
		case 156u: goto tr57;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) > 15u ) {
			if ( 24u <= (*( current_position)) && (*( current_position)) <= 31u )
				goto tr52;
		} else
			goto tr52;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) < 128u ) {
			if ( 88u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr55;
		} else if ( (*( current_position)) > 143u ) {
			if ( 152u <= (*( current_position)) && (*( current_position)) <= 159u )
				goto tr54;
		} else
			goto tr54;
	} else
		goto tr55;
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
tr68:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st19;
tr69:
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
		case 12u: goto tr64;
		case 13u: goto tr19;
		case 28u: goto tr64;
		case 29u: goto tr19;
		case 138u: goto tr64;
		case 142u: goto tr64;
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
		case 187u: goto tr64;
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
	goto tr68;
st25:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof25;
case 25:
	goto tr69;
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
		case 4u: goto tr71;
		case 5u: goto tr72;
		case 12u: goto tr71;
		case 13u: goto tr72;
		case 20u: goto tr71;
		case 21u: goto tr72;
		case 28u: goto tr71;
		case 29u: goto tr72;
		case 36u: goto tr71;
		case 37u: goto tr72;
		case 44u: goto tr71;
		case 45u: goto tr72;
		case 52u: goto tr71;
		case 53u: goto tr72;
		case 60u: goto tr71;
		case 61u: goto tr72;
		case 68u: goto tr74;
		case 76u: goto tr74;
		case 84u: goto tr74;
		case 92u: goto tr74;
		case 100u: goto tr74;
		case 108u: goto tr74;
		case 116u: goto tr74;
		case 124u: goto tr74;
		case 132u: goto tr75;
		case 140u: goto tr75;
		case 148u: goto tr75;
		case 156u: goto tr75;
		case 164u: goto tr75;
		case 172u: goto tr75;
		case 180u: goto tr75;
		case 188u: goto tr75;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr72;
	} else if ( (*( current_position)) >= 64u )
		goto tr73;
	goto tr70;
tr23:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st29;
tr42:
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st29;
tr48:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st29;
tr77:
	{
    SET_CPU_FEATURE(CPUFeature_MOVBE);
  }
	goto st29;
tr147:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st29;
tr170:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st29;
tr318:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st29;
tr414:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st29;
tr407:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st29;
tr422:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st29;
tr448:
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
st32:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof32;
case 32:
	if ( (*( current_position)) == 4u )
		goto tr32;
	if ( (*( current_position)) < 28u ) {
		if ( (*( current_position)) <= 11u )
			goto tr76;
	} else if ( (*( current_position)) > 30u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
			goto tr77;
	} else
		goto tr76;
	goto tr16;
st33:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof33;
case 33:
	if ( (*( current_position)) == 15u )
		goto tr78;
	goto tr16;
tr41:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st34;
tr33:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st34;
tr78:
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st34;
tr152:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr174:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st34;
tr173:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st34;
tr177:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st34;
tr178:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st34;
tr176:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_CLMUL);
  }
	goto st34;
tr260:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st34;
tr319:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st34;
tr366:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st34;
tr380:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st34;
tr362:
	{
    SET_CPU_FEATURE(CPUFeature_CLMULAVX);
  }
	goto st34;
tr409:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr425:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st34;
tr424:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr445:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st34;
st34:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof34;
case 34:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else if ( (*( current_position)) >= 64u )
		goto st40;
	goto st10;
tr109:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st35;
tr180:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st35;
tr187:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st35;
tr333:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st35;
st35:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof35;
case 35:
	switch( (*( current_position)) ) {
		case 5u: goto st36;
		case 13u: goto st36;
		case 21u: goto st36;
		case 29u: goto st36;
		case 37u: goto st36;
		case 45u: goto st36;
		case 53u: goto st36;
		case 61u: goto st36;
		case 69u: goto st36;
		case 77u: goto st36;
		case 85u: goto st36;
		case 93u: goto st36;
		case 101u: goto st36;
		case 109u: goto st36;
		case 117u: goto st36;
		case 125u: goto st36;
		case 133u: goto st36;
		case 141u: goto st36;
		case 149u: goto st36;
		case 157u: goto st36;
		case 165u: goto st36;
		case 173u: goto st36;
		case 181u: goto st36;
		case 189u: goto st36;
		case 197u: goto st36;
		case 205u: goto st36;
		case 213u: goto st36;
		case 221u: goto st36;
		case 229u: goto st36;
		case 237u: goto st36;
		case 245u: goto st36;
		case 253u: goto st36;
	}
	goto st10;
tr110:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st36;
tr181:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st36;
tr188:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st36;
tr334:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
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
	goto st39;
st39:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof39;
case 39:
	goto tr88;
tr111:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st40;
tr182:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st40;
tr189:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st40;
tr335:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st40;
st40:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof40;
case 40:
	goto tr89;
tr112:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st41;
tr183:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st41;
tr190:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st41;
tr336:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st41;
st41:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof41;
case 41:
	goto st40;
tr113:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st42;
tr184:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st42;
tr191:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st42;
tr337:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st42;
st42:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof42;
case 42:
	goto st36;
tr31:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st43;
tr46:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st43;
tr49:
	{
    SET_CPU_FEATURE(CPUFeature_EMMX);
  }
	goto st43;
tr151:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st43;
tr155:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st43;
tr329:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st43;
tr413:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st43;
tr411:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st43;
tr426:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st43;
st43:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof43;
case 43:
	if ( 192u <= (*( current_position)) )
		goto tr0;
	goto tr16;
st44:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof44;
case 44:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr90;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr90;
	} else
		goto tr90;
	goto tr16;
st45:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof45;
case 45:
	if ( (*( current_position)) > 215u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr90;
	} else if ( (*( current_position)) >= 208u )
		goto tr90;
	goto tr16;
tr467:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
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
	goto st48;
st48:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof48;
case 48:
	goto st49;
st49:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof49;
case 49:
	goto tr94;
st50:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof50;
case 50:
	switch( (*( current_position)) ) {
		case 4u: goto tr95;
		case 5u: goto tr96;
		case 12u: goto tr95;
		case 13u: goto tr96;
		case 20u: goto tr71;
		case 21u: goto tr72;
		case 28u: goto tr71;
		case 29u: goto tr72;
		case 36u: goto tr95;
		case 37u: goto tr96;
		case 44u: goto tr95;
		case 45u: goto tr96;
		case 52u: goto tr95;
		case 53u: goto tr96;
		case 60u: goto tr98;
		case 61u: goto tr99;
		case 68u: goto tr101;
		case 76u: goto tr101;
		case 84u: goto tr74;
		case 92u: goto tr74;
		case 100u: goto tr101;
		case 108u: goto tr101;
		case 116u: goto tr101;
		case 124u: goto tr103;
		case 132u: goto tr104;
		case 140u: goto tr104;
		case 148u: goto tr75;
		case 156u: goto tr75;
		case 164u: goto tr104;
		case 172u: goto tr104;
		case 180u: goto tr104;
		case 188u: goto tr105;
		case 232u: goto tr106;
		case 240u: goto tr106;
		case 248u: goto tr107;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
					goto tr70;
			} else
				goto tr51;
		} else if ( (*( current_position)) > 55u ) {
			if ( (*( current_position)) > 63u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr100;
			} else if ( (*( current_position)) >= 56u )
				goto tr97;
		} else
			goto tr51;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 128u ) {
			if ( (*( current_position)) > 119u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr102;
			} else if ( (*( current_position)) >= 96u )
				goto tr100;
		} else if ( (*( current_position)) > 143u ) {
			if ( (*( current_position)) < 160u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr72;
			} else if ( (*( current_position)) > 183u ) {
				if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr99;
			} else
				goto tr96;
		} else
			goto tr96;
	} else
		goto tr73;
	goto tr16;
st51:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof51;
case 51:
	switch( (*( current_position)) ) {
		case 4u: goto tr109;
		case 5u: goto tr110;
		case 12u: goto tr109;
		case 13u: goto tr110;
		case 20u: goto tr109;
		case 21u: goto tr110;
		case 28u: goto tr109;
		case 29u: goto tr110;
		case 36u: goto tr109;
		case 37u: goto tr110;
		case 44u: goto tr109;
		case 45u: goto tr110;
		case 52u: goto tr109;
		case 53u: goto tr110;
		case 60u: goto tr109;
		case 61u: goto tr110;
		case 68u: goto tr112;
		case 76u: goto tr112;
		case 84u: goto tr112;
		case 92u: goto tr112;
		case 100u: goto tr112;
		case 108u: goto tr112;
		case 116u: goto tr112;
		case 124u: goto tr112;
		case 132u: goto tr113;
		case 140u: goto tr113;
		case 148u: goto tr113;
		case 156u: goto tr113;
		case 164u: goto tr113;
		case 172u: goto tr113;
		case 180u: goto tr113;
		case 188u: goto tr113;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr110;
	} else if ( (*( current_position)) >= 64u )
		goto tr111;
	goto tr108;
tr44:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st52;
tr158:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st52;
tr332:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st52;
st52:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof52;
case 52:
	if ( 192u <= (*( current_position)) )
		goto st10;
	goto tr16;
st53:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof53;
case 53:
	switch( (*( current_position)) ) {
		case 12u: goto tr115;
		case 13u: goto tr116;
		case 76u: goto tr118;
		case 140u: goto tr119;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
			goto tr114;
	} else if ( (*( current_position)) > 79u ) {
		if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr116;
	} else
		goto tr117;
	goto tr16;
tr439:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_NOT_TAKEN(TRUE);
  }
	goto st54;
tr440:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_TAKEN(TRUE);
  }
	goto st54;
st54:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof54;
case 54:
	if ( (*( current_position)) == 15u )
		goto st55;
	if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
		goto st56;
	goto tr16;
st55:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof55;
case 55:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
		goto st46;
	goto tr16;
tr446:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st56;
st56:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof56;
case 56:
	goto tr122;
tr442:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st57;
st57:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof57;
case 57:
	switch( (*( current_position)) ) {
		case 139u: goto st58;
		case 161u: goto st59;
	}
	goto tr16;
st58:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof58;
case 58:
	switch( (*( current_position)) ) {
		case 5u: goto st59;
		case 13u: goto st59;
		case 21u: goto st59;
		case 29u: goto st59;
		case 37u: goto st59;
		case 45u: goto st59;
		case 53u: goto st59;
		case 61u: goto st59;
	}
	goto tr16;
st59:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof59;
case 59:
	switch( (*( current_position)) ) {
		case 0u: goto st60;
		case 4u: goto st60;
	}
	goto tr16;
st60:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof60;
case 60:
	if ( (*( current_position)) == 0u )
		goto st61;
	goto tr16;
st61:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof61;
case 61:
	if ( (*( current_position)) == 0u )
		goto st62;
	goto tr16;
st62:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof62;
case 62:
	if ( (*( current_position)) == 0u )
		goto tr0;
	goto tr16;
tr443:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st63;
tr476:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st63;
st63:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof63;
case 63:
	switch( (*( current_position)) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st64;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st64;
		case 15u: goto st66;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st64;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st64;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st64;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st64;
		case 46u: goto st81;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st64;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st64;
		case 102u: goto st89;
		case 104u: goto st64;
		case 105u: goto st94;
		case 107u: goto st34;
		case 129u: goto st94;
		case 131u: goto st34;
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
		case 169u: goto st64;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st103;
		case 199u: goto st104;
		case 209u: goto st105;
		case 211u: goto st105;
		case 217u: goto st106;
		case 221u: goto st107;
		case 240u: goto tr139;
		case 242u: goto tr140;
		case 243u: goto tr141;
		case 247u: goto st119;
		case 255u: goto st120;
	}
	if ( (*( current_position)) < 144u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr0;
	} else if ( (*( current_position)) > 153u ) {
		if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st64;
	} else
		goto tr0;
	goto tr16;
tr212:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st64;
tr213:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st64;
st64:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof64;
case 64:
	goto st65;
st65:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof65;
case 65:
	goto tr145;
st66:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof66;
case 66:
	switch( (*( current_position)) ) {
		case 31u: goto st67;
		case 43u: goto tr147;
		case 56u: goto st75;
		case 58u: goto st76;
		case 80u: goto tr151;
		case 81u: goto tr146;
		case 112u: goto tr152;
		case 115u: goto st79;
		case 121u: goto tr155;
		case 164u: goto st34;
		case 165u: goto st1;
		case 172u: goto st34;
		case 173u: goto st1;
		case 175u: goto st1;
		case 177u: goto st1;
		case 193u: goto st1;
		case 194u: goto tr152;
		case 196u: goto st80;
		case 197u: goto tr158;
		case 198u: goto tr152;
		case 215u: goto tr151;
		case 231u: goto tr147;
		case 247u: goto tr151;
	}
	if ( (*( current_position)) < 113u ) {
		if ( (*( current_position)) < 22u ) {
			if ( (*( current_position)) < 18u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
					goto tr146;
			} else if ( (*( current_position)) > 19u ) {
				if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
					goto tr146;
			} else
				goto tr147;
		} else if ( (*( current_position)) > 23u ) {
			if ( (*( current_position)) < 64u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr146;
			} else if ( (*( current_position)) > 79u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr146;
			} else
				goto tr30;
		} else
			goto tr147;
	} else if ( (*( current_position)) > 114u ) {
		if ( (*( current_position)) < 182u ) {
			if ( (*( current_position)) < 124u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr146;
			} else if ( (*( current_position)) > 125u ) {
				if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr146;
			} else
				goto tr156;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( 188u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto st1;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr146;
			} else
				goto tr146;
		} else
			goto st1;
	} else
		goto st78;
	goto tr16;
st67:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof67;
case 67:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st68;
		case 132u: goto st70;
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
st68:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof68;
case 68:
	if ( (*( current_position)) == 0u )
		goto st69;
	goto st7;
st69:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof69;
case 69:
	if ( (*( current_position)) == 0u )
		goto tr162;
	goto tr10;
st70:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof70;
case 70:
	if ( (*( current_position)) == 0u )
		goto st71;
	goto st3;
st71:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof71;
case 71:
	if ( (*( current_position)) == 0u )
		goto st72;
	goto st4;
st72:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof72;
case 72:
	if ( (*( current_position)) == 0u )
		goto st73;
	goto st5;
st73:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof73;
case 73:
	if ( (*( current_position)) == 0u )
		goto st74;
	goto st6;
st74:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof74;
case 74:
	if ( (*( current_position)) == 0u )
		goto tr167;
	goto tr9;
st75:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof75;
case 75:
	switch( (*( current_position)) ) {
		case 16u: goto tr169;
		case 23u: goto tr169;
		case 42u: goto tr170;
		case 55u: goto tr171;
	}
	if ( (*( current_position)) < 40u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) <= 11u )
				goto tr168;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) > 30u ) {
				if ( 32u <= (*( current_position)) && (*( current_position)) <= 37u )
					goto tr169;
			} else if ( (*( current_position)) >= 28u )
				goto tr168;
		} else
			goto tr169;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 56u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 53u )
				goto tr169;
		} else if ( (*( current_position)) > 65u ) {
			if ( (*( current_position)) > 223u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
					goto tr77;
			} else if ( (*( current_position)) >= 219u )
				goto tr172;
		} else
			goto tr169;
	} else
		goto tr169;
	goto tr16;
st76:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof76;
case 76:
	switch( (*( current_position)) ) {
		case 15u: goto tr174;
		case 32u: goto st77;
		case 68u: goto tr176;
		case 223u: goto tr178;
	}
	if ( (*( current_position)) < 22u ) {
		if ( (*( current_position)) > 14u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto st77;
		} else if ( (*( current_position)) >= 8u )
			goto tr173;
	} else if ( (*( current_position)) > 23u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 33u <= (*( current_position)) && (*( current_position)) <= 34u )
				goto tr173;
		} else if ( (*( current_position)) > 66u ) {
			if ( 96u <= (*( current_position)) && (*( current_position)) <= 99u )
				goto tr177;
		} else
			goto tr173;
	} else
		goto tr173;
	goto tr16;
st77:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof77;
case 77:
	switch( (*( current_position)) ) {
		case 4u: goto tr180;
		case 5u: goto tr181;
		case 12u: goto tr180;
		case 13u: goto tr181;
		case 20u: goto tr180;
		case 21u: goto tr181;
		case 28u: goto tr180;
		case 29u: goto tr181;
		case 36u: goto tr180;
		case 37u: goto tr181;
		case 44u: goto tr180;
		case 45u: goto tr181;
		case 52u: goto tr180;
		case 53u: goto tr181;
		case 60u: goto tr180;
		case 61u: goto tr181;
		case 68u: goto tr183;
		case 76u: goto tr183;
		case 84u: goto tr183;
		case 92u: goto tr183;
		case 100u: goto tr183;
		case 108u: goto tr183;
		case 116u: goto tr183;
		case 124u: goto tr183;
		case 132u: goto tr184;
		case 140u: goto tr184;
		case 148u: goto tr184;
		case 156u: goto tr184;
		case 164u: goto tr184;
		case 172u: goto tr184;
		case 180u: goto tr184;
		case 188u: goto tr184;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr181;
	} else if ( (*( current_position)) >= 64u )
		goto tr182;
	goto tr179;
st78:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof78;
case 78:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr185;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr185;
	} else
		goto tr185;
	goto tr16;
st79:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof79;
case 79:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr185;
	} else if ( (*( current_position)) >= 208u )
		goto tr185;
	goto tr16;
st80:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof80;
case 80:
	switch( (*( current_position)) ) {
		case 4u: goto tr187;
		case 5u: goto tr188;
		case 12u: goto tr187;
		case 13u: goto tr188;
		case 20u: goto tr187;
		case 21u: goto tr188;
		case 28u: goto tr187;
		case 29u: goto tr188;
		case 36u: goto tr187;
		case 37u: goto tr188;
		case 44u: goto tr187;
		case 45u: goto tr188;
		case 52u: goto tr187;
		case 53u: goto tr188;
		case 60u: goto tr187;
		case 61u: goto tr188;
		case 68u: goto tr190;
		case 76u: goto tr190;
		case 84u: goto tr190;
		case 92u: goto tr190;
		case 100u: goto tr190;
		case 108u: goto tr190;
		case 116u: goto tr190;
		case 124u: goto tr190;
		case 132u: goto tr191;
		case 140u: goto tr191;
		case 148u: goto tr191;
		case 156u: goto tr191;
		case 164u: goto tr191;
		case 172u: goto tr191;
		case 180u: goto tr191;
		case 188u: goto tr191;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr188;
	} else if ( (*( current_position)) >= 64u )
		goto tr189;
	goto tr186;
st81:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof81;
case 81:
	if ( (*( current_position)) == 15u )
		goto st82;
	goto tr16;
st82:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof82;
case 82:
	if ( (*( current_position)) == 31u )
		goto st83;
	goto tr16;
st83:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof83;
case 83:
	if ( (*( current_position)) == 132u )
		goto st84;
	goto tr16;
st84:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof84;
case 84:
	if ( (*( current_position)) == 0u )
		goto st85;
	goto tr16;
st85:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof85;
case 85:
	if ( (*( current_position)) == 0u )
		goto st86;
	goto tr16;
st86:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof86;
case 86:
	if ( (*( current_position)) == 0u )
		goto st87;
	goto tr16;
st87:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof87;
case 87:
	if ( (*( current_position)) == 0u )
		goto st88;
	goto tr16;
st88:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof88;
case 88:
	if ( (*( current_position)) == 0u )
		goto tr199;
	goto tr16;
st89:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof89;
case 89:
	switch( (*( current_position)) ) {
		case 46u: goto st81;
		case 102u: goto st90;
	}
	goto tr16;
st90:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof90;
case 90:
	switch( (*( current_position)) ) {
		case 46u: goto st81;
		case 102u: goto st91;
	}
	goto tr16;
st91:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof91;
case 91:
	switch( (*( current_position)) ) {
		case 46u: goto st81;
		case 102u: goto st92;
	}
	goto tr16;
st92:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof92;
case 92:
	switch( (*( current_position)) ) {
		case 46u: goto st81;
		case 102u: goto st93;
	}
	goto tr16;
st93:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof93;
case 93:
	if ( (*( current_position)) == 46u )
		goto st81;
	goto tr16;
st94:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof94;
case 94:
	switch( (*( current_position)) ) {
		case 4u: goto st95;
		case 5u: goto st96;
		case 12u: goto st95;
		case 13u: goto st96;
		case 20u: goto st95;
		case 21u: goto st96;
		case 28u: goto st95;
		case 29u: goto st96;
		case 36u: goto st95;
		case 37u: goto st96;
		case 44u: goto st95;
		case 45u: goto st96;
		case 52u: goto st95;
		case 53u: goto st96;
		case 60u: goto st95;
		case 61u: goto st96;
		case 68u: goto st101;
		case 76u: goto st101;
		case 84u: goto st101;
		case 92u: goto st101;
		case 100u: goto st101;
		case 108u: goto st101;
		case 116u: goto st101;
		case 124u: goto st101;
		case 132u: goto st102;
		case 140u: goto st102;
		case 148u: goto st102;
		case 156u: goto st102;
		case 164u: goto st102;
		case 172u: goto st102;
		case 180u: goto st102;
		case 188u: goto st102;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st96;
	} else if ( (*( current_position)) >= 64u )
		goto st100;
	goto st64;
st95:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof95;
case 95:
	switch( (*( current_position)) ) {
		case 5u: goto st96;
		case 13u: goto st96;
		case 21u: goto st96;
		case 29u: goto st96;
		case 37u: goto st96;
		case 45u: goto st96;
		case 53u: goto st96;
		case 61u: goto st96;
		case 69u: goto st96;
		case 77u: goto st96;
		case 85u: goto st96;
		case 93u: goto st96;
		case 101u: goto st96;
		case 109u: goto st96;
		case 117u: goto st96;
		case 125u: goto st96;
		case 133u: goto st96;
		case 141u: goto st96;
		case 149u: goto st96;
		case 157u: goto st96;
		case 165u: goto st96;
		case 173u: goto st96;
		case 181u: goto st96;
		case 189u: goto st96;
		case 197u: goto st96;
		case 205u: goto st96;
		case 213u: goto st96;
		case 221u: goto st96;
		case 229u: goto st96;
		case 237u: goto st96;
		case 245u: goto st96;
		case 253u: goto st96;
	}
	goto st64;
st96:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof96;
case 96:
	goto st97;
st97:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof97;
case 97:
	goto st98;
st98:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof98;
case 98:
	goto st99;
st99:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof99;
case 99:
	goto tr212;
st100:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof100;
case 100:
	goto tr213;
st101:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof101;
case 101:
	goto st100;
st102:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof102;
case 102:
	goto st96;
tr452:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st103;
st103:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof103;
case 103:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st40;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st36;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st36;
		} else
			goto tr16;
	} else
		goto st40;
	goto st10;
st104:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof104;
case 104:
	switch( (*( current_position)) ) {
		case 4u: goto st95;
		case 5u: goto st96;
		case 68u: goto st101;
		case 132u: goto st102;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st64;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st64;
		} else if ( (*( current_position)) >= 128u )
			goto st96;
	} else
		goto st100;
	goto tr16;
tr458:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st105;
st105:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof105;
case 105:
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
st106:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof106;
case 106:
	switch( (*( current_position)) ) {
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 52u: goto st2;
		case 53u: goto st3;
		case 100u: goto tr218;
		case 116u: goto st8;
		case 164u: goto tr219;
		case 180u: goto st9;
	}
	if ( (*( current_position)) < 96u ) {
		if ( (*( current_position)) > 39u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr0;
		} else if ( (*( current_position)) >= 32u )
			goto tr214;
	} else if ( (*( current_position)) > 103u ) {
		if ( (*( current_position)) < 160u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto st7;
		} else if ( (*( current_position)) > 167u ) {
			if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
				goto st3;
		} else
			goto tr216;
	} else
		goto tr217;
	goto tr16;
st107:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof107;
case 107:
	switch( (*( current_position)) ) {
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 100u: goto tr218;
		case 116u: goto tr218;
		case 164u: goto tr219;
		case 180u: goto tr219;
	}
	if ( (*( current_position)) < 96u ) {
		if ( (*( current_position)) > 39u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr214;
		} else if ( (*( current_position)) >= 32u )
			goto tr214;
	} else if ( (*( current_position)) > 103u ) {
		if ( (*( current_position)) < 160u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr217;
		} else if ( (*( current_position)) > 167u ) {
			if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
				goto tr216;
		} else
			goto tr216;
	} else
		goto tr217;
	goto tr16;
tr139:
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st108;
tr401:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st108;
st108:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof108;
case 108:
	switch( (*( current_position)) ) {
		case 1u: goto st29;
		case 9u: goto st29;
		case 15u: goto st109;
		case 17u: goto st29;
		case 25u: goto st29;
		case 33u: goto st29;
		case 41u: goto st29;
		case 49u: goto st29;
		case 129u: goto st110;
		case 131u: goto st111;
		case 135u: goto st29;
		case 247u: goto st112;
		case 255u: goto st113;
	}
	goto tr16;
st109:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof109;
case 109:
	switch( (*( current_position)) ) {
		case 177u: goto st29;
		case 193u: goto st29;
	}
	goto tr16;
st110:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof110;
case 110:
	switch( (*( current_position)) ) {
		case 4u: goto st95;
		case 5u: goto st96;
		case 12u: goto st95;
		case 13u: goto st96;
		case 20u: goto st95;
		case 21u: goto st96;
		case 28u: goto st95;
		case 29u: goto st96;
		case 36u: goto st95;
		case 37u: goto st96;
		case 44u: goto st95;
		case 45u: goto st96;
		case 52u: goto st95;
		case 53u: goto st96;
		case 68u: goto st101;
		case 76u: goto st101;
		case 84u: goto st101;
		case 92u: goto st101;
		case 100u: goto st101;
		case 108u: goto st101;
		case 116u: goto st101;
		case 132u: goto st102;
		case 140u: goto st102;
		case 148u: goto st102;
		case 156u: goto st102;
		case 164u: goto st102;
		case 172u: goto st102;
		case 180u: goto st102;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st64;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st96;
	} else
		goto st100;
	goto tr16;
st111:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof111;
case 111:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st10;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st36;
	} else
		goto st40;
	goto tr16;
st112:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof112;
case 112:
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
st113:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof113;
case 113:
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
tr140:
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st114;
tr404:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st114;
st114:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof114;
case 114:
	switch( (*( current_position)) ) {
		case 15u: goto st115;
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st115:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof115;
case 115:
	if ( (*( current_position)) == 56u )
		goto st116;
	goto tr16;
st116:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof116;
case 116:
	if ( (*( current_position)) == 241u )
		goto tr227;
	goto tr16;
tr141:
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st117;
tr418:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st117;
st117:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof117;
case 117:
	switch( (*( current_position)) ) {
		case 15u: goto st118;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st118:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof118;
case 118:
	switch( (*( current_position)) ) {
		case 184u: goto tr229;
		case 188u: goto tr230;
		case 189u: goto tr231;
	}
	goto tr16;
st119:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof119;
case 119:
	switch( (*( current_position)) ) {
		case 4u: goto st95;
		case 5u: goto st96;
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
		case 68u: goto st101;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st102;
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
				goto st64;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st100;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st96;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st64;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr474:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st120;
st120:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof120;
case 120:
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
tr296:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st121;
tr444:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st121;
st121:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof121;
case 121:
	switch( (*( current_position)) ) {
		case 4u: goto st122;
		case 5u: goto st123;
		case 12u: goto st122;
		case 13u: goto st123;
		case 20u: goto st122;
		case 21u: goto st123;
		case 28u: goto st122;
		case 29u: goto st123;
		case 36u: goto st122;
		case 37u: goto st123;
		case 44u: goto st122;
		case 45u: goto st123;
		case 52u: goto st122;
		case 53u: goto st123;
		case 60u: goto st122;
		case 61u: goto st123;
		case 68u: goto st128;
		case 76u: goto st128;
		case 84u: goto st128;
		case 92u: goto st128;
		case 100u: goto st128;
		case 108u: goto st128;
		case 116u: goto st128;
		case 124u: goto st128;
		case 132u: goto st129;
		case 140u: goto st129;
		case 148u: goto st129;
		case 156u: goto st129;
		case 164u: goto st129;
		case 172u: goto st129;
		case 180u: goto st129;
		case 188u: goto st129;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st123;
	} else if ( (*( current_position)) >= 64u )
		goto st127;
	goto st11;
tr291:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st122;
st122:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof122;
case 122:
	switch( (*( current_position)) ) {
		case 5u: goto st123;
		case 13u: goto st123;
		case 21u: goto st123;
		case 29u: goto st123;
		case 37u: goto st123;
		case 45u: goto st123;
		case 53u: goto st123;
		case 61u: goto st123;
		case 69u: goto st123;
		case 77u: goto st123;
		case 85u: goto st123;
		case 93u: goto st123;
		case 101u: goto st123;
		case 109u: goto st123;
		case 117u: goto st123;
		case 125u: goto st123;
		case 133u: goto st123;
		case 141u: goto st123;
		case 149u: goto st123;
		case 157u: goto st123;
		case 165u: goto st123;
		case 173u: goto st123;
		case 181u: goto st123;
		case 189u: goto st123;
		case 197u: goto st123;
		case 205u: goto st123;
		case 213u: goto st123;
		case 221u: goto st123;
		case 229u: goto st123;
		case 237u: goto st123;
		case 245u: goto st123;
		case 253u: goto st123;
	}
	goto st11;
tr292:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st123;
st123:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof123;
case 123:
	goto st124;
st124:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof124;
case 124:
	goto st125;
st125:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof125;
case 125:
	goto st126;
st126:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof126;
case 126:
	goto tr241;
tr293:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st127;
st127:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof127;
case 127:
	goto tr242;
tr294:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st128;
st128:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof128;
case 128:
	goto st127;
tr295:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st129;
st129:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof129;
case 129:
	goto st123;
tr447:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st130;
st130:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof130;
case 130:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
		case 224u: goto st131;
		case 225u: goto st241;
		case 226u: goto st243;
		case 227u: goto st245;
		case 228u: goto st247;
		case 229u: goto st249;
		case 230u: goto st251;
		case 231u: goto st253;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else if ( (*( current_position)) >= 64u )
		goto st40;
	goto st10;
st131:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof131;
case 131:
	if ( (*( current_position)) == 224u )
		goto tr251;
	goto tr11;
tr251:
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
	goto st256;
st256:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof256;
case 256:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr475;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr449:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st132;
st132:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof132;
case 132:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 232u: goto st133;
		case 233u: goto st148;
		case 234u: goto st156;
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
st133:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof133;
case 133:
	switch( (*( current_position)) ) {
		case 64u: goto tr255;
		case 68u: goto tr256;
		case 72u: goto tr255;
		case 76u: goto tr256;
		case 80u: goto tr255;
		case 84u: goto tr256;
		case 88u: goto tr255;
		case 92u: goto tr256;
		case 96u: goto tr255;
		case 100u: goto tr256;
		case 104u: goto tr255;
		case 108u: goto tr256;
		case 112u: goto tr255;
		case 116u: goto tr256;
		case 120u: goto tr257;
		case 124u: goto tr256;
		case 192u: goto tr258;
		case 196u: goto tr256;
		case 200u: goto tr258;
		case 204u: goto tr256;
		case 208u: goto tr258;
		case 212u: goto tr256;
		case 216u: goto tr258;
		case 220u: goto tr256;
		case 224u: goto tr258;
		case 228u: goto tr256;
		case 232u: goto tr258;
		case 236u: goto tr256;
		case 240u: goto tr258;
		case 244u: goto tr256;
		case 248u: goto tr258;
		case 252u: goto tr256;
	}
	goto tr16;
tr255:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st134;
st134:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof134;
case 134:
	switch( (*( current_position)) ) {
		case 166u: goto tr259;
		case 182u: goto tr259;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr259;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr259;
		} else
			goto tr259;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 204u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr259;
		} else if ( (*( current_position)) > 207u ) {
			if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
				goto tr260;
		} else
			goto tr260;
	} else
		goto tr259;
	goto tr16;
tr259:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st135;
tr364:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st135;
tr365:
	{
    SET_CPU_FEATURE(CPUFeature_FMA4);
  }
	goto st135;
st135:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof135;
case 135:
	switch( (*( current_position)) ) {
		case 4u: goto st137;
		case 5u: goto st138;
		case 12u: goto st137;
		case 13u: goto st138;
		case 20u: goto st137;
		case 21u: goto st138;
		case 28u: goto st137;
		case 29u: goto st138;
		case 36u: goto st137;
		case 37u: goto st138;
		case 44u: goto st137;
		case 45u: goto st138;
		case 52u: goto st137;
		case 53u: goto st138;
		case 60u: goto st137;
		case 61u: goto st138;
		case 68u: goto st143;
		case 76u: goto st143;
		case 84u: goto st143;
		case 92u: goto st143;
		case 100u: goto st143;
		case 108u: goto st143;
		case 116u: goto st143;
		case 124u: goto st143;
		case 132u: goto st144;
		case 140u: goto st144;
		case 148u: goto st144;
		case 156u: goto st144;
		case 164u: goto st144;
		case 172u: goto st144;
		case 180u: goto st144;
		case 188u: goto st144;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st138;
	} else if ( (*( current_position)) >= 64u )
		goto st142;
	goto st136;
tr270:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st136;
tr271:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st136;
st136:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof136;
case 136:
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
st137:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof137;
case 137:
	switch( (*( current_position)) ) {
		case 5u: goto st138;
		case 13u: goto st138;
		case 21u: goto st138;
		case 29u: goto st138;
		case 37u: goto st138;
		case 45u: goto st138;
		case 53u: goto st138;
		case 61u: goto st138;
		case 69u: goto st138;
		case 77u: goto st138;
		case 85u: goto st138;
		case 93u: goto st138;
		case 101u: goto st138;
		case 109u: goto st138;
		case 117u: goto st138;
		case 125u: goto st138;
		case 133u: goto st138;
		case 141u: goto st138;
		case 149u: goto st138;
		case 157u: goto st138;
		case 165u: goto st138;
		case 173u: goto st138;
		case 181u: goto st138;
		case 189u: goto st138;
		case 197u: goto st138;
		case 205u: goto st138;
		case 213u: goto st138;
		case 221u: goto st138;
		case 229u: goto st138;
		case 237u: goto st138;
		case 245u: goto st138;
		case 253u: goto st138;
	}
	goto st136;
st138:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof138;
case 138:
	goto st139;
st139:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof139;
case 139:
	goto st140;
st140:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof140;
case 140:
	goto st141;
st141:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof141;
case 141:
	goto tr270;
st142:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof142;
case 142:
	goto tr271;
st143:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof143;
case 143:
	goto st142;
st144:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof144;
case 144:
	goto st138;
tr256:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st145;
st145:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof145;
case 145:
	if ( (*( current_position)) == 162u )
		goto tr259;
	goto tr16;
tr257:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st146;
st146:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof146;
case 146:
	switch( (*( current_position)) ) {
		case 166u: goto tr259;
		case 182u: goto tr259;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr259;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr259;
		} else
			goto tr259;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 192u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr259;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr260;
			} else if ( (*( current_position)) >= 204u )
				goto tr260;
		} else
			goto tr260;
	} else
		goto tr259;
	goto tr16;
tr258:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st147;
st147:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof147;
case 147:
	if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
		goto tr259;
	goto tr16;
st148:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof148;
case 148:
	switch( (*( current_position)) ) {
		case 64u: goto tr272;
		case 72u: goto tr272;
		case 80u: goto tr272;
		case 88u: goto tr272;
		case 96u: goto tr272;
		case 104u: goto tr272;
		case 112u: goto tr272;
		case 120u: goto tr273;
		case 124u: goto tr274;
		case 192u: goto tr275;
		case 200u: goto tr275;
		case 208u: goto tr275;
		case 216u: goto tr275;
		case 224u: goto tr275;
		case 232u: goto tr275;
		case 240u: goto tr275;
		case 248u: goto tr275;
	}
	goto tr16;
tr272:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st149;
st149:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof149;
case 149:
	switch( (*( current_position)) ) {
		case 1u: goto st150;
		case 2u: goto st151;
	}
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr278;
	goto tr16;
st150:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof150;
case 150:
	switch( (*( current_position)) ) {
		case 12u: goto tr280;
		case 13u: goto tr281;
		case 20u: goto tr280;
		case 21u: goto tr281;
		case 28u: goto tr280;
		case 29u: goto tr281;
		case 36u: goto tr280;
		case 37u: goto tr281;
		case 44u: goto tr280;
		case 45u: goto tr281;
		case 52u: goto tr280;
		case 53u: goto tr281;
		case 60u: goto tr280;
		case 61u: goto tr281;
		case 76u: goto tr283;
		case 84u: goto tr283;
		case 92u: goto tr283;
		case 100u: goto tr283;
		case 108u: goto tr283;
		case 116u: goto tr283;
		case 124u: goto tr283;
		case 140u: goto tr284;
		case 148u: goto tr284;
		case 156u: goto tr284;
		case 164u: goto tr284;
		case 172u: goto tr284;
		case 180u: goto tr284;
		case 188u: goto tr284;
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
			goto tr281;
	} else
		goto tr282;
	goto tr279;
st151:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof151;
case 151:
	switch( (*( current_position)) ) {
		case 12u: goto tr280;
		case 13u: goto tr281;
		case 52u: goto tr280;
		case 53u: goto tr281;
		case 76u: goto tr283;
		case 116u: goto tr283;
		case 140u: goto tr284;
		case 180u: goto tr284;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr279;
		} else if ( (*( current_position)) > 55u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto tr282;
		} else
			goto tr279;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto tr281;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr279;
			} else if ( (*( current_position)) >= 200u )
				goto tr279;
		} else
			goto tr281;
	} else
		goto tr282;
	goto tr16;
tr273:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st152;
st152:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof152;
case 152:
	switch( (*( current_position)) ) {
		case 1u: goto st150;
		case 2u: goto st151;
		case 18u: goto st153;
		case 203u: goto tr278;
		case 219u: goto tr278;
	}
	if ( (*( current_position)) < 198u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 131u )
				goto tr278;
		} else if ( (*( current_position)) > 155u ) {
			if ( 193u <= (*( current_position)) && (*( current_position)) <= 195u )
				goto tr278;
		} else
			goto tr278;
	} else if ( (*( current_position)) > 199u ) {
		if ( (*( current_position)) < 214u ) {
			if ( 209u <= (*( current_position)) && (*( current_position)) <= 211u )
				goto tr278;
		} else if ( (*( current_position)) > 215u ) {
			if ( 225u <= (*( current_position)) && (*( current_position)) <= 227u )
				goto tr278;
		} else
			goto tr278;
	} else
		goto tr278;
	goto tr16;
st153:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof153;
case 153:
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr286;
	goto tr16;
tr274:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st154;
st154:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof154;
case 154:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 129u )
		goto tr278;
	goto tr16;
tr275:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st155;
st155:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof155;
case 155:
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr278;
	goto tr16;
st156:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof156;
case 156:
	switch( (*( current_position)) ) {
		case 64u: goto tr287;
		case 72u: goto tr287;
		case 80u: goto tr287;
		case 88u: goto tr287;
		case 96u: goto tr287;
		case 104u: goto tr287;
		case 112u: goto tr287;
		case 120u: goto tr288;
	}
	goto tr16;
tr287:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st157;
st157:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof157;
case 157:
	if ( (*( current_position)) == 18u )
		goto st158;
	goto tr16;
st158:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof158;
case 158:
	switch( (*( current_position)) ) {
		case 4u: goto tr291;
		case 5u: goto tr292;
		case 12u: goto tr291;
		case 13u: goto tr292;
		case 68u: goto tr294;
		case 76u: goto tr294;
		case 132u: goto tr295;
		case 140u: goto tr295;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr290;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr290;
		} else if ( (*( current_position)) >= 128u )
			goto tr292;
	} else
		goto tr293;
	goto tr16;
tr288:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st159;
st159:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof159;
case 159:
	switch( (*( current_position)) ) {
		case 16u: goto tr296;
		case 18u: goto st158;
	}
	goto tr16;
tr450:
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
	goto st257;
st257:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof257;
case 257:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr476;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr474;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr453:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st160;
st160:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof160;
case 160:
	switch( (*( current_position)) ) {
		case 225u: goto st161;
		case 226u: goto st183;
		case 227u: goto st192;
	}
	goto tr16;
st161:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof161;
case 161:
	switch( (*( current_position)) ) {
		case 65u: goto tr301;
		case 66u: goto tr302;
		case 67u: goto tr303;
		case 68u: goto tr304;
		case 69u: goto tr305;
		case 70u: goto tr306;
		case 71u: goto tr307;
		case 73u: goto tr301;
		case 74u: goto tr302;
		case 75u: goto tr303;
		case 76u: goto tr304;
		case 77u: goto tr305;
		case 78u: goto tr306;
		case 79u: goto tr307;
		case 81u: goto tr301;
		case 82u: goto tr302;
		case 83u: goto tr303;
		case 84u: goto tr304;
		case 85u: goto tr305;
		case 86u: goto tr306;
		case 87u: goto tr307;
		case 89u: goto tr301;
		case 90u: goto tr302;
		case 91u: goto tr303;
		case 92u: goto tr304;
		case 93u: goto tr305;
		case 94u: goto tr306;
		case 95u: goto tr307;
		case 97u: goto tr301;
		case 98u: goto tr302;
		case 99u: goto tr303;
		case 100u: goto tr304;
		case 101u: goto tr305;
		case 102u: goto tr306;
		case 103u: goto tr307;
		case 105u: goto tr301;
		case 106u: goto tr302;
		case 107u: goto tr303;
		case 108u: goto tr304;
		case 109u: goto tr305;
		case 110u: goto tr306;
		case 111u: goto tr307;
		case 113u: goto tr301;
		case 114u: goto tr302;
		case 115u: goto tr303;
		case 116u: goto tr304;
		case 117u: goto tr305;
		case 118u: goto tr306;
		case 119u: goto tr307;
		case 120u: goto tr308;
		case 121u: goto tr309;
		case 122u: goto tr310;
		case 123u: goto tr311;
		case 124u: goto tr312;
		case 125u: goto tr313;
		case 126u: goto tr314;
		case 127u: goto tr315;
	}
	if ( 64u <= (*( current_position)) && (*( current_position)) <= 112u )
		goto tr300;
	goto tr16;
tr300:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st162;
tr381:
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
		case 18u: goto st163;
		case 22u: goto st163;
		case 23u: goto tr318;
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 198u: goto tr319;
	}
	if ( (*( current_position)) < 46u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr317;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) > 89u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr317;
		} else if ( (*( current_position)) >= 84u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
st163:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof163;
case 163:
	switch( (*( current_position)) ) {
		case 4u: goto tr321;
		case 5u: goto tr322;
		case 12u: goto tr321;
		case 13u: goto tr322;
		case 20u: goto tr321;
		case 21u: goto tr322;
		case 28u: goto tr321;
		case 29u: goto tr322;
		case 36u: goto tr321;
		case 37u: goto tr322;
		case 44u: goto tr321;
		case 45u: goto tr322;
		case 52u: goto tr321;
		case 53u: goto tr322;
		case 60u: goto tr321;
		case 61u: goto tr322;
		case 68u: goto tr324;
		case 76u: goto tr324;
		case 84u: goto tr324;
		case 92u: goto tr324;
		case 100u: goto tr324;
		case 108u: goto tr324;
		case 116u: goto tr324;
		case 124u: goto tr324;
		case 132u: goto tr325;
		case 140u: goto tr325;
		case 148u: goto tr325;
		case 156u: goto tr325;
		case 164u: goto tr325;
		case 172u: goto tr325;
		case 180u: goto tr325;
		case 188u: goto tr325;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr322;
	} else if ( (*( current_position)) >= 64u )
		goto tr323;
	goto tr320;
tr301:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st164;
tr382:
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
		case 18u: goto tr318;
		case 81u: goto tr317;
		case 115u: goto st166;
		case 194u: goto tr319;
		case 198u: goto tr319;
	}
	if ( (*( current_position)) < 116u ) {
		if ( (*( current_position)) < 46u ) {
			if ( (*( current_position)) > 21u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr318;
			} else if ( (*( current_position)) >= 20u )
				goto tr317;
		} else if ( (*( current_position)) > 47u ) {
			if ( (*( current_position)) < 92u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 89u )
					goto tr317;
			} else if ( (*( current_position)) > 109u ) {
				if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
					goto st165;
			} else
				goto tr317;
		} else
			goto tr317;
	} else if ( (*( current_position)) > 118u ) {
		if ( (*( current_position)) < 216u ) {
			if ( (*( current_position)) > 125u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr317;
			} else if ( (*( current_position)) >= 124u )
				goto tr317;
		} else if ( (*( current_position)) > 229u ) {
			if ( (*( current_position)) < 241u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr317;
			} else if ( (*( current_position)) > 246u ) {
				if ( 248u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr317;
			} else
				goto tr317;
		} else
			goto tr317;
	} else
		goto tr317;
	goto tr16;
st165:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof165;
case 165:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr328;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr328;
	} else
		goto tr328;
	goto tr16;
st166:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof166;
case 166:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr328;
	} else if ( (*( current_position)) >= 208u )
		goto tr328;
	goto tr16;
tr302:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st167;
st167:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof167;
case 167:
	switch( (*( current_position)) ) {
		case 42u: goto tr317;
		case 81u: goto tr317;
		case 83u: goto tr317;
		case 194u: goto tr319;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr317;
	} else if ( (*( current_position)) >= 88u )
		goto tr317;
	goto tr16;
tr303:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st168;
st168:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof168;
case 168:
	switch( (*( current_position)) ) {
		case 42u: goto tr317;
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 208u: goto tr317;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr317;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr304:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st169;
tr385:
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
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 198u: goto tr319;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr317;
	} else if ( (*( current_position)) > 89u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr305:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st170;
tr386:
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
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 198u: goto tr319;
		case 208u: goto tr317;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr317;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr317;
		} else if ( (*( current_position)) >= 92u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr306:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st171;
tr387:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st171;
st171:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof171;
case 171:
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr329;
	goto tr16;
tr307:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st172;
tr388:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st172;
st172:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof172;
case 172:
	if ( (*( current_position)) == 208u )
		goto tr317;
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr317;
	} else if ( (*( current_position)) >= 16u )
		goto tr329;
	goto tr16;
tr308:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st173;
tr389:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st173;
st173:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof173;
case 173:
	switch( (*( current_position)) ) {
		case 18u: goto st163;
		case 19u: goto tr318;
		case 22u: goto st163;
		case 23u: goto tr318;
		case 43u: goto tr318;
		case 80u: goto tr329;
		case 119u: goto tr320;
		case 174u: goto st174;
		case 194u: goto tr319;
		case 198u: goto tr319;
	}
	if ( (*( current_position)) < 40u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr317;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 47u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr317;
		} else if ( (*( current_position)) >= 46u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
st174:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof174;
case 174:
	switch( (*( current_position)) ) {
		case 20u: goto tr321;
		case 21u: goto tr322;
		case 28u: goto tr321;
		case 29u: goto tr322;
		case 84u: goto tr324;
		case 92u: goto tr324;
		case 148u: goto tr325;
		case 156u: goto tr325;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr320;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr322;
	} else
		goto tr323;
	goto tr16;
tr309:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st175;
tr390:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st175;
st175:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof175;
case 175:
	switch( (*( current_position)) ) {
		case 43u: goto tr318;
		case 80u: goto tr329;
		case 81u: goto tr317;
		case 112u: goto tr319;
		case 115u: goto st166;
		case 127u: goto tr317;
		case 194u: goto tr319;
		case 196u: goto st176;
		case 197u: goto tr332;
		case 198u: goto tr319;
		case 215u: goto tr329;
		case 231u: goto tr318;
		case 247u: goto tr329;
	}
	if ( (*( current_position)) < 84u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 18u <= (*( current_position)) && (*( current_position)) <= 19u )
					goto tr318;
			} else if ( (*( current_position)) >= 16u )
				goto tr317;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) < 40u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr318;
			} else if ( (*( current_position)) > 41u ) {
				if ( 46u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr317;
			} else
				goto tr317;
		} else
			goto tr317;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 124u ) {
			if ( (*( current_position)) > 114u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr317;
			} else if ( (*( current_position)) >= 113u )
				goto st165;
		} else if ( (*( current_position)) > 125u ) {
			if ( (*( current_position)) < 216u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr317;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr317;
			} else
				goto tr317;
		} else
			goto tr317;
	} else
		goto tr317;
	goto tr16;
st176:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof176;
case 176:
	switch( (*( current_position)) ) {
		case 4u: goto tr333;
		case 5u: goto tr334;
		case 12u: goto tr333;
		case 13u: goto tr334;
		case 20u: goto tr333;
		case 21u: goto tr334;
		case 28u: goto tr333;
		case 29u: goto tr334;
		case 36u: goto tr333;
		case 37u: goto tr334;
		case 44u: goto tr333;
		case 45u: goto tr334;
		case 52u: goto tr333;
		case 53u: goto tr334;
		case 60u: goto tr333;
		case 61u: goto tr334;
		case 68u: goto tr336;
		case 76u: goto tr336;
		case 84u: goto tr336;
		case 92u: goto tr336;
		case 100u: goto tr336;
		case 108u: goto tr336;
		case 116u: goto tr336;
		case 124u: goto tr336;
		case 132u: goto tr337;
		case 140u: goto tr337;
		case 148u: goto tr337;
		case 156u: goto tr337;
		case 164u: goto tr337;
		case 172u: goto tr337;
		case 180u: goto tr337;
		case 188u: goto tr337;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr334;
	} else if ( (*( current_position)) >= 64u )
		goto tr335;
	goto tr328;
tr310:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st177;
st177:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof177;
case 177:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 22u: goto tr317;
		case 42u: goto tr317;
		case 111u: goto tr317;
		case 112u: goto tr319;
		case 194u: goto tr319;
		case 230u: goto tr317;
	}
	if ( (*( current_position)) < 81u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr317;
		} else if ( (*( current_position)) >= 16u )
			goto tr318;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr317;
		} else if ( (*( current_position)) >= 88u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr311:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st178;
st178:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof178;
case 178:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 42u: goto tr317;
		case 81u: goto tr317;
		case 112u: goto tr319;
		case 194u: goto tr319;
		case 208u: goto tr317;
		case 230u: goto tr317;
		case 240u: goto tr318;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr317;
		} else if ( (*( current_position)) >= 16u )
			goto tr318;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr317;
		} else if ( (*( current_position)) >= 92u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr312:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st179;
tr393:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st179;
st179:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof179;
case 179:
	switch( (*( current_position)) ) {
		case 43u: goto tr318;
		case 80u: goto tr329;
		case 119u: goto tr320;
		case 194u: goto tr319;
		case 198u: goto tr319;
	}
	if ( (*( current_position)) < 20u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr317;
	} else if ( (*( current_position)) > 21u ) {
		if ( (*( current_position)) > 41u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr317;
		} else if ( (*( current_position)) >= 40u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr313:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st180;
tr394:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st180;
st180:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof180;
case 180:
	switch( (*( current_position)) ) {
		case 43u: goto tr318;
		case 80u: goto tr329;
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 198u: goto tr319;
		case 208u: goto tr317;
		case 214u: goto tr317;
		case 230u: goto tr317;
		case 231u: goto tr318;
	}
	if ( (*( current_position)) < 40u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto tr317;
		} else if ( (*( current_position)) >= 16u )
			goto tr317;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr317;
		} else if ( (*( current_position)) >= 84u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr314:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st181;
tr395:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st181;
st181:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof181;
case 181:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 22u: goto tr317;
		case 91u: goto tr317;
		case 127u: goto tr317;
		case 230u: goto tr317;
	}
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr329;
	goto tr16;
tr315:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st182;
tr396:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st182;
st182:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof182;
case 182:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 208u: goto tr317;
		case 230u: goto tr317;
		case 240u: goto tr318;
	}
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr317;
	} else if ( (*( current_position)) >= 16u )
		goto tr329;
	goto tr16;
st183:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof183;
case 183:
	switch( (*( current_position)) ) {
		case 64u: goto tr338;
		case 65u: goto tr339;
		case 69u: goto tr340;
		case 72u: goto tr338;
		case 73u: goto tr339;
		case 77u: goto tr340;
		case 80u: goto tr338;
		case 81u: goto tr339;
		case 85u: goto tr340;
		case 88u: goto tr338;
		case 89u: goto tr339;
		case 93u: goto tr340;
		case 96u: goto tr338;
		case 97u: goto tr339;
		case 101u: goto tr340;
		case 104u: goto tr338;
		case 105u: goto tr339;
		case 109u: goto tr340;
		case 112u: goto tr338;
		case 113u: goto tr339;
		case 117u: goto tr340;
		case 120u: goto tr338;
		case 121u: goto tr341;
		case 125u: goto tr342;
		case 193u: goto tr343;
		case 197u: goto tr344;
		case 201u: goto tr343;
		case 205u: goto tr344;
		case 209u: goto tr343;
		case 213u: goto tr344;
		case 217u: goto tr343;
		case 221u: goto tr344;
		case 225u: goto tr343;
		case 229u: goto tr344;
		case 233u: goto tr343;
		case 237u: goto tr344;
		case 241u: goto tr343;
		case 245u: goto tr344;
		case 249u: goto tr343;
		case 253u: goto tr344;
	}
	goto tr16;
tr338:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st184;
st184:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof184;
case 184:
	switch( (*( current_position)) ) {
		case 242u: goto tr345;
		case 243u: goto st185;
		case 247u: goto tr345;
	}
	goto tr16;
st185:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof185;
case 185:
	switch( (*( current_position)) ) {
		case 12u: goto tr348;
		case 13u: goto tr349;
		case 20u: goto tr348;
		case 21u: goto tr349;
		case 28u: goto tr348;
		case 29u: goto tr349;
		case 76u: goto tr351;
		case 84u: goto tr351;
		case 92u: goto tr351;
		case 140u: goto tr352;
		case 148u: goto tr352;
		case 156u: goto tr352;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr347;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 159u ) {
			if ( 200u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr347;
		} else if ( (*( current_position)) >= 136u )
			goto tr349;
	} else
		goto tr350;
	goto tr16;
tr339:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st186;
st186:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof186;
case 186:
	if ( (*( current_position)) == 43u )
		goto tr317;
	if ( (*( current_position)) < 55u ) {
		if ( (*( current_position)) < 40u ) {
			if ( (*( current_position)) <= 13u )
				goto tr317;
		} else if ( (*( current_position)) > 41u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr318;
		} else
			goto tr317;
	} else if ( (*( current_position)) > 64u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
				goto tr353;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr354;
			} else if ( (*( current_position)) >= 182u )
				goto tr353;
		} else
			goto tr353;
	} else
		goto tr317;
	goto tr16;
tr340:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st187;
st187:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof187;
case 187:
	switch( (*( current_position)) ) {
		case 154u: goto tr353;
		case 156u: goto tr353;
		case 158u: goto tr353;
		case 170u: goto tr353;
		case 172u: goto tr353;
		case 174u: goto tr353;
		case 186u: goto tr353;
		case 188u: goto tr353;
		case 190u: goto tr353;
	}
	if ( (*( current_position)) < 150u ) {
		if ( (*( current_position)) > 13u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr318;
		} else if ( (*( current_position)) >= 12u )
			goto tr317;
	} else if ( (*( current_position)) > 152u ) {
		if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr353;
		} else if ( (*( current_position)) >= 166u )
			goto tr353;
	} else
		goto tr353;
	goto tr16;
tr341:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st188;
st188:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof188;
case 188:
	switch( (*( current_position)) ) {
		case 19u: goto tr355;
		case 23u: goto tr317;
		case 24u: goto tr318;
		case 42u: goto tr318;
	}
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
					goto tr317;
			} else
				goto tr317;
		} else if ( (*( current_position)) > 37u ) {
			if ( (*( current_position)) > 43u ) {
				if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr318;
			} else if ( (*( current_position)) >= 40u )
				goto tr317;
		} else
			goto tr317;
	} else if ( (*( current_position)) > 53u ) {
		if ( (*( current_position)) < 166u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr353;
			} else if ( (*( current_position)) >= 55u )
				goto tr317;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr354;
			} else if ( (*( current_position)) >= 182u )
				goto tr353;
		} else
			goto tr353;
	} else
		goto tr317;
	goto tr16;
tr342:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st189;
st189:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof189;
case 189:
	switch( (*( current_position)) ) {
		case 19u: goto tr355;
		case 23u: goto tr317;
		case 154u: goto tr353;
		case 156u: goto tr353;
		case 158u: goto tr353;
		case 170u: goto tr353;
		case 172u: goto tr353;
		case 174u: goto tr353;
		case 186u: goto tr353;
		case 188u: goto tr353;
		case 190u: goto tr353;
	}
	if ( (*( current_position)) < 44u ) {
		if ( (*( current_position)) > 15u ) {
			if ( 24u <= (*( current_position)) && (*( current_position)) <= 26u )
				goto tr318;
		} else if ( (*( current_position)) >= 12u )
			goto tr317;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
				goto tr353;
		} else if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr353;
		} else
			goto tr353;
	} else
		goto tr318;
	goto tr16;
tr343:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st190;
st190:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof190;
case 190:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr353;
	} else if ( (*( current_position)) > 175u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr353;
	} else
		goto tr353;
	goto tr16;
tr344:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st191;
st191:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof191;
case 191:
	switch( (*( current_position)) ) {
		case 154u: goto tr353;
		case 156u: goto tr353;
		case 158u: goto tr353;
		case 170u: goto tr353;
		case 172u: goto tr353;
		case 174u: goto tr353;
		case 186u: goto tr353;
		case 188u: goto tr353;
		case 190u: goto tr353;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto tr353;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto tr353;
	} else
		goto tr353;
	goto tr16;
st192:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof192;
case 192:
	switch( (*( current_position)) ) {
		case 65u: goto tr356;
		case 69u: goto tr357;
		case 73u: goto tr356;
		case 77u: goto tr357;
		case 81u: goto tr356;
		case 85u: goto tr357;
		case 89u: goto tr356;
		case 93u: goto tr357;
		case 97u: goto tr356;
		case 101u: goto tr357;
		case 105u: goto tr356;
		case 109u: goto tr357;
		case 113u: goto tr356;
		case 117u: goto tr357;
		case 121u: goto tr358;
		case 125u: goto tr359;
		case 193u: goto tr360;
		case 197u: goto tr361;
		case 201u: goto tr360;
		case 205u: goto tr361;
		case 209u: goto tr360;
		case 213u: goto tr361;
		case 217u: goto tr360;
		case 221u: goto tr361;
		case 225u: goto tr360;
		case 229u: goto tr361;
		case 233u: goto tr360;
		case 237u: goto tr361;
		case 241u: goto tr360;
		case 245u: goto tr361;
		case 249u: goto tr360;
		case 253u: goto tr361;
	}
	goto tr16;
tr356:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st193;
st193:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof193;
case 193:
	switch( (*( current_position)) ) {
		case 33u: goto tr319;
		case 68u: goto tr362;
		case 223u: goto tr366;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr319;
		} else if ( (*( current_position)) > 66u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
				goto tr363;
		} else
			goto tr319;
	} else if ( (*( current_position)) > 76u ) {
		if ( (*( current_position)) < 104u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr365;
		} else if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr365;
		} else
			goto tr365;
	} else
		goto tr364;
	goto tr16;
tr363:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st194;
st194:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof194;
case 194:
	switch( (*( current_position)) ) {
		case 4u: goto st196;
		case 5u: goto st197;
		case 12u: goto st196;
		case 13u: goto st197;
		case 20u: goto st196;
		case 21u: goto st197;
		case 28u: goto st196;
		case 29u: goto st197;
		case 36u: goto st196;
		case 37u: goto st197;
		case 44u: goto st196;
		case 45u: goto st197;
		case 52u: goto st196;
		case 53u: goto st197;
		case 60u: goto st196;
		case 61u: goto st197;
		case 68u: goto st202;
		case 76u: goto st202;
		case 84u: goto st202;
		case 92u: goto st202;
		case 100u: goto st202;
		case 108u: goto st202;
		case 116u: goto st202;
		case 124u: goto st202;
		case 132u: goto st203;
		case 140u: goto st203;
		case 148u: goto st203;
		case 156u: goto st203;
		case 164u: goto st203;
		case 172u: goto st203;
		case 180u: goto st203;
		case 188u: goto st203;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st197;
	} else if ( (*( current_position)) >= 64u )
		goto st201;
	goto st195;
tr377:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st195;
tr378:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st195;
st195:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof195;
case 195:
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 16u ) {
			if ( (*( current_position)) <= 3u )
				goto tr373;
		} else if ( (*( current_position)) > 19u ) {
			if ( 32u <= (*( current_position)) && (*( current_position)) <= 35u )
				goto tr373;
		} else
			goto tr373;
	} else if ( (*( current_position)) > 51u ) {
		if ( (*( current_position)) < 80u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 67u )
				goto tr373;
		} else if ( (*( current_position)) > 83u ) {
			if ( (*( current_position)) > 99u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 115u )
					goto tr373;
			} else if ( (*( current_position)) >= 96u )
				goto tr373;
		} else
			goto tr373;
	} else
		goto tr373;
	goto tr16;
st196:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof196;
case 196:
	switch( (*( current_position)) ) {
		case 5u: goto st197;
		case 13u: goto st197;
		case 21u: goto st197;
		case 29u: goto st197;
		case 37u: goto st197;
		case 45u: goto st197;
		case 53u: goto st197;
		case 61u: goto st197;
		case 69u: goto st197;
		case 77u: goto st197;
		case 85u: goto st197;
		case 93u: goto st197;
		case 101u: goto st197;
		case 109u: goto st197;
		case 117u: goto st197;
		case 125u: goto st197;
		case 133u: goto st197;
		case 141u: goto st197;
		case 149u: goto st197;
		case 157u: goto st197;
		case 165u: goto st197;
		case 173u: goto st197;
		case 181u: goto st197;
		case 189u: goto st197;
		case 197u: goto st197;
		case 205u: goto st197;
		case 213u: goto st197;
		case 221u: goto st197;
		case 229u: goto st197;
		case 237u: goto st197;
		case 245u: goto st197;
		case 253u: goto st197;
	}
	goto st195;
st197:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof197;
case 197:
	goto st198;
st198:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof198;
case 198:
	goto st199;
st199:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof199;
case 199:
	goto st200;
st200:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof200;
case 200:
	goto tr377;
st201:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof201;
case 201:
	goto tr378;
st202:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof202;
case 202:
	goto st201;
st203:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof203;
case 203:
	goto st197;
tr357:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st204;
st204:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof204;
case 204:
	switch( (*( current_position)) ) {
		case 6u: goto tr319;
		case 64u: goto tr319;
	}
	if ( (*( current_position)) < 92u ) {
		if ( (*( current_position)) < 12u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
				goto tr319;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 73u ) {
				if ( 74u <= (*( current_position)) && (*( current_position)) <= 75u )
					goto tr364;
			} else if ( (*( current_position)) >= 72u )
				goto tr363;
		} else
			goto tr319;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 108u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto tr365;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr365;
			} else if ( (*( current_position)) >= 120u )
				goto tr365;
		} else
			goto tr365;
	} else
		goto tr365;
	goto tr16;
tr358:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st205;
st205:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof205;
case 205:
	switch( (*( current_position)) ) {
		case 22u: goto tr319;
		case 23u: goto tr379;
		case 29u: goto tr380;
		case 32u: goto st176;
		case 68u: goto tr362;
		case 223u: goto tr366;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 5u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
					goto tr319;
			} else if ( (*( current_position)) >= 4u )
				goto tr319;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) > 34u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 66u )
					goto tr319;
			} else if ( (*( current_position)) >= 33u )
				goto tr319;
		} else
			goto st176;
	} else if ( (*( current_position)) > 73u ) {
		if ( (*( current_position)) < 96u ) {
			if ( (*( current_position)) > 76u ) {
				if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
					goto tr365;
			} else if ( (*( current_position)) >= 74u )
				goto tr364;
		} else if ( (*( current_position)) > 99u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr365;
			} else if ( (*( current_position)) >= 104u )
				goto tr365;
		} else
			goto tr319;
	} else
		goto tr363;
	goto tr16;
tr379:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st206;
st206:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof206;
case 206:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 63u )
			goto st10;
	} else if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else
		goto st40;
	goto tr16;
tr359:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st207;
st207:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof207;
case 207:
	switch( (*( current_position)) ) {
		case 29u: goto tr380;
		case 64u: goto tr319;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 12u ) {
			if ( (*( current_position)) > 6u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
					goto tr319;
			} else if ( (*( current_position)) >= 4u )
				goto tr319;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 25u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
					goto tr363;
			} else if ( (*( current_position)) >= 24u )
				goto tr319;
		} else
			goto tr319;
	} else if ( (*( current_position)) > 75u ) {
		if ( (*( current_position)) < 108u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
					goto tr365;
			} else if ( (*( current_position)) >= 92u )
				goto tr365;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr365;
			} else if ( (*( current_position)) >= 120u )
				goto tr365;
		} else
			goto tr365;
	} else
		goto tr364;
	goto tr16;
tr360:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st208;
st208:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof208;
case 208:
	if ( (*( current_position)) < 92u ) {
		if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
			goto tr363;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr365;
		} else if ( (*( current_position)) >= 104u )
			goto tr365;
	} else
		goto tr365;
	goto tr16;
tr361:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st209;
st209:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof209;
case 209:
	if ( (*( current_position)) < 104u ) {
		if ( (*( current_position)) > 73u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr365;
		} else if ( (*( current_position)) >= 72u )
			goto tr363;
	} else if ( (*( current_position)) > 105u ) {
		if ( (*( current_position)) < 120u ) {
			if ( 108u <= (*( current_position)) && (*( current_position)) <= 109u )
				goto tr365;
		} else if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr365;
		} else
			goto tr365;
	} else
		goto tr365;
	goto tr16;
tr454:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st210;
st210:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof210;
case 210:
	switch( (*( current_position)) ) {
		case 193u: goto tr382;
		case 194u: goto tr383;
		case 195u: goto tr384;
		case 196u: goto tr385;
		case 197u: goto tr386;
		case 198u: goto tr387;
		case 199u: goto tr388;
		case 201u: goto tr382;
		case 202u: goto tr383;
		case 203u: goto tr384;
		case 204u: goto tr385;
		case 205u: goto tr386;
		case 206u: goto tr387;
		case 207u: goto tr388;
		case 209u: goto tr382;
		case 210u: goto tr383;
		case 211u: goto tr384;
		case 212u: goto tr385;
		case 213u: goto tr386;
		case 214u: goto tr387;
		case 215u: goto tr388;
		case 217u: goto tr382;
		case 218u: goto tr383;
		case 219u: goto tr384;
		case 220u: goto tr385;
		case 221u: goto tr386;
		case 222u: goto tr387;
		case 223u: goto tr388;
		case 225u: goto tr382;
		case 226u: goto tr383;
		case 227u: goto tr384;
		case 228u: goto tr385;
		case 229u: goto tr386;
		case 230u: goto tr387;
		case 231u: goto tr388;
		case 233u: goto tr382;
		case 234u: goto tr383;
		case 235u: goto tr384;
		case 236u: goto tr385;
		case 237u: goto tr386;
		case 238u: goto tr387;
		case 239u: goto tr388;
		case 241u: goto tr382;
		case 242u: goto tr383;
		case 243u: goto tr384;
		case 244u: goto tr385;
		case 245u: goto tr386;
		case 246u: goto tr387;
		case 247u: goto tr388;
		case 248u: goto tr389;
		case 249u: goto tr390;
		case 250u: goto tr391;
		case 251u: goto tr392;
		case 252u: goto tr393;
		case 253u: goto tr394;
		case 254u: goto tr395;
		case 255u: goto tr396;
	}
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 240u )
		goto tr381;
	goto tr16;
tr383:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st211;
st211:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof211;
case 211:
	switch( (*( current_position)) ) {
		case 81u: goto tr317;
		case 83u: goto tr317;
		case 194u: goto tr319;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr317;
	} else if ( (*( current_position)) >= 88u )
		goto tr317;
	goto tr16;
tr384:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st212;
st212:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof212;
case 212:
	switch( (*( current_position)) ) {
		case 81u: goto tr317;
		case 194u: goto tr319;
		case 208u: goto tr317;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr317;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr391:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st213;
st213:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof213;
case 213:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 22u: goto tr317;
		case 111u: goto tr317;
		case 112u: goto tr319;
		case 194u: goto tr319;
		case 230u: goto tr317;
	}
	if ( (*( current_position)) < 81u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr318;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr317;
		} else if ( (*( current_position)) >= 88u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr392:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st214;
st214:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof214;
case 214:
	switch( (*( current_position)) ) {
		case 18u: goto tr317;
		case 81u: goto tr317;
		case 112u: goto tr319;
		case 194u: goto tr319;
		case 208u: goto tr317;
		case 230u: goto tr317;
		case 240u: goto tr318;
	}
	if ( (*( current_position)) < 88u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr318;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr317;
		} else if ( (*( current_position)) >= 92u )
			goto tr317;
	} else
		goto tr317;
	goto tr16;
tr455:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st215;
st215:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof215;
case 215:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 68u: goto st41;
		case 132u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st10;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st10;
		} else if ( (*( current_position)) >= 128u )
			goto st36;
	} else
		goto st40;
	goto tr16;
tr456:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st216;
st216:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof216;
case 216:
	switch( (*( current_position)) ) {
		case 4u: goto st122;
		case 5u: goto st123;
		case 68u: goto st128;
		case 132u: goto st129;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st11;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st11;
		} else if ( (*( current_position)) >= 128u )
			goto st123;
	} else
		goto st127;
	goto tr16;
tr457:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st217;
st217:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof217;
case 217:
	goto st218;
st218:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof218;
case 218:
	goto tr398;
tr459:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st219;
st219:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof219;
case 219:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr216;
	} else if ( (*( current_position)) >= 64u )
		goto tr217;
	goto tr214;
tr460:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st220;
st220:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof220;
case 220:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
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
				goto tr217;
		} else
			goto tr217;
	} else if ( (*( current_position)) > 135u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) > 143u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr216;
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
		goto tr216;
	goto tr214;
tr461:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st221;
st221:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof221;
case 221:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 12u: goto tr215;
		case 20u: goto tr215;
		case 28u: goto tr215;
		case 36u: goto tr215;
		case 44u: goto tr215;
		case 52u: goto tr215;
		case 60u: goto tr215;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
		case 233u: goto tr214;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr214;
			} else
				goto tr214;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr214;
			} else if ( (*( current_position)) >= 22u )
				goto tr214;
		} else
			goto tr214;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr214;
			} else if ( (*( current_position)) >= 46u )
				goto tr214;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) < 192u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr217;
			} else if ( (*( current_position)) > 223u ) {
				if ( 224u <= (*( current_position)) )
					goto tr16;
			} else
				goto tr399;
		} else
			goto tr214;
	} else
		goto tr214;
	goto tr216;
tr462:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st222;
st222:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof222;
case 222:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 108u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 172u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 56u ) {
			if ( (*( current_position)) > 31u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr214;
			} else
				goto tr214;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr217;
			} else if ( (*( current_position)) >= 64u )
				goto tr217;
		} else
			goto tr214;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 184u ) {
			if ( (*( current_position)) > 159u ) {
				if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr216;
			} else if ( (*( current_position)) >= 128u )
				goto tr216;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 192u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr399;
			} else if ( (*( current_position)) > 227u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr214;
			} else
				goto tr214;
		} else
			goto tr216;
	} else
		goto tr217;
	goto tr16;
tr463:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st223;
st223:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof223;
case 223:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr217;
	} else if ( (*( current_position)) > 191u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 223u )
			goto tr16;
	} else
		goto tr216;
	goto tr214;
tr464:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st224;
st224:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof224;
case 224:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr16;
		} else if ( (*( current_position)) > 103u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr217;
			} else if ( (*( current_position)) >= 104u )
				goto tr16;
		} else
			goto tr217;
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
			goto tr216;
	} else
		goto tr216;
	goto tr214;
tr465:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st225;
st225:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof225;
case 225:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr217;
	} else if ( (*( current_position)) > 191u ) {
		if ( (*( current_position)) > 216u ) {
			if ( 218u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr16;
		} else if ( (*( current_position)) >= 208u )
			goto tr16;
	} else
		goto tr216;
	goto tr214;
tr466:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st226;
st226:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof226;
case 226:
	switch( (*( current_position)) ) {
		case 4u: goto tr215;
		case 5u: goto tr216;
		case 12u: goto tr215;
		case 13u: goto tr216;
		case 20u: goto tr215;
		case 21u: goto tr216;
		case 28u: goto tr215;
		case 29u: goto tr216;
		case 36u: goto tr215;
		case 37u: goto tr216;
		case 44u: goto tr215;
		case 45u: goto tr216;
		case 52u: goto tr215;
		case 53u: goto tr216;
		case 60u: goto tr215;
		case 61u: goto tr216;
		case 68u: goto tr218;
		case 76u: goto tr218;
		case 84u: goto tr218;
		case 92u: goto tr218;
		case 100u: goto tr218;
		case 108u: goto tr218;
		case 116u: goto tr218;
		case 124u: goto tr218;
		case 132u: goto tr219;
		case 140u: goto tr219;
		case 148u: goto tr219;
		case 156u: goto tr219;
		case 164u: goto tr219;
		case 172u: goto tr219;
		case 180u: goto tr219;
		case 188u: goto tr219;
	}
	if ( (*( current_position)) < 192u ) {
		if ( (*( current_position)) > 127u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr216;
		} else if ( (*( current_position)) >= 64u )
			goto tr217;
	} else if ( (*( current_position)) > 223u ) {
		if ( (*( current_position)) > 231u ) {
			if ( 248u <= (*( current_position)) )
				goto tr16;
		} else if ( (*( current_position)) >= 225u )
			goto tr16;
	} else
		goto tr16;
	goto tr214;
tr468:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st227;
st227:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof227;
case 227:
	switch( (*( current_position)) ) {
		case 15u: goto st228;
		case 102u: goto tr401;
		case 128u: goto st111;
		case 129u: goto st229;
		case 131u: goto st111;
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
					goto st113;
			} else if ( (*( current_position)) >= 246u )
				goto st112;
		} else
			goto st29;
	} else
		goto st29;
	goto tr16;
st228:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof228;
case 228:
	if ( (*( current_position)) == 199u )
		goto st53;
	if ( (*( current_position)) > 177u ) {
		if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
			goto st29;
	} else if ( (*( current_position)) >= 176u )
		goto st29;
	goto tr16;
st229:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof229;
case 229:
	switch( (*( current_position)) ) {
		case 4u: goto st122;
		case 5u: goto st123;
		case 12u: goto st122;
		case 13u: goto st123;
		case 20u: goto st122;
		case 21u: goto st123;
		case 28u: goto st122;
		case 29u: goto st123;
		case 36u: goto st122;
		case 37u: goto st123;
		case 44u: goto st122;
		case 45u: goto st123;
		case 52u: goto st122;
		case 53u: goto st123;
		case 68u: goto st128;
		case 76u: goto st128;
		case 84u: goto st128;
		case 92u: goto st128;
		case 100u: goto st128;
		case 108u: goto st128;
		case 116u: goto st128;
		case 132u: goto st129;
		case 140u: goto st129;
		case 148u: goto st129;
		case 156u: goto st129;
		case 164u: goto st129;
		case 172u: goto st129;
		case 180u: goto st129;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st11;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st123;
	} else
		goto st127;
	goto tr16;
tr469:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st230;
st230:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof230;
case 230:
	switch( (*( current_position)) ) {
		case 15u: goto st231;
		case 102u: goto tr404;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 166u )
		goto tr0;
	goto tr16;
st231:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof231;
case 231:
	switch( (*( current_position)) ) {
		case 18u: goto tr406;
		case 43u: goto tr407;
		case 56u: goto st232;
		case 81u: goto tr405;
		case 112u: goto tr409;
		case 120u: goto tr410;
		case 121u: goto tr411;
		case 194u: goto tr409;
		case 208u: goto tr412;
		case 214u: goto tr413;
		case 230u: goto tr405;
		case 240u: goto tr414;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 42u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr405;
		} else if ( (*( current_position)) >= 16u )
			goto tr405;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr406;
		} else if ( (*( current_position)) >= 92u )
			goto tr405;
	} else
		goto tr405;
	goto tr16;
st232:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof232;
case 232:
	if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
		goto tr227;
	goto tr16;
tr410:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st233;
st233:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof233;
case 233:
	if ( 192u <= (*( current_position)) )
		goto st234;
	goto tr16;
st234:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof234;
case 234:
	goto tr416;
tr470:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st235;
st235:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof235;
case 235:
	switch( (*( current_position)) ) {
		case 15u: goto st236;
		case 102u: goto tr418;
		case 144u: goto tr419;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 170u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 164u )
		goto tr0;
	goto tr16;
st236:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof236;
case 236:
	switch( (*( current_position)) ) {
		case 18u: goto tr421;
		case 22u: goto tr421;
		case 43u: goto tr422;
		case 111u: goto tr423;
		case 112u: goto tr424;
		case 184u: goto tr229;
		case 188u: goto tr230;
		case 189u: goto tr231;
		case 194u: goto tr425;
		case 214u: goto tr426;
		case 230u: goto tr423;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 42u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
				goto tr420;
		} else if ( (*( current_position)) > 45u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 83u )
				goto tr420;
		} else
			goto tr420;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) < 92u ) {
			if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
				goto tr423;
		} else if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr423;
		} else
			goto tr420;
	} else
		goto tr420;
	goto tr16;
tr471:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st237;
st237:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof237;
case 237:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
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
		case 68u: goto st41;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st42;
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
				goto st40;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st36;
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
tr472:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st238;
st238:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof238;
case 238:
	switch( (*( current_position)) ) {
		case 4u: goto st122;
		case 5u: goto st123;
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
		case 68u: goto st128;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st129;
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
				goto st127;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st123;
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
tr473:
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
tr475:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st240;
st240:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof240;
case 240:
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
		case 208u: goto tr427;
		case 224u: goto tr427;
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
st241:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof241;
case 241:
	if ( (*( current_position)) == 224u )
		goto tr428;
	goto tr11;
tr428:
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
	goto st258;
st258:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof258;
case 258:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr477;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr477:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st242;
st242:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof242;
case 242:
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
		case 209u: goto tr427;
		case 225u: goto tr427;
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
st243:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof243;
case 243:
	if ( (*( current_position)) == 224u )
		goto tr429;
	goto tr11;
tr429:
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
	goto st259;
st259:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof259;
case 259:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr478;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr478:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st244;
st244:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof244;
case 244:
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
		case 210u: goto tr427;
		case 226u: goto tr427;
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
st245:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof245;
case 245:
	if ( (*( current_position)) == 224u )
		goto tr430;
	goto tr11;
tr430:
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
	goto st260;
st260:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof260;
case 260:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr479;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr479:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st246;
st246:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof246;
case 246:
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
		case 211u: goto tr427;
		case 227u: goto tr427;
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
st247:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof247;
case 247:
	if ( (*( current_position)) == 224u )
		goto tr431;
	goto tr11;
tr431:
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
	goto st261;
st261:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof261;
case 261:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr480;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr480:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st248;
st248:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof248;
case 248:
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
		case 212u: goto tr427;
		case 228u: goto tr427;
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
st249:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof249;
case 249:
	if ( (*( current_position)) == 224u )
		goto tr432;
	goto tr11;
tr432:
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
	goto st262;
st262:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof262;
case 262:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr481;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr481:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st250;
st250:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof250;
case 250:
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
		case 213u: goto tr427;
		case 229u: goto tr427;
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
st251:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof251;
case 251:
	if ( (*( current_position)) == 224u )
		goto tr433;
	goto tr11;
tr433:
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
	goto st263;
st263:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof263;
case 263:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr482;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr482:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st252;
st252:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof252;
case 252:
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
		case 214u: goto tr427;
		case 230u: goto tr427;
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
st253:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof253;
case 253:
	if ( (*( current_position)) == 224u )
		goto tr434;
	goto tr11;
tr434:
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
	goto st264;
st264:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof264;
case 264:
	switch( (*( current_position)) ) {
		case 4u: goto tr436;
		case 5u: goto tr437;
		case 12u: goto tr436;
		case 13u: goto tr437;
		case 14u: goto tr16;
		case 15u: goto tr438;
		case 20u: goto tr436;
		case 21u: goto tr437;
		case 28u: goto tr436;
		case 29u: goto tr437;
		case 36u: goto tr436;
		case 37u: goto tr437;
		case 44u: goto tr436;
		case 45u: goto tr437;
		case 46u: goto tr439;
		case 47u: goto tr16;
		case 52u: goto tr436;
		case 53u: goto tr437;
		case 60u: goto tr436;
		case 61u: goto tr437;
		case 62u: goto tr440;
		case 63u: goto tr16;
		case 101u: goto tr442;
		case 102u: goto tr443;
		case 104u: goto tr437;
		case 105u: goto tr444;
		case 106u: goto tr436;
		case 107u: goto tr445;
		case 128u: goto tr445;
		case 129u: goto tr444;
		case 130u: goto tr16;
		case 131u: goto tr447;
		case 141u: goto tr448;
		case 143u: goto tr449;
		case 155u: goto tr450;
		case 168u: goto tr436;
		case 169u: goto tr437;
		case 196u: goto tr453;
		case 197u: goto tr454;
		case 198u: goto tr455;
		case 199u: goto tr456;
		case 200u: goto tr457;
		case 216u: goto tr459;
		case 217u: goto tr460;
		case 218u: goto tr461;
		case 219u: goto tr462;
		case 220u: goto tr463;
		case 221u: goto tr464;
		case 222u: goto tr465;
		case 223u: goto tr466;
		case 235u: goto tr446;
		case 240u: goto tr468;
		case 242u: goto tr469;
		case 243u: goto tr470;
		case 246u: goto tr471;
		case 247u: goto tr472;
		case 254u: goto tr473;
		case 255u: goto tr483;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr16;
				} else
					goto tr435;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr16;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr16;
				} else
					goto tr435;
			} else
				goto tr435;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr435;
				} else if ( (*( current_position)) >= 38u )
					goto tr16;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr435;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr446;
				} else
					goto tr16;
			} else
				goto tr16;
		} else
			goto tr435;
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
						goto tr436;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr452;
				} else
					goto tr437;
			} else
				goto tr451;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr458;
				} else if ( (*( current_position)) >= 202u )
					goto tr16;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr467;
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
		goto tr435;
	goto tr441;
tr483:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st254;
st254:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof254;
case 254:
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
		case 215u: goto tr427;
		case 231u: goto tr427;
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
	_test_eof255: ( current_state) = 255; goto _test_eof; 
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
	_test_eof256: ( current_state) = 256; goto _test_eof; 
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
	_test_eof257: ( current_state) = 257; goto _test_eof; 
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
	_test_eof231: ( current_state) = 231; goto _test_eof; 
	_test_eof232: ( current_state) = 232; goto _test_eof; 
	_test_eof233: ( current_state) = 233; goto _test_eof; 
	_test_eof234: ( current_state) = 234; goto _test_eof; 
	_test_eof235: ( current_state) = 235; goto _test_eof; 
	_test_eof236: ( current_state) = 236; goto _test_eof; 
	_test_eof237: ( current_state) = 237; goto _test_eof; 
	_test_eof238: ( current_state) = 238; goto _test_eof; 
	_test_eof239: ( current_state) = 239; goto _test_eof; 
	_test_eof240: ( current_state) = 240; goto _test_eof; 
	_test_eof241: ( current_state) = 241; goto _test_eof; 
	_test_eof258: ( current_state) = 258; goto _test_eof; 
	_test_eof242: ( current_state) = 242; goto _test_eof; 
	_test_eof243: ( current_state) = 243; goto _test_eof; 
	_test_eof259: ( current_state) = 259; goto _test_eof; 
	_test_eof244: ( current_state) = 244; goto _test_eof; 
	_test_eof245: ( current_state) = 245; goto _test_eof; 
	_test_eof260: ( current_state) = 260; goto _test_eof; 
	_test_eof246: ( current_state) = 246; goto _test_eof; 
	_test_eof247: ( current_state) = 247; goto _test_eof; 
	_test_eof261: ( current_state) = 261; goto _test_eof; 
	_test_eof248: ( current_state) = 248; goto _test_eof; 
	_test_eof249: ( current_state) = 249; goto _test_eof; 
	_test_eof262: ( current_state) = 262; goto _test_eof; 
	_test_eof250: ( current_state) = 250; goto _test_eof; 
	_test_eof251: ( current_state) = 251; goto _test_eof; 
	_test_eof263: ( current_state) = 263; goto _test_eof; 
	_test_eof252: ( current_state) = 252; goto _test_eof; 
	_test_eof253: ( current_state) = 253; goto _test_eof; 
	_test_eof264: ( current_state) = 264; goto _test_eof; 
	_test_eof254: ( current_state) = 254; goto _test_eof; 

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
	case 244: 
	case 245: 
	case 246: 
	case 247: 
	case 248: 
	case 249: 
	case 250: 
	case 251: 
	case 252: 
	case 253: 
	case 254: 
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
