#!/usr/bin/python

import hashlib
import inspect
import string
import sys

 
def EscapeComment(comment):
  # This isn't quite right.  At the very least, a "*/" in the comment string
  # gets turned into "(*)/".
  return "/* " + comment + " */"


def EscapeString(s):
  # Need to quote internal quotation marks and backslashes.
  return '"' + s + '"'


def EscapeValue(value):
  # These heuristics are too simplistic.  Other names that should be quoted are
  # "<group>" and "com.apple.product-type.library.static" (but check that this
  # is right for all contexts), and note that a single dot isn't enough to make
  # something be quoted, so it must be the dash?
  if value == "" or string.find(value, " ") != -1 or string.find(value, "<") != -1:
    return EscapeString(value)
  return value


def XCPrint(file, tabs, line):
  print >> file, "\t" * tabs + line


def XCObjPrintBegin(file, tabs, object):
  line = object.id.id + " "
  if object.Comment():
    line += EscapeComment(object.Comment()) + " "
  line += "= {"
  XCPrint(file, tabs, line)
  XCKVPrint(file, tabs + 1, "isa", object.__class__)


def XCObjPrintEnd(file, tabs, object):
  XCPrint(file, tabs, "};")


def XCKVPrint(file, tabs, key, value, comment=None):
  if value is None:
    return

  line = key + " = "
  if inspect.isclass(value):
    line += value.__name__
  elif isinstance(value, XCObject):
    line += value.id.id
  elif isinstance(value, str):
    line += EscapeValue(value)
  else:
    line += str(value)
  if isinstance(value, XCObject) and value.Comment():
    line += " " + EscapeComment(value.Comment())
  line += ";"
  XCPrint(file, tabs, line)


def XCLPrint(file, tabs, key, list):
  if list is None:
    return

  XCPrint(file, tabs, key + " = (")
  for item in list:
    XCPrint(file, tabs + 1, item.id.id + " " +
            EscapeComment(item.Comment()) + ",")
  XCPrint(file, tabs, ");")


def XCDPrint(file, tabs, key, dict):
  if dict is None:
    return

  XCPrint(file, tabs, key + " = {")
  for element_key, element_value in dict.iteritems():
    # PRINT SOMETHING / does element_key need escaping if it has weird chars?
    XCPrint(file, tabs + 1, element_key + " = " +
                            EscapeValue(element_value) + ";")
  XCPrint(file, tabs, "};")


class XCID(object):
  def __init__(self, id=None):
    self.id = id 


class XCProject(object):
  def __init__(self,
               archive_version=1,
               classes={},
               object_version=45,
               root_object=None):
    self.archive_version=archive_version
    self.classes=classes
    self.object_version=object_version
    self.root_object=root_object

  def Print(self, file=sys.stdout):
    XCPrint(file, 0, "// !$*UTF8*$!")
    XCPrint(file, 0, "{")
    XCKVPrint(file, 1, "archiveVersion", self.archive_version)
    XCDPrint(file, 1, "classes", self.classes)
    XCKVPrint(file, 1, "objectVersion", self.object_version)
    XCPrint(file, 1, "objects = {")

    objects_by_class = {}
    for object in self.root_object.Descendants():
      class_name = object.__class__.__name__
      if not class_name in objects_by_class:
        objects_by_class[class_name] = []
      objects_by_class[class_name].append(object)

    for class_name in sorted(objects_by_class):
      XCPrint(file, 0, "")
      XCPrint(file, 0, "/* Begin " + class_name + " section */")
      for object in sorted(objects_by_class[class_name],
                           cmp=lambda x,y: cmp(x.id.id, y.id.id)):
        object.Print()
      XCPrint(file, 0, "/* End " + class_name + " section */")

    XCPrint(file, 1, "};")
    XCKVPrint(file, 1, "rootObject", self.root_object)
    XCPrint(file, 0, "}")


