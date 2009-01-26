#!/usr/bin/python

import hashlib
import string
import sys


def StringContainsOnly(s, chars):
  for c in s:
    if chars.find(c) == -1:
      return False
  return True


class XCObject(object):
  _schema = {}
  _should_print_single_line = False
  _encode_transforms = []
  i = 0
  while i < ord(" "):
    _encode_transforms.append("\\U%04x" % i)
    i = i + 1
  _encode_transforms[7] = "\\a"
  _encode_transforms[8] = "\\b"
  _encode_transforms[9] = "\\t"
  _encode_transforms[10] = "\\n"
  _encode_transforms[11] = "\\v"
  _encode_transforms[12] = "\\f"
  _encode_transforms[13] = "\\n"

  def __init__(self, id=None, properties=None):
    self.id = id
    self.parent = None
    self._properties = {}
    self._SetDefaultsFromSchema()
    self.UpdateProperties(properties)

  def Name(self):
    # Note: not all objects need to be nameable, and not all that do have a
    # "name" property.  Override as needed.
    if "name" in self._properties:
      return self._properties["name"]
    return None

  def Comment(self):
    raise NotImplementedError, \
          self.__class__.__name__ + " must implement Comment"

  def ComputeIDs(self, recursive=True, overwrite=True, hash=hashlib.sha1()):
    def HashUpdate(hash, data):
      data_length = len(data)
      byte = (data_length & 0xff000000) >> 24
      hash.update(chr(byte))
      byte = (data_length & 0x00ff0000) >> 16
      hash.update(chr(byte))
      byte = (data_length & 0x0000ff00) >> 8
      hash.update(chr(byte))
      byte = data_length & 0x000000ff
      hash.update(chr(byte))
      hash.update(data)
      

    HashUpdate(hash, self.__class__.__name__)
    comment = self.Comment()
    if comment != None:
      # TODO(mark): Improve?  Or maybe this is enough...
      HashUpdate(hash, comment)

    if recursive:
      for child in self.Children():
        child.ComputeIDs(recursive, overwrite, hash.copy())

    if overwrite or self.id == None:
      # TODO(mark): Xcode IDs are only 96 bits (24 hex characters), but a SHA-1
      # digest is 160 bits.  Instead of throwing out 64 bits of the digest, xor
      # them into the portion that gets used.  As-is, this should be fine,
      # because hashes are awesome and we're only after uniqueness here, not
      # security, but I hate computing perfectly good bits just to throw them
      # out.
      self.id = hash.hexdigest()[0:24].upper()

  def Children(self):
    children = []
    for property, attributes in self._schema.iteritems():
      (is_list, property_type, is_strong) = attributes[0:3]
      if is_strong and property in self._properties:
        if not is_list:
          children.append(self._properties[property])
        else:
          children.extend(self._properties[property])
    return children

  def Descendants(self):
    children = self.Children()
    descendants = [self]
    for child in children:
      descendants.extend(child.Descendants())
    return descendants

  def _EncodeComment(self, comment):
    # Mimic Xcode behavior.  Wrap the comment in "/*" and "*/", but if the
    # string already contains a "*/", turn it into "(*)/".  This keeps the file
    # writer from outputting something that would be treated as the end of a
    # comment in the middle of something intended to be entirely a comment.
    return "/* " + comment.replace("*/", "(*)/") + " */"

  def _EncodeString(self, value):
    # Use quotation marks when any character outside of the range A-Z, a-z, 0-9,
    # $ (dollar sign), . (period), and _ (underscore) is present.  Also use
    # quotation marks to represent empty strings.
    # Escape " (double-quote) and \ (backslash) by preceding them with a
    # backslash.
    # Some characters below the printable ASCII range are encoded specially:
    #     7 ^G BEL is encoded as "\a"
    #     8 ^H BS  is encoded as "\b"
    #    11 ^K VT  is encoded as "\v"
    #    12 ^L NP  is encoded as "\f"
    #   127 ^? DEL is passed through as-is without escaping
    #  - In PBXFileReference and PBXBuildFile objects:
    #     9 ^I HT  is passed through as-is without escaping
    #    10 ^J NL  is passed through as-is without escaping
    #    13 ^M CR  is passed through as-is without escaping
    #  - In other objects:
    #     9 ^I HT  is encoded as "\t"
    #    10 ^J NL  is encoded as "\n"
    #    13 ^M CR  is encoded as "\n rendering it indistinguishable from
    #              10 ^J NL
    # All other nonprintable characters within the ASCII range (0 through 127
    # inclusive) are encoded as "\U001f" referring to the Unicode code point in
    # hexadecimal.  For example, character 14 (^N SO) is encoded as "\U000e".
    # Characters above the ASCII range are passed through to the output encoded
    # as UTF-8 without any escaping.

    if value != "" and StringContainsOnly(value,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$._"):
      return value

    encoded = value.replace('\\', '\\\\')
    encoded = encoded.replace('"', '\\"')
    i = 0
    while i < len(self._encode_transforms):
      encoded = encoded.replace(chr(i), self._encode_transforms[i])
      i = i + 1

    return '"' + encoded + '"'

  def _XCPrint(self, file, tabs, line):
    # print >> file, "\t" * tabs + line,
    file.write("\t" * tabs + line)

  def _XCPrintableValue(self, tabs, value):
    printable = ""
    comment = None

    if self._should_print_single_line:
      sep = " "
      element_tabs = ""
      end_tabs = ""
    else:
      sep = "\n"
      element_tabs = "\t" * (tabs + 1)
      end_tabs = "\t" * tabs

    if isinstance(value, XCObject):
      printable += value.id
      comment = value.Comment()
    elif isinstance(value, str):
      printable += self._EncodeString(value)
    elif isinstance(value, int):
      printable += str(value)
    elif isinstance(value, list):
      printable = "(" + sep
      for item in value:
        printable += element_tabs + \
                     self._XCPrintableValue(tabs, item) + "," + sep
      printable += end_tabs + ")"
    elif isinstance(value, dict):
      printable = "{" + sep
      for item_key, item_value in sorted(value.iteritems()):
        printable += element_tabs + \
                     self._XCPrintableValue(tabs, item_key) + " = " + \
                     self._XCPrintableValue(tabs, item_value) + ";" + sep
      printable += end_tabs + "}"
    else:
      raise TypeError, "Can't make " + value.__class__.__name__ + " printable"

    if comment != None:
      printable += " " + self._EncodeComment(comment)

    return printable

  def _XCKVPrint(self, file, tabs, key, value):
    printable = ""

    if not self._should_print_single_line:
      printable = "\t" * tabs

    printable += self._XCPrintableValue(tabs, key) + " = " + \
                 self._XCPrintableValue(tabs, value) + ";"
    if self._should_print_single_line:
      printable += " "
    else:
      printable += "\n"

    self._XCPrint(file, 0, printable)

  def Print(self, file=sys.stdout):
    self.VerifyHasRequiredProperties()

    if self._should_print_single_line:
      # When printing an object in a single line, Xcode doesn't put any space
      # between the beginning of a dictionary (or presumably a list) and the
      # first contained item, so you wind up with snippets like
      #   ...CDEF = {isa = PBXFileReference; fileRef = 0123...
      # If it were me, I would have put a space in there after the opening
      # curly, but I guess this is just another one of those inconsistencies
      # between how Xcode prints PBXFileReference and PBXBuildFile objects as
      # compared to other objects.  Mimic Xcode's behavior here by using an
      # empty string for sep.
      sep = ""
      end_tabs = 0
    else:
      sep = "\n"
      end_tabs = 2

    self._XCPrint(file, 2, self._XCPrintableValue(2, self) + " = {" + sep)

    self._XCKVPrint(file, 3, "isa", self.__class__.__name__)

    for property, value in sorted(self._properties.iteritems()):
      self._XCKVPrint(file, 3, property, value)

    self._XCPrint(file, end_tabs, "};\n")

  def UpdateProperties(self, properties):
    if properties == None:
      return

    for property, value in properties.iteritems():
      if not property in self._schema:
        raise KeyError, property + " not in " + self.__class__.__name__

      (is_list, property_type, is_strong) = self._schema[property][0:3]

      if is_list:
        if value.__class__ != list:
          raise TypeError, \
                property + " of " + self.__class__.__name__ + " must be list"
        for item in value:
          if not isinstance(item, property_type):
            raise TypeError, \
                  "item of " + property + " of " + self.__class__.__name__ + \
                  " must be " + property_type.__name__
      elif not isinstance(value, property_type):
        raise TypeError, \
              property + " of " + self.__class__.__name__ + " must be " + \
              property_type.__name__

      self._properties[property] = value

      if is_strong:
        if not is_list:
          value.parent = self
        else:
          for item in value:
            item.parent = self

  def VerifyHasRequiredProperties(self):
    for property, attributes in self._schema.iteritems():
      (is_list, property_type, is_strong, is_required) = attributes[0:4]
      if is_required and not property in self._properties:
        raise KeyError, self.__class__.__name__ + " requires " + property

  def _SetDefaultsFromSchema(self):
    defaults = {}
    for property, attributes in self._schema.iteritems():
      (is_list, property_type, is_strong, is_required) = attributes[0:4]
      if is_required and len(attributes) >= 5:
        default = attributes[4]
        defaults[property] = default

    if len(defaults) > 0:
      self.UpdateProperties(defaults)


class XCHierarchicalElement(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "comments":       [0, str, 0, 0],
    "fileEncoding":   [0, str, 0, 0],
    "includeInIndex": [0, int, 0, 0],
    "indentWidth":    [0, int, 0, 0],
    "lineEnding":     [0, int, 0, 0],
    "sourceTree":     [0, str, 0, 1],
    "tabWidth":       [0, int, 0, 0],
    "usesTabs":       [0, int, 0, 0],
    "wrapsLines":     [0, int, 0, 0],
  })


