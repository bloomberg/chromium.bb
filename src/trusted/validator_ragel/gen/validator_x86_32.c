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





static const int x86_64_decoder_start = 247;
static const int x86_64_decoder_first_final = 247;
static const int x86_64_decoder_error = 0;

static const int x86_64_decoder_en_main = 247;




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
	goto st247;
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
	goto st247;
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
	goto st247;
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
	goto st247;
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
	goto st247;
tr21:
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
	goto st247;
tr29:
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
	goto st247;
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
	goto st247;
tr49:
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
	goto st247;
tr50:
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
	goto st247;
tr51:
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
	goto st247;
tr63:
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
	goto st247;
tr69:
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
	goto st247;
tr90:
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
	goto st247;
tr93:
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
	goto st247;
tr102:
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
	goto st247;
tr103:
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
	goto st247;
tr110:
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
	goto st247;
tr118:
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
	goto st247;
tr139:
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
tr186:
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
	goto st247;
tr260:
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
	goto st247;
tr267:
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
	goto st247;
tr301:
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
	goto st247;
tr328:
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
	goto st247;
tr354:
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
tr380:
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
	goto st247;
tr386:
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
	goto st247;
tr406:
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
	goto st247;
tr414:
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
	goto st247;
tr428:
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
	goto st247;
tr437:
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
	goto st247;
st247:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof247;
case 247:
	switch( (*( current_position)) ) {
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr461;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr23:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr28:
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
tr46:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st1;
tr142:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr151:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr154:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
  }
	goto st1;
tr155:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st1;
tr157:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr158:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st1;
tr208:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st1;
tr210:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_POPCNT);
  }
	goto st1;
tr211:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_TZCNT);
  }
	goto st1;
tr212:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_LZCNT);
  }
	goto st1;
tr259:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st1;
tr326:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st1;
tr298:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st1;
tr334:
	{
    SET_CPU_FEATURE(CPUFeature_FMA);
  }
	goto st1;
tr335:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st1;
tr336:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st1;
tr392:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr393:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr399:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr407:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st1;
tr408:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st1;
tr410:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st1;
tr422:
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
tr70:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st2;
tr52:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st2;
tr91:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st2;
tr94:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st2;
tr111:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st2;
tr261:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st2;
tr329:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st2;
tr302:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st2;
tr381:
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
tr71:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st3;
tr53:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st3;
tr92:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st3;
tr95:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st3;
tr112:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st3;
tr262:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st3;
tr330:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st3;
tr303:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st3;
tr382:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st3;
tr438:
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
tr72:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st7;
tr54:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st7;
tr96:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st7;
tr98:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st7;
tr113:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st7;
tr263:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st7;
tr331:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st7;
tr304:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st7;
tr383:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st7;
st7:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof7;
case 7:
	goto tr10;
tr73:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st8;
tr55:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st8;
tr97:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st8;
tr99:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st8;
tr114:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st8;
tr264:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st8;
tr332:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st8;
tr305:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st8;
tr384:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st8;
st8:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof8;
case 8:
	goto st7;
tr74:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st9;
tr56:
	{
    SET_CPU_FEATURE(CPUFeature_3DPRFTCH);
  }
	goto st9;
tr100:
	{
    SET_CPU_FEATURE(CPUFeature_FXSR);
  }
	goto st9;
tr101:
	{
    SET_CPU_FEATURE(CPUFeature_CLFLUSH);
  }
	goto st9;
tr115:
	{
    SET_CPU_FEATURE(CPUFeature_CX8);
  }
	goto st9;
tr265:
	{
    SET_CPU_FEATURE(CPUFeature_TBM);
  }
	goto st9;
tr333:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st9;
tr306:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st9;
tr385:
	{
    SET_CPU_FEATURE(CPUFeature_x87);
  }
	goto st9;
st9:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof9;
case 9:
	goto st3;
tr86:
	{
    SET_CPU_FEATURE(CPUFeature_MMX);
  }
	goto st10;
tr104:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st10;
tr84:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st10;
tr85:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st10;
tr172:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st10;
tr166:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st10;
tr173:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st10;
tr309:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st10;
tr379:
	{ }
	goto st10;
tr403:
	{ }
	goto st10;
tr423:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st10;
st10:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof10;
case 10:
	goto tr11;
tr222:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st11;
tr223:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st11;
tr271:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st11;
tr424:
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
tr19:
	{
        process_error(instruction_start, UNRECOGNIZED_INSTRUCTION, userdata);
        result = 1;
        goto error_detected;
    }
	goto st0;
st0:
( current_state) = 0;
	goto _out;
tr425:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st15;
st15:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof15;
case 15:
	switch( (*( current_position)) ) {
		case 0u: goto st16;
		case 1u: goto st17;
		case 11u: goto tr0;
		case 13u: goto st18;
		case 14u: goto tr21;
		case 15u: goto st19;
		case 18u: goto st29;
		case 19u: goto tr25;
		case 22u: goto st29;
		case 23u: goto tr25;
		case 24u: goto st31;
		case 31u: goto st32;
		case 43u: goto tr25;
		case 49u: goto tr29;
		case 80u: goto tr31;
		case 112u: goto tr33;
		case 115u: goto st44;
		case 119u: goto tr36;
		case 162u: goto tr0;
		case 164u: goto st34;
		case 165u: goto st1;
		case 172u: goto st34;
		case 174u: goto st49;
		case 195u: goto tr41;
		case 196u: goto st50;
		case 197u: goto tr43;
		case 199u: goto st52;
		case 212u: goto tr28;
		case 215u: goto tr45;
		case 218u: goto tr46;
		case 222u: goto tr46;
		case 224u: goto tr46;
		case 229u: goto tr32;
		case 231u: goto tr47;
		case 234u: goto tr46;
		case 238u: goto tr46;
		case 244u: goto tr28;
		case 246u: goto tr46;
		case 247u: goto tr48;
		case 251u: goto tr28;
	}
	if ( (*( current_position)) < 116u ) {
		if ( (*( current_position)) < 64u ) {
			if ( (*( current_position)) < 40u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 16u <= (*( current_position)) && (*( current_position)) <= 21u )
						goto tr23;
				} else if ( (*( current_position)) >= 2u )
					goto st1;
			} else if ( (*( current_position)) > 41u ) {
				if ( (*( current_position)) > 45u ) {
					if ( 46u <= (*( current_position)) && (*( current_position)) <= 47u )
						goto tr23;
				} else if ( (*( current_position)) >= 42u )
					goto tr28;
			} else
				goto tr23;
		} else if ( (*( current_position)) > 79u ) {
			if ( (*( current_position)) < 92u ) {
				if ( (*( current_position)) > 89u ) {
					if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
						goto tr28;
				} else if ( (*( current_position)) >= 81u )
					goto tr23;
			} else if ( (*( current_position)) > 95u ) {
				if ( (*( current_position)) < 110u ) {
					if ( 96u <= (*( current_position)) && (*( current_position)) <= 107u )
						goto tr32;
				} else if ( (*( current_position)) > 111u ) {
					if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
						goto st43;
				} else
					goto tr32;
			} else
				goto tr23;
		} else
			goto tr30;
	} else if ( (*( current_position)) > 118u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 144u ) {
				if ( (*( current_position)) > 127u ) {
					if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
						goto st45;
				} else if ( (*( current_position)) >= 126u )
					goto tr32;
			} else if ( (*( current_position)) > 159u ) {
				if ( (*( current_position)) < 182u ) {
					if ( 173u <= (*( current_position)) && (*( current_position)) <= 177u )
						goto st1;
				} else if ( (*( current_position)) > 183u ) {
					if ( 188u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st1;
				} else
					goto st1;
			} else
				goto tr30;
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
						goto tr46;
				} else if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto tr32;
				} else
					goto tr32;
			} else
				goto tr32;
		} else
			goto tr40;
	} else
		goto tr32;
	goto tr19;
st16:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof16;
case 16:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 24u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 47u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 152u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 216u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr0;
			} else if ( (*( current_position)) >= 200u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr19;
st17:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof17;
case 17:
	switch( (*( current_position)) ) {
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
		case 208u: goto tr50;
	}
	if ( (*( current_position)) < 160u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 32u <= (*( current_position)) && (*( current_position)) <= 39u )
				goto tr0;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 103u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto st7;
			} else if ( (*( current_position)) >= 96u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 167u ) {
		if ( (*( current_position)) < 200u ) {
			if ( 176u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto st3;
		} else if ( (*( current_position)) > 201u ) {
			if ( (*( current_position)) > 231u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 224u )
				goto tr0;
		} else
			goto tr49;
	} else
		goto st3;
	goto tr19;
st18:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof18;
case 18:
	switch( (*( current_position)) ) {
		case 4u: goto tr52;
		case 5u: goto tr53;
		case 12u: goto tr52;
		case 13u: goto tr53;
		case 68u: goto tr55;
		case 76u: goto tr55;
		case 132u: goto tr56;
		case 140u: goto tr56;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr51;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr53;
	} else
		goto tr54;
	goto tr19;
st19:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof19;
case 19:
	switch( (*( current_position)) ) {
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
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st22;
	} else if ( (*( current_position)) >= 64u )
		goto st26;
	goto st20;
tr67:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st20;
tr68:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st20;
st20:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof20;
case 20:
	switch( (*( current_position)) ) {
		case 12u: goto tr63;
		case 13u: goto tr21;
		case 28u: goto tr63;
		case 29u: goto tr21;
		case 138u: goto tr63;
		case 142u: goto tr63;
		case 144u: goto tr21;
		case 148u: goto tr21;
		case 154u: goto tr21;
		case 158u: goto tr21;
		case 160u: goto tr21;
		case 164u: goto tr21;
		case 170u: goto tr21;
		case 174u: goto tr21;
		case 176u: goto tr21;
		case 180u: goto tr21;
		case 187u: goto tr63;
		case 191u: goto tr21;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 151u )
			goto tr21;
	} else if ( (*( current_position)) > 167u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto tr21;
	} else
		goto tr21;
	goto tr19;
st21:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof21;
case 21:
	switch( (*( current_position)) ) {
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
	goto st25;
st25:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof25;
case 25:
	goto tr67;
st26:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof26;
case 26:
	goto tr68;
st27:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof27;
case 27:
	goto st26;
st28:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof28;
case 28:
	goto st22;
st29:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof29;
case 29:
	switch( (*( current_position)) ) {
		case 4u: goto tr70;
		case 5u: goto tr71;
		case 12u: goto tr70;
		case 13u: goto tr71;
		case 20u: goto tr70;
		case 21u: goto tr71;
		case 28u: goto tr70;
		case 29u: goto tr71;
		case 36u: goto tr70;
		case 37u: goto tr71;
		case 44u: goto tr70;
		case 45u: goto tr71;
		case 52u: goto tr70;
		case 53u: goto tr71;
		case 60u: goto tr70;
		case 61u: goto tr71;
		case 68u: goto tr73;
		case 76u: goto tr73;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 100u: goto tr73;
		case 108u: goto tr73;
		case 116u: goto tr73;
		case 124u: goto tr73;
		case 132u: goto tr74;
		case 140u: goto tr74;
		case 148u: goto tr74;
		case 156u: goto tr74;
		case 164u: goto tr74;
		case 172u: goto tr74;
		case 180u: goto tr74;
		case 188u: goto tr74;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr71;
	} else if ( (*( current_position)) >= 64u )
		goto tr72;
	goto tr69;
tr25:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st30;
tr41:
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st30;
tr47:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st30;
tr143:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st30;
tr156:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st30;
tr299:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st30;
tr401:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE3);
  }
	goto st30;
tr394:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st30;
tr409:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st30;
tr435:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st30;
st30:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof30;
case 30:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 12u: goto st2;
		case 20u: goto st2;
		case 28u: goto st2;
		case 36u: goto st2;
		case 44u: goto st2;
		case 52u: goto st2;
		case 60u: goto st2;
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
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr0;
			} else
				goto tr0;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr0;
			} else if ( (*( current_position)) >= 22u )
				goto tr0;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr0;
			} else if ( (*( current_position)) >= 46u )
				goto tr0;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr19;
			} else if ( (*( current_position)) >= 64u )
				goto st7;
		} else
			goto tr0;
	} else
		goto tr0;
	goto st3;
