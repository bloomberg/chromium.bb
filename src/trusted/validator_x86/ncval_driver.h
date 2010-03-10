/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Run the validator within the set up environment.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_DRIVER_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_DRIVER_H__

/* The routine that loads the code segment(s) into memory, returning
 * any data to be passed to the analysis step.
 */
typedef void* (*NaClValidateLoad)(int argc, const char* argv[]);

/* The actual validation analysis, applied to the data returned by
 * ValidateLoad. Assume that this function also deallocates any memory
 * in loaded_data. Returns the exit statis of the analysis.
 */
typedef int (*NaClValidateAnalyze)(void* loaded_data);

/* Run the validator using the given command line arguments. Initially
 * strips off arguments needed by this driver, and then passes the
 * remaining command line arguments to the given load function. The
 * (allocated) memory returned from the load function is then passed
 * to the analyze function to do the analysis. Returns the exit status
 * returned by the analyze function.
 */
int NaClRunValidator(int argc, const char* argv[],
                     NaClValidateLoad load,
                     NaClValidateAnalyze analyze);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_DRIVER_H__ */