class PBXGroup(XCHierarchicalElement):
  _schema = XCHierarchicalElement._schema.copy()
  _schema.update({
    "children": [1, XCHierarchicalElement, 1, 1, []],
    "name":     [0, str,                   0, 0],
    "path":     [0, str,                   0, 0],
  })

  def Comment(self):
    # TODO(mark): Use path if no name?  Share with PBXFileReference?
    return self.Name()


class PBXFileReference(XCHierarchicalElement):
  _schema = XCHierarchicalElement._schema.copy()
  _schema.update({
    "explicitFileType":  [0, str, 0, 0],
    "lastKnownFileType": [0, str, 0, 0],
    "name":              [0, str, 0, 0],
    "path":              [0, str, 0, 1],
  })
  _should_print_single_line = True

  def Comment(self):
    if "path" in self._properties:
      return self._properties["path"]
    return self.Name()


class XCBuildConfiguration(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "baseConfigurationReference": [0, PBXFileReference, 0, 0],
    "buildSettings":              [0, dict, 0, 1, {}],
    "name":                       [0, str,  0, 1],
  })

  def Comment(self):
    return self.Name()


class XCConfigurationList(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "buildConfigurations":           [1, XCBuildConfiguration, 1, 1, []],
    "defaultConfigurationIsVisible": [0, int,                  0, 1, 1],
    "defaultConfigurationName":      [0, str,                  0, 1],
  })

  def Comment(self):
    return "Build configuration list for " + \
           self.parent.__class__.__name__ + ' "' + self.parent.Name() + '"'


