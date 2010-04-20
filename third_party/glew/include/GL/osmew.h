// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __osmew_h__
#define __osmew_h__
#define __OSMEW_H__

#include <GL/glew.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(GLAPIENTRY)
#if defined(_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#endif

#define OSMEW_GET_FUN(x) x

// Subset of OSMesa functions.

typedef struct osmesa_context *OSMesaContext;
typedef OSMesaContext (GLAPIENTRY * PFNOSMESACREATECONTEXTPROC) (GLenum format, OSMesaContext sharelist);
typedef void (GLAPIENTRY * PFNOSMESADESTROYCONTEXTPROC) (OSMesaContext ctx);
typedef GLboolean (GLAPIENTRY * PFNOSMESAMAKECURRENTPROC) (OSMesaContext ctx, void* buffer, GLenum type, GLsizei width, GLsizei height);
typedef OSMesaContext (GLAPIENTRY * PFNOSMESAGETCURRENTCONTEXTPROC) (void);

#define OSMesaCreateContext OSMEW_GET_FUN(__osmesaCreateContext)
#define OSMesaDestroyContext OSMEW_GET_FUN(__osmesaDestroyContext)
#define OSMesaMakeCurrent OSMEW_GET_FUN(__osmesaMakeCurrent)
#define OSMesaGetCurrentContext OSMEW_GET_FUN(__osmesaGetCurrentContext)

extern PFNOSMESACREATECONTEXTPROC __osmesaCreateContext;
extern PFNOSMESADESTROYCONTEXTPROC __osmesaDestroyContext;
extern PFNOSMESAMAKECURRENTPROC __osmesaMakeCurrent;
extern PFNOSMESAGETCURRENTCONTEXTPROC __osmesaGetCurrentContext;

void osmewInit ();

#undef GLAPIENTRY

#ifdef __cplusplus
}
#endif

#endif /* __osmew_h__ */