class XCObject(object):
  """Base Xcode project file object.

  XCObject is a bare class intended to serve as a base for other types of
  Xcode objects.

  Attributes:
    Note: Although unset in XCObject, each object of a derived class should
          set id and parent appropriately.  As an exception, the root
          PBXProject object need not have a parent set.
    id: An object's unique identifying value represented as a 24-character
        hexadecimal string.
    parent: A reference to the object's owner.
  """

  def __init__(self, id=None):
    self.id = id
    self.name = None

  def ComputeIDs(self, recursive=True, overwrite=True, hash=hashlib.sha1()):
    hash.update(self.__class__.__name__)
    if self.name != None:
      hash.update(self.name)

    if recursive:
      for child in self.Children():
        child.ComputeIDs(recursive, overwrite, hash.copy())

    if overwrite or not self.id:
      self.id = XCID(hash.hexdigest()[0:24].upper())

  def Children(self):
    return []

  def Descendants(self):
    """Returns a list containing a reference to an object and all of its
    descendants.

    Derived classes that can serve as parents should override this method.
    Every element in the returned list must be of an XCObject subclass.
    """
    children = self.Children()
    descendants = [self]
    for child in children:
      descendants += child.Descendants()
    return descendants

  def Comment(self):
    """Returns a comment describing an object, formatted /* like this. */

    Derived classes should override this method.
    """
    raise "Can't comment on a raw XCObject"

  def Print(self, file=sys.stdout):
    """Prints an object in Xcode project file format to file.

    Derived classes should override this method.
    """
    raise "Can't print a raw XCObject"


class PBXProject(XCObject):
  """The root object in an Xcode project file.

  A project file has one PBXProject object, which is accessed by the
  rootObject property in the proejct file's root dictionary.

  Attributes:
    name: This is a pseudo-attribute; it is not actually represented directly
      as a "name" property in the project file itself, but is set based on
      the project file bundle's name.  The name attribute of
      "MyProject.xcodeproj" would be "MyProject".

    Children:
      build_configuration_list: An XCConfigurationList corresponding to the
        project's build configurations.
      main_group: A PBXGroup corresponding to the project's top-level group.
      targets: A list of PBXNativeTarget objects corresponding to the project's
        targets.
  """

  compatibility_version = "Xcode 3.1"
  has_scanned_for_encodings = 1
  project_dir_path = ""
  project_root = ""

  def _GetBuildConfigurationList(self):
    return self._build_configuration_list

  def _SetBuildConfigurationList(self, build_configuration_list):
    self._build_configuration_list = build_configuration_list
    build_configuration_list.parent = self

  def _GetMainGroup(self):
    return self._main_group

  def _SetMainGroup(self, main_group):
    self._main_group = main_group
    main_group.parent = self

  def _GetTargets(self):
    return self._targets

  def _SetTargets(self, targets):
    self._targets = targets
    for target in targets:
      target.parent = self

  build_configuration_list = property(_GetBuildConfigurationList,
                                      _SetBuildConfigurationList)
  main_group = property(_GetMainGroup, _SetMainGroup)
  targets = property(_GetTargets, _SetTargets)

  def __init__(self,
               id=None,
               name=None,
               build_configuration_list=None,
               main_group=None,
               targets=[]):
    XCObject.__init__(self, id)
    self.name = name
    self.build_configuration_list = build_configuration_list
    self.main_group = main_group
    self.targets = targets

  def Children(self):
    return [self.build_configuration_list, self.main_group] + self.targets

  def Comment(self):
    return "Project object"

  def Print(self, file=sys.stdout):
    XCObjPrintBegin(file, 2, self)
    XCKVPrint(file, 3, "buildConfigurationList", self.build_configuration_list)
    XCKVPrint(file, 3, "compatibilityVersion", self.compatibility_version)
    XCKVPrint(file, 3, "hasScannedForEncodings", self.has_scanned_for_encodings)
    XCKVPrint(file, 3, "mainGroup", self.main_group)
    XCKVPrint(file, 3, "projectDirPath", self.project_dir_path)
    XCKVPrint(file, 3, "projectRoot", self.project_root)
    XCLPrint(file, 3, "targets", self.targets)
    XCObjPrintEnd(file, 2, self)


