/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
// collada_edge.cpp : Defines the entry point for the console application.
#include "precompile.h"
#include "conditioner.h"
#include <cstdio>

static const char *usage =
"\nUsage: ColladaEdgeConditioner <infile.dae> <outifle.dae> [ options ]\n\n\
Options:\n\
  -sharpEdgeThreshold t  : sharp edge threshold defined by dihedral \n\
                           angle. Edges with a angle smaller than t\n\
                           will be defined as a sharp edge.\n\
  -sharpEdgeColor r,g,b  : color of addtional edges.\n\
                           default value is 1,0,0 .(no space)\n\n\
ColladaEdgeConditioner checks all polygon edges in <infile.dae> and add\n\
addtional edges to the model if normal angle of certain edge is larger\n\
than the pre-defined threshold.\n";

int wmain(int argc, wchar_t **argv) {
  wchar_t* input_file = NULL;
  wchar_t* output_file = NULL;
  Options options;

  if (argc < 3) {
    printf("%s", usage);
    return -1;
  }
  int file_count = 0;
  for (int i = 1; i < argc; i++) {
    if (wcscmp(argv[i], L"-sharpEdgeThreshold") == 0) {
      if (++i < argc) {
        options.enable_soften_edge = true;
        options.soften_edge_threshold = static_cast<float>(_wtof(argv[i]));
      }
    } else if (wcscmp(argv[i], L"-sharpEdgeColor") == 0) {
      if (++i < argc) {
        int r, g, b;
        if (swscanf_s(argv[i], L"%d,%d,%d", &r, &g, &b) != 3) {
          printf("Invalid -sharpEdgeColor. Should be -sharpEdgeColor=x,y,z\n");
          return -1;
        }
        options.sharp_edge_color = Vector3(static_cast<float>(r),
                                           static_cast<float>(g),
                                           static_cast<float>(b));
      }
    } else if (file_count < 2) {
      ++file_count;
      if (file_count == 1)
        input_file = argv[i];
      else
        output_file = argv[i];
    }
  }

  bool result = Condition(input_file, output_file, options);
  return 0;
}
