#!/usr/bin/python
#
# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#

"""
Responsible for generating the decoder based on parsed table representations.
"""

import dgen_opt

def generate_decoder_h(tables, decoder_name, filename, named_decoders, out):
    """Entry point to the decoder for .h file.

    Args:
        tables: list of Table objects to process.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        named_decoders: If true, generate a decoder state with named instances.
        out: a COutput object to write to.
    """
    if len(tables) == 0: raise Exception('No tables provided.')

    ifdef_name = _generate_ifdef_name(filename)
    named_prefix = ''
    if named_decoders:
      named_prefix = 'Named'

    _generate_header(out)
    out.line()
    out.line("#ifndef %s" % ifdef_name)
    out.line("#define %s" % ifdef_name)
    out.line()
    out.line('#include "native_client/src/trusted/validator_arm/decode.h"')
    out.line()
    out.line('namespace nacl_arm_dec {')
    out.line()
    if named_decoders:
      _generate_named_decoder_classes(tables, named_prefix, out)
    _generate_decoder_state_type(tables, decoder_name, named_prefix, out)
    out.line('}  // namespace')
    out.line("#endif  // %s" % ifdef_name)


def generate_decoder_cc(tables, decoder_name, filename, named_decoders, out):
    """Entry point to the decoder for .cc file

    Args:
        tables: list of Table objects to process.
        decoder_name: The name of the decoder state to build.
        filename: The (localized) name for the .h file.
        named_decoders: If true, generate a decoder state with named instances.
        out: a COutput object to write to.
    """
    if len(tables) == 0: raise Exception('No tables provided.')

    named_prefix = ''
    if named_decoders:
      named_prefix = 'Named'

    _generate_header(out)
    out.line()
    out.line('#include "%s"' % (filename[:-2] + 'h'))
    out.line();
    out.line('#include <stdio.h>')
    out.line()
    out.line('namespace nacl_arm_dec {')
    out.line()
    if named_decoders:
      _generate_named_decoder_classes_impls(tables, named_prefix, out)
    _generate_prototypes(tables, decoder_name, named_prefix, out)
    _generate_implementations(tables, decoder_name, named_prefix, out)
    _generate_constructors(tables, decoder_name, named_prefix, out)
    _generate_entry_point(decoder_name, named_prefix, tables[0].name, out)
    out.line('}  // namespace')


def _generate_ifdef_name(filename):
  return filename.replace("/", "_").replace(".", "_").upper() + "_"

def _generate_header(out):
    out.block_comment(
        'Copyright 2012 The Native Client Authors.  All rights reserved.',
        'Use of this source code is governed by a BSD-style license that can',
        'be found in the LICENSE file.',
        )
    out.line()
    out.block_comment('DO NOT EDIT: GENERATED CODE')

def _table_terminals(tables):
    terminals = set()
    for t in tables:
        for r in t.rows:
            if r.action.startswith('='):
                terminals.add(r.action[1:])
    return terminals

def _generate_named_decoder_classes(tables, named_prefix, out):
  for t in _table_terminals(tables):
    out.enter_block('struct %s%s : public %s' % (named_prefix, t, t))
    out.line('virtual ~%s%s() {}' % (named_prefix, t))
    out.line('virtual const char* name() const;')
    out.exit_block(';')
    out.line()

def _generate_named_decoder_classes_impls(tables, named_prefix, out):
  for t in _table_terminals(tables):
    out.enter_block('const char* %s%s::name() const' % (named_prefix, t));
    out.line('return "%s";' % t)
    out.exit_block()
    out.line()

def _generate_decoder_state_type(tables, decoder_name, named_prefix, out):
    cls_name = '%s%s' % (named_prefix, decoder_name)
    out.block_comment(
        'Defines a stateless decoder class selector for instructions'
    )
    out.block_comment('Define the class decoders used by this decoder state.')
    out.enter_block('class %s : DecoderState' % cls_name)
    out.visibility('public')
    out.comment('Generates an instance of a decoder state.')
    out.line('explicit %s();' % cls_name)
    out.line('virtual ~%s();' % cls_name)
    out.line()
    out.comment('Parses the given instruction, returning the decoder to use.')
    out.line('virtual const class ClassDecoder '
             '&decode(const Instruction) const;')
    out.line()
    out.comment('Define the decoders to use in this decoder state')
    for t in _table_terminals(tables):
        out.line('%s%s %s_instance_;' % (named_prefix, t, t))
    out.line()
    out.visibility('private')
    out.comment("Don't allow the following!")
    out.line('explicit %s(const %s&);' % (cls_name, cls_name))
    out.line('void operator=(const %s&);' % cls_name)
    out.exit_block(';')
    out.line()