st31:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof31;
case 31:
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
	goto tr19;
st32:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof32;
case 32:
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
	goto tr19;
tr31:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st33;
tr45:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st33;
tr48:
	{
    SET_CPU_FEATURE(CPUFeature_EMMX);
  }
	goto st33;
tr146:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
tr150:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st33;
tr310:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st33;
tr400:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
tr398:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st33;
tr413:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st33;
st33:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof33;
case 33:
	if ( 192u <= (*( current_position)) )
		goto tr0;
	goto tr19;
tr40:
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st34;
tr33:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st34;
tr147:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr160:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSSE3);
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
tr164:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE42);
  }
	goto st34;
tr165:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_AES);
  }
	goto st34;
tr163:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_CLMUL);
  }
	goto st34;
tr241:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st34;
tr300:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st34;
tr347:
	{
    SET_CPU_FEATURE(CPUFeature_AESAVX);
  }
	goto st34;
tr361:
	{
    SET_CPU_FEATURE(CPUFeature_F16C);
  }
	goto st34;
tr343:
	{
    SET_CPU_FEATURE(CPUFeature_CLMULAVX);
  }
	goto st34;
tr396:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr412:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st34;
tr411:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st34;
tr432:
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
tr105:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st35;
tr167:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st35;
tr174:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st35;
tr314:
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
tr106:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st36;
tr168:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st36;
tr175:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st36;
tr315:
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
	goto tr84;
tr107:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st40;
tr169:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st40;
tr176:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st40;
tr316:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st40;
st40:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof40;
case 40:
	goto tr85;
tr108:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st41;
tr170:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st41;
tr177:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st41;
tr317:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st41;
st41:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof41;
case 41:
	goto st40;
tr109:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st42;
tr171:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE41);
  }
	goto st42;
tr178:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE);
  }
	goto st42;
tr318:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st42;
st42:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof42;
case 42:
	goto st36;
st43:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof43;
case 43:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr86;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr86;
	} else
		goto tr86;
	goto tr19;
st44:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof44;
case 44:
	if ( (*( current_position)) > 215u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr86;
	} else if ( (*( current_position)) >= 208u )
		goto tr86;
	goto tr19;
tr454:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
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
	goto st48;
st48:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof48;
case 48:
	goto tr90;
st49:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof49;
case 49:
	switch( (*( current_position)) ) {
		case 20u: goto tr70;
		case 21u: goto tr71;
		case 28u: goto tr70;
		case 29u: goto tr71;
		case 36u: goto tr91;
		case 37u: goto tr92;
		case 44u: goto tr91;
		case 45u: goto tr92;
		case 52u: goto tr91;
		case 53u: goto tr92;
		case 60u: goto tr94;
		case 61u: goto tr95;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 100u: goto tr97;
		case 108u: goto tr97;
		case 116u: goto tr97;
		case 124u: goto tr99;
		case 148u: goto tr74;
		case 156u: goto tr74;
		case 164u: goto tr100;
		case 172u: goto tr100;
		case 180u: goto tr100;
		case 188u: goto tr101;
		case 232u: goto tr102;
		case 240u: goto tr102;
		case 248u: goto tr103;
	}
	if ( (*( current_position)) < 96u ) {
		if ( (*( current_position)) < 32u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
				goto tr69;
		} else if ( (*( current_position)) > 55u ) {
			if ( (*( current_position)) > 63u ) {
				if ( 80u <= (*( current_position)) && (*( current_position)) <= 95u )
					goto tr72;
			} else if ( (*( current_position)) >= 56u )
				goto tr93;
		} else
			goto tr50;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr98;
		} else if ( (*( current_position)) > 159u ) {
			if ( (*( current_position)) > 183u ) {
				if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr95;
			} else if ( (*( current_position)) >= 160u )
				goto tr92;
		} else
			goto tr71;
	} else
		goto tr96;
	goto tr19;
st50:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof50;
case 50:
	switch( (*( current_position)) ) {
		case 4u: goto tr105;
		case 5u: goto tr106;
		case 12u: goto tr105;
		case 13u: goto tr106;
		case 20u: goto tr105;
		case 21u: goto tr106;
		case 28u: goto tr105;
		case 29u: goto tr106;
		case 36u: goto tr105;
		case 37u: goto tr106;
		case 44u: goto tr105;
		case 45u: goto tr106;
		case 52u: goto tr105;
		case 53u: goto tr106;
		case 60u: goto tr105;
		case 61u: goto tr106;
		case 68u: goto tr108;
		case 76u: goto tr108;
		case 84u: goto tr108;
		case 92u: goto tr108;
		case 100u: goto tr108;
		case 108u: goto tr108;
		case 116u: goto tr108;
		case 124u: goto tr108;
		case 132u: goto tr109;
		case 140u: goto tr109;
		case 148u: goto tr109;
		case 156u: goto tr109;
		case 164u: goto tr109;
		case 172u: goto tr109;
		case 180u: goto tr109;
		case 188u: goto tr109;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr106;
	} else if ( (*( current_position)) >= 64u )
		goto tr107;
	goto tr104;
tr43:
	{
    SET_CPU_FEATURE(CPUFeature_EMMXSSE);
  }
	goto st51;
tr153:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE2);
  }
	goto st51;
tr313:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st51;
st51:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof51;
case 51:
	if ( 192u <= (*( current_position)) )
		goto st10;
	goto tr19;
st52:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof52;
case 52:
	switch( (*( current_position)) ) {
		case 12u: goto tr111;
		case 13u: goto tr112;
		case 76u: goto tr114;
		case 140u: goto tr115;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
			goto tr110;
	} else if ( (*( current_position)) > 79u ) {
		if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr112;
	} else
		goto tr113;
	goto tr19;
tr426:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_NOT_TAKEN(TRUE);
  }
	goto st53;
tr427:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_BRANCH_TAKEN(TRUE);
  }
	goto st53;
st53:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof53;
case 53:
	if ( (*( current_position)) == 15u )
		goto st54;
	if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
		goto st55;
	goto tr19;
st54:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof54;
case 54:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
		goto st45;
	goto tr19;
tr433:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st55;
st55:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof55;
case 55:
	goto tr118;
tr429:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st56;
st56:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof56;
case 56:
	switch( (*( current_position)) ) {
		case 139u: goto st57;
		case 161u: goto st58;
	}
	goto tr19;
st57:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof57;
case 57:
	switch( (*( current_position)) ) {
		case 5u: goto st58;
		case 13u: goto st58;
		case 21u: goto st58;
		case 29u: goto st58;
		case 37u: goto st58;
		case 45u: goto st58;
		case 53u: goto st58;
		case 61u: goto st58;
	}
	goto tr19;
st58:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof58;
case 58:
	switch( (*( current_position)) ) {
		case 0u: goto st59;
		case 4u: goto st59;
	}
	goto tr19;
st59:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof59;
case 59:
	if ( (*( current_position)) == 0u )
		goto st60;
	goto tr19;
st60:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof60;
case 60:
	if ( (*( current_position)) == 0u )
		goto st61;
	goto tr19;
st61:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof61;
case 61:
	if ( (*( current_position)) == 0u )
		goto tr0;
	goto tr19;
tr430:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st62;
st62:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof62;
case 62:
	switch( (*( current_position)) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st63;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st63;
		case 15u: goto st65;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st63;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st63;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st63;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st63;
		case 46u: goto st75;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st63;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st63;
		case 102u: goto st83;
		case 104u: goto st63;
		case 105u: goto st88;
		case 107u: goto st34;
		case 129u: goto st88;
		case 131u: goto st34;
		case 133u: goto st1;
		case 135u: goto st1;
		case 137u: goto st1;
		case 139u: goto st1;
		case 141u: goto st30;
		case 143u: goto st32;
		case 161u: goto st3;
		case 163u: goto st3;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 169u: goto st63;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st97;
		case 199u: goto st98;
		case 209u: goto st99;
		case 211u: goto st99;
		case 240u: goto tr133;
		case 242u: goto tr134;
		case 243u: goto tr135;
		case 247u: goto st111;
		case 255u: goto st112;
	}
	if ( (*( current_position)) < 144u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr0;
	} else if ( (*( current_position)) > 153u ) {
		if ( (*( current_position)) > 157u ) {
			if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto st63;
		} else if ( (*( current_position)) >= 156u )
			goto tr0;
	} else
		goto tr0;
	goto tr19;
tr199:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st63;
tr200:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st63;
st63:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof63;
case 63:
	goto st64;
st64:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof64;
case 64:
	goto tr139;
st65:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof65;
case 65:
	switch( (*( current_position)) ) {
		case 0u: goto st66;
		case 1u: goto st67;
		case 31u: goto st32;
		case 43u: goto tr143;
		case 56u: goto st68;
		case 58u: goto st69;
		case 80u: goto tr146;
		case 81u: goto tr142;
		case 112u: goto tr147;
		case 115u: goto st73;
		case 121u: goto tr150;
		case 164u: goto st34;
		case 165u: goto st1;
		case 172u: goto st34;
		case 173u: goto st1;
		case 175u: goto st1;
		case 177u: goto st1;
		case 193u: goto st1;
		case 194u: goto tr147;
		case 196u: goto st74;
		case 197u: goto tr153;
		case 198u: goto tr147;
		case 215u: goto tr146;
		case 231u: goto tr143;
		case 247u: goto tr146;
	}
	if ( (*( current_position)) < 84u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) < 16u ) {
				if ( 2u <= (*( current_position)) && (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 17u ) {
				if ( 18u <= (*( current_position)) && (*( current_position)) <= 19u )
					goto tr143;
			} else
				goto tr142;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) < 40u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr143;
			} else if ( (*( current_position)) > 47u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr30;
			} else
				goto tr142;
		} else
			goto tr142;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 126u ) {
			if ( (*( current_position)) < 116u ) {
				if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
					goto st72;
			} else if ( (*( current_position)) > 118u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr151;
			} else
				goto tr142;
		} else if ( (*( current_position)) > 127u ) {
			if ( (*( current_position)) < 188u ) {
				if ( 182u <= (*( current_position)) && (*( current_position)) <= 183u )
					goto st1;
			} else if ( (*( current_position)) > 191u ) {
				if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto tr142;
				} else if ( (*( current_position)) >= 208u )
					goto tr142;
			} else
				goto st1;
		} else
			goto tr142;
	} else
		goto tr142;
	goto tr19;
st66:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof66;
case 66:
	if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr0;
	goto tr19;
st67:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof67;
case 67:
	if ( 224u <= (*( current_position)) && (*( current_position)) <= 231u )
		goto tr0;
	goto tr19;
st68:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof68;
case 68:
	switch( (*( current_position)) ) {
		case 16u: goto tr155;
		case 23u: goto tr155;
		case 42u: goto tr156;
		case 55u: goto tr157;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) <= 11u )
				goto tr154;
		} else if ( (*( current_position)) > 21u ) {
			if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
				goto tr154;
		} else
			goto tr155;
	} else if ( (*( current_position)) > 37u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 43u )
				goto tr155;
		} else if ( (*( current_position)) > 53u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr158;
			} else if ( (*( current_position)) >= 56u )
				goto tr155;
		} else
			goto tr155;
	} else
		goto tr155;
	goto tr19;
st69:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof69;
case 69:
	switch( (*( current_position)) ) {
		case 15u: goto tr160;
		case 22u: goto st71;
		case 23u: goto tr159;
		case 32u: goto st70;
		case 68u: goto tr163;
		case 223u: goto tr165;
	}
	if ( (*( current_position)) < 33u ) {
		if ( (*( current_position)) > 14u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto st70;
		} else if ( (*( current_position)) >= 8u )
			goto tr159;
	} else if ( (*( current_position)) > 34u ) {
		if ( (*( current_position)) > 66u ) {
			if ( 96u <= (*( current_position)) && (*( current_position)) <= 99u )
				goto tr164;
		} else if ( (*( current_position)) >= 64u )
			goto tr159;
	} else
		goto tr159;
	goto tr19;
