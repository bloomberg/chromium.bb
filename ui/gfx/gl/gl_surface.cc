// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "ui/gfx/gl/gl_context.h"

namespace gfx {

namespace {
base::LazyInstance<
    base::ThreadLocalPointer<GLSurface>,
    base::LeakyLazyInstanceTraits<base::ThreadLocalPointer<GLSurface> > >
        current_surface_(base::LINKER_INITIALIZED);
}  // namespace

GLSurface::GLSurface() {
}

GLSurface::~GLSurface() {
  if (GetCurrent() == this)
    SetCurrent(NULL);
}

bool GLSurface::Initialize()
{
  return true;
}

bool GLSurface::Resize(const gfx::Size& size) {
  NOTIMPLEMENTED();
  return false;
}

unsigned int GLSurface::GetBackingFrameBufferObject() {
  return 0;
}

bool GLSurface::OnMakeCurrent(GLContext* context) {
  return true;
}

void GLSurface::SetVisible(bool visible) {
}

void* GLSurface::GetShareHandle() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetDisplay() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetConfig() {
  NOTIMPLEMENTED();
  return NULL;
}

unsigned GLSurface::GetFormat() {
  NOTIMPLEMENTED();
  return 0;
}

GLSurface* GLSurface::GetCurrent() {
  return current_surface_.Pointer()->Get();
}

void GLSurface::SetCurrent(GLSurface* surface) {
  current_surface_.Pointer()->Set(surface);
}

GLSurfaceAdapter::GLSurfaceAdapter(GLSurface* surface) : surface_(surface) {
}

GLSurfaceAdapter::~GLSurfaceAdapter() {
}

bool GLSurfaceAdapter::Initialize() {
  return surface_->Initialize();
}

void GLSurfaceAdapter::Destroy() {
  surface_->Destroy();
}

bool GLSurfaceAdapter::Resize(const gfx::Size& size) {
  return surface_->Resize(size);
}

bool GLSurfaceAdapter::IsOffscreen() {
  return surface_->IsOffscreen();
}

bool GLSurfaceAdapter::SwapBuffers() {
  return surface_->SwapBuffers();
}

gfx::Size GLSurfaceAdapter::GetSize() {
  return surface_->GetSize();
}

void* GLSurfaceAdapter::GetHandle() {
  return surface_->GetHandle();
}

unsigned int GLSurfaceAdapter::GetBackingFrameBufferObject() {
  return surface_->GetBackingFrameBufferObject();
}

bool GLSurfaceAdapter::OnMakeCurrent(GLContext* context) {
  return surface_->OnMakeCurrent(context);
}

void* GLSurfaceAdapter::GetShareHandle() {
  return surface_->GetShareHandle();
}

void* GLSurfaceAdapter::GetDisplay() {
  return surface_->GetDisplay();
}

void* GLSurfaceAdapter::GetConfig() {
  return surface_->GetConfig();
}

unsigned GLSurfaceAdapter::GetFormat() {
  return surface_->GetFormat();
}

}  // namespace gfx
