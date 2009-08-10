# SCons "tool" module that simply sets a -D value.
def generate(env):
  env['CPPDEFINES'] = ['THIS_TOOL']

def exists(env):
  pass
