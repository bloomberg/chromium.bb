// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _INFOSINK_INCLUDED_
#define _INFOSINK_INCLUDED_

#include <math.h>
#include "Common.h"

// Returns the fractional part of the given floating-point number.
inline float fractionalPart(float f) {
  float intPart = 0.0f;
  return modff(f, &intPart);
}

//
// TPrefixType is used to centralize how info log messages start.
// See below.
//
enum TPrefixType {
	EPrefixNone,
	EPrefixInfo,
	EPrefixWarning,
	EPrefixError,
	EPrefixInternalError,
	EPrefixUnimplemented,
	EPrefixNote
};

//
// Encapsulate info logs for all objects that have them.
//
// The methods are a general set of tools for getting a variety of
// messages and types inserted into the log.
//
class TInfoSinkBase {
public:
	TInfoSinkBase() {}

	template <typename T>
	TInfoSinkBase& operator<<(const T& t) {
		TPersistStringStream stream;
		stream << t;
		sink.append(stream.str());
		return *this;
	}
	// Override << operator for specific types. It is faster to append strings
	// and characters directly to the sink.
	TInfoSinkBase& operator<<(char c) {
		sink.append(1, c);
		return *this;
	}
	TInfoSinkBase& operator<<(const char* str) {
		sink.append(str);
		return *this;
	}
	TInfoSinkBase& operator<<(const TPersistString& str) {
		sink.append(str);
		return *this;
	}
	TInfoSinkBase& operator<<(const TString& str) {
		sink.append(str.c_str());
		return *this;
	}
	// Make sure floats are written with correct precision.
	TInfoSinkBase& operator<<(float f) {
		// Make sure that at least one decimal point is written. If a number
		// does not have a fractional part, the default precision format does
		// not write the decimal portion which gets interpreted as integer by
		// the compiler.
		TPersistStringStream stream;
		if (fractionalPart(f) == 0.0f) {
			stream.precision(1);
			stream << std::showpoint << std::fixed << f;
		} else {
			stream.unsetf(std::ios::fixed);
			stream.unsetf(std::ios::scientific);
			stream.precision(8);
			stream << f;
		}
		sink.append(stream.str());
		return *this;
	}
	// Write boolean values as their names instead of integral value.
	TInfoSinkBase& operator<<(bool b) {
		const char* str = b ? "true" : "false";
		sink.append(str);
		return *this;
	}

	void erase() { sink.clear(); }
	int size() { return static_cast<int>(sink.size()); }

	const TPersistString& str() const { return sink; }
	const char* c_str() const { return sink.c_str(); }

	void prefix(TPrefixType message);
	void location(const TSourceLoc& loc);
	void message(TPrefixType message, const char* s);
	void message(TPrefixType message, const char* s, TSourceLoc loc);

private:
	TPersistString sink;
};

class TInfoSink {
public:
	TInfoSinkBase info;
	TInfoSinkBase debug;
	TInfoSinkBase obj;
};

#endif // _INFOSINK_INCLUDED_