st70:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof70;
case 70:
	switch( (*( current_position)) ) {
		case 4u: goto tr167;
		case 5u: goto tr168;
		case 12u: goto tr167;
		case 13u: goto tr168;
		case 20u: goto tr167;
		case 21u: goto tr168;
		case 28u: goto tr167;
		case 29u: goto tr168;
		case 36u: goto tr167;
		case 37u: goto tr168;
		case 44u: goto tr167;
		case 45u: goto tr168;
		case 52u: goto tr167;
		case 53u: goto tr168;
		case 60u: goto tr167;
		case 61u: goto tr168;
		case 68u: goto tr170;
		case 76u: goto tr170;
		case 84u: goto tr170;
		case 92u: goto tr170;
		case 100u: goto tr170;
		case 108u: goto tr170;
		case 116u: goto tr170;
		case 124u: goto tr170;
		case 132u: goto tr171;
		case 140u: goto tr171;
		case 148u: goto tr171;
		case 156u: goto tr171;
		case 164u: goto tr171;
		case 172u: goto tr171;
		case 180u: goto tr171;
		case 188u: goto tr171;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr168;
	} else if ( (*( current_position)) >= 64u )
		goto tr169;
	goto tr166;
st71:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof71;
case 71:
	if ( 192u <= (*( current_position)) )
		goto tr166;
	goto tr19;
st72:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof72;
case 72:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr172;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr172;
	} else
		goto tr172;
	goto tr19;
st73:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof73;
case 73:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr172;
	} else if ( (*( current_position)) >= 208u )
		goto tr172;
	goto tr19;
st74:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof74;
case 74:
	switch( (*( current_position)) ) {
		case 4u: goto tr174;
		case 5u: goto tr175;
		case 12u: goto tr174;
		case 13u: goto tr175;
		case 20u: goto tr174;
		case 21u: goto tr175;
		case 28u: goto tr174;
		case 29u: goto tr175;
		case 36u: goto tr174;
		case 37u: goto tr175;
		case 44u: goto tr174;
		case 45u: goto tr175;
		case 52u: goto tr174;
		case 53u: goto tr175;
		case 60u: goto tr174;
		case 61u: goto tr175;
		case 68u: goto tr177;
		case 76u: goto tr177;
		case 84u: goto tr177;
		case 92u: goto tr177;
		case 100u: goto tr177;
		case 108u: goto tr177;
		case 116u: goto tr177;
		case 124u: goto tr177;
		case 132u: goto tr178;
		case 140u: goto tr178;
		case 148u: goto tr178;
		case 156u: goto tr178;
		case 164u: goto tr178;
		case 172u: goto tr178;
		case 180u: goto tr178;
		case 188u: goto tr178;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr175;
	} else if ( (*( current_position)) >= 64u )
		goto tr176;
	goto tr173;
st75:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof75;
case 75:
	if ( (*( current_position)) == 15u )
		goto st76;
	goto tr19;
st76:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof76;
case 76:
	if ( (*( current_position)) == 31u )
		goto st77;
	goto tr19;
st77:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof77;
case 77:
	if ( (*( current_position)) == 132u )
		goto st78;
	goto tr19;
st78:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof78;
case 78:
	if ( (*( current_position)) == 0u )
		goto st79;
	goto tr19;
st79:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof79;
case 79:
	if ( (*( current_position)) == 0u )
		goto st80;
	goto tr19;
st80:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof80;
case 80:
	if ( (*( current_position)) == 0u )
		goto st81;
	goto tr19;
st81:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof81;
case 81:
	if ( (*( current_position)) == 0u )
		goto st82;
	goto tr19;
st82:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof82;
case 82:
	if ( (*( current_position)) == 0u )
		goto tr186;
	goto tr19;
st83:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof83;
case 83:
	switch( (*( current_position)) ) {
		case 46u: goto st75;
		case 102u: goto st84;
	}
	goto tr19;
st84:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof84;
case 84:
	switch( (*( current_position)) ) {
		case 46u: goto st75;
		case 102u: goto st85;
	}
	goto tr19;
st85:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof85;
case 85:
	switch( (*( current_position)) ) {
		case 46u: goto st75;
		case 102u: goto st86;
	}
	goto tr19;
st86:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof86;
case 86:
	switch( (*( current_position)) ) {
		case 46u: goto st75;
		case 102u: goto st87;
	}
	goto tr19;
st87:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof87;
case 87:
	if ( (*( current_position)) == 46u )
		goto st75;
	goto tr19;
st88:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof88;
case 88:
	switch( (*( current_position)) ) {
		case 4u: goto st89;
		case 5u: goto st90;
		case 12u: goto st89;
		case 13u: goto st90;
		case 20u: goto st89;
		case 21u: goto st90;
		case 28u: goto st89;
		case 29u: goto st90;
		case 36u: goto st89;
		case 37u: goto st90;
		case 44u: goto st89;
		case 45u: goto st90;
		case 52u: goto st89;
		case 53u: goto st90;
		case 60u: goto st89;
		case 61u: goto st90;
		case 68u: goto st95;
		case 76u: goto st95;
		case 84u: goto st95;
		case 92u: goto st95;
		case 100u: goto st95;
		case 108u: goto st95;
		case 116u: goto st95;
		case 124u: goto st95;
		case 132u: goto st96;
		case 140u: goto st96;
		case 148u: goto st96;
		case 156u: goto st96;
		case 164u: goto st96;
		case 172u: goto st96;
		case 180u: goto st96;
		case 188u: goto st96;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st90;
	} else if ( (*( current_position)) >= 64u )
		goto st94;
	goto st63;
st89:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof89;
case 89:
	switch( (*( current_position)) ) {
		case 5u: goto st90;
		case 13u: goto st90;
		case 21u: goto st90;
		case 29u: goto st90;
		case 37u: goto st90;
		case 45u: goto st90;
		case 53u: goto st90;
		case 61u: goto st90;
		case 69u: goto st90;
		case 77u: goto st90;
		case 85u: goto st90;
		case 93u: goto st90;
		case 101u: goto st90;
		case 109u: goto st90;
		case 117u: goto st90;
		case 125u: goto st90;
		case 133u: goto st90;
		case 141u: goto st90;
		case 149u: goto st90;
		case 157u: goto st90;
		case 165u: goto st90;
		case 173u: goto st90;
		case 181u: goto st90;
		case 189u: goto st90;
		case 197u: goto st90;
		case 205u: goto st90;
		case 213u: goto st90;
		case 221u: goto st90;
		case 229u: goto st90;
		case 237u: goto st90;
		case 245u: goto st90;
		case 253u: goto st90;
	}
	goto st63;
st90:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof90;
case 90:
	goto st91;
st91:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof91;
case 91:
	goto st92;
st92:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof92;
case 92:
	goto st93;
st93:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof93;
case 93:
	goto tr199;
st94:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof94;
case 94:
	goto tr200;
st95:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof95;
case 95:
	goto st94;
st96:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof96;
case 96:
	goto st90;
tr439:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st97;
st97:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof97;
case 97:
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
				goto tr19;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr19;
		} else
			goto st40;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st36;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr19;
			} else if ( (*( current_position)) >= 184u )
				goto st36;
		} else
			goto tr19;
	} else
		goto st40;
	goto st10;
st98:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof98;
case 98:
	switch( (*( current_position)) ) {
		case 4u: goto st89;
		case 5u: goto st90;
		case 68u: goto st95;
		case 132u: goto st96;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st63;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st63;
		} else if ( (*( current_position)) >= 128u )
			goto st90;
	} else
		goto st94;
	goto tr19;
tr445:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st99;
st99:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof99;
case 99:
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
				goto tr19;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr19;
		} else
			goto st7;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr19;
			} else if ( (*( current_position)) >= 184u )
				goto st3;
		} else
			goto tr19;
	} else
		goto st7;
	goto tr0;
tr133:
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st100;
tr388:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st100;
st100:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof100;
case 100:
	switch( (*( current_position)) ) {
		case 1u: goto st30;
		case 9u: goto st30;
		case 15u: goto st101;
		case 17u: goto st30;
		case 25u: goto st30;
		case 33u: goto st30;
		case 41u: goto st30;
		case 49u: goto st30;
		case 129u: goto st102;
		case 131u: goto st103;
		case 135u: goto st30;
		case 247u: goto st104;
		case 255u: goto st105;
	}
	goto tr19;
st101:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof101;
case 101:
	switch( (*( current_position)) ) {
		case 177u: goto st30;
		case 193u: goto st30;
	}
	goto tr19;
st102:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof102;
case 102:
	switch( (*( current_position)) ) {
		case 4u: goto st89;
		case 5u: goto st90;
		case 12u: goto st89;
		case 13u: goto st90;
		case 20u: goto st89;
		case 21u: goto st90;
		case 28u: goto st89;
		case 29u: goto st90;
		case 36u: goto st89;
		case 37u: goto st90;
		case 44u: goto st89;
		case 45u: goto st90;
		case 52u: goto st89;
		case 53u: goto st90;
		case 68u: goto st95;
		case 76u: goto st95;
		case 84u: goto st95;
		case 92u: goto st95;
		case 100u: goto st95;
		case 108u: goto st95;
		case 116u: goto st95;
		case 132u: goto st96;
		case 140u: goto st96;
		case 148u: goto st96;
		case 156u: goto st96;
		case 164u: goto st96;
		case 172u: goto st96;
		case 180u: goto st96;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st63;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st90;
	} else
		goto st94;
	goto tr19;
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
	goto tr19;
st104:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof104;
case 104:
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
	goto tr19;
st105:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof105;
case 105:
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
	goto tr19;
tr134:
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st106;
tr391:
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
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr19;
st107:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof107;
case 107:
	if ( (*( current_position)) == 56u )
		goto st108;
	goto tr19;
st108:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof108;
case 108:
	if ( (*( current_position)) == 241u )
		goto tr208;
	goto tr19;
tr135:
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st109;
tr405:
	{
    SET_DATA16_PREFIX(TRUE);
  }
	goto st109;
st109:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof109;
case 109:
	switch( (*( current_position)) ) {
		case 15u: goto st110;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 173u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr19;
st110:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof110;
case 110:
	switch( (*( current_position)) ) {
		case 184u: goto tr210;
		case 188u: goto tr211;
		case 189u: goto tr212;
	}
	goto tr19;
st111:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof111;
case 111:
	switch( (*( current_position)) ) {
		case 4u: goto st89;
		case 5u: goto st90;
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
		case 68u: goto st95;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st96;
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
				goto st63;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr19;
			} else if ( (*( current_position)) >= 64u )
				goto st94;
		} else
			goto tr19;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr19;
			} else if ( (*( current_position)) >= 128u )
				goto st90;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr19;
			} else if ( (*( current_position)) >= 192u )
				goto st63;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr461:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st112;
st112:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof112;
case 112:
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
	goto tr19;
tr277:
	{
    SET_CPU_FEATURE(CPUFeature_BMI1);
  }
	goto st113;
tr431:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st113;
st113:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof113;
case 113:
	switch( (*( current_position)) ) {
		case 4u: goto st114;
		case 5u: goto st115;
		case 12u: goto st114;
		case 13u: goto st115;
		case 20u: goto st114;
		case 21u: goto st115;
		case 28u: goto st114;
		case 29u: goto st115;
		case 36u: goto st114;
		case 37u: goto st115;
		case 44u: goto st114;
		case 45u: goto st115;
		case 52u: goto st114;
		case 53u: goto st115;
		case 60u: goto st114;
		case 61u: goto st115;
		case 68u: goto st120;
		case 76u: goto st120;
		case 84u: goto st120;
		case 92u: goto st120;
		case 100u: goto st120;
		case 108u: goto st120;
		case 116u: goto st120;
		case 124u: goto st120;
		case 132u: goto st121;
		case 140u: goto st121;
		case 148u: goto st121;
		case 156u: goto st121;
		case 164u: goto st121;
		case 172u: goto st121;
		case 180u: goto st121;
		case 188u: goto st121;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st115;
	} else if ( (*( current_position)) >= 64u )
		goto st119;
	goto st11;
