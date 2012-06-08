# Copyright 2012 Benjamin Kalman
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import re

""" Handlebar templates are logicless templates inspired by ctemplate (or more
specifically mustache templates) then taken in their own direction because I
found those to be inadequate.

from handlebar import Handlebar

template = Handlebar('hello {{#foo}}{{bar}}{{/}} world')
input = {
  'foo': [
    { 'bar': 1 },
    { 'bar': 2 },
    { 'bar': 3 }
  ]
}
print(template.render(input).text)

Handlebar will use get() on contexts to return values, so to create custom
getters (e.g. something that populates values lazily from keys) just add
a get() method.

class CustomContext(object):
  def get(self, key):
    return 10

# Any time {{ }} is used, will fill it with 10.
print(Handlebar('hello {{world}}').render(CustomContext()).text)
"""

class ParseException(Exception):
  """ Exception thrown while parsing the template.
  """
  def __init__(self, message):
    Exception.__init__(self, message)

class RenderResult(object):
  """ Result of a render operation.
  """
  def __init__(self, text, errors):
    self.text = text;
    self.errors = errors

class StringBuilder(object):
  """ Mimics Java's StringBuilder for easy porting from the Java version of
  this file to Python.
  """
  def __init__(self):
    self._buf = []

  def append(self, obj):
    self._buf.append(str(obj))

  def toString(self):
    return ''.join(self._buf)

class PathIdentifier(object):
  """ An identifier of the form "foo.bar.baz".
  """
  def __init__(self, name):
    if name == '':
      raise ParseException("Cannot have empty identifiers")
    if not re.match('^[a-zA-Z0-9._]*$', name):
      raise ParseException(name + " is not a valid identifier")
    self.path = name.split(".")

  def resolve(self, contexts, errors):
    resolved = None
    for context in contexts:
      if not context:
        continue
      resolved = self._resolveFrom(context)
      if resolved:
        return resolved
    _RenderError(errors, "Couldn't resolve identifier ", self.path, " in ", contexts)
    return None

  def _resolveFrom(self, context):
    result = context
    for next in self.path:
      # Only require that contexts provide a get method, meaning that callers
      # can provide dict-like contexts (for example, to populate values lazily).
      if not result or not getattr(result, "get", None):
        return None
      result = result.get(next)
    return result

  def __str__(self):
    return '.'.join(self.path)

class ThisIdentifier(object):
  """ An identifier of the form "@".
  """
  def resolve(self, contexts, errors):
    return contexts[0]

  def __str__(self):
    return '@'

class SelfClosingNode(object):
  """ Nodes which are "self closing", e.g. {{foo}}, {{*foo}}.
  """
  def init(self, id):
    self.id = id

class HasChildrenNode(object):
  """ Nodes which are not self closing, and have 0..n children.
  """
  def init(self, id, children):
    self.id = id
    self.children = children

class StringNode(object):
  """ Just a string.
  """
  def __init__(self, string):
    self.string = string

  def render(self, buf, contexts, errors):
    buf.append(self.string)

