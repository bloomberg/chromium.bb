echo off
:: Copyright (c) 2018 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.
setlocal

:: Defer control.
python "%~dp0fake_cipd.py" %*
