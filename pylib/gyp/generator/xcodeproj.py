#!/usr/bin/python

import gyp
import gyp.xcodeproj_file as xf
import errno
import os


class XcodeProject(object):
  def __init__(self, name):
    self.name = name
    self.project = xf.PBXProject(name=name)
    self.project_file = xf.XCProjectFile({"rootObject": self.project})

  def AddTarget(self, name, type):
    _types = { "static_library": "com.apple.product-type.library.static",
               "executable":     "com.apple.product-type.tool",
             }
    target = xf.PBXNativeTarget({"name": name, "productType": _types[type]},
                                parent=self.project)
    self.project.AppendProperty("targets", target)
    return target

  def Write(self):
    self.project_file.ComputeIDs()
    xcodeproj_path = self.name + ".xcodeproj"
    pbxproj_path = xcodeproj_path + "/project.pbxproj"

    try:
      os.mkdir(xcodeproj_path)
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

    output_file = open(pbxproj_path, "w")
    self.project_file.Print(output_file)


def GenerateOutput(targets, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-6:] != ".build":
      # TODO(mark): Pick an exception class
      raise "Build file name must end in .build"
    build_file_stem = build_file[:-6]
    xcode_projects[build_file] = XcodeProject(build_file_stem)

  xcode_targets = {}
  for qualified_target in targets:
    [build_file, target] = gyp.BuildFileAndTarget("", qualified_target)[0:2]
    spec = data[build_file]["targets"][target]
    xcode_targets[qualified_target] = \
        xcode_projects[build_file].AddTarget(target, spec["type"])
    for source in spec["sources"]:
      xcode_targets[qualified_target].SourcesPhase().AddFile(source)
    if "dependencies" in spec:
      for dependency in spec["dependencies"]:
        dependency = gyp.QualifiedTarget(build_file, dependency)
        xcode_targets[qualified_target].AddDependency(xcode_targets[dependency])

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Write()