tr272:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st114;
st114:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof114;
case 114:
	switch( (*( current_position)) ) {
		case 5u: goto st115;
		case 13u: goto st115;
		case 21u: goto st115;
		case 29u: goto st115;
		case 37u: goto st115;
		case 45u: goto st115;
		case 53u: goto st115;
		case 61u: goto st115;
		case 69u: goto st115;
		case 77u: goto st115;
		case 85u: goto st115;
		case 93u: goto st115;
		case 101u: goto st115;
		case 109u: goto st115;
		case 117u: goto st115;
		case 125u: goto st115;
		case 133u: goto st115;
		case 141u: goto st115;
		case 149u: goto st115;
		case 157u: goto st115;
		case 165u: goto st115;
		case 173u: goto st115;
		case 181u: goto st115;
		case 189u: goto st115;
		case 197u: goto st115;
		case 205u: goto st115;
		case 213u: goto st115;
		case 221u: goto st115;
		case 229u: goto st115;
		case 237u: goto st115;
		case 245u: goto st115;
		case 253u: goto st115;
	}
	goto st11;
tr273:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st115;
st115:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof115;
case 115:
	goto st116;
st116:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof116;
case 116:
	goto st117;
st117:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof117;
case 117:
	goto st118;
st118:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof118;
case 118:
	goto tr222;
tr274:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st119;
st119:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof119;
case 119:
	goto tr223;
tr275:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st120;
st120:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof120;
case 120:
	goto st119;
tr276:
	{
    SET_CPU_FEATURE(CPUFeature_LWP);
  }
	goto st121;
st121:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof121;
case 121:
	goto st115;
tr434:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st122;
st122:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof122;
case 122:
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
		case 224u: goto st123;
		case 225u: goto st233;
		case 226u: goto st235;
		case 227u: goto st237;
		case 228u: goto st239;
		case 229u: goto st241;
		case 230u: goto st243;
		case 231u: goto st245;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else if ( (*( current_position)) >= 64u )
		goto st40;
	goto st10;
st123:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof123;
case 123:
	if ( (*( current_position)) == 224u )
		goto tr232;
	goto tr11;
tr232:
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
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr462;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr436:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st124;
st124:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof124;
case 124:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 232u: goto st125;
		case 233u: goto st140;
		case 234u: goto st148;
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
	goto tr19;
st125:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof125;
case 125:
	switch( (*( current_position)) ) {
		case 64u: goto tr236;
		case 68u: goto tr237;
		case 72u: goto tr236;
		case 76u: goto tr237;
		case 80u: goto tr236;
		case 84u: goto tr237;
		case 88u: goto tr236;
		case 92u: goto tr237;
		case 96u: goto tr236;
		case 100u: goto tr237;
		case 104u: goto tr236;
		case 108u: goto tr237;
		case 112u: goto tr236;
		case 116u: goto tr237;
		case 120u: goto tr238;
		case 124u: goto tr237;
		case 192u: goto tr239;
		case 196u: goto tr237;
		case 200u: goto tr239;
		case 204u: goto tr237;
		case 208u: goto tr239;
		case 212u: goto tr237;
		case 216u: goto tr239;
		case 220u: goto tr237;
		case 224u: goto tr239;
		case 228u: goto tr237;
		case 232u: goto tr239;
		case 236u: goto tr237;
		case 240u: goto tr239;
		case 244u: goto tr237;
		case 248u: goto tr239;
		case 252u: goto tr237;
	}
	goto tr19;
tr236:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st126;
st126:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof126;
case 126:
	switch( (*( current_position)) ) {
		case 166u: goto tr240;
		case 182u: goto tr240;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr240;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr240;
		} else
			goto tr240;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 204u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr240;
		} else if ( (*( current_position)) > 207u ) {
			if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
				goto tr241;
		} else
			goto tr241;
	} else
		goto tr240;
	goto tr19;
tr240:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st127;
tr345:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st127;
tr346:
	{
    SET_CPU_FEATURE(CPUFeature_FMA4);
  }
	goto st127;
st127:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof127;
case 127:
	switch( (*( current_position)) ) {
		case 4u: goto st129;
		case 5u: goto st130;
		case 12u: goto st129;
		case 13u: goto st130;
		case 20u: goto st129;
		case 21u: goto st130;
		case 28u: goto st129;
		case 29u: goto st130;
		case 36u: goto st129;
		case 37u: goto st130;
		case 44u: goto st129;
		case 45u: goto st130;
		case 52u: goto st129;
		case 53u: goto st130;
		case 60u: goto st129;
		case 61u: goto st130;
		case 68u: goto st135;
		case 76u: goto st135;
		case 84u: goto st135;
		case 92u: goto st135;
		case 100u: goto st135;
		case 108u: goto st135;
		case 116u: goto st135;
		case 124u: goto st135;
		case 132u: goto st136;
		case 140u: goto st136;
		case 148u: goto st136;
		case 156u: goto st136;
		case 164u: goto st136;
		case 172u: goto st136;
		case 180u: goto st136;
		case 188u: goto st136;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st130;
	} else if ( (*( current_position)) >= 64u )
		goto st134;
	goto st128;
tr251:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st128;
tr252:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st128;
st128:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof128;
case 128:
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
	goto tr19;
st129:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof129;
case 129:
	switch( (*( current_position)) ) {
		case 5u: goto st130;
		case 13u: goto st130;
		case 21u: goto st130;
		case 29u: goto st130;
		case 37u: goto st130;
		case 45u: goto st130;
		case 53u: goto st130;
		case 61u: goto st130;
		case 69u: goto st130;
		case 77u: goto st130;
		case 85u: goto st130;
		case 93u: goto st130;
		case 101u: goto st130;
		case 109u: goto st130;
		case 117u: goto st130;
		case 125u: goto st130;
		case 133u: goto st130;
		case 141u: goto st130;
		case 149u: goto st130;
		case 157u: goto st130;
		case 165u: goto st130;
		case 173u: goto st130;
		case 181u: goto st130;
		case 189u: goto st130;
		case 197u: goto st130;
		case 205u: goto st130;
		case 213u: goto st130;
		case 221u: goto st130;
		case 229u: goto st130;
		case 237u: goto st130;
		case 245u: goto st130;
		case 253u: goto st130;
	}
	goto st128;
st130:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof130;
case 130:
	goto st131;
st131:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof131;
case 131:
	goto st132;
st132:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof132;
case 132:
	goto st133;
st133:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof133;
case 133:
	goto tr251;
st134:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof134;
case 134:
	goto tr252;
st135:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof135;
case 135:
	goto st134;
st136:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof136;
case 136:
	goto st130;
tr237:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st137;
st137:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof137;
case 137:
	if ( (*( current_position)) == 162u )
		goto tr240;
	goto tr19;
tr238:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st138;
st138:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof138;
case 138:
	switch( (*( current_position)) ) {
		case 166u: goto tr240;
		case 182u: goto tr240;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr240;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr240;
		} else
			goto tr240;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 192u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr240;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr241;
			} else if ( (*( current_position)) >= 204u )
				goto tr241;
		} else
			goto tr241;
	} else
		goto tr240;
	goto tr19;
tr239:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st139;
st139:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof139;
case 139:
	if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
		goto tr240;
	goto tr19;
st140:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof140;
case 140:
	switch( (*( current_position)) ) {
		case 64u: goto tr253;
		case 72u: goto tr253;
		case 80u: goto tr253;
		case 88u: goto tr253;
		case 96u: goto tr253;
		case 104u: goto tr253;
		case 112u: goto tr253;
		case 120u: goto tr254;
		case 124u: goto tr255;
		case 192u: goto tr256;
		case 200u: goto tr256;
		case 208u: goto tr256;
		case 216u: goto tr256;
		case 224u: goto tr256;
		case 232u: goto tr256;
		case 240u: goto tr256;
		case 248u: goto tr256;
	}
	goto tr19;
tr253:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st141;
st141:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof141;
case 141:
	switch( (*( current_position)) ) {
		case 1u: goto st142;
		case 2u: goto st143;
	}
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr259;
	goto tr19;
st142:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof142;
case 142:
	switch( (*( current_position)) ) {
		case 12u: goto tr261;
		case 13u: goto tr262;
		case 20u: goto tr261;
		case 21u: goto tr262;
		case 28u: goto tr261;
		case 29u: goto tr262;
		case 36u: goto tr261;
		case 37u: goto tr262;
		case 44u: goto tr261;
		case 45u: goto tr262;
		case 52u: goto tr261;
		case 53u: goto tr262;
		case 60u: goto tr261;
		case 61u: goto tr262;
		case 76u: goto tr264;
		case 84u: goto tr264;
		case 92u: goto tr264;
		case 100u: goto tr264;
		case 108u: goto tr264;
		case 116u: goto tr264;
		case 124u: goto tr264;
		case 140u: goto tr265;
		case 148u: goto tr265;
		case 156u: goto tr265;
		case 164u: goto tr265;
		case 172u: goto tr265;
		case 180u: goto tr265;
		case 188u: goto tr265;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) > 7u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 71u )
				goto tr19;
		} else
			goto tr19;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 136u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr19;
		} else if ( (*( current_position)) > 191u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr19;
		} else
			goto tr262;
	} else
		goto tr263;
	goto tr260;
st143:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof143;
case 143:
	switch( (*( current_position)) ) {
		case 12u: goto tr261;
		case 13u: goto tr262;
		case 52u: goto tr261;
		case 53u: goto tr262;
		case 76u: goto tr264;
		case 116u: goto tr264;
		case 140u: goto tr265;
		case 180u: goto tr265;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr260;
		} else if ( (*( current_position)) > 55u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto tr263;
		} else
			goto tr260;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto tr262;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr260;
			} else if ( (*( current_position)) >= 200u )
				goto tr260;
		} else
			goto tr262;
	} else
		goto tr263;
	goto tr19;
tr254:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st144;
st144:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof144;
case 144:
	switch( (*( current_position)) ) {
		case 1u: goto st142;
		case 2u: goto st143;
		case 18u: goto st145;
		case 203u: goto tr259;
		case 219u: goto tr259;
	}
	if ( (*( current_position)) < 198u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 131u )
				goto tr259;
		} else if ( (*( current_position)) > 155u ) {
			if ( 193u <= (*( current_position)) && (*( current_position)) <= 195u )
				goto tr259;
		} else
			goto tr259;
	} else if ( (*( current_position)) > 199u ) {
		if ( (*( current_position)) < 214u ) {
			if ( 209u <= (*( current_position)) && (*( current_position)) <= 211u )
				goto tr259;
		} else if ( (*( current_position)) > 215u ) {
			if ( 225u <= (*( current_position)) && (*( current_position)) <= 227u )
				goto tr259;
		} else
			goto tr259;
	} else
		goto tr259;
	goto tr19;
st145:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof145;
case 145:
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr267;
	goto tr19;
tr255:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st146;
st146:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof146;
case 146:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 129u )
		goto tr259;
	goto tr19;
tr256:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st147;
st147:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof147;
case 147:
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr259;
	goto tr19;
st148:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof148;
case 148:
	switch( (*( current_position)) ) {
		case 64u: goto tr268;
		case 72u: goto tr268;
		case 80u: goto tr268;
		case 88u: goto tr268;
		case 96u: goto tr268;
		case 104u: goto tr268;
		case 112u: goto tr268;
		case 120u: goto tr269;
	}
	goto tr19;
tr268:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st149;
st149:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof149;
case 149:
	if ( (*( current_position)) == 18u )
		goto st150;
	goto tr19;
st150:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof150;
case 150:
	switch( (*( current_position)) ) {
		case 4u: goto tr272;
		case 5u: goto tr273;
		case 12u: goto tr272;
		case 13u: goto tr273;
		case 68u: goto tr275;
		case 76u: goto tr275;
		case 132u: goto tr276;
		case 140u: goto tr276;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr271;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr271;
		} else if ( (*( current_position)) >= 128u )
			goto tr273;
	} else
		goto tr274;
	goto tr19;
tr269:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st151;
st151:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof151;
case 151:
	switch( (*( current_position)) ) {
		case 16u: goto tr277;
		case 18u: goto st150;
	}
	goto tr19;
tr440:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st152;
st152:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof152;
case 152:
	switch( (*( current_position)) ) {
		case 225u: goto st153;
		case 226u: goto st175;
		case 227u: goto st184;
	}
	goto tr19;