class PBXBuildFile(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "fileRef": [0, PBXFileReference, 0, 1],
  })
  _should_print_single_line = True

  def Comment(self):
    # Example: "main.cc in Sources"
    return self._properties["fileRef"].Comment() + " in " + \
           self.parent.Comment()


class XCBuildPhase(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "buildActionMask":                    [0, int,          0, 1, 0x7fffffff],
    "files":                              [1, PBXBuildFile, 1, 1, []],
    "runOnlyForDeploymentPostprocessing": [0, int,          0, 1, 0],
  })


class PBXSourcesBuildPhase(XCBuildPhase):
  _schema = XCBuildPhase._schema.copy()
  _schema.update({
  })

  def Comment(self):
    return "Sources"


class PBXFrameworksBuildPhase(XCBuildPhase):
  _schema = XCBuildPhase._schema.copy()
  _schema.update({
  })

  def Comment(self):
    return "Frameworks"


class PBXShellScriptBuildPhase(XCBuildPhase):
  _schema = XCBuildPhase._schema.copy()
  _schema.update({
    "inputPaths":       [1, str, 0, 1, []],
    "name":             [0, str, 0, 0],
    "outputPaths":      [1, str, 0, 1, []],
    "shellPath":        [0, str, 0, 1, "/bin/sh"],
    "shellScript":      [0, str, 0, 1],
    "showEnvVarsInLog": [0, int, 0, 0],
  })

  def Comment(self):
    name = self.Name()
    if name != None:
      return name

    return "ShellScript"


