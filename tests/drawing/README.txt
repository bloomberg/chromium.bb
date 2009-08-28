A drawing demo for Native Client using the open source Anti-Grain Geometry
project.  Anti-Grain Geometry is a software rendering engine with many
capabilities.  For more information, please see the Anti-Grain website:

  http://www.antigrain.com/

First, download and build the Anti-Grain library for Native Client.  This
make target will download the Anti-Grain sources, apply a small patch to
add Native Client as a build platform, and build an optimized version
of libagg.a for Native Client, which the demo can statically link against.

  make download nacl

To build and run the Native Client demo using this library,

  make release nacl run

Once the demo is built, you can point your browser (with the Native
Client plugin installed) to the drawing.html file in this directory.
This html file will run the demo embedded in a web page.

If you need to make local changes to Anti-Grain Geometry, and rebuild
the Native Client libagg.a library, use the make command listed below.

  make agg nacl

Please see ../../common/README.txt for more instructions.

