#!/usr/bin/python

import xcodeproj_file as xf


class XcodeProject(object):
  def __init__(self, name):
    self.name = name
    self.project = xf.PBXProject(name=name)
    self.project_file = xf.XCProjectFile({"rootObject": self.project})

  def AddTarget(self, name):
    target = xf.PBXNativeTarget(
        {
          "name":        name,
          "productType": "com.apple.product-type.library.static",
        },
        parent=self.project)
    self.project.AppendProperty("targets", target)
    return target

  def Dump(self):
    self.project_file.ComputeIDs()
    self.project_file.Print()


# TEST TEST TEST

def main():
  p = XcodeProject("testproj")
  t1 = p.AddTarget("testtarg")
  t1.SourcesPhase().AddFile("source1.cc")
  t2 = p.AddTarget("dependent")
  t2.SourcesPhase().AddFile("source2.cc")
  t2.AddDependency(t1)
  p.Dump()


if __name__ == "__main__":
  main()
