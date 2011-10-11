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

  # These are temporary. Since we don't have
  # DT_NEEDED metadata in bitcode yet, it must be provided
  # on the command-line.
  'EXTRA_LD_FLAGS': '',

  # If translating a .pexe which was linked statically against
  # glibc, then you must do pnacl-translate -static. This will
  # be removed once we can detect .pexe type.
  'STATIC'        : '0',

  'INPUTS'        : '',
  'OUTPUT'        : '',
  'OUTPUT_TYPE'   : '',
  'LLC'           : '${SANDBOXED ? ${LLC_SB} : ${LLVM_LLC}}',

  'TRIPLE'      : '${TRIPLE_%ARCH%}',
  'TRIPLE_ARM'  : 'armv7a-none-nacl-gnueabi',
  'TRIPLE_X8632': 'i686-none-nacl-gnu',
  'TRIPLE_X8664': 'x86_64-none-nacl-gnu',

  'LLC_FLAGS_COMMON': '-asm-verbose=false ' +
                      '-tail-merge-threshold=50 ' +
                      '${PIC ? -relocation-model=pic} ' +
                      '${PIC && ARCH==X8664 && LIBMODE_NEWLIB ? ' +
                      '  -force-tls-non-pic }',
  'LLC_FLAGS_ARM'    :
    # The following options might come in handy and are left here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float -aeabi-calls -sfi-zero-mask',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    ('-arm-reserve-r9 -sfi-disable-cp -arm_static_tls ' +
     '-sfi-load -sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables'),

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
  ( '-o(.+)',          "env.set('OUTPUT', pathtools.normalize($0))"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', pathtools.normalize($0))"),

  ( '-S',              "env.set('OUTPUT_TYPE', 's')"), # Stop at .s
  ( '-c',              "env.set('OUTPUT_TYPE', 'o')"), # Stop at .o

  # Expose a very limited set of llc flags. Used primarily for
  # the shared lib ad-hoc tests, c.f. tests/pnacl_ld_example
  ( '(-sfi-.+)',       "env.append('LLC_FLAGS', $0)"),
  ( '(-mtls-use-call)',  "env.append('LLC_FLAGS', $0)"),

  ( '-static',         "env.set('STATIC', '1')"),
  ( '-fPIC',           "env.set('PIC', '1')"),

  ( '(-L.*)',          "env.append('EXTRA_LD_FLAGS', $0)"),
  ( ('(-L)','(.*)'),   "env.append('EXTRA_LD_FLAGS', $0, $1)"),
  ( '(-l.*)',          "env.append('EXTRA_LD_FLAGS', $0)"),
  ( '(-Wl,.*)',        "env.append('EXTRA_LD_FLAGS', $0)"),

  ( '(-*)',            UnrecognizedOption),

  ( '(.*)',            "env.append('INPUTS', pathtools.normalize($0))"),
]


def main(argv):
  ParseArgs(argv, TranslatorPatterns)

  GetArch(required = True)

  inputs = env.get('INPUTS')
  output = env.getone('OUTPUT')
  output_type = env.getone('OUTPUT_TYPE')

  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')

  infile = inputs[0]

  if not IsBitcode(infile):
    Log.Fatal('%s: File is not bitcode', pathtools.touser(infile))

  intype = FileType(infile)
  if intype not in ('pso', 'po', 'pexe'):
    Log.Fatal('Expecting input file to be bitcode (.pexe, .pso, .po, or .bc)')

  if intype == 'pso':
    env.set('PIC', '1')

  # Read the bitcode metadata to extract library
  # dependencies and SOName.
  metadata = GetBitcodeMetadata(infile)
  for lib in metadata['NeedsLibrary']:
    env.append('EXTRA_LD_FLAGS', '-l:' + lib)

  if intype == 'pso':
    soname = metadata['SOName']
    if soname:
      env.append('EXTRA_LD_FLAGS', '-Wl,-soname=' + soname)

  if output_type == '':
    DefaultOutputTypes = {
      'pso' : 'so',
      'pexe': 'nexe',
      'po'  : 'o',
    }
    output_type = DefaultOutputTypes[intype]

  if output == '':
    output = DefaultOutputName(infile, output_type)

  tng = TempNameGen([infile], output)
  chain = DriverChain(infile, output, tng)
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

# For now, we use pnacl-g++ instead of ld,
# because we need to link against native libraries.
def RunLD(infile, outfile, shared):
  args = ['--pnacl-native-hack', infile, '-o', outfile]

  if shared:
    args += ['-shared']
  elif env.getbool('STATIC'):
    args += ['-static']

  extra_ld_flags = env.get('EXTRA_LD_FLAGS')
  RunDriver('pnacl-g++', args + extra_ld_flags)


def RunLLC(infile, outfile, filetype):
  UseSRPC = env.getbool('SANDBOXED') and env.getbool('SRPC')
  # For now, sel_universal doesn't properly support dynamic sb translator
  if env.getbool('SANDBOXED') and env.getbool('SB_DYNAMIC'):
    CheckTranslatorPrerequisites()
    UseSRPC = False
  env.push()
  env.setmany(input=infile, output=outfile, filetype=filetype)
  if UseSRPC:
    RunLLCSRPC()
  else:
    RunWithLog("${RUN_LLC}")
  env.pop()
  return 0

def RunLLCSRPC():
  CheckTranslatorPrerequisites()
  infile = env.getone("input")
  outfile = env.getone("output")
  flags = env.get("LLC_FLAGS")
  script = MakeSelUniversalScriptForLLC(infile, outfile, flags)

  RunWithLog('${SEL_UNIVERSAL_PREFIX} ${SEL_UNIVERSAL} ' +
             '${SEL_UNIVERSAL_FLAGS} -- ${LLC_SRPC}',
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
