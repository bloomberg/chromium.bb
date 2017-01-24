" Copyright 2017 The Chromium Authors. All rights reserved.
" Use of this source code is governed by a BSD-style license that can be
" found in the LICENSE file.

function! gn#TranslateToBuildFile(name) abort
  " Strip '//' prefix
  let l:new_path = substitute(a:name, '\v^//', '', '')

  " Strip the build target name (necessary if 'isfname' contains ':')
  let l:new_path = substitute(l:new_path, '\v:.*$', '', '')

  " Append 'BUILD.gn', only if this is a directory and not a file
  if isdirectory(l:new_path)
    let l:new_path = substitute(l:new_path, '\v/?$', '/BUILD.gn', '')
  endif
  return l:new_path
endfunction
