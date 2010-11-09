#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import re

def GetNaClElfConstants(header_file):

  symbols = [ 'ELFOSABI_NACL',
              'EF_NACL_ALIGN_MASK',
              'EF_NACL_ALIGN_16',
              'EF_NACL_ALIGN_32',
              'EF_NACL_ALIGN_LIB',
              'EF_NACL_ABIVERSION',
              ]

  extractors = [ r'#\s*define\s+%s\s+([x\d]+)' % s
                 for s in symbols ]

  nfas = [ re.compile(extractor) for extractor in extractors ]
  results = len(extractors) * [ -1 ]
  for line in open(header_file):
    for ix, nfa in enumerate(nfas):
      m = nfa.search(line)
      if m is not None:
        results[ix] = int(m.group(1), 0)
        continue
  if -1 in results:
    raise RuntimeError, 'Could not parse header file %s' % header_file
  return dict(zip(symbols, results))