st153:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof153;
case 153:
	switch( (*( current_position)) ) {
		case 65u: goto tr282;
		case 66u: goto tr283;
		case 67u: goto tr284;
		case 68u: goto tr285;
		case 69u: goto tr286;
		case 70u: goto tr287;
		case 71u: goto tr288;
		case 73u: goto tr282;
		case 74u: goto tr283;
		case 75u: goto tr284;
		case 76u: goto tr285;
		case 77u: goto tr286;
		case 78u: goto tr287;
		case 79u: goto tr288;
		case 81u: goto tr282;
		case 82u: goto tr283;
		case 83u: goto tr284;
		case 84u: goto tr285;
		case 85u: goto tr286;
		case 86u: goto tr287;
		case 87u: goto tr288;
		case 89u: goto tr282;
		case 90u: goto tr283;
		case 91u: goto tr284;
		case 92u: goto tr285;
		case 93u: goto tr286;
		case 94u: goto tr287;
		case 95u: goto tr288;
		case 97u: goto tr282;
		case 98u: goto tr283;
		case 99u: goto tr284;
		case 100u: goto tr285;
		case 101u: goto tr286;
		case 102u: goto tr287;
		case 103u: goto tr288;
		case 105u: goto tr282;
		case 106u: goto tr283;
		case 107u: goto tr284;
		case 108u: goto tr285;
		case 109u: goto tr286;
		case 110u: goto tr287;
		case 111u: goto tr288;
		case 113u: goto tr282;
		case 114u: goto tr283;
		case 115u: goto tr284;
		case 116u: goto tr285;
		case 117u: goto tr286;
		case 118u: goto tr287;
		case 119u: goto tr288;
		case 120u: goto tr289;
		case 121u: goto tr290;
		case 122u: goto tr291;
		case 123u: goto tr292;
		case 124u: goto tr293;
		case 125u: goto tr294;
		case 126u: goto tr295;
		case 127u: goto tr296;
	}
	if ( 64u <= (*( current_position)) && (*( current_position)) <= 112u )
		goto tr281;
	goto tr19;
tr281:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st154;
tr362:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st154;
st154:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof154;
case 154:
	switch( (*( current_position)) ) {
		case 18u: goto st155;
		case 22u: goto st155;
		case 23u: goto tr299;
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 198u: goto tr300;
	}
	if ( (*( current_position)) < 46u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr298;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) > 89u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr298;
		} else if ( (*( current_position)) >= 84u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
st155:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof155;
case 155:
	switch( (*( current_position)) ) {
		case 4u: goto tr302;
		case 5u: goto tr303;
		case 12u: goto tr302;
		case 13u: goto tr303;
		case 20u: goto tr302;
		case 21u: goto tr303;
		case 28u: goto tr302;
		case 29u: goto tr303;
		case 36u: goto tr302;
		case 37u: goto tr303;
		case 44u: goto tr302;
		case 45u: goto tr303;
		case 52u: goto tr302;
		case 53u: goto tr303;
		case 60u: goto tr302;
		case 61u: goto tr303;
		case 68u: goto tr305;
		case 76u: goto tr305;
		case 84u: goto tr305;
		case 92u: goto tr305;
		case 100u: goto tr305;
		case 108u: goto tr305;
		case 116u: goto tr305;
		case 124u: goto tr305;
		case 132u: goto tr306;
		case 140u: goto tr306;
		case 148u: goto tr306;
		case 156u: goto tr306;
		case 164u: goto tr306;
		case 172u: goto tr306;
		case 180u: goto tr306;
		case 188u: goto tr306;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr303;
	} else if ( (*( current_position)) >= 64u )
		goto tr304;
	goto tr301;
tr282:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st156;
tr363:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st156;
st156:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof156;
case 156:
	switch( (*( current_position)) ) {
		case 18u: goto tr299;
		case 81u: goto tr298;
		case 115u: goto st158;
		case 194u: goto tr300;
		case 198u: goto tr300;
	}
	if ( (*( current_position)) < 116u ) {
		if ( (*( current_position)) < 46u ) {
			if ( (*( current_position)) > 21u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr299;
			} else if ( (*( current_position)) >= 20u )
				goto tr298;
		} else if ( (*( current_position)) > 47u ) {
			if ( (*( current_position)) < 92u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 89u )
					goto tr298;
			} else if ( (*( current_position)) > 109u ) {
				if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
					goto st157;
			} else
				goto tr298;
		} else
			goto tr298;
	} else if ( (*( current_position)) > 118u ) {
		if ( (*( current_position)) < 216u ) {
			if ( (*( current_position)) > 125u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr298;
			} else if ( (*( current_position)) >= 124u )
				goto tr298;
		} else if ( (*( current_position)) > 229u ) {
			if ( (*( current_position)) < 241u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr298;
			} else if ( (*( current_position)) > 246u ) {
				if ( 248u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr298;
			} else
				goto tr298;
		} else
			goto tr298;
	} else
		goto tr298;
	goto tr19;
st157:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof157;
case 157:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr309;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr309;
	} else
		goto tr309;
	goto tr19;
st158:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof158;
case 158:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr309;
	} else if ( (*( current_position)) >= 208u )
		goto tr309;
	goto tr19;
tr283:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st159;
st159:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof159;
case 159:
	switch( (*( current_position)) ) {
		case 42u: goto tr298;
		case 81u: goto tr298;
		case 83u: goto tr298;
		case 194u: goto tr300;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr298;
	} else if ( (*( current_position)) >= 88u )
		goto tr298;
	goto tr19;
tr284:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st160;
st160:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof160;
case 160:
	switch( (*( current_position)) ) {
		case 42u: goto tr298;
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 208u: goto tr298;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr298;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr285:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st161;
tr366:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st161;
st161:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof161;
case 161:
	switch( (*( current_position)) ) {
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 198u: goto tr300;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr298;
	} else if ( (*( current_position)) > 89u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr286:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st162;
tr367:
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
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 198u: goto tr300;
		case 208u: goto tr298;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr298;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr298;
		} else if ( (*( current_position)) >= 92u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr287:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st163;
tr368:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st163;
st163:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof163;
case 163:
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr310;
	goto tr19;
tr288:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st164;
tr369:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st164;
st164:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof164;
case 164:
	if ( (*( current_position)) == 208u )
		goto tr298;
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr298;
	} else if ( (*( current_position)) >= 16u )
		goto tr310;
	goto tr19;
tr289:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st165;
tr370:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st165;
st165:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof165;
case 165:
	switch( (*( current_position)) ) {
		case 18u: goto st155;
		case 19u: goto tr299;
		case 22u: goto st155;
		case 23u: goto tr299;
		case 43u: goto tr299;
		case 80u: goto tr310;
		case 119u: goto tr301;
		case 174u: goto st166;
		case 194u: goto tr300;
		case 198u: goto tr300;
	}
	if ( (*( current_position)) < 40u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr298;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 47u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr298;
		} else if ( (*( current_position)) >= 46u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
st166:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof166;
case 166:
	switch( (*( current_position)) ) {
		case 20u: goto tr302;
		case 21u: goto tr303;
		case 28u: goto tr302;
		case 29u: goto tr303;
		case 84u: goto tr305;
		case 92u: goto tr305;
		case 148u: goto tr306;
		case 156u: goto tr306;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr301;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr303;
	} else
		goto tr304;
	goto tr19;
tr290:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st167;
tr371:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st167;
st167:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof167;
case 167:
	switch( (*( current_position)) ) {
		case 43u: goto tr299;
		case 80u: goto tr310;
		case 81u: goto tr298;
		case 112u: goto tr300;
		case 115u: goto st158;
		case 127u: goto tr298;
		case 194u: goto tr300;
		case 196u: goto st168;
		case 197u: goto tr313;
		case 198u: goto tr300;
		case 215u: goto tr310;
		case 231u: goto tr299;
		case 247u: goto tr310;
	}
	if ( (*( current_position)) < 84u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 18u <= (*( current_position)) && (*( current_position)) <= 19u )
					goto tr299;
			} else if ( (*( current_position)) >= 16u )
				goto tr298;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) < 40u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr299;
			} else if ( (*( current_position)) > 41u ) {
				if ( 46u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr298;
			} else
				goto tr298;
		} else
			goto tr298;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 124u ) {
			if ( (*( current_position)) > 114u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr298;
			} else if ( (*( current_position)) >= 113u )
				goto st157;
		} else if ( (*( current_position)) > 125u ) {
			if ( (*( current_position)) < 216u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr298;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr298;
			} else
				goto tr298;
		} else
			goto tr298;
	} else
		goto tr298;
	goto tr19;
st168:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof168;
case 168:
	switch( (*( current_position)) ) {
		case 4u: goto tr314;
		case 5u: goto tr315;
		case 12u: goto tr314;
		case 13u: goto tr315;
		case 20u: goto tr314;
		case 21u: goto tr315;
		case 28u: goto tr314;
		case 29u: goto tr315;
		case 36u: goto tr314;
		case 37u: goto tr315;
		case 44u: goto tr314;
		case 45u: goto tr315;
		case 52u: goto tr314;
		case 53u: goto tr315;
		case 60u: goto tr314;
		case 61u: goto tr315;
		case 68u: goto tr317;
		case 76u: goto tr317;
		case 84u: goto tr317;
		case 92u: goto tr317;
		case 100u: goto tr317;
		case 108u: goto tr317;
		case 116u: goto tr317;
		case 124u: goto tr317;
		case 132u: goto tr318;
		case 140u: goto tr318;
		case 148u: goto tr318;
		case 156u: goto tr318;
		case 164u: goto tr318;
		case 172u: goto tr318;
		case 180u: goto tr318;
		case 188u: goto tr318;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr315;
	} else if ( (*( current_position)) >= 64u )
		goto tr316;
	goto tr309;
tr291:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st169;
st169:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof169;
case 169:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 22u: goto tr298;
		case 42u: goto tr298;
		case 111u: goto tr298;
		case 112u: goto tr300;
		case 194u: goto tr300;
		case 230u: goto tr298;
	}
	if ( (*( current_position)) < 81u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr298;
		} else if ( (*( current_position)) >= 16u )
			goto tr299;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr298;
		} else if ( (*( current_position)) >= 88u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr292:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st170;
st170:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof170;
case 170:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 42u: goto tr298;
		case 81u: goto tr298;
		case 112u: goto tr300;
		case 194u: goto tr300;
		case 208u: goto tr298;
		case 230u: goto tr298;
		case 240u: goto tr299;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr298;
		} else if ( (*( current_position)) >= 16u )
			goto tr299;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr298;
		} else if ( (*( current_position)) >= 92u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr293:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st171;
tr374:
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
		case 43u: goto tr299;
		case 80u: goto tr310;
		case 119u: goto tr301;
		case 194u: goto tr300;
		case 198u: goto tr300;
	}
	if ( (*( current_position)) < 20u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr298;
	} else if ( (*( current_position)) > 21u ) {
		if ( (*( current_position)) > 41u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr298;
		} else if ( (*( current_position)) >= 40u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr294:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st172;
tr375:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st172;
st172:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof172;
case 172:
	switch( (*( current_position)) ) {
		case 43u: goto tr299;
		case 80u: goto tr310;
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 198u: goto tr300;
		case 208u: goto tr298;
		case 214u: goto tr298;
		case 230u: goto tr298;
		case 231u: goto tr299;
	}
	if ( (*( current_position)) < 40u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto tr298;
		} else if ( (*( current_position)) >= 16u )
			goto tr298;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr298;
		} else if ( (*( current_position)) >= 84u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr295:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st173;
tr376:
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
		case 18u: goto tr298;
		case 22u: goto tr298;
		case 91u: goto tr298;
		case 127u: goto tr298;
		case 230u: goto tr298;
	}
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr310;
	goto tr19;
tr296:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st174;
tr377:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st174;
st174:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof174;
case 174:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 208u: goto tr298;
		case 230u: goto tr298;
		case 240u: goto tr299;
	}
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr298;
	} else if ( (*( current_position)) >= 16u )
		goto tr310;
	goto tr19;
