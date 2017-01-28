/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#ifndef HexNumber_h
#define HexNumber_h

#include "wtf/text/StringConcatenate.h"

namespace WTF {

namespace Internal {

const LChar lowerHexDigits[17] = "0123456789abcdef";
const LChar upperHexDigits[17] = "0123456789ABCDEF";

}  // namespace Internal

class HexNumber final {
  STATIC_ONLY(HexNumber);

 public:
  enum HexConversionMode { Lowercase, Uppercase };

  template <typename T>
  static inline void appendByteAsHex(unsigned char byte,
                                     T& destination,
                                     HexConversionMode mode = Uppercase) {
    const LChar* hexDigits = hexDigitsForMode(mode);
    destination.append(hexDigits[byte >> 4]);
    destination.append(hexDigits[byte & 0xF]);
  }

  static inline void appendByteAsHex(unsigned char byte,
                                     Vector<LChar>& destination,
                                     HexConversionMode mode = Uppercase) {
    const LChar* hexDigits = hexDigitsForMode(mode);
    destination.push_back(hexDigits[byte >> 4]);
    destination.push_back(hexDigits[byte & 0xF]);
  }

  static inline void appendByteAsHex(unsigned char byte,
                                     Vector<char>& destination,
                                     HexConversionMode mode = Uppercase) {
    const LChar* hexDigits = hexDigitsForMode(mode);
    destination.push_back(hexDigits[byte >> 4]);
    destination.push_back(hexDigits[byte & 0xF]);
  }

  template <typename T>
  static inline void appendUnsignedAsHex(unsigned number,
                                         T& destination,
                                         HexConversionMode mode = Uppercase) {
    const LChar* hexDigits = hexDigitsForMode(mode);
    Vector<LChar, 8> result;
    do {
      result.prepend(hexDigits[number % 16]);
      number >>= 4;
    } while (number > 0);

    destination.append(result.data(), result.size());
  }

  // Same as appendUnsignedAsHex, but using exactly 'desiredDigits' for the
  // conversion.
  template <typename T>
  static inline void appendUnsignedAsHexFixedSize(
      unsigned number,
      T& destination,
      unsigned desiredDigits,
      HexConversionMode mode = Uppercase) {
    DCHECK(desiredDigits);

    const LChar* hexDigits = hexDigitsForMode(mode);
    Vector<LChar, 8> result;
    do {
      result.prepend(hexDigits[number % 16]);
      number >>= 4;
    } while (result.size() < desiredDigits);

    DCHECK_EQ(result.size(), desiredDigits);
    destination.append(result.data(), result.size());
  }

 private:
  static inline const LChar* hexDigitsForMode(HexConversionMode mode) {
    return mode == Lowercase ? Internal::lowerHexDigits
                             : Internal::upperHexDigits;
  }
};

}  // namespace WTF

using WTF::HexNumber;

#endif  // HexNumber_h