def _generate_prototypes(tables, decoder_name, named_prefix, out):
    out.block_comment('Prototypes for static table-matching functions.')
    for t in tables:

        out.line('static inline const ClassDecoder &decode_%s(' % t.name)
        out.line('    const Instruction insn, const %s%s *state);' %
                 (named_prefix, decoder_name))
        out.line()

def _generate_implementations(tables, decoder_name, named_prefix, out):
    out.block_comment('Table-matching function implementations.')
    for t in tables:
        out.line()
        _generate_table(t, decoder_name, named_prefix, out)
    out.line()

def _generate_constructors(tables, decoder_name, named_prefix, out):
    out.enter_constructor_header('%s%s::%s%s()' % (named_prefix, decoder_name,
                                                   named_prefix, decoder_name))
    out.line('DecoderState()')
    for t in _table_terminals(tables):
      out.line(', %s_instance_()' % t)
    out.exit_constructor_header()
    out.enter_block()
    out.exit_block()
    out.line()
    out.enter_block('%s%s::~%s%s()' % (named_prefix, decoder_name,
                                       named_prefix, decoder_name))
    out.exit_block()
    out.line()

def _generate_entry_point(decoder_name, named_prefix, initial_table_name, out):
    out.enter_block(
        'const ClassDecoder &%s%s::decode(const Instruction insn) const' %
        (named_prefix, decoder_name))
    out.line('return decode_%s(insn, this);'
        % initial_table_name)
    out.exit_block()
    out.line()


def _generate_table(table, decoder_name, named_prefix, out):
    """Generates the implementation of a single table."""
    out.block_comment(
        'Implementation of table %s.' % table.name,
        'Specified by: %s.' % table.citation
    )
    out.line('static inline const ClassDecoder &decode_%s(' %table.name)
    out.enter_block('    const Instruction insn, const %s%s *state)' %
                    (named_prefix, decoder_name))

    optimized = dgen_opt.optimize_rows(table.rows)
    print ("Table %s: %d rows minimized to %d"
        % (table.name, len(table.rows), len(optimized)))
    for row in sorted(optimized):
        exprs = ["(%s)" % p.to_c_expr('insn') for p in row.patterns]
        out.enter_block('if (%s)' % ' && '.join(exprs))

        if row.action.startswith('='):
            _generate_terminal(row.action[1:], out)
        elif row.action.startswith('->'):
            _generate_table_change(row.action[2:], out)
        else:
            raise Exception('Bad table action: %s' % row.action)

        out.exit_block()
        out.line()

    _generate_safety_net(table, out)
    out.exit_block()

def _generate_terminal(name, out):
    out.line('return state->%s_instance_;' % name)

def _generate_table_change(name, out):
    out.line('return decode_%s(insn, state);' % name)

def _generate_safety_net(table, out):
    out.line('// Catch any attempt to fall through...')
    out.line('fprintf(stderr, "TABLE IS INCOMPLETE: %s could not parse %%08X",'
        'insn.bits(31,0));' % table.name)
    _generate_terminal('Forbidden', out)


class COutput(object):
    """Provides nicely-formatted C++ output."""

    def __init__(self, out):
        self._out = out
        self._indent = 0

    def line(self, str = ''):
        self._out.write(self._tabs())
        self._out.write(str + '\n')

    def enter_constructor_header(self, headline):
        self.line(headline + ' :')
        self._indent += 1

    def exit_constructor_header(self):
        self._indent -= 1

    def visibility(self, spec):
        self._indent -= 1
        self.line(' %s:' % spec)
        self._indent += 1

    def comment(self, line_comment):
        self.line('// %s' % line_comment)

    def enter_block(self, headline=''):
        self.line(headline + ' {')
        self._indent += 1

    def exit_block(self, footer = ''):
        self._indent -= 1
        self.line('}' + footer)

    def block_comment(self, *lines):
        self.line('/*')
        for s in lines:
            self.line(' * ' + s)
        self.line(' */')

    def _tabs(self):
        return '  ' * self._indent


def each_index_pair(sequence):
    """Utility method: Generates each unique index pair in sequence."""
    for i in range(0, len(sequence)):
        for j in range(i + 1, len(sequence)):
            yield (i, j)
