# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    #
    # Mesa EGL implementation has a pluggable architecture. The core consists
    # of an API dispatcher which routes all eglFooBar() API calls to a
    # pluggable driver. mesa_egl_sources contains the API dispatcher source
    # files. These sources when compiled with driver sources will produce
    # EGL library.
    #
    # Note that we cannot compile the API dispatcher into a static library
    # because they need to include EGL headers which might be driver specific.
    #
    'mesa_egl_sources': [
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglapi.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglapi.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglarray.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglarray.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglcompiler.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglconfig.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglconfig.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglcontext.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglcontext.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglcurrent.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglcurrent.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egldefines.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egldisplay.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egldisplay.h',
      #
      # Do not include egldriver.c which dynamically loads a driver at
      # runtime. We will statically bake our driver with the API dispatcher.
      #
      # '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egldriver.c',
      #
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egldriver.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglglobals.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglglobals.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglimage.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglimage.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egllog.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egllog.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglmisc.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglmisc.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglmode.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglmode.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglmutex.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglscreen.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglscreen.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglstring.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglstring.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglsurface.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglsurface.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglsync.c',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/eglsync.h',
      '<(DEPTH)/third_party/mesa/MesaLib/src/egl/main/egltypedefs.h',
    ],
  },
}