st175:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof175;
case 175:
	switch( (*( current_position)) ) {
		case 64u: goto tr319;
		case 65u: goto tr320;
		case 69u: goto tr321;
		case 72u: goto tr319;
		case 73u: goto tr320;
		case 77u: goto tr321;
		case 80u: goto tr319;
		case 81u: goto tr320;
		case 85u: goto tr321;
		case 88u: goto tr319;
		case 89u: goto tr320;
		case 93u: goto tr321;
		case 96u: goto tr319;
		case 97u: goto tr320;
		case 101u: goto tr321;
		case 104u: goto tr319;
		case 105u: goto tr320;
		case 109u: goto tr321;
		case 112u: goto tr319;
		case 113u: goto tr320;
		case 117u: goto tr321;
		case 120u: goto tr319;
		case 121u: goto tr322;
		case 125u: goto tr323;
		case 193u: goto tr324;
		case 197u: goto tr325;
		case 201u: goto tr324;
		case 205u: goto tr325;
		case 209u: goto tr324;
		case 213u: goto tr325;
		case 217u: goto tr324;
		case 221u: goto tr325;
		case 225u: goto tr324;
		case 229u: goto tr325;
		case 233u: goto tr324;
		case 237u: goto tr325;
		case 241u: goto tr324;
		case 245u: goto tr325;
		case 249u: goto tr324;
		case 253u: goto tr325;
	}
	goto tr19;
tr319:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st176;
st176:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof176;
case 176:
	switch( (*( current_position)) ) {
		case 242u: goto tr326;
		case 243u: goto st177;
		case 247u: goto tr326;
	}
	goto tr19;
st177:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof177;
case 177:
	switch( (*( current_position)) ) {
		case 12u: goto tr329;
		case 13u: goto tr330;
		case 20u: goto tr329;
		case 21u: goto tr330;
		case 28u: goto tr329;
		case 29u: goto tr330;
		case 76u: goto tr332;
		case 84u: goto tr332;
		case 92u: goto tr332;
		case 140u: goto tr333;
		case 148u: goto tr333;
		case 156u: goto tr333;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr328;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 159u ) {
			if ( 200u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr328;
		} else if ( (*( current_position)) >= 136u )
			goto tr330;
	} else
		goto tr331;
	goto tr19;
tr320:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st178;
st178:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof178;
case 178:
	if ( (*( current_position)) == 43u )
		goto tr298;
	if ( (*( current_position)) < 55u ) {
		if ( (*( current_position)) < 40u ) {
			if ( (*( current_position)) <= 13u )
				goto tr298;
		} else if ( (*( current_position)) > 41u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr299;
		} else
			goto tr298;
	} else if ( (*( current_position)) > 64u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
				goto tr334;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr335;
			} else if ( (*( current_position)) >= 182u )
				goto tr334;
		} else
			goto tr334;
	} else
		goto tr298;
	goto tr19;
tr321:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st179;
st179:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof179;
case 179:
	switch( (*( current_position)) ) {
		case 154u: goto tr334;
		case 156u: goto tr334;
		case 158u: goto tr334;
		case 170u: goto tr334;
		case 172u: goto tr334;
		case 174u: goto tr334;
		case 186u: goto tr334;
		case 188u: goto tr334;
		case 190u: goto tr334;
	}
	if ( (*( current_position)) < 150u ) {
		if ( (*( current_position)) > 13u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr299;
		} else if ( (*( current_position)) >= 12u )
			goto tr298;
	} else if ( (*( current_position)) > 152u ) {
		if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr334;
		} else if ( (*( current_position)) >= 166u )
			goto tr334;
	} else
		goto tr334;
	goto tr19;
tr322:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st180;
st180:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof180;
case 180:
	switch( (*( current_position)) ) {
		case 19u: goto tr336;
		case 23u: goto tr298;
		case 24u: goto tr299;
		case 42u: goto tr299;
	}
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
					goto tr298;
			} else
				goto tr298;
		} else if ( (*( current_position)) > 37u ) {
			if ( (*( current_position)) > 43u ) {
				if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr299;
			} else if ( (*( current_position)) >= 40u )
				goto tr298;
		} else
			goto tr298;
	} else if ( (*( current_position)) > 53u ) {
		if ( (*( current_position)) < 166u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr334;
			} else if ( (*( current_position)) >= 55u )
				goto tr298;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr335;
			} else if ( (*( current_position)) >= 182u )
				goto tr334;
		} else
			goto tr334;
	} else
		goto tr298;
	goto tr19;
tr323:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st181;
st181:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof181;
case 181:
	switch( (*( current_position)) ) {
		case 19u: goto tr336;
		case 23u: goto tr298;
		case 154u: goto tr334;
		case 156u: goto tr334;
		case 158u: goto tr334;
		case 170u: goto tr334;
		case 172u: goto tr334;
		case 174u: goto tr334;
		case 186u: goto tr334;
		case 188u: goto tr334;
		case 190u: goto tr334;
	}
	if ( (*( current_position)) < 44u ) {
		if ( (*( current_position)) > 15u ) {
			if ( 24u <= (*( current_position)) && (*( current_position)) <= 26u )
				goto tr299;
		} else if ( (*( current_position)) >= 12u )
			goto tr298;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
				goto tr334;
		} else if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr334;
		} else
			goto tr334;
	} else
		goto tr299;
	goto tr19;
tr324:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st182;
st182:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof182;
case 182:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr334;
	} else if ( (*( current_position)) > 175u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr334;
	} else
		goto tr334;
	goto tr19;
tr325:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st183;
st183:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof183;
case 183:
	switch( (*( current_position)) ) {
		case 154u: goto tr334;
		case 156u: goto tr334;
		case 158u: goto tr334;
		case 170u: goto tr334;
		case 172u: goto tr334;
		case 174u: goto tr334;
		case 186u: goto tr334;
		case 188u: goto tr334;
		case 190u: goto tr334;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto tr334;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto tr334;
	} else
		goto tr334;
	goto tr19;
st184:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof184;
case 184:
	switch( (*( current_position)) ) {
		case 65u: goto tr337;
		case 69u: goto tr338;
		case 73u: goto tr337;
		case 77u: goto tr338;
		case 81u: goto tr337;
		case 85u: goto tr338;
		case 89u: goto tr337;
		case 93u: goto tr338;
		case 97u: goto tr337;
		case 101u: goto tr338;
		case 105u: goto tr337;
		case 109u: goto tr338;
		case 113u: goto tr337;
		case 117u: goto tr338;
		case 121u: goto tr339;
		case 125u: goto tr340;
		case 193u: goto tr341;
		case 197u: goto tr342;
		case 201u: goto tr341;
		case 205u: goto tr342;
		case 209u: goto tr341;
		case 213u: goto tr342;
		case 217u: goto tr341;
		case 221u: goto tr342;
		case 225u: goto tr341;
		case 229u: goto tr342;
		case 233u: goto tr341;
		case 237u: goto tr342;
		case 241u: goto tr341;
		case 245u: goto tr342;
		case 249u: goto tr341;
		case 253u: goto tr342;
	}
	goto tr19;
tr337:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st185;
st185:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof185;
case 185:
	switch( (*( current_position)) ) {
		case 33u: goto tr300;
		case 68u: goto tr343;
		case 223u: goto tr347;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr300;
		} else if ( (*( current_position)) > 66u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
				goto tr344;
		} else
			goto tr300;
	} else if ( (*( current_position)) > 76u ) {
		if ( (*( current_position)) < 104u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr346;
		} else if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr346;
		} else
			goto tr346;
	} else
		goto tr345;
	goto tr19;
tr344:
	{
    SET_CPU_FEATURE(CPUFeature_XOP);
  }
	goto st186;
st186:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof186;
case 186:
	switch( (*( current_position)) ) {
		case 4u: goto st188;
		case 5u: goto st189;
		case 12u: goto st188;
		case 13u: goto st189;
		case 20u: goto st188;
		case 21u: goto st189;
		case 28u: goto st188;
		case 29u: goto st189;
		case 36u: goto st188;
		case 37u: goto st189;
		case 44u: goto st188;
		case 45u: goto st189;
		case 52u: goto st188;
		case 53u: goto st189;
		case 60u: goto st188;
		case 61u: goto st189;
		case 68u: goto st194;
		case 76u: goto st194;
		case 84u: goto st194;
		case 92u: goto st194;
		case 100u: goto st194;
		case 108u: goto st194;
		case 116u: goto st194;
		case 124u: goto st194;
		case 132u: goto st195;
		case 140u: goto st195;
		case 148u: goto st195;
		case 156u: goto st195;
		case 164u: goto st195;
		case 172u: goto st195;
		case 180u: goto st195;
		case 188u: goto st195;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st189;
	} else if ( (*( current_position)) >= 64u )
		goto st193;
	goto st187;
tr358:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	goto st187;
tr359:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	goto st187;
st187:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof187;
case 187:
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 16u ) {
			if ( (*( current_position)) <= 3u )
				goto tr354;
		} else if ( (*( current_position)) > 19u ) {
			if ( 32u <= (*( current_position)) && (*( current_position)) <= 35u )
				goto tr354;
		} else
			goto tr354;
	} else if ( (*( current_position)) > 51u ) {
		if ( (*( current_position)) < 80u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 67u )
				goto tr354;
		} else if ( (*( current_position)) > 83u ) {
			if ( (*( current_position)) > 99u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 115u )
					goto tr354;
			} else if ( (*( current_position)) >= 96u )
				goto tr354;
		} else
			goto tr354;
	} else
		goto tr354;
	goto tr19;
st188:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof188;
case 188:
	switch( (*( current_position)) ) {
		case 5u: goto st189;
		case 13u: goto st189;
		case 21u: goto st189;
		case 29u: goto st189;
		case 37u: goto st189;
		case 45u: goto st189;
		case 53u: goto st189;
		case 61u: goto st189;
		case 69u: goto st189;
		case 77u: goto st189;
		case 85u: goto st189;
		case 93u: goto st189;
		case 101u: goto st189;
		case 109u: goto st189;
		case 117u: goto st189;
		case 125u: goto st189;
		case 133u: goto st189;
		case 141u: goto st189;
		case 149u: goto st189;
		case 157u: goto st189;
		case 165u: goto st189;
		case 173u: goto st189;
		case 181u: goto st189;
		case 189u: goto st189;
		case 197u: goto st189;
		case 205u: goto st189;
		case 213u: goto st189;
		case 221u: goto st189;
		case 229u: goto st189;
		case 237u: goto st189;
		case 245u: goto st189;
		case 253u: goto st189;
	}
	goto st187;
st189:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof189;
case 189:
	goto st190;
st190:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof190;
case 190:
	goto st191;
st191:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof191;
case 191:
	goto st192;
st192:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof192;
case 192:
	goto tr358;
st193:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof193;
case 193:
	goto tr359;
st194:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof194;
case 194:
	goto st193;
st195:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof195;
case 195:
	goto st189;
tr338:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st196;
st196:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof196;
case 196:
	switch( (*( current_position)) ) {
		case 6u: goto tr300;
		case 64u: goto tr300;
	}
	if ( (*( current_position)) < 92u ) {
		if ( (*( current_position)) < 12u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
				goto tr300;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 73u ) {
				if ( 74u <= (*( current_position)) && (*( current_position)) <= 75u )
					goto tr345;
			} else if ( (*( current_position)) >= 72u )
				goto tr344;
		} else
			goto tr300;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 108u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto tr346;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr346;
			} else if ( (*( current_position)) >= 120u )
				goto tr346;
		} else
			goto tr346;
	} else
		goto tr346;
	goto tr19;
tr339:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st197;
st197:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof197;
case 197:
	switch( (*( current_position)) ) {
		case 22u: goto tr300;
		case 23u: goto tr360;
		case 29u: goto tr361;
		case 32u: goto st168;
		case 68u: goto tr343;
		case 223u: goto tr347;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 5u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
					goto tr300;
			} else if ( (*( current_position)) >= 4u )
				goto tr300;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) > 34u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 66u )
					goto tr300;
			} else if ( (*( current_position)) >= 33u )
				goto tr300;
		} else
			goto st168;
	} else if ( (*( current_position)) > 73u ) {
		if ( (*( current_position)) < 96u ) {
			if ( (*( current_position)) > 76u ) {
				if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
					goto tr346;
			} else if ( (*( current_position)) >= 74u )
				goto tr345;
		} else if ( (*( current_position)) > 99u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr346;
			} else if ( (*( current_position)) >= 104u )
				goto tr346;
		} else
			goto tr300;
	} else
		goto tr344;
	goto tr19;
tr360:
	{
    SET_CPU_FEATURE(CPUFeature_AVX);
  }
	goto st198;
