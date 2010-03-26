@echo off
:: Copyright (c) 2010 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

set PYTHONPATH=%~dp0../python
python %~dp0/chrome_tests.py %*
