/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGTests_h
#define SVGTests_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class QualifiedName;
class SVGElement;
class SVGStaticStringList;
class SVGStringListTearOff;

class CORE_EXPORT SVGTests : public GarbageCollectedMixin {
 public:
  // JS API
  SVGStringListTearOff* requiredExtensions();
  SVGStringListTearOff* systemLanguage();

  bool IsValid() const;

  bool IsKnownAttribute(const QualifiedName&);

  virtual void Trace(blink::Visitor*);

 protected:
  explicit SVGTests(SVGElement* context_element);

 private:
  Member<SVGStaticStringList> required_extensions_;
  Member<SVGStaticStringList> system_language_;
};

}  // namespace blink

#endif  // SVGTests_h