st198:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof198;
case 198:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 12u: goto st35;
		case 20u: goto st35;
		case 28u: goto st35;
		case 36u: goto st35;
		case 44u: goto st35;
		case 52u: goto st35;
		case 60u: goto st35;
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
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto st10;
			} else
				goto st10;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto st10;
			} else if ( (*( current_position)) >= 22u )
				goto st10;
		} else
			goto st10;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto st10;
			} else if ( (*( current_position)) >= 46u )
				goto st10;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr19;
			} else if ( (*( current_position)) >= 64u )
				goto st40;
		} else
			goto st10;
	} else
		goto st10;
	goto st36;
tr340:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st199;
st199:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof199;
case 199:
	switch( (*( current_position)) ) {
		case 29u: goto tr361;
		case 64u: goto tr300;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 12u ) {
			if ( (*( current_position)) > 6u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
					goto tr300;
			} else if ( (*( current_position)) >= 4u )
				goto tr300;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 25u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
					goto tr344;
			} else if ( (*( current_position)) >= 24u )
				goto tr300;
		} else
			goto tr300;
	} else if ( (*( current_position)) > 75u ) {
		if ( (*( current_position)) < 108u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
					goto tr346;
			} else if ( (*( current_position)) >= 92u )
				goto tr346;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr346;
			} else if ( (*( current_position)) >= 120u )
				goto tr346;
		} else
			goto tr346;
	} else
		goto tr345;
	goto tr19;
tr341:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st200;
st200:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof200;
case 200:
	if ( (*( current_position)) < 92u ) {
		if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
			goto tr344;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr346;
		} else if ( (*( current_position)) >= 104u )
			goto tr346;
	} else
		goto tr346;
	goto tr19;
tr342:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st201;
st201:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof201;
case 201:
	if ( (*( current_position)) < 104u ) {
		if ( (*( current_position)) > 73u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr346;
		} else if ( (*( current_position)) >= 72u )
			goto tr344;
	} else if ( (*( current_position)) > 105u ) {
		if ( (*( current_position)) < 120u ) {
			if ( 108u <= (*( current_position)) && (*( current_position)) <= 109u )
				goto tr346;
		} else if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr346;
		} else
			goto tr346;
	} else
		goto tr346;
	goto tr19;
tr441:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st202;
st202:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof202;
case 202:
	switch( (*( current_position)) ) {
		case 193u: goto tr363;
		case 194u: goto tr364;
		case 195u: goto tr365;
		case 196u: goto tr366;
		case 197u: goto tr367;
		case 198u: goto tr368;
		case 199u: goto tr369;
		case 201u: goto tr363;
		case 202u: goto tr364;
		case 203u: goto tr365;
		case 204u: goto tr366;
		case 205u: goto tr367;
		case 206u: goto tr368;
		case 207u: goto tr369;
		case 209u: goto tr363;
		case 210u: goto tr364;
		case 211u: goto tr365;
		case 212u: goto tr366;
		case 213u: goto tr367;
		case 214u: goto tr368;
		case 215u: goto tr369;
		case 217u: goto tr363;
		case 218u: goto tr364;
		case 219u: goto tr365;
		case 220u: goto tr366;
		case 221u: goto tr367;
		case 222u: goto tr368;
		case 223u: goto tr369;
		case 225u: goto tr363;
		case 226u: goto tr364;
		case 227u: goto tr365;
		case 228u: goto tr366;
		case 229u: goto tr367;
		case 230u: goto tr368;
		case 231u: goto tr369;
		case 233u: goto tr363;
		case 234u: goto tr364;
		case 235u: goto tr365;
		case 236u: goto tr366;
		case 237u: goto tr367;
		case 238u: goto tr368;
		case 239u: goto tr369;
		case 241u: goto tr363;
		case 242u: goto tr364;
		case 243u: goto tr365;
		case 244u: goto tr366;
		case 245u: goto tr367;
		case 246u: goto tr368;
		case 247u: goto tr369;
		case 248u: goto tr370;
		case 249u: goto tr371;
		case 250u: goto tr372;
		case 251u: goto tr373;
		case 252u: goto tr374;
		case 253u: goto tr375;
		case 254u: goto tr376;
		case 255u: goto tr377;
	}
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 240u )
		goto tr362;
	goto tr19;
tr364:
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
		case 81u: goto tr298;
		case 83u: goto tr298;
		case 194u: goto tr300;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr298;
	} else if ( (*( current_position)) >= 88u )
		goto tr298;
	goto tr19;
tr365:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st204;
st204:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof204;
case 204:
	switch( (*( current_position)) ) {
		case 81u: goto tr298;
		case 194u: goto tr300;
		case 208u: goto tr298;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr298;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr372:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st205;
st205:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof205;
case 205:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 22u: goto tr298;
		case 111u: goto tr298;
		case 112u: goto tr300;
		case 194u: goto tr300;
		case 230u: goto tr298;
	}
	if ( (*( current_position)) < 81u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr299;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr298;
		} else if ( (*( current_position)) >= 88u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr373:
	{
    /* VEX.R is not used in ia32 mode.  */
    SET_VEX_PREFIX3((*current_position) & 0x7f);
  }
	goto st206;
st206:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof206;
case 206:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 81u: goto tr298;
		case 112u: goto tr300;
		case 194u: goto tr300;
		case 208u: goto tr298;
		case 230u: goto tr298;
		case 240u: goto tr299;
	}
	if ( (*( current_position)) < 88u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr299;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr298;
		} else if ( (*( current_position)) >= 92u )
			goto tr298;
	} else
		goto tr298;
	goto tr19;
tr442:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st207;
st207:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof207;
case 207:
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
	goto tr19;
tr443:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st208;
st208:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof208;
case 208:
	switch( (*( current_position)) ) {
		case 4u: goto st114;
		case 5u: goto st115;
		case 68u: goto st120;
		case 132u: goto st121;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st11;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st11;
		} else if ( (*( current_position)) >= 128u )
			goto st115;
	} else
		goto st119;
	goto tr19;
tr444:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st209;
st209:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof209;
case 209:
	goto st210;
st210:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof210;
case 210:
	goto tr379;
tr446:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st211;
st211:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof211;
case 211:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 52u: goto tr381;
		case 53u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr382;
	} else if ( (*( current_position)) >= 64u )
		goto tr383;
	goto tr380;
tr447:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st212;
st212:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof212;
case 212:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto st9;
		case 188u: goto st9;
		case 239u: goto tr19;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 63u )
					goto tr0;
			} else if ( (*( current_position)) >= 8u )
				goto tr19;
		} else if ( (*( current_position)) > 71u ) {
			if ( (*( current_position)) < 80u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr19;
			} else if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto st7;
			} else
				goto tr383;
		} else
			goto tr383;
	} else if ( (*( current_position)) > 135u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) > 143u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr382;
			} else if ( (*( current_position)) >= 136u )
				goto tr19;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 209u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr19;
			} else if ( (*( current_position)) > 227u ) {
				if ( 230u <= (*( current_position)) && (*( current_position)) <= 231u )
					goto tr19;
			} else
				goto tr19;
		} else
			goto st3;
	} else
		goto tr382;
	goto tr380;
tr448:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st213;
st213:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof213;
case 213:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 12u: goto tr381;
		case 20u: goto tr381;
		case 28u: goto tr381;
		case 36u: goto tr381;
		case 44u: goto tr381;
		case 52u: goto tr381;
		case 60u: goto tr381;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
		case 233u: goto tr380;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr380;
			} else
				goto tr380;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr380;
			} else if ( (*( current_position)) >= 22u )
				goto tr380;
		} else
			goto tr380;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr380;
			} else if ( (*( current_position)) >= 46u )
				goto tr380;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) < 192u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr383;
			} else if ( (*( current_position)) > 223u ) {
				if ( 224u <= (*( current_position)) )
					goto tr19;
			} else
				goto tr386;
		} else
			goto tr380;
	} else
		goto tr380;
	goto tr382;
tr449:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st214;
st214:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof214;
case 214:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 108u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 172u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 56u ) {
			if ( (*( current_position)) > 31u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr380;
			} else
				goto tr380;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr383;
			} else if ( (*( current_position)) >= 64u )
				goto tr383;
		} else
			goto tr380;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 184u ) {
			if ( (*( current_position)) > 159u ) {
				if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr382;
			} else if ( (*( current_position)) >= 128u )
				goto tr382;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 192u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr386;
			} else if ( (*( current_position)) > 227u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr380;
			} else
				goto tr380;
		} else
			goto tr382;
	} else
		goto tr383;
	goto tr19;
tr450:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st215;
st215:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof215;
case 215:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 52u: goto tr381;
		case 53u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr383;
	} else if ( (*( current_position)) > 191u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 223u )
			goto tr19;
	} else
		goto tr382;
	goto tr380;
tr451:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st216;
st216:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof216;
case 216:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 52u: goto tr381;
		case 53u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr19;
		} else if ( (*( current_position)) > 103u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr383;
			} else if ( (*( current_position)) >= 104u )
				goto tr19;
		} else
			goto tr383;
	} else if ( (*( current_position)) > 167u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto tr19;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) )
					goto tr19;
			} else if ( (*( current_position)) >= 200u )
				goto tr19;
		} else
			goto tr382;
	} else
		goto tr382;
	goto tr380;
tr452:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st217;
st217:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof217;
case 217:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 52u: goto tr381;
		case 53u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr383;
	} else if ( (*( current_position)) > 191u ) {
		if ( (*( current_position)) > 216u ) {
			if ( 218u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr19;
		} else if ( (*( current_position)) >= 208u )
			goto tr19;
	} else
		goto tr382;
	goto tr380;
tr453:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st218;
st218:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof218;
case 218:
	switch( (*( current_position)) ) {
		case 4u: goto tr381;
		case 5u: goto tr382;
		case 12u: goto tr381;
		case 13u: goto tr382;
		case 20u: goto tr381;
		case 21u: goto tr382;
		case 28u: goto tr381;
		case 29u: goto tr382;
		case 36u: goto tr381;
		case 37u: goto tr382;
		case 44u: goto tr381;
		case 45u: goto tr382;
		case 52u: goto tr381;
		case 53u: goto tr382;
		case 60u: goto tr381;
		case 61u: goto tr382;
		case 68u: goto tr384;
		case 76u: goto tr384;
		case 84u: goto tr384;
		case 92u: goto tr384;
		case 100u: goto tr384;
		case 108u: goto tr384;
		case 116u: goto tr384;
		case 124u: goto tr384;
		case 132u: goto tr385;
		case 140u: goto tr385;
		case 148u: goto tr385;
		case 156u: goto tr385;
		case 164u: goto tr385;
		case 172u: goto tr385;
		case 180u: goto tr385;
		case 188u: goto tr385;
	}
	if ( (*( current_position)) < 192u ) {
		if ( (*( current_position)) > 127u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr382;
		} else if ( (*( current_position)) >= 64u )
			goto tr383;
	} else if ( (*( current_position)) > 223u ) {
		if ( (*( current_position)) > 231u ) {
			if ( 248u <= (*( current_position)) )
				goto tr19;
		} else if ( (*( current_position)) >= 225u )
			goto tr19;
	} else
		goto tr19;
	goto tr380;
tr455:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_LOCK_PREFIX(TRUE);
  }
	goto st219;
st219:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof219;
case 219:
	switch( (*( current_position)) ) {
		case 15u: goto st220;
		case 102u: goto tr388;
		case 128u: goto st103;
		case 129u: goto st221;
		case 131u: goto st103;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 1u )
				goto st30;
		} else if ( (*( current_position)) > 9u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 24u <= (*( current_position)) && (*( current_position)) <= 25u )
					goto st30;
			} else if ( (*( current_position)) >= 16u )
				goto st30;
		} else
			goto st30;
	} else if ( (*( current_position)) > 33u ) {
		if ( (*( current_position)) < 134u ) {
			if ( (*( current_position)) > 41u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 49u )
					goto st30;
			} else if ( (*( current_position)) >= 40u )
				goto st30;
		} else if ( (*( current_position)) > 135u ) {
			if ( (*( current_position)) > 247u ) {
				if ( 254u <= (*( current_position)) )
					goto st105;
			} else if ( (*( current_position)) >= 246u )
				goto st104;
		} else
			goto st30;
	} else
		goto st30;
	goto tr19;
