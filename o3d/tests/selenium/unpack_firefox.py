#!/usr/bin/env python

import sys
import os
import shutil
import subprocess

script_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
o3d_dir = os.path.dirname(os.path.dirname(script_dir))
src_dir = os.path.dirname(o3d_dir)
third_party_dir = os.path.join(o3d_dir, 'third_party')
internal_dir = os.path.join(src_dir, 'o3d-internal')
gflags_dir = os.path.join(third_party_dir, 'gflags', 'python')
sys.path.append(gflags_dir)

import gflags

# Command line flags
FLAGS = gflags.FLAGS
gflags.DEFINE_boolean("cleanup", False,
                      "If this is set, then the script cleans up "
                      "instead of unpacking.")
gflags.DEFINE_string("plugin_path", "", "This is the path to the plugin.")
gflags.DEFINE_string("product_path", "",
                     "This is the path to the built products.")

if sys.platform != "darwin":
  print "This script is only for use on Mac OS X"
  sys.exit(-1)

def Cleanup(dir):
  shutil.rmtree(dir, ignore_errors=True)

def InstallInternal(internal_firefox_dir, firefox_dir):
  firefox_app = os.path.join(firefox_dir, 'Firefox.app')
  firefox_tgz = os.path.join(internal_firefox_dir, 'firefox.tgz')
  print "Unpacking Firefox in '%s' to staging area." % firefox_tgz
  os.mkdir(firefox_dir)
  subprocess.call(['tar', '-C', firefox_dir, 'xfz', firefox_tgz])
  dest_plugin_path = os.path.join(firefox_app, 'Contents', 'MacOS',
                                  'plugins',
                                  os.path.basename(FLAGS.plugin_path))
  os.symlink(FLAGS.plugin_path, dest_plugin_path)
  return 0

def Install(firefox_dir):
  # No hermetic version, look for Firefox.app in /Applications
  source_app = '/Applications/Firefox.app'
  print "Copying Firefox in '%s' to staging area." % source_app

  if not os.path.exists(source_app):
    print "Unable to find Firefox in %s, is it installed?" % source_app
    sys.exit(-1)

  firefox_app = os.path.join(firefox_dir, 'Firefox.app')
  dest_plugin_path = os.path.join(firefox_app, 'Contents', 'MacOS',
                                  'plugins',
                                  os.path.basename(FLAGS.plugin_path))
  os.mkdir(firefox_dir)
  subprocess.call(['ditto', source_app,  firefox_app])
  os.symlink(FLAGS.plugin_path, dest_plugin_path)
  return 0

def main(args):
  internal_firefox_dir = os.path.join(internal_dir, 'firefox')
  if not os.path.exists(FLAGS.plugin_path):
    print "Plugin '%s' is missing." % FLAGS.plugin_path
    sys.exit(-1)

  if not os.path.exists(FLAGS.product_path):
    print "Product path '%s' is missing." % FLAGS.product_path
    sys.exit(-1)

  # Cleanup any residual stuff.
  firefox_dir = os.path.join(FLAGS.product_path, 'selenium_firefox')
  Cleanup(firefox_dir)

  if not FLAGS.cleanup:
    # if we have a hermetic version of Firefox, then use it.
    if os.path.exists(internal_firefox_dir):
        sys.exit(InstallInternal(internal_firefox_dir, firefox_dir))
    # Otherwise, we just copy what the user has installed.
    else:
      if not FLAGS.cleanup:
        sys.exit(Install(firefox_dir))
  sys.exit(0)

if __name__ == "__main__":
  bare_args = FLAGS(sys.argv)
  main(bare_args)