class EscapedVariableNode(SelfClosingNode):
  """ {{foo}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if value:
      buf.append(self._htmlEscape(str(value)))

  def _htmlEscape(self, unescaped):
    escaped = StringBuilder()
    for c in unescaped:
      if c == '<':
        escaped.append("&lt;")
      elif c == '>':
        escaped.append("&gt;")
      elif c == '&':
        escaped.append("&amp;")
      else:
        escaped.append(c)
    return escaped.toString()

class UnescapedVariableNode(SelfClosingNode):
  """ {{{foo}}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if value:
      buf.append(value)

class SectionNode(HasChildrenNode):
  """ {{#foo}} ... {{/}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if not value:
      return

    type_ = type(value)
    if value == None:
      pass
    elif type_ == list:
      for item in value:
        contexts.insert(0, item)
        _RenderNodes(buf, self.children, contexts, errors)
        contexts.pop(0)
    elif type_ == dict:
      contexts.insert(0, value)
      _RenderNodes(buf, self.children, contexts, errors)
      contexts.pop(0)
    else:
      _RenderError(errors, "{{#", self.id, "}} cannot be rendered with a ", type_)

class VertedSectionNode(HasChildrenNode):
  """ {{?foo}} ... {{/}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if value and _VertedSectionNodeShouldRender(value):
      contexts.insert(0, value)
      _RenderNodes(buf, self.children, contexts, errors)
      contexts.pop(0)

def _VertedSectionNodeShouldRender(value):
  type_ = type(value)
  if value == None:
    return False
  elif type_ == bool:
    return value
  elif type_ == int or type_ == float:
    return value > 0
  elif type_ == str or type_ == unicode:
    return value != ''
  elif type_ == list or type_ == dict:
    return len(value) > 0
  raise TypeError("Unhandled type: " + str(type_))

class InvertedSectionNode(HasChildrenNode):
  """ {{^foo}} ... {{/}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if not value or not _VertedSectionNodeShouldRender(value):
      _RenderNodes(buf, self.children, contexts, errors)

class JsonNode(SelfClosingNode):
  """ {{*foo}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if value:
      buf.append(json.dumps(value, separators=(',',':')))

class PartialNode(SelfClosingNode):
  """ {{+foo}}
  """
  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if not isinstance(value, Handlebar):
      _RenderError(errors, id, " didn't resolve to a Handlebar")
      return
    _RenderNodes(buf, value.nodes, contexts, errors)

class SwitchNode(object):
  """ {{:foo}}
  """
  def __init__(self, id):
    self.id = id
    self._cases = {}

  def addCase(self, caseValue, caseNode):
    self._cases[caseValue] = caseNode

  def render(self, buf, contexts, errors):
    value = self.id.resolve(contexts, errors)
    if not value:
      _RenderError(errors, id, " didn't resolve to any value")
      return
    if not (type(value) == str or type(value) == unicode):
      _RenderError(errors, id, " didn't resolve to a String")
      return
    caseNode = self._cases.get(value)
    if caseNode:
      caseNode.render(buf, contexts, errors)

class CaseNode(object):
  """ {{=foo}}
  """
  def __init__(self, children):
    self.children = children

  def render(self, buf, contexts, errors):
    for child in self.children:
      child.render(buf, contexts, errors)

class Token(object):
  """ The tokens that can appear in a template.
  """
  class Data(object):
    def __init__(self, name, text, clazz):
      self.name = name
      self.text = text
      self.clazz = clazz

  OPEN_START_SECTION          = Data("OPEN_START_SECTION"         , "{{#", SectionNode)
  OPEN_START_VERTED_SECTION   = Data("OPEN_START_VERTED_SECTION"  , "{{?", VertedSectionNode)
  OPEN_START_INVERTED_SECTION = Data("OPEN_START_INVERTED_SECTION", "{{^", InvertedSectionNode)
  OPEN_START_JSON             = Data("OPEN_START_JSON"            , "{{*", JsonNode)
  OPEN_START_PARTIAL          = Data("OPEN_START_PARTIAL"         , "{{+", PartialNode)
  OPEN_START_SWITCH           = Data("OPEN_START_SWITCH"          , "{{:", SwitchNode)
  OPEN_CASE                   = Data("OPEN_CASE"                  , "{{=", CaseNode)
  OPEN_END_SECTION            = Data("OPEN_END_SECTION"           , "{{/", None)
  OPEN_UNESCAPED_VARIABLE     = Data("OPEN_UNESCAPED_VARIABLE"    , "{{{", UnescapedVariableNode)
  CLOSE_MUSTACHE3             = Data("CLOSE_MUSTACHE3"            , "}}}", None)
  OPEN_VARIABLE               = Data("OPEN_VARIABLE"              , "{{" , EscapedVariableNode)
  CLOSE_MUSTACHE              = Data("CLOSE_MUSTACHE"             , "}}" , None)
  CHARACTER                   = Data("CHARACTER"                  , "."  , StringNode)

# List of tokens in order of longest to shortest, to avoid any prefix matching
# issues.
_tokenList = [
  Token.OPEN_START_SECTION,
  Token.OPEN_START_VERTED_SECTION,
  Token.OPEN_START_INVERTED_SECTION,
  Token.OPEN_START_JSON,
  Token.OPEN_START_PARTIAL,
  Token.OPEN_START_SWITCH,
  Token.OPEN_CASE,
  Token.OPEN_END_SECTION,
  Token.OPEN_UNESCAPED_VARIABLE,
  Token.CLOSE_MUSTACHE3,
  Token.OPEN_VARIABLE,
  Token.CLOSE_MUSTACHE,
  Token.CHARACTER
]

class TokenStream(object):
  """ Tokeniser for template parsing.
  """
  def __init__(self, string):
    self.nextToken = None
    self._remainder = string
    self._nextContents = None
    self.advance()

  def hasNext(self):
    return self.nextToken != None

  def advanceOver(self, token):
    if self.nextToken != token:
      raise ParseException(
          "Expecting token " + token.name + " but got " + self.nextToken.name)
    return self.advance()

  def advance(self):
    self.nextToken = None
    self._nextContents = None

    if self._remainder == '':
      return None

    for token in _tokenList:
      if self._remainder.startswith(token.text):
        self.nextToken = token
        break

    if self.nextToken == None:
      self.nextToken = Token.CHARACTER

    self._nextContents = self._remainder[0:len(self.nextToken.text)]
    self._remainder = self._remainder[len(self.nextToken.text):]
    return self.nextToken

  def nextString(self):
    buf = StringBuilder()
    while self.nextToken == Token.CHARACTER:
      buf.append(self._nextContents)
      self.advance()
    return buf.toString()

def _CreateIdentifier(path):
  if path == '@':
    return ThisIdentifier()
  else:
    return PathIdentifier(path)

class Handlebar(object):
  """ A handlebar template.
  """
  def __init__(self, template):
    self.nodes = []
    self._parseTemplate(template, self.nodes)

  def _parseTemplate(self, template, nodes):
    tokens = TokenStream(template)
    self._parseSection(tokens, nodes)
    if tokens.hasNext():
      raise ParseException("There are still tokens remaining, "
                           "was there an end-section without a start-section:")

  def _parseSection(self, tokens, nodes):
    sectionEnded = False
    while tokens.hasNext() and not sectionEnded:
      next = tokens.nextToken
      if next == Token.CHARACTER:
        nodes.append(StringNode(tokens.nextString()))
      elif next == Token.OPEN_VARIABLE or \
           next == Token.OPEN_UNESCAPED_VARIABLE or \
           next == Token.OPEN_START_JSON or \
           next == Token.OPEN_START_PARTIAL:
        token = tokens.nextToken
        id = self._openSection(tokens)
        node = token.clazz()
        node.init(id)
        nodes.append(node)
      elif next == Token.OPEN_START_SECTION or \
           next == Token.OPEN_START_VERTED_SECTION or \
           next == Token.OPEN_START_INVERTED_SECTION:
        token = tokens.nextToken
        id = self._openSection(tokens)

        children = []
        self._parseSection(tokens, children)
        self._closeSection(tokens, id)

        node = token.clazz()
        node.init(id, children)
        nodes.append(node)
      elif next == Token.OPEN_START_SWITCH:
        id = self._openSection(tokens)
        while tokens.nextToken == Token.CHARACTER:
          tokens.advanceOver(Token.CHARACTER)

        switchNode = SwitchNode(id)
        nodes.append(switchNode)

        while tokens.hasNext() and tokens.nextToken == Token.OPEN_CASE:
          tokens.advanceOver(Token.OPEN_CASE)
          caseValue = tokens.nextString()
          tokens.advanceOver(Token.CLOSE_MUSTACHE)

          caseChildren = []
          self._parseSection(tokens, caseChildren)

          switchNode.addCase(caseValue, CaseNode(caseChildren))

        self._closeSection(tokens, id)
      elif next == Token.OPEN_CASE:
        # See below.
        sectionEnded = True
      elif next == Token.OPEN_END_SECTION:
        # Handled after running parseSection within the SECTION cases, so this is a
        # terminating condition. If there *is* an orphaned OPEN_END_SECTION, it will be caught
        # by noticing that there are leftover tokens after termination.
        sectionEnded = True
      elif Token.CLOSE_MUSTACHE:
        raise ParseException("Orphaned " + tokens.nextToken.name)

  def _openSection(self, tokens):
    openToken = tokens.nextToken
    tokens.advance()
    id = _CreateIdentifier(tokens.nextString())
    if openToken == Token.OPEN_UNESCAPED_VARIABLE:
      tokens.advanceOver(Token.CLOSE_MUSTACHE3)
    else:
      tokens.advanceOver(Token.CLOSE_MUSTACHE)
    return id

  def _closeSection(self, tokens, id):
    tokens.advanceOver(Token.OPEN_END_SECTION)
    nextString = tokens.nextString()
    if nextString != '' and str(id) != nextString:
      raise ParseException(
          "Start section " + str(id) + " doesn't match end section " + nextString)
    tokens.advanceOver(Token.CLOSE_MUSTACHE)

  def render(self, *contexts):
    """ Renders this template given a variable number of "contexts" to read
    out values from (such as those appearing in {{foo}}).
    """
    contextDeque = []
    for context in contexts:
      contextDeque.append(context)
    buf = StringBuilder()
    errors = []
    _RenderNodes(buf, self.nodes, contextDeque, errors)
    return RenderResult(buf.toString(), errors)

def _RenderNodes(buf, nodes, contexts, errors):
  for node in nodes:
    node.render(buf, contexts, errors)
  
def _RenderError(errors, *messages):
  if not errors:
    return
  buf = StringBuilder()
  for message in messages:
    buf.append(message)
  errors.append(buf.toString())
