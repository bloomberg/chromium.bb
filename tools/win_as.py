#!/usr/bin/python
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import getopt
import sys
import subprocess
import re
import os

third_party = '/native_client/src/third_party'
as32 = third_party + '/gnu_binutils/files/as.exe'
as64 = third_party + '/mingw-w64/mingw/bin/x86_64-w64-mingw32-as.exe'

def main(argv):
  try:
    (opts, args) = getopt.getopt(argv[1:], 'a:p:o:')

    nacl_build_subarch = 0
    output_file = sys.stdout
    nacl_path = os.getcwd()

    for opt, val in opts:
      if opt == '-o':
        output_filename = val
      elif opt == '-a':
        if val.lower() == 'win32':
          nacl_build_subarch = 32
          as_exe = as32
        elif val.lower() == 'x64':
          nacl_build_subarch = 64
          as_exe = as64
        else:
          raise getopt.error('Unknown target architecture ' + val)
        # endif
      elif opt == '-p':
        nacl_path = (val)
      # endif
    # endfor

    if nacl_build_subarch == 0:
      raise getopt.error( 'You must specify a build architecture: '
                          + 'either -a Win32 or -a x64' )
    # endif

    if not output_filename:
      raise getopt.error( 'No output file specified' )
    # endif

    for filename in args:
      # normalize the filename to avoid any DOS-type funkiness
      filename = os.path.abspath(filename)
      filename = filename.replace('\\', '/')

      #
      # Run the C compiler as a preprocessor and pipe the output into a string
      #
      print >>sys.stderr, 'Preprocessing...'
      p = subprocess.Popen(['cl.exe',
                             '/DNACL_BLOCK_SHIFT=5'
                             '/DNACL_BUILD_ARCH=' + str(nacl_build_subarch),
                             '/DNACL_BUILD_SUBARCH=' + str(nacl_build_subarch),
                             '/DNACL_WINDOWS=1',
                             '/E',
                             '/I' + nacl_path,
                             filename],
                          stdout=subprocess.PIPE)
      cl_output = p.communicate()[0]

      if p.wait() == 0: # success
        #
        # Pipe the preprocessor output into the assembler
        #
        print >>sys.stderr, 'Assembling...'
        p = subprocess.Popen([nacl_path + as_exe,
                              '-defsym','@feat.00=1',
                              '--' + str(nacl_build_subarch),
                              '-o', output_filename ],
                            stdin=subprocess.PIPE,
                            stderr=subprocess.PIPE)
        as_output, as_error = p.communicate(cl_output)

        #
        # massage the assembler stderr into a format that Visual Studio likes
        #
        as_error = re.sub(r'\{standard input\}', filename, as_error)
        as_error = re.sub(r':([0-9]+):', r'(\1) :', as_error)
        as_error = re.sub(r'Error', 'error', as_error)
        as_error = re.sub(r'Warning', 'warning', as_error)

        print >>sys.stderr, as_error

      # endif
    # endfor

  except getopt.error, e:
    print >>sys.stderr, str(e)
    print >>sys.std4err, ['Usage: ',
      argv[0],
      '-a {Win32|x64} -o output_file [-p native_client_path] input_file']
  # endtry

# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
