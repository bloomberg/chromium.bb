# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic for parsing a C/C++ function signature."""


def _FindParameterListParen(name):
  """Finds index of the "(" that denotes the start of a paremeter list."""
  # This loops from left-to-right, but the only reason (I think) that this
  # is necessary (rather than reusing _FindLastCharOutsideOfBrackets), is
  # to capture the outer-most function in the case where classes are nested.
  start_idx = 0
  while True:
    template_balance_count = 0
    paren_balance_count = 0
    while True:
      idx = name.find('(', start_idx)
      if idx == -1:
        return -1
      template_balance_count += (
          name.count('<', start_idx, idx) - name.count('>', start_idx, idx))
      # Special: operators with angle brackets.
      operator_idx = name.find('operator<', start_idx, idx)
      if operator_idx != -1:
        if name[operator_idx + 9] == '<':
          template_balance_count -= 2
        else:
          template_balance_count -= 1
      else:
        operator_idx = name.find('operator>', start_idx, idx)
        if operator_idx != -1:
          if name[operator_idx + 9] == '>':
            template_balance_count += 2
          else:
            template_balance_count += 1

      paren_balance_count += (
          name.count('(', start_idx, idx) - name.count(')', start_idx, idx))
      if template_balance_count == 0 and paren_balance_count == 0:
        # Special case: skip "(anonymous namespace)".
        if -1 != name.find('(anonymous namespace)', idx, idx + 21):
          start_idx = idx + 21
          continue
        # Special case: skip "decltype (...)"
        # Special case: skip "{lambda(PaintOp*)#63}"
        if name[idx - 1] != ' ' and name[idx - 7:idx] != '{lambda':
          return idx
      start_idx = idx + 1
      paren_balance_count += 1


def _FindLastCharOutsideOfBrackets(name, target_char, prev_idx=None):
  """Returns the last index of |target_char| that is not within ()s nor <>s."""
  paren_balance_count = 0
  template_balance_count = 0
  while True:
    idx = name.rfind(target_char, 0, prev_idx)
    if idx == -1:
      return -1
    # It is much faster to use.find() and.count() than to loop over each
    # character.
    template_balance_count += (
        name.count('<', idx, prev_idx) - name.count('>', idx, prev_idx))
    paren_balance_count += (
        name.count('(', idx, prev_idx) - name.count(')', idx, prev_idx))
    if template_balance_count == 0 and paren_balance_count == 0:
      return idx
    prev_idx = idx


def _FindReturnValueSpace(name, paren_idx):
  """Returns the index of the space that comes after the return type."""
  space_idx = paren_idx
  # Special case: const cast operators (see tests).
  if -1 != name.find(' const', paren_idx - 6, paren_idx):
    space_idx = paren_idx - 6
  while True:
    space_idx = _FindLastCharOutsideOfBrackets(name, ' ', space_idx)
    # Special case: "operator new", and "operator<< <template>".
    if -1 == space_idx or (
        -1 == name.find('operator', space_idx - 8, space_idx) and
        -1 == name.find('operator<<', space_idx - 10, space_idx)):
      break
    space_idx -= 8
  return space_idx


def _StripTemplateArgs(name):
  last_right_idx = None
  while True:
    last_right_idx = name.rfind('>', 0, last_right_idx)
    if last_right_idx == -1:
      return name
    left_idx = _FindLastCharOutsideOfBrackets(name, '<', last_right_idx + 1)
    if left_idx == -1:
      return name
    # Special case: std::operator<< <
    if left_idx > 0 and name[left_idx - 1] == ' ':
      left_idx -= 1
    name = name[:left_idx] + name[last_right_idx + 1:]
    last_right_idx = left_idx


def _NormalizeTopLevelGccLambda(name, left_paren_idx):
  # cc::{lambda(PaintOp*)#63}::_FUN() -> cc::$lambda#63()
  left_brace_idx = name.index('{')
  hash_idx = name.index('#', left_brace_idx + 1)
  right_brace_idx = name.index('}', hash_idx + 1)
  number = name[hash_idx + 1:right_brace_idx]
  return '{}$lambda#{}{}'.format(
      name[:left_brace_idx], number, name[left_paren_idx:])


def _NormalizeTopLevelClangLambda(name, left_paren_idx):
  # cc::$_21::__invoke() -> cc::$lambda#21()
  dollar_idx = name.index('$')
  colon_idx = name.index(':', dollar_idx + 1)
  number = name[dollar_idx + 2:colon_idx]
  return '{}$lambda#{}{}'.format(
      name[:dollar_idx], number, name[left_paren_idx:])


def Parse(name):
  """Strips return type and breaks function signature into parts.

  See unit tests for example signatures.

  Returns:
    A tuple of:
    * name without return type (symbol.full_name),
    * full_name without params (symbol.template_name),
    * full_name without params and template args (symbol.name)
  """
  left_paren_idx = _FindParameterListParen(name)

  full_name = name
  if left_paren_idx > 0:
    right_paren_idx = name.rindex(')')
    assert right_paren_idx > left_paren_idx
    space_idx = _FindReturnValueSpace(name, left_paren_idx)
    name_no_params = name[space_idx + 1:left_paren_idx]
    # Special case for top-level lamdas.
    if name_no_params.endswith('}::_FUN'):
      # Don't use name_no_params in here since prior _idx will be off if
      # there was a return value.
      name = _NormalizeTopLevelGccLambda(name, left_paren_idx)
      return Parse(name)
    elif name_no_params.endswith('::__invoke') and '$' in name_no_params:
      assert '$_' in name_no_params, 'Surprising lambda: ' + name
      name = _NormalizeTopLevelClangLambda(name, left_paren_idx)
      return Parse(name)

    full_name = name[space_idx + 1:]
    name = name_no_params + name[right_paren_idx + 1:]

  template_name = name
  name = _StripTemplateArgs(name)
  return full_name, template_name, name


# An odd place for this, but pylint doesn't want it as a static in models
# (circular dependency), nor as an method on BaseSymbol
# (attribute-defined-outside-init).
def InternSameNames(symbol):
  """Allow using "is" to compare names (and should help with RAM)."""
  if symbol.template_name == symbol.full_name:
    symbol.template_name = symbol.full_name
  if symbol.name == symbol.template_name:
    symbol.name = symbol.template_name
