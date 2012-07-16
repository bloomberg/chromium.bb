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

""" Handlebar templates are mostly-logicless templates inspired by ctemplate
(or more specifically mustache templates) then taken in their own direction
because I found those to be inadequate.

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
  def __init__(self, error, line):
    Exception.__init__(self, "%s (line %s)" % (error, line.number))

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
    self._length = 0

  def __len__(self):
    return self._length

  def append(self, obj):
    string = str(obj)
    self._buf.append(string)
    self._length += len(string)

  def toString(self):
    return ''.join(self._buf)

class RenderState(object):
  """ The state of a render call.
  """
  def __init__(self, globalContexts, localContexts):
    self.globalContexts = globalContexts
    self.localContexts = localContexts
    self.text = StringBuilder()
    self.errors = []
    self._errorsDisabled = False

  def inSameContext(self):
    return RenderState(self.globalContexts, self.localContexts)

  def getFirstContext(self):
    if len(self.localContexts) > 0:
      return self.localContexts[0]
    if len(self.globalContexts) > 0:
      return self.globalContexts[0]
    return None

  def disableErrors(self):
    self._errorsDisabled = True
    return self

  def addError(self, *messages):
    if self._errorsDisabled:
      return self
    buf = StringBuilder()
    for message in messages:
      buf.append(message)
    self.errors.append(buf.toString())
    return self

  def getResult(self):
    return RenderResult(self.text.toString(), self.errors);

class Identifier(object):
  """ An identifier of the form "@", "foo.bar.baz", or "@.foo.bar.baz".
  """
  def __init__(self, name, line):
    self._isThis = (name == '@')
    if self._isThis:
      self._startsWithThis = False
      self._path = []
      return

    thisDot = '@.'
    self._startsWithThis = name.startswith(thisDot)
    if self._startsWithThis:
      name = name[len(thisDot):]

    if not re.match('^[a-zA-Z0-9._]+$', name):
      raise ParseException(name + " is not a valid identifier", line)
    self._path = name.split('.')

  def resolve(self, renderState):
    if self._isThis:
      return renderState.getFirstContext()

    if self._startsWithThis:
      return self._resolveFromContext(renderState.getFirstContext())

    resolved = self._resolveFromContexts(renderState.localContexts)
    if resolved == None:
      resolved = self._resolveFromContexts(renderState.globalContexts)
    if resolved == None:
      renderState.addError("Couldn't resolve identifier ", self._path)
    return resolved

  def _resolveFromContexts(self, contexts):
    for context in contexts:
      resolved = self._resolveFromContext(context)
      if resolved != None:
        return resolved
    return None

  def _resolveFromContext(self, context):
    result = context
    for next in self._path:
      # Only require that contexts provide a get method, meaning that callers
      # can provide dict-like contexts (for example, to populate values lazily).
      if result == None or not getattr(result, "get", None):
        return None
      result = result.get(next)
    return result

  def __str__(self):
    if self._isThis:
      return '@'
    name = '.'.join(self._path)
    return ('@.' + name) if self._startsWithThis else name

class Line(object):
  def __init__(self, number):
    self.number = number

class LeafNode(object):
  def __init__(self, line):
    self._line = line

  def startsWithNewLine(self):
    return False

  def trimStartingNewLine(self):
    pass

  def trimEndingSpaces(self):
    return 0

  def trimEndingNewLine(self):
    pass

  def endsWithEmptyLine(self):
    return False

  def getStartLine(self):
    return self._line

  def getEndLine(self):
    return self._line

class DecoratorNode(object):
  def __init__(self, content):
    self._content = content

  def startsWithNewLine(self):
    return self._content.startsWithNewLine()

  def trimStartingNewLine(self):
    self._content.trimStartingNewLine()

  def trimEndingSpaces(self):
    return self._content.trimEndingSpaces()

  def trimEndingNewLine(self):
    self._content.trimEndingNewLine()

  def endsWithEmptyLine(self):
    return self._content.endsWithEmptyLine()

  def getStartLine(self):
    return self._content.getStartLine()

  def getEndLine(self):
    return self._content.getEndLine()

class InlineNode(DecoratorNode):
  def __init__(self, content):
    DecoratorNode.__init__(self, content)

  def render(self, renderState):
    contentRenderState = renderState.inSameContext()
    self._content.render(contentRenderState)

    renderState.errors.extend(contentRenderState.errors)

    for c in contentRenderState.text.toString():
      if c != '\n':
        renderState.text.append(c)

class IndentedNode(DecoratorNode):
  def __init__(self, content, indentation):
    DecoratorNode.__init__(self, content)
    self._indentation = indentation

  def render(self, renderState):
    contentRenderState = renderState.inSameContext()
    self._content.render(contentRenderState)

    renderState.errors.extend(contentRenderState.errors)

    self._indent(renderState.text)
    for i, c in enumerate(contentRenderState.text.toString()):
      renderState.text.append(c)
      if c == '\n' and i < len(contentRenderState.text) - 1:
        self._indent(renderState.text)
    renderState.text.append('\n')

  def _indent(self, buf):
    buf.append(' ' * self._indentation)

class BlockNode(DecoratorNode):
  def __init__(self, content):
    DecoratorNode.__init__(self, content)
    content.trimStartingNewLine()
    content.trimEndingSpaces()

  def render(self, renderState):
    self._content.render(renderState)

class NodeCollection(object):
  def __init__(self, nodes):
    if len(nodes) == 0:
      raise ValueError()
    self._nodes = nodes

  def render(self, renderState):
    for node in self._nodes:
      node.render(renderState)

  def startsWithNewLine(self):
    return self._nodes[0].startsWithNewLine()

  def trimStartingNewLine(self):
    self._nodes[0].trimStartingNewLine()

  def trimEndingSpaces(self):
    return self._nodes[-1].trimEndingSpaces()

  def trimEndingNewLine(self):
    self._nodes[-1].trimEndingNewLine()

  def endsWithEmptyLine(self):
    return self._nodes[-1].endsWithEmptyLine()

  def getStartLine(self):
    return self._nodes[0].getStartLine()

  def getEndLine(self):
    return self._nodes[-1].getEndLine()

class StringNode(object):
  """ Just a string.
  """
  def __init__(self, string, startLine, endLine):
    self._string = string
    self._startLine = startLine
    self._endLine = endLine

  def render(self, renderState):
    renderState.text.append(self._string)

  def startsWithNewLine(self):
    return self._string.startswith('\n')

  def trimStartingNewLine(self):
    if self.startsWithNewLine():
      self._string = self._string[1:]

  def trimEndingSpaces(self):
    originalLength = len(self._string)
    self._string = self._string[:self._lastIndexOfSpaces()]
    return originalLength - len(self._string)

  def trimEndingNewLine(self):
    if self._string.endswith('\n'):
      self._string = self._string[:len(self._string) - 1]

  def endsWithEmptyLine(self):
    index = self._lastIndexOfSpaces()
    return index == 0 or self._string[index - 1] == '\n'

  def _lastIndexOfSpaces(self):
    index = len(self._string)
    while index > 0 and self._string[index - 1] == ' ':
      index -= 1
    return index

  def getStartLine(self):
    return self._startLine

  def getEndLine(self):
    return self._endLine

class EscapedVariableNode(LeafNode):
  """ {{foo}}
  """
  def __init__(self, id, line):
    LeafNode.__init__(self, line)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState)
    if value != None:
      self._appendEscapedHtml(renderState.text, str(value))

  def _appendEscapedHtml(self, escaped, unescaped):
    for c in unescaped:
      if c == '<':
        escaped.append("&lt;")
      elif c == '>':
        escaped.append("&gt;")
      elif c == '&':
        escaped.append("&amp;")
      else:
        escaped.append(c)

class UnescapedVariableNode(LeafNode):
  """ {{{foo}}}
  """
  def __init__(self, id, line):
    LeafNode.__init__(self, line)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState)
    if value != None:
      renderState.text.append(value)

class SectionNode(DecoratorNode):
  """ {{#foo}} ... {{/}}
  """
  def __init__(self, id, content):
    DecoratorNode.__init__(self, content)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState)
    if value == None:
      return

    type_ = type(value)
    if type_ == list:
      for item in value:
        renderState.localContexts.insert(0, item)
        self._content.render(renderState)
        renderState.localContexts.pop(0)
    elif type_ == dict:
      renderState.localContexts.insert(0, value)
      self._content.render(renderState)
      renderState.localContexts.pop(0)
    else:
      renderState.addError("{{#", self._id,
                           "}} cannot be rendered with a ", type_)

class VertedSectionNode(DecoratorNode):
  """ {{?foo}} ... {{/}}
  """
  def __init__(self, id, content):
    DecoratorNode.__init__(self, content)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState.inSameContext().disableErrors())
    if _VertedSectionNodeShouldRender(value):
      renderState.localContexts.insert(0, value)
      self._content.render(renderState)
      renderState.localContexts.pop(0)

def _VertedSectionNodeShouldRender(value):
  if value == None:
    return False
  type_ = type(value)
  if type_ == bool:
    return value
  if type_ == int or type_ == float:
    return True
  if type_ == str or type_ == unicode:
    return True
  if type_ == list:
    return len(value) > 0
  if type_ == dict:
    return True
  raise TypeError("Unhandled type: " + str(type_))

class InvertedSectionNode(DecoratorNode):
  """ {{^foo}} ... {{/}}
  """
  def __init__(self, id, content):
    DecoratorNode.__init__(self, content)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState.inSameContext().disableErrors())
    if not _VertedSectionNodeShouldRender(value):
      self._content.render(renderState)

class JsonNode(LeafNode):
  """ {{*foo}}
  """
  def __init__(self, id, line):
    LeafNode.__init__(self, line)
    self._id = id

  def render(self, renderState):
    value = self._id.resolve(renderState)
    if value != None:
      renderState.text.append(json.dumps(value, separators=(',',':')))

class PartialNode(LeafNode):
  """ {{+foo}}
  """
  def __init__(self, id, line):
    LeafNode.__init__(self, line)
    self._id = id
    self._args = None

  def render(self, renderState):
    value = self._id.resolve(renderState)
    if not isinstance(value, Handlebar):
      renderState.addError(self._id, " didn't resolve to a Handlebar")
      return

    argContext = []
    if len(renderState.localContexts) > 0:
      argContext.append(renderState.localContexts[0])

    if self._args:
      argContextMap = {}
      for key, valueId in self._args.items():
        context = valueId.resolve(renderState)
        if context:
          argContextMap[key] = context
      argContext.append(argContextMap)

    partialRenderState = RenderState(renderState.globalContexts, argContext)
    value._topNode.render(partialRenderState)

    text = partialRenderState.text.toString()
    if len(text) > 0 and text[-1] == '\n':
      text = text[:-1]

    renderState.text.append(text)
    renderState.errors.extend(partialRenderState.errors)

  def addArgument(self, key, valueId):
    if not self._args:
      self._args = {}
    self._args[key] = valueId

# List of tokens in order of longest to shortest, to avoid any prefix matching
# issues.
TokenValues = []

class Token(object):
  """ The tokens that can appear in a template.
  """
  class Data(object):
    def __init__(self, name, text, clazz):
      self.name = name
      self.text = text
      self.clazz = clazz
      TokenValues.append(self)

    def elseNodeClass(self):
      if self.clazz == VertedSectionNode:
        return InvertedSectionNode
      if self.clazz == InvertedSectionNode:
        return VertedSectionNode
      raise ValueError(self.clazz + " can not have an else clause.")

  OPEN_START_SECTION          = Data("OPEN_START_SECTION"         , "{{#", SectionNode)
  OPEN_START_VERTED_SECTION   = Data("OPEN_START_VERTED_SECTION"  , "{{?", VertedSectionNode)
  OPEN_START_INVERTED_SECTION = Data("OPEN_START_INVERTED_SECTION", "{{^", InvertedSectionNode)
  OPEN_START_JSON             = Data("OPEN_START_JSON"            , "{{*", JsonNode)
  OPEN_START_PARTIAL          = Data("OPEN_START_PARTIAL"         , "{{+", PartialNode)
  OPEN_ELSE                   = Data("OPEN_ELSE"                  , "{{:", None)
  OPEN_END_SECTION            = Data("OPEN_END_SECTION"           , "{{/", None)
  OPEN_UNESCAPED_VARIABLE     = Data("OPEN_UNESCAPED_VARIABLE"    , "{{{", UnescapedVariableNode)
  CLOSE_MUSTACHE3             = Data("CLOSE_MUSTACHE3"            , "}}}", None)
  OPEN_COMMENT                = Data("OPEN_COMMENT"               , "{{-", None)
  CLOSE_COMMENT               = Data("CLOSE_COMMENT"              , "-}}", None)
  OPEN_VARIABLE               = Data("OPEN_VARIABLE"              , "{{" , EscapedVariableNode)
  CLOSE_MUSTACHE              = Data("CLOSE_MUSTACHE"             , "}}" , None)
  CHARACTER                   = Data("CHARACTER"                  , "."  , None)

class TokenStream(object):
  """ Tokeniser for template parsing.
  """
  def __init__(self, string):
    self._remainder = string

    self.nextToken = None
    self.nextContents = None
    self.nextLine = Line(1)
    self.advance()

  def hasNext(self):
    return self.nextToken != None

  def advance(self):
    if self.nextContents == '\n':
      self.nextLine = Line(self.nextLine.number + 1)

    self.nextToken = None
    self.nextContents = None

    if self._remainder == '':
      return None

    for token in TokenValues:
      if self._remainder.startswith(token.text):
        self.nextToken = token
        break

    if not self.nextToken:
      self.nextToken = Token.CHARACTER

    self.nextContents = self._remainder[0:len(self.nextToken.text)]
    self._remainder = self._remainder[len(self.nextToken.text):]
    return self

  def advanceOver(self, token):
    if self.nextToken != token:
      raise ParseException(
          "Expecting token " + token.name + " but got " + self.nextToken.name,
          self.nextLine)
    return self.advance()

  def advanceOverNextString(self, excluded=''):
    buf = StringBuilder()
    while self.nextToken == Token.CHARACTER and \
          excluded.find(self.nextContents) == -1:
      buf.append(self.nextContents)
      self.advance()
    return buf.toString()

class Handlebar(object):
  """ A handlebar template.
  """
  def __init__(self, template):
    self.source = template
    tokens = TokenStream(template)
    self._topNode = self._parseSection(tokens)
    if not self._topNode:
      raise ParseException("Template is empty", tokens.nextLine)
    if tokens.hasNext():
      raise ParseException("There are still tokens remaining, "
                           "was there an end-section without a start-section:",
                           tokens.nextLine)

  def _parseSection(self, tokens):
    nodes = []
    sectionEnded = False

    while tokens.hasNext() and not sectionEnded:
      token = tokens.nextToken

      if token == Token.CHARACTER:
        startLine = tokens.nextLine
        string = tokens.advanceOverNextString()
        nodes.append(StringNode(string, startLine, tokens.nextLine))
      elif token == Token.OPEN_VARIABLE or \
           token == Token.OPEN_UNESCAPED_VARIABLE or \
           token == Token.OPEN_START_JSON:
        id = self._openSectionOrTag(tokens)
        nodes.append(token.clazz(id, tokens.nextLine))
      elif token == Token.OPEN_START_PARTIAL:
        tokens.advance()
        id = Identifier(tokens.advanceOverNextString(excluded=' '),
                        tokens.nextLine)
        partialNode = PartialNode(id, tokens.nextLine)

        while tokens.nextToken == Token.CHARACTER:
          tokens.advance()
          key = tokens.advanceOverNextString(excluded=':')
          tokens.advance()
          partialNode.addArgument(
              key,
              Identifier(tokens.advanceOverNextString(excluded=' '),
                         tokens.nextLine))

        tokens.advanceOver(Token.CLOSE_MUSTACHE)
        nodes.append(partialNode)
      elif token == Token.OPEN_START_SECTION:
        id = self._openSectionOrTag(tokens)
        section = self._parseSection(tokens)
        self._closeSection(tokens, id)
        if section:
          nodes.append(SectionNode(id, section))
      elif token == Token.OPEN_START_VERTED_SECTION or \
           token == Token.OPEN_START_INVERTED_SECTION:
        id = self._openSectionOrTag(tokens)
        section = self._parseSection(tokens)
        elseSection = None
        if tokens.nextToken == Token.OPEN_ELSE:
          self._openElse(tokens, id)
          elseSection = self._parseSection(tokens)
        self._closeSection(tokens, id)
        if section:
          nodes.append(token.clazz(id, section))
        if elseSection:
          nodes.append(token.elseNodeClass()(id, elseSection))
      elif token == Token.OPEN_COMMENT:
        self._advanceOverComment(tokens)
      elif token == Token.OPEN_END_SECTION or \
           token == Token.OPEN_ELSE:
        # Handled after running parseSection within the SECTION cases, so this is a
        # terminating condition. If there *is* an orphaned OPEN_END_SECTION, it will be caught
        # by noticing that there are leftover tokens after termination.
        sectionEnded = True
      elif Token.CLOSE_MUSTACHE:
        raise ParseException("Orphaned " + tokens.nextToken.name,
                             tokens.nextLine)

    for i, node in enumerate(nodes):
      if isinstance(node, StringNode):
        continue

      previousNode = nodes[i - 1] if i > 0 else None
      nextNode = nodes[i + 1] if i < len(nodes) - 1 else None
      renderedNode = None

      if node.getStartLine() != node.getEndLine():
        renderedNode = BlockNode(node)
        if previousNode:
          previousNode.trimEndingSpaces()
        if nextNode:
          nextNode.trimStartingNewLine()
      elif isinstance(node, LeafNode) and \
           (not previousNode or previousNode.endsWithEmptyLine()) and \
           (not nextNode or nextNode.startsWithNewLine()):
        indentation = 0
        if previousNode:
          indentation = previousNode.trimEndingSpaces()
        if nextNode:
          nextNode.trimStartingNewLine()
        renderedNode = IndentedNode(node, indentation)
      else:
        renderedNode = InlineNode(node)

      nodes[i] = renderedNode

    if len(nodes) == 0:
      return None
    if len(nodes) == 1:
      return nodes[0]
    return NodeCollection(nodes)

  def _advanceOverComment(self, tokens):
    tokens.advanceOver(Token.OPEN_COMMENT)
    depth = 1
    while tokens.hasNext() and depth > 0:
      if tokens.nextToken == Token.OPEN_COMMENT:
        depth += 1
      elif tokens.nextToken == Token.CLOSE_COMMENT:
        depth -= 1
      tokens.advance()

  def _openSectionOrTag(self, tokens):
    openToken = tokens.nextToken
    tokens.advance()
    id = Identifier(tokens.advanceOverNextString(), tokens.nextLine)
    if openToken == Token.OPEN_UNESCAPED_VARIABLE:
      tokens.advanceOver(Token.CLOSE_MUSTACHE3)
    else:
      tokens.advanceOver(Token.CLOSE_MUSTACHE)
    return id

  def _closeSection(self, tokens, id):
    tokens.advanceOver(Token.OPEN_END_SECTION)
    nextString = tokens.advanceOverNextString()
    if nextString != '' and nextString != str(id):
      raise ParseException(
          "Start section " + str(id) + " doesn't match end " + nextString)
    tokens.advanceOver(Token.CLOSE_MUSTACHE)

  def _openElse(self, tokens, id):
    tokens.advanceOver(Token.OPEN_ELSE)
    nextString = tokens.advanceOverNextString()
    if nextString != '' and nextString != str(id):
      raise ParseException(
          "Start section " + str(id) + " doesn't match else " + nextString)
    tokens.advanceOver(Token.CLOSE_MUSTACHE)

  def render(self, *contexts):
    """ Renders this template given a variable number of "contexts" to read
    out values from (such as those appearing in {{foo}}).
    """
    globalContexts = []
    for context in contexts:
      globalContexts.append(context)
    renderState = RenderState(globalContexts, [])
    self._topNode.render(renderState)
    return renderState.getResult()
