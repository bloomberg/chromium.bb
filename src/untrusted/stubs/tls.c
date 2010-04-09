
/* Placeholder.  This translation unit will be one of a few in the
 * "crt_platform" library that provides low-level platform-specific
 * intrinsics written in C.  The NaCl SDK linkers will include the
 * platform library in every nexe, along with the crt*.o files.  Code
 * will be moved in here once that change is released in the SDKs.
 */

/* A dummy symbol to make it easy to detect the presence of this library. */
int __nacl_crt_platform_library_is_indeed_present = 1;