# Provide forward declarations for PBXProject and XCTarget.  The problem here
# is that XCTarget depends on PBXTargetDependency, which depends on
# PBXContainerItemProxy, which in turn depends on XCTarget again.  The circle
# can't be broken, so advise Python of the existence of XCTarget before using
# it in PBXContainerItemProxy, in advance of defining XCTarget.  The same
# problem occurs with PBXProject, which depends on XCTarget and is itself
# depended on by PBXContainerItemProxy.
class PBXProject(XCObject):
  pass
class XCTarget(XCObject):
  pass

class PBXContainerItemProxy(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "containerPortal":      [0, PBXProject, 0, 1],
    "proxyType":            [0, int,        0, 1],  # TODO(mark): Default value?
    "remoteGlobalIDString": [0, XCTarget,   0, 1],  # TODO(mark): Just a str?
    "remoteInfo":           [0, str,        0, 1],
  })


class PBXTargetDependency(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "target":      [0, XCTarget,              0, 1],
    "targetProxy": [1, PBXContainerItemProxy, 1, 1],
  })


class PBXBuildRule(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "compilerSpec": [0, str, 0, 1],
    "filePatterns": [0, str, 0, 0],
    "fileType":     [0, str, 0, 1],
    "isEditable":   [0, int, 0, 1, 1],
    "outputFiles":  [1, str, 0, 1, []],
    "script":       [0, str, 0, 0],
  })


class XCTarget(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "buildConfigurationList": [0, XCConfigurationList, 1, 1],
    "buildPhases":            [1, XCBuildPhase,        1, 1, []],
    "dependencies":           [1, PBXTargetDependency, 1, 1, []],
    "name":                   [0, str,                 0, 1],
    "productName":            [0, str,                 0, 1],
  })

  def Comment(self):
    return self.Name()


class PBXNativeTarget(XCTarget):
  _schema = XCTarget._schema.copy()
  _schema.update({
    "buildRules":       [1, PBXBuildRule,     1, 1, []],
    "productReference": [0, PBXFileReference, 0, 1],
    "productType":      [0, str,              0, 1],
  })


class PBXProject(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "buildConfigurationList": [0, XCConfigurationList, 1, 1],
    "compatibilityVersion":   [0, str,                 0, 1, "Xcode 3.1"],
    "hasScannedForEncodings": [0, int,                 0, 1, 1],
    "mainGroup":              [0, PBXGroup,            1, 1],
    "projectDirPath":         [0, str,                 0, 1, ""],
    "projectRoot":            [0, str,                 0, 1, ""],
    "targets":                [1, XCTarget,            1, 1, []],
  })

  def Comment(self):
    return "Project object"

  def Name(self):
    return "FakeNameFixMe"


