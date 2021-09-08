*****************************************************************************************
 The NEON_2_SSE.h file is intended to simplify ARM->IA32 porting.
 It makes the correspondence (or a real porting) of ARM NEON intrinsics as defined in "arm_neon.h" header
 and x86 SSE (up to SSE4.2) intrinsic functions as defined in corresponding x86 compilers headers files.
 ****************************************************************************************

To take advantage of this file just include it in your project that uses ARM NEON intinsics instead of "arm_neon.h", compile it as usual and enjoy the result.

For significant performance improvement in some cases you might need to define USE_SSE4 in your project settings. Otherwise SIMD up to SSSE3 to be used.

If NEON2SSE_DISABLE_PERFORMANCE_WARNING macro is defined, then the performance warnings are disabled.

For more information and license please read the NEON_2_SSE.h content.

The unit tests set used for ARM NEON - x86 SSE conformance verification is  https://github.com/christophe-lyon/arm-neon-tests
