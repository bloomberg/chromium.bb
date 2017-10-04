/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ThemePainter_h
#define ThemePainter_h

#include "platform/ThemeTypes.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class IntRect;
class LayoutObject;
class Node;
struct PaintInfo;

class ThemePainter {
  DISALLOW_NEW();

 public:
  explicit ThemePainter();

  // This method is called to paint the widget as a background of the
  // LayoutObject.  A widget's foreground, e.g., the text of a button, is always
  // rendered by the engine itself.  The boolean return value indicates whether
  // the CSS border/background should also be painted.
  bool Paint(const LayoutObject&, const PaintInfo&, const IntRect&);
  bool PaintBorderOnly(const Node*,
                       const ComputedStyle&,
                       const PaintInfo&,
                       const IntRect&);
  bool PaintDecorations(const Node*,
                        const ComputedStyle&,
                        const PaintInfo&,
                        const IntRect&);

  virtual bool PaintCapsLockIndicator(const LayoutObject&,
                                      const PaintInfo&,
                                      const IntRect&) {
    return 0;
  }
  void PaintSliderTicks(const LayoutObject&, const PaintInfo&, const IntRect&);

 protected:
  virtual bool PaintCheckbox(const Node*,
                             const ComputedStyle&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool PaintRadio(const Node*,
                          const ComputedStyle&,
                          const PaintInfo&,
                          const IntRect&) {
    return true;
  }
  virtual bool PaintButton(const Node*,
                           const ComputedStyle&,
                           const PaintInfo&,
                           const IntRect&) {
    return true;
  }
  virtual bool PaintInnerSpinButton(const Node*,
                                    const ComputedStyle&,
                                    const PaintInfo&,
                                    const IntRect&) {
    return true;
  }
  virtual bool PaintTextField(const Node*,
                              const ComputedStyle&,
                              const PaintInfo&,
                              const IntRect&) {
    return true;
  }
  virtual bool PaintTextArea(const Node*,
                             const ComputedStyle&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool PaintMenuList(const Node*,
                             const ComputedStyle&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool PaintMenuListButton(const Node* node,
                                   const ComputedStyle&,
                                   const PaintInfo&,
                                   const IntRect&) {
    return true;
  }
  virtual bool PaintProgressBar(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool PaintSliderTrack(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool PaintSliderThumb(const Node*,
                                const ComputedStyle&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool PaintSearchField(const Node*,
                                const ComputedStyle&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool PaintSearchFieldCancelButton(const LayoutObject&,
                                            const PaintInfo&,
                                            const IntRect&) {
    return true;
  }

  bool PaintUsingFallbackTheme(const Node*,
                               const ComputedStyle&,
                               const PaintInfo&,
                               const IntRect&);
  bool PaintCheckboxUsingFallbackTheme(const Node*,
                                       const ComputedStyle&,
                                       const PaintInfo&,
                                       const IntRect&);
  bool PaintRadioUsingFallbackTheme(const Node*,
                                    const ComputedStyle&,
                                    const PaintInfo&,
                                    const IntRect&);
};

}  // namespace blink

#endif  // ThemePainter_h