class XCProjectFile(XCObject):
  _schema = XCObject._schema.copy()
  _schema.update({
    "archiveVersion": [0, int,        0, 1, 1],
    "classes":        [0, dict,       0, 1, {}],
    "objectVersion":  [0, int,        0, 1, 45],
    "rootObject":     [0, PBXProject, 1, 1],
  })

  def ComputeIDs(self, recursive=True, overwrite=True, hash=hashlib.sha1()):
    # Although XCProjectFile is implemented here as an XCObject, it's not a
    # proper object in the Xcode sense, and it certainly doesn't have its own
    # ID.  Pass through an attempt to update IDs to the real root object.
    if recursive:
      self._properties["rootObject"].ComputeIDs(recursive, overwrite, hash)

  def Print(self, file=sys.stdout):
    self.VerifyHasRequiredProperties()

    # Add the special "objects" property, which will be caught and handled
    # separately during printing.  This structure allows a fairly standard
    # loop do the normal printing.
    self._properties["objects"] = {}
    self._XCPrint(file, 0, "// !$*UTF8*$!\n")
    if self._should_print_single_line:
      self._XCPrint(file, 0, "{ ")
    else:
      self._XCPrint(file, 0, "{\n")
    for property, value in sorted(self._properties.iteritems(),
                                  cmp=lambda x, y: cmp(x, y)):
      if property == "objects":
        self._PrintObjects(file)
      else:
        self._XCKVPrint(file, 1, property, value)
    self._XCPrint(file, 0, "}\n")
    del self._properties["objects"]

  def _PrintObjects(self, file):
    if self._should_print_single_line:
      self._XCPrint(file, 0, "objects = {")
    else:
      self._XCPrint(file, 1, "objects = {\n")

    objects_by_class = {}
    for object in self.Descendants():
      if object == self:
        continue
      class_name = object.__class__.__name__
      if not class_name in objects_by_class:
        objects_by_class[class_name] = []
      objects_by_class[class_name].append(object)

    for class_name in sorted(objects_by_class):
      self._XCPrint(file, 0, "\n")
      self._XCPrint(file, 0, "/* Begin " + class_name + " section */\n")
      for object in sorted(objects_by_class[class_name],
                           cmp=lambda x, y: cmp(x.id, y.id)):
        object.Print(file)
      self._XCPrint(file, 0, "/* End " + class_name + " section */\n")

    if self._should_print_single_line:
      self._XCPrint(file, 0, "}; ")
    else:
      self._XCPrint(file, 1, "};\n")


# TEST TEST TEST

sf = PBXFileReference(properties={"lastKnownFileType":"sourcecode.cpp.cpp", "path": "source.cc", "sourceTree": "SOURCE_ROOT"})
sbf = PBXBuildFile(properties={"fileRef":sf})

tcr = XCBuildConfiguration(properties={"name":"Release","buildSettings":{"PRODUCT_NAME":"targetty"}})
tcd = XCBuildConfiguration(properties={"name":"Debug","buildSettings":{"PRODUCT_NAME":"targetty"}})
tl = XCConfigurationList(properties={"defaultConfigurationName":"Release","buildConfigurations":[tcd,tcr]})
ts = PBXSourcesBuildPhase(properties={"files":[sbf]})
pr = PBXFileReference(properties={"explicitFileType":"archive.ar","includeInIndex":0,"path":"libtargetty.a","sourceTree":"BUILT_PRODUCTS_DIR"})
t = PBXNativeTarget(properties={"buildConfigurationList":tl,"buildPhases":[ts],"name":"targetty","productName":"targetty","productReference":pr,"productType":"com.apple.product-type.library.static"})

c = XCBuildConfiguration(properties={"name":"Release"})
cd = XCBuildConfiguration(properties={"name":"Debug"})
l = XCConfigurationList(properties={"defaultConfigurationName":"Release","buildConfigurations":[cd,c]})
g = PBXGroup(properties={"sourceTree":"<group>", "children":[sf, pr]})

o = PBXProject(properties={"mainGroup":g, "buildConfigurationList":l, "targets":[t]})
f = XCProjectFile(properties={"rootObject":o})

f.ComputeIDs()
f.Print()
