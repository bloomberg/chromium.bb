#!/usr/bin/python
import optparse
import os
import re
import subprocess
import sys

# This script takes an o3d cg shader from standard input and does the following:
#
# * it extracts entry points to vertex and fragment shaders as specified by
#   VertexShaderEntryPoint and PixelShaderEntryPoint instructions;
#
# * renames NORMAL, TANGENT{,1} and BINORMAL{,1} attributes to ATTR8-12;
#
# * runs cgc with glslv and glslf profiles with those entry points;
#
# * renames attributes and uniforms back to their orignal names;
#
# * changes 'uniform vecN var[N]' to 'uniform matN var';
#
# * renames gl_Vertex and gl_MultiTexCoordN to position and texcoordN
#   respectively and adds attribute declarations;
#
# * prints the results to standard output, separating them with SplitMarker
#   instruction and keeping the MatrixLoadOrder instruction as is.


CGC = '/usr/bin/cgc'

# cgc complains about TANGENT1 and BINORMAL1 semantics, so we hack it by
# replacing standard semantics with ATTR8-ATTR12 and then renaming them back to
# their original names.
ATTRIBUTES_TO_SEMANTICS = dict(
    attr8 = 'normal',
    attr9 = 'tangent',
    attr10 = 'binormal',
    attr11 = 'tangent1',
    attr12 = 'binormal1')

MATRIX_UNIFORM_NAMES = [
    'world',
    'view',
    'projection',
    'worldView',
    'worldViewProjection',
    'worldInverse',
    'viewInverse',
    'projectionInverse',
    'worldViewInverse',
    'viewProjectionInverse',
    'worldViewProjectionInverse',
    'worldTranspose',
    'viewTranspose',
    'projectionTranspose',
    'worldViewTranspose',
    'viewProjectionTranspose',
    'worldViewProjectionTranspose',
    'worldInverseTranspose',
    'viewInverseTranspose',
    'projectionInverseTranspose',
    'worldViewInverseTranspose',
    'viewProjectionInverseTranspose',
    'worldViewProjectionInverseTranspose'
]

def correct_semantic_case(name):
  lower_name = name.lower()
  for current_name in MATRIX_UNIFORM_NAMES:
    if lower_name == current_name.lower():
      return current_name
  return lower_name

def get_input_mapping(header):
  ret = dict()
  for l in header.splitlines():
    if not l.startswith('//var'):
      continue
    old_name_and_type, semantic, new_name, _, _ = l.split(' : ')
    if '[' in new_name:
      new_name = new_name[:new_name.index('[')]
    if new_name.startswith('$'):
      new_name = new_name[1:]
    ret[new_name] = (correct_semantic_case(semantic) if semantic
        else old_name_and_type.split(' ')[2])
  return ret


def fix_glsl_body(body, input_mapping):
  # Change uniform names back to original.
  for match in re.findall(r'(?m)^uniform (?:\w+) (\w+)', body):
    body = re.sub(r'\b%s\b' % match, input_mapping[match], body)

  # Change attribute names back to original.
  for match in re.findall(r'(?m)^attribute (?:\w+) (\w+)', body):
    attr_name = input_mapping[match]
    assert attr_name.startswith('$vin.')
    orig_name = ATTRIBUTES_TO_SEMANTICS[attr_name[len('$vin.'):]]
    body = re.sub(r'\b%s\b' % match, orig_name, body)

  # Change vecN blah[N]; to matN blah;
  body = re.sub(r'(?m)^uniform vec(\d) (\w+)\[\1\];', r'uniform mat\1 \2;',
      body)

  attributes = []
  if 'gl_Vertex' in body:
    # Change gl_Vertex to position and add attribute declaration.
    body = re.sub(r'\bgl_Vertex\b', 'position', body)
    attributes.append('attribute vec4 position;')

  for n in xrange(8):
    if 'gl_MultiTexCoord%d' % n in body:
      # Change gl_MultiTexCoordN (0<=N<=7) to texcoordN and add attribute
      # declaration.
      body = re.sub(r'\bgl_MultiTexCoord%d\b' % n, 'texcoord%d' % n, body)
      attributes.append('attribute vec4 texcoord%d;' % n)

  # ATTRIBUTES_TO_SEMANTICS should have taken care of normals.
  assert 'gl_Normal' not in body

  if 'gl_Position' in body:
    # If there is exactly one assignment to gl_Position, modify it similar to
    # how RewriteVertexProgramSource in gl/effect_gl.cc does it.  The input is
    # taken from vec4 dx_clipping uniform.
    #
    # If there is more than one gl_Position mentioned in the shader, the
    # converter fails.
    assert len(re.findall('gl_Position', body)) == 1
    attributes.append('vec4 _glPositionTemp;')
    attributes.append('uniform vec4 dx_clipping;')
    body = re.sub(r'\bgl_Position\b([^;]*);',
                  r'_glPositionTemp\1; gl_Position = vec4(' +
                  r'_glPositionTemp.x + _glPositionTemp.w * dx_clipping.x, ' +
                  r'dx_clipping.w * ' +
                  r'(_glPositionTemp.y + _glPositionTemp.w * dx_clipping.y), ' +
                  r'_glPositionTemp.z * 2.0 - _glPositionTemp.w, ' +
                  r'_glPositionTemp.w);', body)

  return '\n'.join(attributes) + '\n\n' + body


