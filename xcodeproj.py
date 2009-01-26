#!/usr/bin/python

import errno
import os
import xcodeproj_file as xf


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
