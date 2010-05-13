/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include "talk/base/common.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlconstants.h"

//#define new TRACK_NEW

namespace buzz {

QName::QName() : namespace_(QN_EMPTY.namespace_),
                 localPart_(QN_EMPTY.localPart_) {}

QName::QName(const std::string & ns, const std::string & local) :
  namespace_(ns), localPart_(local) {}

static std::string
QName_LocalPart(const std::string & name) {
  size_t i = name.rfind(':');
  if (i == std::string::npos)
    return name;
  return name.substr(i + 1);
}

static std::string
QName_Namespace(const std::string & name) {
  size_t i = name.rfind(':');
  if (i == std::string::npos)
    return STR_EMPTY;
  return name.substr(0, i);
}

QName::QName(const std::string & mergedOrLocal) :
  namespace_(QName_Namespace(mergedOrLocal)),
  localPart_(QName_LocalPart(mergedOrLocal)) {}

std::string
QName::Merged() const {
  if (namespace_ == STR_EMPTY)
    return localPart_;
  return namespace_ + ':' + localPart_;
}

bool
QName::operator==(const QName & other) const {
  return
    localPart_ == other.localPart_ &&
    namespace_ == other.namespace_;
}

int
QName::Compare(const QName & other) const {
  int result = localPart_.compare(other.localPart_);
  if (result)
    return result;

  return namespace_.compare(other.namespace_);
}

}