st220:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof220;
case 220:
	if ( (*( current_position)) == 199u )
		goto st52;
	if ( (*( current_position)) > 177u ) {
		if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
			goto st30;
	} else if ( (*( current_position)) >= 176u )
		goto st30;
	goto tr19;
st221:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof221;
case 221:
	switch( (*( current_position)) ) {
		case 4u: goto st114;
		case 5u: goto st115;
		case 12u: goto st114;
		case 13u: goto st115;
		case 20u: goto st114;
		case 21u: goto st115;
		case 28u: goto st114;
		case 29u: goto st115;
		case 36u: goto st114;
		case 37u: goto st115;
		case 44u: goto st114;
		case 45u: goto st115;
		case 52u: goto st114;
		case 53u: goto st115;
		case 68u: goto st120;
		case 76u: goto st120;
		case 84u: goto st120;
		case 92u: goto st120;
		case 100u: goto st120;
		case 108u: goto st120;
		case 116u: goto st120;
		case 132u: goto st121;
		case 140u: goto st121;
		case 148u: goto st121;
		case 156u: goto st121;
		case 164u: goto st121;
		case 172u: goto st121;
		case 180u: goto st121;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st11;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st115;
	} else
		goto st119;
	goto tr19;
tr456:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPNZ_PREFIX(TRUE);
  }
	goto st222;
st222:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof222;
case 222:
	switch( (*( current_position)) ) {
		case 15u: goto st223;
		case 102u: goto tr391;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 166u )
		goto tr0;
	goto tr19;
st223:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof223;
case 223:
	switch( (*( current_position)) ) {
		case 18u: goto tr393;
		case 43u: goto tr394;
		case 56u: goto st224;
		case 81u: goto tr392;
		case 112u: goto tr396;
		case 120u: goto tr397;
		case 121u: goto tr398;
		case 194u: goto tr396;
		case 208u: goto tr399;
		case 214u: goto tr400;
		case 230u: goto tr392;
		case 240u: goto tr401;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 42u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr392;
		} else if ( (*( current_position)) >= 16u )
			goto tr392;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr393;
		} else if ( (*( current_position)) >= 92u )
			goto tr392;
	} else
		goto tr392;
	goto tr19;
st224:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof224;
case 224:
	if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
		goto tr208;
	goto tr19;
tr397:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{
    SET_CPU_FEATURE(CPUFeature_SSE4A);
  }
	goto st225;
st225:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof225;
case 225:
	if ( 192u <= (*( current_position)) )
		goto st226;
	goto tr19;
st226:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof226;
case 226:
	goto tr403;
tr457:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	{
    SET_REPZ_PREFIX(TRUE);
  }
	goto st227;
st227:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof227;
case 227:
	switch( (*( current_position)) ) {
		case 15u: goto st228;
		case 102u: goto tr405;
		case 144u: goto tr406;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 170u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 164u )
		goto tr0;
	goto tr19;
st228:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof228;
case 228:
	switch( (*( current_position)) ) {
		case 18u: goto tr408;
		case 22u: goto tr408;
		case 43u: goto tr409;
		case 111u: goto tr410;
		case 112u: goto tr411;
		case 184u: goto tr210;
		case 188u: goto tr211;
		case 189u: goto tr212;
		case 194u: goto tr412;
		case 214u: goto tr413;
		case 230u: goto tr410;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 42u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
				goto tr407;
		} else if ( (*( current_position)) > 45u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 83u )
				goto tr407;
		} else
			goto tr407;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) < 92u ) {
			if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
				goto tr410;
		} else if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr410;
		} else
			goto tr407;
	} else
		goto tr407;
	goto tr19;
tr458:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st229;
st229:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof229;
case 229:
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
					goto tr19;
			} else if ( (*( current_position)) >= 64u )
				goto st40;
		} else
			goto tr19;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr19;
			} else if ( (*( current_position)) >= 128u )
				goto st36;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr19;
			} else if ( (*( current_position)) >= 192u )
				goto st10;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr459:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st230;
st230:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof230;
case 230:
	switch( (*( current_position)) ) {
		case 4u: goto st114;
		case 5u: goto st115;
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
		case 68u: goto st120;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st121;
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
					goto tr19;
			} else if ( (*( current_position)) >= 64u )
				goto st119;
		} else
			goto tr19;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr19;
			} else if ( (*( current_position)) >= 128u )
				goto st115;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr19;
			} else if ( (*( current_position)) >= 192u )
				goto st11;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
tr460:
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
	goto tr19;
tr462:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st232;
st232:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof232;
case 232:
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
		case 208u: goto tr414;
		case 224u: goto tr414;
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
	goto tr19;
st233:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof233;
case 233:
	if ( (*( current_position)) == 224u )
		goto tr415;
	goto tr11;
tr415:
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
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr463;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr463:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st234;
st234:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof234;
case 234:
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
		case 209u: goto tr414;
		case 225u: goto tr414;
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
	goto tr19;
st235:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof235;
case 235:
	if ( (*( current_position)) == 224u )
		goto tr416;
	goto tr11;
tr416:
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
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr464;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr464:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st236;
st236:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof236;
case 236:
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
		case 210u: goto tr414;
		case 226u: goto tr414;
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
	goto tr19;
st237:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof237;
case 237:
	if ( (*( current_position)) == 224u )
		goto tr417;
	goto tr11;
tr417:
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
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr465;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr465:
	{
       BitmapSetBit(valid_targets, current_position - data);
     }
	goto st238;
st238:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof238;
case 238:
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
		case 211u: goto tr414;
		case 227u: goto tr414;
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
	goto tr19;
st239:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof239;
case 239:
	if ( (*( current_position)) == 224u )
		goto tr418;
	goto tr11;
tr418:
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
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr466;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr466:
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
		case 212u: goto tr414;
		case 228u: goto tr414;
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
	goto tr19;
st241:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof241;
case 241:
	if ( (*( current_position)) == 224u )
		goto tr419;
	goto tr11;
tr419:
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
	goto st253;
st253:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof253;
case 253:
	switch( (*( current_position)) ) {
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr467;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr467:
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
		case 213u: goto tr414;
		case 229u: goto tr414;
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
	goto tr19;
st243:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof243;
case 243:
	if ( (*( current_position)) == 224u )
		goto tr420;
	goto tr11;
tr420:
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
	goto st254;
st254:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof254;
case 254:
	switch( (*( current_position)) ) {
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr468;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr468:
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
		case 214u: goto tr414;
		case 230u: goto tr414;
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
	goto tr19;
st245:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof245;
case 245:
	if ( (*( current_position)) == 224u )
		goto tr421;
	goto tr11;
tr421:
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
st255:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof255;
case 255:
	switch( (*( current_position)) ) {
		case 4u: goto tr423;
		case 5u: goto tr424;
		case 12u: goto tr423;
		case 13u: goto tr424;
		case 14u: goto tr19;
		case 15u: goto tr425;
		case 20u: goto tr423;
		case 21u: goto tr424;
		case 28u: goto tr423;
		case 29u: goto tr424;
		case 36u: goto tr423;
		case 37u: goto tr424;
		case 44u: goto tr423;
		case 45u: goto tr424;
		case 46u: goto tr426;
		case 47u: goto tr19;
		case 52u: goto tr423;
		case 53u: goto tr424;
		case 60u: goto tr423;
		case 61u: goto tr424;
		case 62u: goto tr427;
		case 63u: goto tr19;
		case 101u: goto tr429;
		case 102u: goto tr430;
		case 104u: goto tr424;
		case 105u: goto tr431;
		case 106u: goto tr423;
		case 107u: goto tr432;
		case 128u: goto tr432;
		case 129u: goto tr431;
		case 130u: goto tr19;
		case 131u: goto tr434;
		case 141u: goto tr435;
		case 143u: goto tr436;
		case 155u: goto tr437;
		case 168u: goto tr423;
		case 169u: goto tr424;
		case 196u: goto tr440;
		case 197u: goto tr441;
		case 198u: goto tr442;
		case 199u: goto tr443;
		case 200u: goto tr444;
		case 216u: goto tr446;
		case 217u: goto tr447;
		case 218u: goto tr448;
		case 219u: goto tr449;
		case 220u: goto tr450;
		case 221u: goto tr451;
		case 222u: goto tr452;
		case 223u: goto tr453;
		case 235u: goto tr433;
		case 240u: goto tr455;
		case 242u: goto tr456;
		case 243u: goto tr457;
		case 246u: goto tr458;
		case 247u: goto tr459;
		case 254u: goto tr460;
		case 255u: goto tr469;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) > 3u ) {
					if ( 6u <= (*( current_position)) && (*( current_position)) <= 7u )
						goto tr19;
				} else
					goto tr422;
			} else if ( (*( current_position)) > 19u ) {
				if ( (*( current_position)) < 24u ) {
					if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
						goto tr19;
				} else if ( (*( current_position)) > 27u ) {
					if ( 30u <= (*( current_position)) && (*( current_position)) <= 31u )
						goto tr19;
				} else
					goto tr422;
			} else
				goto tr422;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 54u ) {
				if ( (*( current_position)) > 39u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto tr422;
				} else if ( (*( current_position)) >= 38u )
					goto tr19;
			} else if ( (*( current_position)) > 55u ) {
				if ( (*( current_position)) < 96u ) {
					if ( 56u <= (*( current_position)) && (*( current_position)) <= 59u )
						goto tr422;
				} else if ( (*( current_position)) > 111u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto tr433;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr422;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 142u ) {
					if ( 154u <= (*( current_position)) && (*( current_position)) <= 157u )
						goto tr19;
				} else if ( (*( current_position)) >= 140u )
					goto tr19;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) < 184u ) {
					if ( 176u <= (*( current_position)) && (*( current_position)) <= 183u )
						goto tr423;
				} else if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto tr439;
				} else
					goto tr424;
			} else
				goto tr438;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) < 212u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 208u <= (*( current_position)) && (*( current_position)) <= 211u )
						goto tr445;
				} else if ( (*( current_position)) >= 202u )
					goto tr19;
			} else if ( (*( current_position)) > 231u ) {
				if ( (*( current_position)) < 234u ) {
					if ( 232u <= (*( current_position)) && (*( current_position)) <= 233u )
						goto tr454;
				} else if ( (*( current_position)) > 241u ) {
					if ( 250u <= (*( current_position)) && (*( current_position)) <= 251u )
						goto tr19;
				} else
					goto tr19;
			} else
				goto tr19;
		} else
			goto tr19;
	} else
		goto tr422;
	goto tr428;
tr469:
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
		case 215u: goto tr414;
		case 231u: goto tr414;
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
	goto tr19;
	}
	_test_eof247: ( current_state) = 247; goto _test_eof; 
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
	_test_eof248: ( current_state) = 248; goto _test_eof; 
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
	_test_eof231: ( current_state) = 231; goto _test_eof; 
	_test_eof232: ( current_state) = 232; goto _test_eof; 
	_test_eof233: ( current_state) = 233; goto _test_eof; 
	_test_eof249: ( current_state) = 249; goto _test_eof; 
	_test_eof234: ( current_state) = 234; goto _test_eof; 
	_test_eof235: ( current_state) = 235; goto _test_eof; 
	_test_eof250: ( current_state) = 250; goto _test_eof; 
	_test_eof236: ( current_state) = 236; goto _test_eof; 
	_test_eof237: ( current_state) = 237; goto _test_eof; 
	_test_eof251: ( current_state) = 251; goto _test_eof; 
	_test_eof238: ( current_state) = 238; goto _test_eof; 
	_test_eof239: ( current_state) = 239; goto _test_eof; 
	_test_eof252: ( current_state) = 252; goto _test_eof; 
	_test_eof240: ( current_state) = 240; goto _test_eof; 
	_test_eof241: ( current_state) = 241; goto _test_eof; 
	_test_eof253: ( current_state) = 253; goto _test_eof; 
	_test_eof242: ( current_state) = 242; goto _test_eof; 
	_test_eof243: ( current_state) = 243; goto _test_eof; 
	_test_eof254: ( current_state) = 254; goto _test_eof; 
	_test_eof244: ( current_state) = 244; goto _test_eof; 
	_test_eof245: ( current_state) = 245; goto _test_eof; 
	_test_eof255: ( current_state) = 255; goto _test_eof; 
	_test_eof246: ( current_state) = 246; goto _test_eof; 

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
  return result;
}