def fix_glsl(glsl_shader):
  header, body = re.split(r'\n\n', glsl_shader, 1)
  assert all(l.startswith('//') for l in header.splitlines())
  input_mapping = get_input_mapping(header)
  return header + '\n\n' + fix_glsl_body(body, input_mapping)


def cg_rename_attributes(cg_shader):
  for new, old in ATTRIBUTES_TO_SEMANTICS.iteritems():
    cg_shader = re.sub(r'\b%s\b' % old.upper(), new.upper(), cg_shader)
  return cg_shader


def cg_to_glsl(cg_shader):
  cg_shader = cg_rename_attributes(cg_shader)

  vertex_entry = re.search(r'#o3d\s+VertexShaderEntryPoint\s+(\w+)',
      cg_shader).group(1)
  p = subprocess.Popen(CGC+' -profile glslv -entry %s' % vertex_entry,
      shell=True,
      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  glsl_vertex, err_v = p.communicate(cg_shader)

  fragment_entry = re.search(r'#o3d\s+PixelShaderEntryPoint\s+(\w+)',
      cg_shader).group(1)
  p = subprocess.Popen(CGC+' -profile glslf -entry %s' % fragment_entry,
      shell=True,
      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  glsl_fragment, err_f = p.communicate(cg_shader)

  log = (
      '// glslv profile log:\n' +
      '\n'.join('// ' + l for l in err_v.splitlines()) + '\n\n'
      '// glslf profile log:\n' +
      '\n'.join('// ' + l for l in err_f.splitlines())) + '\n'

  return glsl_vertex, glsl_fragment, log


def get_matrixloadorder(cg_shader):
  return re.search(r'(?m)^.*#o3d\s+MatrixLoadOrder\b.*$', cg_shader).group(0)


def check_cg():
  if not os.path.exists(CGC):
    print >>sys.stderr, CGC+' is not found, use --cgc option to specify its'
    print >>sys.stderr, 'location.  You may need to install nvidia cg toolkit.'
    print >>sys.stderr, 'In Ubuntu distribution it can be done by running:'
    print >>sys.stderr, '  "apt-get install nvidia-cg-toolkit"'
    sys.exit(1)


def main(cg_shader):
  matrixloadorder = get_matrixloadorder(cg_shader)
  glsl_vertex, glsl_fragment, log = cg_to_glsl(cg_shader)

  print log
  print fix_glsl(glsl_vertex)
  print
  print '// #o3d SplitMarker'
  print get_matrixloadorder(cg_shader).strip()
  print
  print fix_glsl(glsl_fragment)


if __name__ == '__main__':
    cmdline_parser = optparse.OptionParser()
    cmdline_parser.add_option('--cgc', dest='CGC', default='/usr/bin/cgc',
        help='path to cgc [default: %default]')
    options, args = cmdline_parser.parse_args()
    CGC = options.CGC
    check_cg()

    try:
      input = sys.stdin.read()
    except KeyboardInterrupt:
      input = None

    if not input:
      cmdline_parser.print_help()
    else:
      main(input)
