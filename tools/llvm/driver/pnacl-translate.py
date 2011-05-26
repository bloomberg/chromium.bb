#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

from driver_tools import *

EXTRA_ENV = {
  'PIC'           : '0',
  'INPUTS'        : '',
  'OUTPUT'        : '',
  'OUTPUT_TYPE'   : '',
  'LLC'           : '${SANDBOXED ? ${LLC_SB} : ${LLVM_LLC}}',

  'TRIPLE'      : '${TRIPLE_%ARCH%}',
  'TRIPLE_ARM'  : 'armv7a-none-linux-gnueabi',
  'TRIPLE_X8632': 'i686-none-linux-gnu',
  'TRIPLE_X8664': 'x86_64-none-linux-gnu',


  'LLC_FLAGS_COMMON' : '-asm-verbose=false ${PIC ? -relocation-model=pic}',
  'LLC_FLAGS_ARM'    :
    # The following options might come in hand and are left here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float -aeabi-calls -sfi-zero-mask',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    ('-arm-reserve-r9 -sfi-disable-cp -arm_static_tls ' +
     '-sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables ${PIC ? -arm-elf-force-pic}'),

  'LLC_FLAGS_X8632' : '',
  'LLC_FLAGS_X8664' : '',

  # LLC flags which set the target and output type.
  # These are handled separately by libLTO.
  'LLC_FLAGS_TARGET' : '-march=${LLC_MARCH} -mcpu=${LLC_MCPU_%ARCH%} ' +
                       '-mtriple=${TRIPLE} -filetype=${filetype}',
  'LLC_FLAGS_BASE': '${LLC_FLAGS_COMMON} ${LLC_FLAGS_%ARCH%}',
  'LLC_FLAGS'     : '${LLC_FLAGS_TARGET} ${LLC_FLAGS_BASE}',

  'LLC_MARCH'       : '${LLC_MARCH_%ARCH%}',
  'LLC_MARCH_ARM'   : 'arm',
  'LLC_MARCH_X8632' : 'x86',
  'LLC_MARCH_X8664' : 'x86-64',

  'LLC_MCPU'        : '${LLC_MCPU_%ARCH%}',
  'LLC_MCPU_ARM'    : 'cortex-a8',
  'LLC_MCPU_X8632'  : 'pentium4',
  'LLC_MCPU_X8664'  : 'core2',

  'RUN_LLC'       : '${LLC} ${LLC_FLAGS} ${input} -o ${output}',
}
env.update(EXTRA_ENV)


TranslatorPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '-S',              "env.set('OUTPUT_TYPE', 's')"), # Stop at .s
  ( '-c',              "env.set('OUTPUT_TYPE', 'o')"), # Stop at .o

  ( '-fPIC',           "env.set('PIC', '1')"),

  ( '(-*)',            UnrecognizedOption),

  ( '(.*)',            "env.append('INPUTS', $0)"),
]


def main(argv):
  ParseArgs(argv, TranslatorPatterns)

  GetArch(required = True)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')
  output_type = env.getone('OUTPUT_TYPE')

  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')

  intype = FileType(inputs[0])

  if intype not in ('pso', 'po', 'bc', 'pexe'):
    Log.Fatal('Expecting input file to be bitcode (.pexe, .pso, .po, or .bc)')

  if not IsBitcode(inputs[0]):
    Log.Fatal('%s: File is not bitcode', inputs[0])

  if intype in ('pso'):
    env.set('PIC', '1')

  if output_type == '':
    DefaultOutputTypes = {
      'pso' : 'so',
      'pexe': 'nexe',
      'po'  : 'o',
      'bc'  : 'o',
    }
    output_type = DefaultOutputTypes[intype]

  if output == '':
    output = DefaultOutputName(inputs[0], output_type)

  tng = TempNameGen([inputs[0]], output)
  chain = DriverChain(inputs[0], output, tng)
  SetupChain(chain, output_type)
  chain.run()
  return 0

def SetupChain(chain, output_type):
  assert output_type in ('o','s','so','nexe')

  if output_type == 's' or not env.getbool('MC_DIRECT'):
    chain.add(RunLLC, 's', filetype='asm')
    if output_type == 's':
      return
    chain.add(RunAS, 'o')
  else:
    chain.add(RunLLC, 'o', filetype='obj')

  if output_type == 'o':
    return

  if output_type == 'so':
    chain.add(RunLD, 'so', shared = True)
  elif output_type == 'nexe':
    chain.add(RunLD, 'nexe', shared = False)

def RunAS(infile, outfile):
  RunDriver('pnacl-as', [infile, '-o', outfile])

# For now, we use pnacl-gcc instead of ld,
# because we need to link against native libraries.
def RunLD(infile, outfile, shared):
  args = ['--pnacl-native-hack', infile, '-o', outfile]
  if shared:
    args += ['-shared']
  RunDriver('pnacl-gcc', args)


def RunLLC(infile, outfile, filetype):
  UseSRPC = env.getbool('SANDBOXED') and env.getbool('SRPC')
  env.push()
  env.setmany(input=infile, output=outfile, filetype=filetype)
  if UseSRPC:
    RunLLCSRPC()
  else:
    RunWithLog("${RUN_LLC}")
  env.pop()
  return 0

def RunLLCSRPC():
  CheckPresenceSelUniversal()
  infile = env.getone("input")
  outfile = env.getone("output")
  flags = env.get("LLC_FLAGS")
  script = MakeSelUniversalScriptForLLC(infile, outfile, flags)

  RunWithLog('${SEL_UNIVERSAL_PREFIX} "${SEL_UNIVERSAL}" ' +
             '${SEL_UNIVERSAL_FLAGS} -- "${LLC_SRPC}"',
             stdin=script, echo_stdout = False, echo_stderr = False)

def MakeSelUniversalScriptForLLC(infile, outfile, flags):
  script = []
  script.append('readonly_file myfile %s' % infile)
  for f in flags:
    script.append('rpc AddArg s("%s") *' % f)
  script.append('rpc Translate h(myfile) * h() i()')
  script.append('set_variable out_handle ${result0}')
  script.append('set_variable out_size ${result1}')
  script.append('map_shmem ${out_handle} addr')
  script.append('save_to_file %s ${addr} 0 ${out_size}' % outfile)
  script.append('')
  return '\n'.join(script)

if __name__ == "__main__":
  DriverMain(main)