class XCBuildConfiguration(XCObject):
  def __init__(self,
               id=None,
               build_settings={},
               name=None):
    XCObject.__init__(self, id)
    self.build_settings = build_settings
    self.name = name

  def Comment(self):
    return self.name

  def Print(self, file=sys.stdout):
    XCObjPrintBegin(file, 2, self)
    XCDPrint(file, 3, "buildSettings", self.build_settings)
    XCKVPrint(file, 3, "name", self.name)
    XCObjPrintEnd(file, 2, self)


class XCConfigurationList(XCObject):
  def _GetBuildConfigurations(self):
    return self._build_configurations

  def _SetBuildConfigurations(self, build_configurations):
    self._build_configurations = build_configurations
    for build_configuration in build_configurations:
      build_configuration.parent = self

  build_configurations = property(_GetBuildConfigurations,
                                  _SetBuildConfigurations)

  def __init__(self,
               id=None,
               build_configurations=[],
               default_configuration_is_visible=0,
               default_configuration_name=None):
    XCObject.__init__(self, id)
    self.build_configurations = build_configurations
    self.default_configuration_is_visible = default_configuration_is_visible
    self.default_configuration_name = default_configuration_name

  def Children(self):
    return self.build_configurations

  def Comment(self):
    return "Build configuration list for " + \
           self.parent.__class__.__name__ + " " + EscapeString(self.parent.name)

  def Print(self, file=sys.stdout):
    XCObjPrintBegin(file, 2, self)
    XCLPrint(file, 3, "buildConfigurations", self.build_configurations)
    XCKVPrint(file, 3, "defaultConfigurationIsVisible",
                       self.default_configuration_is_visible)
    XCKVPrint(file, 3, "defaultConfigurationName",
                       self.default_configuration_name)
    XCObjPrintEnd(file, 2, self)


class PBXGroup(XCObject):
  # FLESH OUT THIS OBJECT
  children = []
  name = None
  source_tree = "<group>"

  def Comment(self):
    return self.name

  def Print(self, file=sys.stdout):
    XCObjPrintBegin(file, 2, self)
    XCLPrint(file, 3, "children", self.children)
    XCKVPrint(file, 3, "name", self.name)
    XCKVPrint(file, 3, "sourceTree", self.source_tree)
    XCObjPrintEnd(file, 2, self)


class PBXNativeTarget(XCObject):
  def __init__(self,
               id=None,
               build_configuration_list=None,
               build_phases=[],
               build_rules=[],
               dependencies=[],
               name=None,
               product_name=None,
               product_reference=None,
               product_type=None):
    XCObject.__init__(self, id)
    self.build_configuration_list = build_configuration_list
    self.build_phases = build_phases
    self.build_rules = build_rules  # NOT YET SUPPORTED
    self.dependencies = dependencies  # NOT YET SUPPORTED
    self.name = name
    self.product_name = product_name
    self.product_reference = product_reference  # WEAK
    self.product_type = product_type

  def Comment(self):
    return self.name

  def Print(self, file=sys.stdout):
    XCObjPrintBegin(file, 2, self)
    XCKVPrint(file, 3, "buildConfigurationList", self.build_configuration_list)
    XCLPrint(file, 3, "buildPhases", self.build_phases)
    XCLPrint(file, 3, "buildRules", self.build_rules)
    XCLPrint(file, 3, "dependencies", self.dependencies)
    XCKVPrint(file, 3, "name", self.name)
    XCKVPrint(file, 3, "productName", self.product_name)
    XCKVPrint(file, 3, "productReference", self.product_reference)
    XCKVPrint(file, 3, "productType", self.product_type)
    XCObjPrintEnd(file, 2, self)


cf_debug = XCBuildConfiguration(name="Debug")

cf_release = XCBuildConfiguration(name="Release")

list = XCConfigurationList(build_configurations=[cf_debug, cf_release],
                           default_configuration_name="Release")

group = PBXGroup()
#group.name = "ssl"

target = PBXNativeTarget(name="ssl")

project = PBXProject(name="ssl",
                     build_configuration_list=list,
                     main_group=group,
                     targets=[target])

project.ComputeIDs()

xcproject = XCProject(root_object=project)

xcproject.Print()
