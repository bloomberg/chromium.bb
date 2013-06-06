import os
import subprocess
import sys
import tempfile

import objdump_parser
import test_format


class RdfaTestRunner(test_format.TestRunner):

  SECTION_NAME = 'dis'

  def CommandLineOptions(self, parser):
    parser.add_option('--objdump',
                      help='Path to objdump')

  def GetSectionContent(self, options, hex_content):
    arch = {32: '-Mi386', 64: '-Mx86-64'}[options.bits]
    data = ''.join(test_format.ParseHex(hex_content))

    tmp = tempfile.NamedTemporaryFile(mode='wb', delete=False)
    try:
      tmp.write(data)
      tmp.close()

      objdump_proc = subprocess.Popen(
          [options.objdump,
           '-mi386', arch, '--target=binary',
           '--disassemble-all', '--disassemble-zeroes',
           tmp.name],
          stdout=subprocess.PIPE)

      result = ''.join(objdump_parser.SkipHeader(objdump_proc.stdout))
      return_code = objdump_proc.wait()
      assert return_code == 0, 'error running objdump'

    finally:
      tmp.close()
      os.remove(tmp.name)

    return result


def main(argv):
  RdfaTestRunner().Run(argv)


if __name__ == '__main__':
  main(sys.argv[1:])
