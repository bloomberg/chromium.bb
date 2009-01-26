#!/usr/bin/python

import xcodeproj


def main():
  project1 = xcodeproj.XcodeProject("testproj")
  t1 = project1.AddTarget("testtarg", "static_library")
  t1.SourcesPhase().AddFile("source1.cc")
  t2 = project1.AddTarget("dependent", "executable")
  t2.SourcesPhase().AddFile("source2.cc")
  t2.AddDependency(t1)
  project1.Write()

  project2 = xcodeproj.XcodeProject("testproj2")
  t3 = project2.AddTarget("crossy", "executable")
  t3.SourcesPhase().AddFile("source3.cc")
  t3.AddDependency(t1)
  project2.Write()

if __name__ == "__main__":
  main()
