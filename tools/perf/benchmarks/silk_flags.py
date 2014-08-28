# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CustomizeBrowserOptionsForFastPath(options):
  """Enables flags needed for bleeding edge rendering fast paths."""
  options.AppendExtraBrowserArgs('--enable-bleeding-edge-rendering-fast-paths')

def CustomizeBrowserOptionsForSoftwareRasterization(options):
  """Enables flags needed for forced software rasterization."""
  options.AppendExtraBrowserArgs('--disable-gpu-rasterization')

def CustomizeBrowserOptionsForGpuRasterization(options):
  """Enables flags needed for forced GPU rasterization using Ganesh."""
  options.AppendExtraBrowserArgs('--enable-threaded-compositing')
  options.AppendExtraBrowserArgs('--enable-impl-side-painting')
  options.AppendExtraBrowserArgs('--force-gpu-rasterization')
