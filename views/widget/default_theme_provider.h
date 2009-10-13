// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DEFAULT_THEME_PROVIDER_H_
#define VIEWS_DEFAULT_THEME_PROVIDER_H_

#include <vector>

#include "app/theme_provider.h"
#include "base/basictypes.h"

class Profile;
class ResourceBundle;

namespace views {

class DefaultThemeProvider : public ThemeProvider {
 public:
  DefaultThemeProvider() { };
  virtual ~DefaultThemeProvider() { };

  // Overridden from ThemeProvider.
  virtual void Init(Profile* profile) { }
  virtual SkBitmap* GetBitmapNamed(int id) const;
  virtual SkColor GetColor(int id) const {
    // Return debugging-blue.
    return 0xff0000ff;
  }
  virtual bool GetDisplayProperty(int id, int* result) const { return false; }
  virtual bool ShouldUseNativeFrame() const;
  virtual bool HasCustomImage(int id) const { return false; }
  virtual bool GetRawData(int id, std::vector<unsigned char>* raw_data) const {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultThemeProvider);
};

}  // namespace views

#endif  // VIEWS_DEFAULT_THEME_PROVIDER_H_
