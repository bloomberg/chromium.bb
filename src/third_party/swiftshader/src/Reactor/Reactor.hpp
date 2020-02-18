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

#ifndef rr_Reactor_hpp
#define rr_Reactor_hpp

#include "Nucleus.hpp"
#include "Routine.hpp"
#include "Traits.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>

#include <string>
#include <tuple>
#include <unordered_set>

#undef Bool // b/127920555

#if !defined(NDEBUG)
#define ENABLE_RR_PRINT 1 // Enables RR_PRINT(), RR_WATCH()
#endif // !defined(NDEBUG)

#ifdef ENABLE_RR_DEBUG_INFO
	// Functions used for generating JIT debug info.
	// See docs/ReactorDebugInfo.md for more information.
	namespace rr
	{
		// Update the current source location for debug.
		void EmitDebugLocation();
		// Bind value to its symbolic name taken from the backtrace.
		void EmitDebugVariable(class Value* value);
		// Flush any pending variable bindings before the line ends.
		void FlushDebug();
	}
	#define RR_DEBUG_INFO_UPDATE_LOC()    rr::EmitDebugLocation()
	#define RR_DEBUG_INFO_EMIT_VAR(value) rr::EmitDebugVariable(value)
	#define RR_DEBUG_INFO_FLUSH()         rr::FlushDebug()
#else
	#define RR_DEBUG_INFO_UPDATE_LOC()
	#define RR_DEBUG_INFO_EMIT_VAR(value)
	#define RR_DEBUG_INFO_FLUSH()
#endif // ENABLE_RR_DEBUG_INFO

namespace rr
{
	struct Capabilities
	{
		bool CallSupported;       // Support for rr::Call()
		bool CoroutinesSupported; // Support for rr::Coroutine<F>
	};
	extern const Capabilities Caps;

	class Bool;
	class Byte;
	class SByte;
	class Byte4;
	class SByte4;
	class Byte8;
	class SByte8;
	class Byte16;
	class SByte16;
	class Short;
	class UShort;
	class Short2;
	class UShort2;
	class Short4;
	class UShort4;
	class Short8;
	class UShort8;
	class Int;
	class UInt;
	class Int2;
	class UInt2;
	class Int4;
	class UInt4;
	class Long;
	class Half;
	class Float;
	class Float2;
	class Float4;

	class Void
	{
	public:
		static Type *getType();

		static bool isVoid()
		{
			return true;
		}
	};

	template<class T>
	class RValue;

	template<class T>
	class Pointer;

	class Variable
	{
		friend class Nucleus;
		friend class PrintValue;

		Variable() = delete;
		Variable &operator=(const Variable&) = delete;

	public:
		void materialize() const;

		Value *loadValue() const;
		Value *storeValue(Value *value) const;

		Value *getBaseAddress() const;
		Value *getElementPointer(Value *index, bool unsignedIndex) const;

	protected:
		Variable(Type *type, int arraySize);
		Variable(const Variable&) = default;

		~Variable();

		const int arraySize;

	private:
		static void materializeAll();
		static void killUnmaterialized();

		static std::unordered_set<Variable*> unmaterializedVariables;

		Type *const type;
		mutable Value *rvalue = nullptr;
		mutable Value *address = nullptr;
	};

	template<class T>
	class LValue : public Variable
	{
	public:
		LValue(int arraySize = 0);

		RValue<Pointer<T>> operator&();

		static bool isVoid()
		{
			return false;
		}

		// self() returns the this pointer to this LValue<T> object.
		// This function exists because operator&() is overloaded.
		inline LValue<T>* self() { return this; }
	};

	template<class T>
	class Reference
	{
	public:
		using reference_underlying_type = T;

		explicit Reference(Value *pointer, int alignment = 1);

		RValue<T> operator=(RValue<T> rhs) const;
		RValue<T> operator=(const Reference<T> &ref) const;

		RValue<T> operator+=(RValue<T> rhs) const;

		RValue<Pointer<T>> operator&() const { return RValue<Pointer<T>>(address); }

		Value *loadValue() const;
		int getAlignment() const;

	private:
		Value *address;

		const int alignment;
	};

	template<class T>
	struct BoolLiteral
	{
		struct type;
	};

	template<>
	struct BoolLiteral<Bool>
	{
		typedef bool type;
	};

	template<class T>
	struct IntLiteral
	{
		struct type;
	};

	template<>
	struct IntLiteral<Int>
	{
		typedef int type;
	};

	template<>
	struct IntLiteral<UInt>
	{
		typedef unsigned int type;
	};

	template<>
	struct IntLiteral<Long>
	{
		typedef int64_t type;
	};

	template<class T>
	struct FloatLiteral
	{
		struct type;
	};

	template<>
	struct FloatLiteral<Float>
	{
		typedef float type;
	};

	template<class T>
	class RValue
	{
	public:
		using rvalue_underlying_type = T;

		explicit RValue(Value *rvalue);

#ifdef ENABLE_RR_DEBUG_INFO
		RValue(const RValue<T> &rvalue);
#endif // ENABLE_RR_DEBUG_INFO

		RValue(const T &lvalue);
		RValue(typename BoolLiteral<T>::type i);
		RValue(typename IntLiteral<T>::type i);
		RValue(typename FloatLiteral<T>::type f);
		RValue(const Reference<T> &rhs);

		RValue<T> &operator=(const RValue<T>&) = delete;

		Value *value;   // FIXME: Make private
	};

	template<typename T>
	struct Argument
	{
		explicit Argument(Value *value) : value(value) {}

		Value *value;
	};

	class Bool : public LValue<Bool>
	{
	public:
		Bool(Argument<Bool> argument);

		Bool() = default;
		Bool(bool x);
		Bool(RValue<Bool> rhs);
		Bool(const Bool &rhs);
		Bool(const Reference<Bool> &rhs);

	//	RValue<Bool> operator=(bool rhs);   // FIXME: Implement
		RValue<Bool> operator=(RValue<Bool> rhs);
		RValue<Bool> operator=(const Bool &rhs);
		RValue<Bool> operator=(const Reference<Bool> &rhs);

		static Type *getType();
	};

	RValue<Bool> operator!(RValue<Bool> val);
	RValue<Bool> operator&&(RValue<Bool> lhs, RValue<Bool> rhs);
	RValue<Bool> operator||(RValue<Bool> lhs, RValue<Bool> rhs);
	RValue<Bool> operator!=(RValue<Bool> lhs, RValue<Bool> rhs);
	RValue<Bool> operator==(RValue<Bool> lhs, RValue<Bool> rhs);

	class Byte : public LValue<Byte>
	{
	public:
		Byte(Argument<Byte> argument);

		explicit Byte(RValue<Int> cast);
		explicit Byte(RValue<UInt> cast);
		explicit Byte(RValue<UShort> cast);

		Byte() = default;
		Byte(int x);
		Byte(unsigned char x);
		Byte(RValue<Byte> rhs);
		Byte(const Byte &rhs);
		Byte(const Reference<Byte> &rhs);

	//	RValue<Byte> operator=(unsigned char rhs);   // FIXME: Implement
		RValue<Byte> operator=(RValue<Byte> rhs);
		RValue<Byte> operator=(const Byte &rhs);
		RValue<Byte> operator=(const Reference<Byte> &rhs);

		static Type *getType();
	};

	RValue<Byte> operator+(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator-(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator*(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator/(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator%(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator&(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator|(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator^(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator<<(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator>>(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Byte> operator+=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator-=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator*=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator/=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator%=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator&=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator|=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator^=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator<<=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator>>=(Byte &lhs, RValue<Byte> rhs);
	RValue<Byte> operator+(RValue<Byte> val);
	RValue<Byte> operator-(RValue<Byte> val);
	RValue<Byte> operator~(RValue<Byte> val);
	RValue<Byte> operator++(Byte &val, int);   // Post-increment
	const Byte &operator++(Byte &val);   // Pre-increment
	RValue<Byte> operator--(Byte &val, int);   // Post-decrement
	const Byte &operator--(Byte &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Bool> operator<=(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Bool> operator>(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Bool> operator>=(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Bool> operator!=(RValue<Byte> lhs, RValue<Byte> rhs);
	RValue<Bool> operator==(RValue<Byte> lhs, RValue<Byte> rhs);

	class SByte : public LValue<SByte>
	{
	public:
		SByte(Argument<SByte> argument);

		explicit SByte(RValue<Int> cast);
		explicit SByte(RValue<Short> cast);

		SByte() = default;
		SByte(signed char x);
		SByte(RValue<SByte> rhs);
		SByte(const SByte &rhs);
		SByte(const Reference<SByte> &rhs);

	//	RValue<SByte> operator=(signed char rhs);   // FIXME: Implement
		RValue<SByte> operator=(RValue<SByte> rhs);
		RValue<SByte> operator=(const SByte &rhs);
		RValue<SByte> operator=(const Reference<SByte> &rhs);

		static Type *getType();
	};

	RValue<SByte> operator+(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator-(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator*(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator/(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator%(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator&(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator|(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator^(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator<<(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator>>(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<SByte> operator+=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator-=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator*=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator/=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator%=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator&=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator|=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator^=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator<<=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator>>=(SByte &lhs, RValue<SByte> rhs);
	RValue<SByte> operator+(RValue<SByte> val);
	RValue<SByte> operator-(RValue<SByte> val);
	RValue<SByte> operator~(RValue<SByte> val);
	RValue<SByte> operator++(SByte &val, int);   // Post-increment
	const SByte &operator++(SByte &val);   // Pre-increment
	RValue<SByte> operator--(SByte &val, int);   // Post-decrement
	const SByte &operator--(SByte &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<Bool> operator<=(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<Bool> operator>(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<Bool> operator>=(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<Bool> operator!=(RValue<SByte> lhs, RValue<SByte> rhs);
	RValue<Bool> operator==(RValue<SByte> lhs, RValue<SByte> rhs);

	class Short : public LValue<Short>
	{
	public:
		Short(Argument<Short> argument);

		explicit Short(RValue<Int> cast);

		Short() = default;
		Short(short x);
		Short(RValue<Short> rhs);
		Short(const Short &rhs);
		Short(const Reference<Short> &rhs);

	//	RValue<Short> operator=(short rhs);   // FIXME: Implement
		RValue<Short> operator=(RValue<Short> rhs);
		RValue<Short> operator=(const Short &rhs);
		RValue<Short> operator=(const Reference<Short> &rhs);

		static Type *getType();
	};

	RValue<Short> operator+(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator-(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator*(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator/(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator%(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator&(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator|(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator^(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator<<(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator>>(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Short> operator+=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator-=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator*=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator/=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator%=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator&=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator|=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator^=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator<<=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator>>=(Short &lhs, RValue<Short> rhs);
	RValue<Short> operator+(RValue<Short> val);
	RValue<Short> operator-(RValue<Short> val);
	RValue<Short> operator~(RValue<Short> val);
	RValue<Short> operator++(Short &val, int);   // Post-increment
	const Short &operator++(Short &val);   // Pre-increment
	RValue<Short> operator--(Short &val, int);   // Post-decrement
	const Short &operator--(Short &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Bool> operator<=(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Bool> operator>(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Bool> operator>=(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Bool> operator!=(RValue<Short> lhs, RValue<Short> rhs);
	RValue<Bool> operator==(RValue<Short> lhs, RValue<Short> rhs);

	class UShort : public LValue<UShort>
	{
	public:
		UShort(Argument<UShort> argument);

		explicit UShort(RValue<UInt> cast);
		explicit UShort(RValue<Int> cast);

		UShort() = default;
		UShort(unsigned short x);
		UShort(RValue<UShort> rhs);
		UShort(const UShort &rhs);
		UShort(const Reference<UShort> &rhs);

	//	RValue<UShort> operator=(unsigned short rhs);   // FIXME: Implement
		RValue<UShort> operator=(RValue<UShort> rhs);
		RValue<UShort> operator=(const UShort &rhs);
		RValue<UShort> operator=(const Reference<UShort> &rhs);

		static Type *getType();
	};

	RValue<UShort> operator+(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator-(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator*(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator/(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator%(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator&(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator|(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator^(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator<<(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator>>(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<UShort> operator+=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator-=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator*=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator/=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator%=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator&=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator|=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator^=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator<<=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator>>=(UShort &lhs, RValue<UShort> rhs);
	RValue<UShort> operator+(RValue<UShort> val);
	RValue<UShort> operator-(RValue<UShort> val);
	RValue<UShort> operator~(RValue<UShort> val);
	RValue<UShort> operator++(UShort &val, int);   // Post-increment
	const UShort &operator++(UShort &val);   // Pre-increment
	RValue<UShort> operator--(UShort &val, int);   // Post-decrement
	const UShort &operator--(UShort &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<Bool> operator<=(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<Bool> operator>(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<Bool> operator>=(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<Bool> operator!=(RValue<UShort> lhs, RValue<UShort> rhs);
	RValue<Bool> operator==(RValue<UShort> lhs, RValue<UShort> rhs);

	class Byte4 : public LValue<Byte4>
	{
	public:
		explicit Byte4(RValue<Byte8> cast);

		Byte4() = default;
	//	Byte4(int x, int y, int z, int w);
	//	Byte4(RValue<Byte4> rhs);
	//	Byte4(const Byte4 &rhs);
		Byte4(const Reference<Byte4> &rhs);

	//	RValue<Byte4> operator=(RValue<Byte4> rhs);
	//	RValue<Byte4> operator=(const Byte4 &rhs);
	//	RValue<Byte4> operator=(const Reference<Byte4> &rhs);

		static Type *getType();
	};

//	RValue<Byte4> operator+(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator-(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator*(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator/(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator%(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator&(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator|(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator^(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator<<(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator>>(RValue<Byte4> lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator+=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator-=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator*=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator/=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator%=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator&=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator|=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator^=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator<<=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator>>=(Byte4 &lhs, RValue<Byte4> rhs);
//	RValue<Byte4> operator+(RValue<Byte4> val);
//	RValue<Byte4> operator-(RValue<Byte4> val);
//	RValue<Byte4> operator~(RValue<Byte4> val);
//	RValue<Byte4> operator++(Byte4 &val, int);   // Post-increment
//	const Byte4 &operator++(Byte4 &val);   // Pre-increment
//	RValue<Byte4> operator--(Byte4 &val, int);   // Post-decrement
//	const Byte4 &operator--(Byte4 &val);   // Pre-decrement

	class SByte4 : public LValue<SByte4>
	{
	public:
		SByte4() = default;
	//	SByte4(int x, int y, int z, int w);
	//	SByte4(RValue<SByte4> rhs);
	//	SByte4(const SByte4 &rhs);
	//	SByte4(const Reference<SByte4> &rhs);

	//	RValue<SByte4> operator=(RValue<SByte4> rhs);
	//	RValue<SByte4> operator=(const SByte4 &rhs);
	//	RValue<SByte4> operator=(const Reference<SByte4> &rhs);

		static Type *getType();
	};

//	RValue<SByte4> operator+(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator-(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator*(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator/(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator%(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator&(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator|(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator^(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator<<(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator>>(RValue<SByte4> lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator+=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator-=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator*=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator/=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator%=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator&=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator|=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator^=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator<<=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator>>=(SByte4 &lhs, RValue<SByte4> rhs);
//	RValue<SByte4> operator+(RValue<SByte4> val);
//	RValue<SByte4> operator-(RValue<SByte4> val);
//	RValue<SByte4> operator~(RValue<SByte4> val);
//	RValue<SByte4> operator++(SByte4 &val, int);   // Post-increment
//	const SByte4 &operator++(SByte4 &val);   // Pre-increment
//	RValue<SByte4> operator--(SByte4 &val, int);   // Post-decrement
//	const SByte4 &operator--(SByte4 &val);   // Pre-decrement

	class Byte8 : public LValue<Byte8>
	{
	public:
		Byte8() = default;
		Byte8(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6, uint8_t x7);
		Byte8(RValue<Byte8> rhs);
		Byte8(const Byte8 &rhs);
		Byte8(const Reference<Byte8> &rhs);

		RValue<Byte8> operator=(RValue<Byte8> rhs);
		RValue<Byte8> operator=(const Byte8 &rhs);
		RValue<Byte8> operator=(const Reference<Byte8> &rhs);

		static Type *getType();
	};

	RValue<Byte8> operator+(RValue<Byte8> lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator-(RValue<Byte8> lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator*(RValue<Byte8> lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator/(RValue<Byte8> lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator%(RValue<Byte8> lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator&(RValue<Byte8> lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator|(RValue<Byte8> lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator^(RValue<Byte8> lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator<<(RValue<Byte8> lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator>>(RValue<Byte8> lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator+=(Byte8 &lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator-=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator*=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator/=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator%=(Byte8 &lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator&=(Byte8 &lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator|=(Byte8 &lhs, RValue<Byte8> rhs);
	RValue<Byte8> operator^=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator<<=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator>>=(Byte8 &lhs, RValue<Byte8> rhs);
//	RValue<Byte8> operator+(RValue<Byte8> val);
//	RValue<Byte8> operator-(RValue<Byte8> val);
	RValue<Byte8> operator~(RValue<Byte8> val);
//	RValue<Byte8> operator++(Byte8 &val, int);   // Post-increment
//	const Byte8 &operator++(Byte8 &val);   // Pre-increment
//	RValue<Byte8> operator--(Byte8 &val, int);   // Post-decrement
//	const Byte8 &operator--(Byte8 &val);   // Pre-decrement

	RValue<Byte8> AddSat(RValue<Byte8> x, RValue<Byte8> y);
	RValue<Byte8> SubSat(RValue<Byte8> x, RValue<Byte8> y);
	RValue<Short4> Unpack(RValue<Byte4> x);
	RValue<Short4> Unpack(RValue<Byte4> x, RValue<Byte4> y);
	RValue<Short4> UnpackLow(RValue<Byte8> x, RValue<Byte8> y);
	RValue<Short4> UnpackHigh(RValue<Byte8> x, RValue<Byte8> y);
	RValue<Int> SignMask(RValue<Byte8> x);
//	RValue<Byte8> CmpGT(RValue<Byte8> x, RValue<Byte8> y);
	RValue<Byte8> CmpEQ(RValue<Byte8> x, RValue<Byte8> y);

	class SByte8 : public LValue<SByte8>
	{
	public:
		SByte8() = default;
		SByte8(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6, uint8_t x7);
		SByte8(RValue<SByte8> rhs);
		SByte8(const SByte8 &rhs);
		SByte8(const Reference<SByte8> &rhs);

		RValue<SByte8> operator=(RValue<SByte8> rhs);
		RValue<SByte8> operator=(const SByte8 &rhs);
		RValue<SByte8> operator=(const Reference<SByte8> &rhs);

		static Type *getType();
	};

	RValue<SByte8> operator+(RValue<SByte8> lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator-(RValue<SByte8> lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator*(RValue<SByte8> lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator/(RValue<SByte8> lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator%(RValue<SByte8> lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator&(RValue<SByte8> lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator|(RValue<SByte8> lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator^(RValue<SByte8> lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator<<(RValue<SByte8> lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator>>(RValue<SByte8> lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator+=(SByte8 &lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator-=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator*=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator/=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator%=(SByte8 &lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator&=(SByte8 &lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator|=(SByte8 &lhs, RValue<SByte8> rhs);
	RValue<SByte8> operator^=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator<<=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator>>=(SByte8 &lhs, RValue<SByte8> rhs);
//	RValue<SByte8> operator+(RValue<SByte8> val);
//	RValue<SByte8> operator-(RValue<SByte8> val);
	RValue<SByte8> operator~(RValue<SByte8> val);
//	RValue<SByte8> operator++(SByte8 &val, int);   // Post-increment
//	const SByte8 &operator++(SByte8 &val);   // Pre-increment
//	RValue<SByte8> operator--(SByte8 &val, int);   // Post-decrement
//	const SByte8 &operator--(SByte8 &val);   // Pre-decrement

	RValue<SByte8> AddSat(RValue<SByte8> x, RValue<SByte8> y);
	RValue<SByte8> SubSat(RValue<SByte8> x, RValue<SByte8> y);
	RValue<Short4> UnpackLow(RValue<SByte8> x, RValue<SByte8> y);
	RValue<Short4> UnpackHigh(RValue<SByte8> x, RValue<SByte8> y);
	RValue<Int> SignMask(RValue<SByte8> x);
	RValue<Byte8> CmpGT(RValue<SByte8> x, RValue<SByte8> y);
	RValue<Byte8> CmpEQ(RValue<SByte8> x, RValue<SByte8> y);

	class Byte16 : public LValue<Byte16>
	{
	public:
		Byte16() = default;
	//	Byte16(int x, int y, int z, int w);
		Byte16(RValue<Byte16> rhs);
		Byte16(const Byte16 &rhs);
		Byte16(const Reference<Byte16> &rhs);

		RValue<Byte16> operator=(RValue<Byte16> rhs);
		RValue<Byte16> operator=(const Byte16 &rhs);
		RValue<Byte16> operator=(const Reference<Byte16> &rhs);

		static Type *getType();
	};

//	RValue<Byte16> operator+(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator-(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator*(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator/(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator%(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator&(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator|(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator^(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator<<(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator>>(RValue<Byte16> lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator+=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator-=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator*=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator/=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator%=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator&=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator|=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator^=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator<<=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator>>=(Byte16 &lhs, RValue<Byte16> rhs);
//	RValue<Byte16> operator+(RValue<Byte16> val);
//	RValue<Byte16> operator-(RValue<Byte16> val);
//	RValue<Byte16> operator~(RValue<Byte16> val);
//	RValue<Byte16> operator++(Byte16 &val, int);   // Post-increment
//	const Byte16 &operator++(Byte16 &val);   // Pre-increment
//	RValue<Byte16> operator--(Byte16 &val, int);   // Post-decrement
//	const Byte16 &operator--(Byte16 &val);   // Pre-decrement

	class SByte16 : public LValue<SByte16>
	{
	public:
		SByte16() = default;
	//	SByte16(int x, int y, int z, int w);
	//	SByte16(RValue<SByte16> rhs);
	//	SByte16(const SByte16 &rhs);
	//	SByte16(const Reference<SByte16> &rhs);

	//	RValue<SByte16> operator=(RValue<SByte16> rhs);
	//	RValue<SByte16> operator=(const SByte16 &rhs);
	//	RValue<SByte16> operator=(const Reference<SByte16> &rhs);

		static Type *getType();
	};

//	RValue<SByte16> operator+(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator-(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator*(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator/(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator%(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator&(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator|(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator^(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator<<(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator>>(RValue<SByte16> lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator+=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator-=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator*=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator/=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator%=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator&=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator|=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator^=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator<<=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator>>=(SByte16 &lhs, RValue<SByte16> rhs);
//	RValue<SByte16> operator+(RValue<SByte16> val);
//	RValue<SByte16> operator-(RValue<SByte16> val);
//	RValue<SByte16> operator~(RValue<SByte16> val);
//	RValue<SByte16> operator++(SByte16 &val, int);   // Post-increment
//	const SByte16 &operator++(SByte16 &val);   // Pre-increment
//	RValue<SByte16> operator--(SByte16 &val, int);   // Post-decrement
//	const SByte16 &operator--(SByte16 &val);   // Pre-decrement

	class Short2 : public LValue<Short2>
	{
	public:
		explicit Short2(RValue<Short4> cast);

		static Type *getType();
	};

	class UShort2 : public LValue<UShort2>
	{
	public:
		explicit UShort2(RValue<UShort4> cast);

		static Type *getType();
	};

	class Short4 : public LValue<Short4>
	{
	public:
		explicit Short4(RValue<Int> cast);
		explicit Short4(RValue<Int4> cast);
	//	explicit Short4(RValue<Float> cast);
		explicit Short4(RValue<Float4> cast);

		Short4() = default;
		Short4(short xyzw);
		Short4(short x, short y, short z, short w);
		Short4(RValue<Short4> rhs);
		Short4(const Short4 &rhs);
		Short4(const Reference<Short4> &rhs);
		Short4(RValue<UShort4> rhs);
		Short4(const UShort4 &rhs);
		Short4(const Reference<UShort4> &rhs);

		RValue<Short4> operator=(RValue<Short4> rhs);
		RValue<Short4> operator=(const Short4 &rhs);
		RValue<Short4> operator=(const Reference<Short4> &rhs);
		RValue<Short4> operator=(RValue<UShort4> rhs);
		RValue<Short4> operator=(const UShort4 &rhs);
		RValue<Short4> operator=(const Reference<UShort4> &rhs);

		static Type *getType();
	};

	RValue<Short4> operator+(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator-(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator*(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Short4> operator/(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Short4> operator%(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator&(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator|(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator^(RValue<Short4> lhs, RValue<Short4> rhs);
	RValue<Short4> operator<<(RValue<Short4> lhs, unsigned char rhs);
	RValue<Short4> operator>>(RValue<Short4> lhs, unsigned char rhs);
	RValue<Short4> operator+=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator-=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator*=(Short4 &lhs, RValue<Short4> rhs);
//	RValue<Short4> operator/=(Short4 &lhs, RValue<Short4> rhs);
//	RValue<Short4> operator%=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator&=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator|=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator^=(Short4 &lhs, RValue<Short4> rhs);
	RValue<Short4> operator<<=(Short4 &lhs, unsigned char rhs);
	RValue<Short4> operator>>=(Short4 &lhs, unsigned char rhs);
//	RValue<Short4> operator+(RValue<Short4> val);
	RValue<Short4> operator-(RValue<Short4> val);
	RValue<Short4> operator~(RValue<Short4> val);
//	RValue<Short4> operator++(Short4 &val, int);   // Post-increment
//	const Short4 &operator++(Short4 &val);   // Pre-increment
//	RValue<Short4> operator--(Short4 &val, int);   // Post-decrement
//	const Short4 &operator--(Short4 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Bool> operator<=(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Bool> operator>(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Bool> operator>=(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Bool> operator!=(RValue<Short4> lhs, RValue<Short4> rhs);
//	RValue<Bool> operator==(RValue<Short4> lhs, RValue<Short4> rhs);

	RValue<Short4> RoundShort4(RValue<Float4> cast);
	RValue<Short4> Max(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> Min(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> AddSat(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> SubSat(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> MulHigh(RValue<Short4> x, RValue<Short4> y);
	RValue<Int2> MulAdd(RValue<Short4> x, RValue<Short4> y);
	RValue<SByte8> PackSigned(RValue<Short4> x, RValue<Short4> y);
	RValue<Byte8> PackUnsigned(RValue<Short4> x, RValue<Short4> y);
	RValue<Int2> UnpackLow(RValue<Short4> x, RValue<Short4> y);
	RValue<Int2> UnpackHigh(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> Swizzle(RValue<Short4> x, unsigned char select);
	RValue<Short4> Insert(RValue<Short4> val, RValue<Short> element, int i);
	RValue<Short> Extract(RValue<Short4> val, int i);
	RValue<Short4> CmpGT(RValue<Short4> x, RValue<Short4> y);
	RValue<Short4> CmpEQ(RValue<Short4> x, RValue<Short4> y);

	class UShort4 : public LValue<UShort4>
	{
	public:
		explicit UShort4(RValue<Int4> cast);
		explicit UShort4(RValue<Float4> cast, bool saturate = false);

		UShort4() = default;
		UShort4(unsigned short xyzw);
		UShort4(unsigned short x, unsigned short y, unsigned short z, unsigned short w);
		UShort4(RValue<UShort4> rhs);
		UShort4(const UShort4 &rhs);
		UShort4(const Reference<UShort4> &rhs);
		UShort4(RValue<Short4> rhs);
		UShort4(const Short4 &rhs);
		UShort4(const Reference<Short4> &rhs);

		RValue<UShort4> operator=(RValue<UShort4> rhs);
		RValue<UShort4> operator=(const UShort4 &rhs);
		RValue<UShort4> operator=(const Reference<UShort4> &rhs);
		RValue<UShort4> operator=(RValue<Short4> rhs);
		RValue<UShort4> operator=(const Short4 &rhs);
		RValue<UShort4> operator=(const Reference<Short4> &rhs);

		static Type *getType();
	};

	RValue<UShort4> operator+(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator-(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator*(RValue<UShort4> lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator/(RValue<UShort4> lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator%(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator&(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator|(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator^(RValue<UShort4> lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator<<(RValue<UShort4> lhs, unsigned char rhs);
	RValue<UShort4> operator>>(RValue<UShort4> lhs, unsigned char rhs);
//	RValue<UShort4> operator+=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator-=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator*=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator/=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator%=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator&=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator|=(UShort4 &lhs, RValue<UShort4> rhs);
//	RValue<UShort4> operator^=(UShort4 &lhs, RValue<UShort4> rhs);
	RValue<UShort4> operator<<=(UShort4 &lhs, unsigned char rhs);
	RValue<UShort4> operator>>=(UShort4 &lhs, unsigned char rhs);
//	RValue<UShort4> operator+(RValue<UShort4> val);
//	RValue<UShort4> operator-(RValue<UShort4> val);
	RValue<UShort4> operator~(RValue<UShort4> val);
//	RValue<UShort4> operator++(UShort4 &val, int);   // Post-increment
//	const UShort4 &operator++(UShort4 &val);   // Pre-increment
//	RValue<UShort4> operator--(UShort4 &val, int);   // Post-decrement
//	const UShort4 &operator--(UShort4 &val);   // Pre-decrement

	RValue<UShort4> Max(RValue<UShort4> x, RValue<UShort4> y);
	RValue<UShort4> Min(RValue<UShort4> x, RValue<UShort4> y);
	RValue<UShort4> AddSat(RValue<UShort4> x, RValue<UShort4> y);
	RValue<UShort4> SubSat(RValue<UShort4> x, RValue<UShort4> y);
	RValue<UShort4> MulHigh(RValue<UShort4> x, RValue<UShort4> y);
	RValue<UShort4> Average(RValue<UShort4> x, RValue<UShort4> y);

	class Short8 : public LValue<Short8>
	{
	public:
		Short8() = default;
		Short8(short c);
		Short8(short c0, short c1, short c2, short c3, short c4, short c5, short c6, short c7);
		Short8(RValue<Short8> rhs);
	//	Short8(const Short8 &rhs);
		Short8(const Reference<Short8> &rhs);
		Short8(RValue<Short4> lo, RValue<Short4> hi);

		RValue<Short8> operator=(RValue<Short8> rhs);
		RValue<Short8> operator=(const Short8 &rhs);
		RValue<Short8> operator=(const Reference<Short8> &rhs);

		static Type *getType();
	};

	RValue<Short8> operator+(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator-(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator*(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator/(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator%(RValue<Short8> lhs, RValue<Short8> rhs);
	RValue<Short8> operator&(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator|(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator^(RValue<Short8> lhs, RValue<Short8> rhs);
	RValue<Short8> operator<<(RValue<Short8> lhs, unsigned char rhs);
	RValue<Short8> operator>>(RValue<Short8> lhs, unsigned char rhs);
//	RValue<Short8> operator<<(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator>>(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Short8> operator+=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator-=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator*=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator/=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator%=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator&=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator|=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator^=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator<<=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator>>=(Short8 &lhs, RValue<Short8> rhs);
//	RValue<Short8> operator+(RValue<Short8> val);
//	RValue<Short8> operator-(RValue<Short8> val);
//	RValue<Short8> operator~(RValue<Short8> val);
//	RValue<Short8> operator++(Short8 &val, int);   // Post-increment
//	const Short8 &operator++(Short8 &val);   // Pre-increment
//	RValue<Short8> operator--(Short8 &val, int);   // Post-decrement
//	const Short8 &operator--(Short8 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Bool> operator<=(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Bool> operator>(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Bool> operator>=(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Bool> operator!=(RValue<Short8> lhs, RValue<Short8> rhs);
//	RValue<Bool> operator==(RValue<Short8> lhs, RValue<Short8> rhs);

	RValue<Short8> MulHigh(RValue<Short8> x, RValue<Short8> y);
	RValue<Int4> MulAdd(RValue<Short8> x, RValue<Short8> y);
	RValue<Int4> Abs(RValue<Int4> x);

	class UShort8 : public LValue<UShort8>
	{
	public:
		UShort8() = default;
		UShort8(unsigned short c);
		UShort8(unsigned short c0, unsigned short c1, unsigned short c2, unsigned short c3, unsigned short c4, unsigned short c5, unsigned short c6, unsigned short c7);
		UShort8(RValue<UShort8> rhs);
	//	UShort8(const UShort8 &rhs);
		UShort8(const Reference<UShort8> &rhs);
		UShort8(RValue<UShort4> lo, RValue<UShort4> hi);

		RValue<UShort8> operator=(RValue<UShort8> rhs);
		RValue<UShort8> operator=(const UShort8 &rhs);
		RValue<UShort8> operator=(const Reference<UShort8> &rhs);

		static Type *getType();
	};

	RValue<UShort8> operator+(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator-(RValue<UShort8> lhs, RValue<UShort8> rhs);
	RValue<UShort8> operator*(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator/(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator%(RValue<UShort8> lhs, RValue<UShort8> rhs);
	RValue<UShort8> operator&(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator|(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator^(RValue<UShort8> lhs, RValue<UShort8> rhs);
	RValue<UShort8> operator<<(RValue<UShort8> lhs, unsigned char rhs);
	RValue<UShort8> operator>>(RValue<UShort8> lhs, unsigned char rhs);
//	RValue<UShort8> operator<<(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator>>(RValue<UShort8> lhs, RValue<UShort8> rhs);
	RValue<UShort8> operator+=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator-=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator*=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator/=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator%=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator&=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator|=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator^=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator<<=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator>>=(UShort8 &lhs, RValue<UShort8> rhs);
//	RValue<UShort8> operator+(RValue<UShort8> val);
//	RValue<UShort8> operator-(RValue<UShort8> val);
	RValue<UShort8> operator~(RValue<UShort8> val);
//	RValue<UShort8> operator++(UShort8 &val, int);   // Post-increment
//	const UShort8 &operator++(UShort8 &val);   // Pre-increment
//	RValue<UShort8> operator--(UShort8 &val, int);   // Post-decrement
//	const UShort8 &operator--(UShort8 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<Bool> operator<=(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<Bool> operator>(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<Bool> operator>=(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<Bool> operator!=(RValue<UShort8> lhs, RValue<UShort8> rhs);
//	RValue<Bool> operator==(RValue<UShort8> lhs, RValue<UShort8> rhs);

	RValue<UShort8> Swizzle(RValue<UShort8> x, char select0, char select1, char select2, char select3, char select4, char select5, char select6, char select7);
	RValue<UShort8> MulHigh(RValue<UShort8> x, RValue<UShort8> y);

	class Int : public LValue<Int>
	{
	public:
		Int(Argument<Int> argument);

		explicit Int(RValue<Byte> cast);
		explicit Int(RValue<SByte> cast);
		explicit Int(RValue<Short> cast);
		explicit Int(RValue<UShort> cast);
		explicit Int(RValue<Int2> cast);
		explicit Int(RValue<Long> cast);
		explicit Int(RValue<Float> cast);

		Int() = default;
		Int(int x);
		Int(RValue<Int> rhs);
		Int(RValue<UInt> rhs);
		Int(const Int &rhs);
		Int(const UInt &rhs);
		Int(const Reference<Int> &rhs);
		Int(const Reference<UInt> &rhs);

		RValue<Int> operator=(int rhs);
		RValue<Int> operator=(RValue<Int> rhs);
		RValue<Int> operator=(RValue<UInt> rhs);
		RValue<Int> operator=(const Int &rhs);
		RValue<Int> operator=(const UInt &rhs);
		RValue<Int> operator=(const Reference<Int> &rhs);
		RValue<Int> operator=(const Reference<UInt> &rhs);

		static Type *getType();
	};

	RValue<Int> operator+(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator-(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator*(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator/(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator%(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator&(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator|(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator^(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator<<(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator>>(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Int> operator+=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator-=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator*=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator/=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator%=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator&=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator|=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator^=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator<<=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator>>=(Int &lhs, RValue<Int> rhs);
	RValue<Int> operator+(RValue<Int> val);
	RValue<Int> operator-(RValue<Int> val);
	RValue<Int> operator~(RValue<Int> val);
	RValue<Int> operator++(Int &val, int);   // Post-increment
	const Int &operator++(Int &val);   // Pre-increment
	RValue<Int> operator--(Int &val, int);   // Post-decrement
	const Int &operator--(Int &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Bool> operator<=(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Bool> operator>(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Bool> operator>=(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Bool> operator!=(RValue<Int> lhs, RValue<Int> rhs);
	RValue<Bool> operator==(RValue<Int> lhs, RValue<Int> rhs);

	RValue<Int> Max(RValue<Int> x, RValue<Int> y);
	RValue<Int> Min(RValue<Int> x, RValue<Int> y);
	RValue<Int> Clamp(RValue<Int> x, RValue<Int> min, RValue<Int> max);
	RValue<Int> RoundInt(RValue<Float> cast);

	class Long : public LValue<Long>
	{
	public:
	//	Long(Argument<Long> argument);

	//	explicit Long(RValue<Short> cast);
	//	explicit Long(RValue<UShort> cast);
		explicit Long(RValue<Int> cast);
		explicit Long(RValue<UInt> cast);
	//	explicit Long(RValue<Float> cast);

		Long() = default;
	//	Long(qword x);
		Long(RValue<Long> rhs);
	//	Long(RValue<ULong> rhs);
	//	Long(const Long &rhs);
	//	Long(const Reference<Long> &rhs);
	//	Long(const ULong &rhs);
	//	Long(const Reference<ULong> &rhs);

		RValue<Long> operator=(int64_t rhs);
		RValue<Long> operator=(RValue<Long> rhs);
	//	RValue<Long> operator=(RValue<ULong> rhs);
		RValue<Long> operator=(const Long &rhs);
		RValue<Long> operator=(const Reference<Long> &rhs);
	//	RValue<Long> operator=(const ULong &rhs);
	//	RValue<Long> operator=(const Reference<ULong> &rhs);

		static Type *getType();
	};

	RValue<Long> operator+(RValue<Long> lhs, RValue<Long> rhs);
	RValue<Long> operator-(RValue<Long> lhs, RValue<Long> rhs);
	RValue<Long> operator*(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator/(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator%(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator&(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator|(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator^(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Long> operator<<(RValue<Long> lhs, RValue<Long> rhs);
	RValue<Long> operator>>(RValue<Long> lhs, RValue<Long> rhs);
	RValue<Long> operator+=(Long &lhs, RValue<Long> rhs);
	RValue<Long> operator-=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator*=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator/=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator%=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator&=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator|=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator^=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator<<=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator>>=(Long &lhs, RValue<Long> rhs);
//	RValue<Long> operator+(RValue<Long> val);
//	RValue<Long> operator-(RValue<Long> val);
//	RValue<Long> operator~(RValue<Long> val);
//	RValue<Long> operator++(Long &val, int);   // Post-increment
//	const Long &operator++(Long &val);   // Pre-increment
//	RValue<Long> operator--(Long &val, int);   // Post-decrement
//	const Long &operator--(Long &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Bool> operator<=(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Bool> operator>(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Bool> operator>=(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Bool> operator!=(RValue<Long> lhs, RValue<Long> rhs);
//	RValue<Bool> operator==(RValue<Long> lhs, RValue<Long> rhs);

//	RValue<Long> RoundLong(RValue<Float> cast);
	RValue<Long> AddAtomic( RValue<Pointer<Long>> x, RValue<Long> y);

	class UInt : public LValue<UInt>
	{
	public:
		UInt(Argument<UInt> argument);

		explicit UInt(RValue<UShort> cast);
		explicit UInt(RValue<Long> cast);
		explicit UInt(RValue<Float> cast);

		UInt() = default;
		UInt(int x);
		UInt(unsigned int x);
		UInt(RValue<UInt> rhs);
		UInt(RValue<Int> rhs);
		UInt(const UInt &rhs);
		UInt(const Int &rhs);
		UInt(const Reference<UInt> &rhs);
		UInt(const Reference<Int> &rhs);

		RValue<UInt> operator=(unsigned int rhs);
		RValue<UInt> operator=(RValue<UInt> rhs);
		RValue<UInt> operator=(RValue<Int> rhs);
		RValue<UInt> operator=(const UInt &rhs);
		RValue<UInt> operator=(const Int &rhs);
		RValue<UInt> operator=(const Reference<UInt> &rhs);
		RValue<UInt> operator=(const Reference<Int> &rhs);

		static Type *getType();
	};

	RValue<UInt> operator+(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator-(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator*(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator/(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator%(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator&(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator|(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator^(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator<<(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator>>(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<UInt> operator+=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator-=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator*=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator/=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator%=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator&=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator|=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator^=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator<<=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator>>=(UInt &lhs, RValue<UInt> rhs);
	RValue<UInt> operator+(RValue<UInt> val);
	RValue<UInt> operator-(RValue<UInt> val);
	RValue<UInt> operator~(RValue<UInt> val);
	RValue<UInt> operator++(UInt &val, int);   // Post-increment
	const UInt &operator++(UInt &val);   // Pre-increment
	RValue<UInt> operator--(UInt &val, int);   // Post-decrement
	const UInt &operator--(UInt &val);   // Pre-decrement
	RValue<Bool> operator<(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<Bool> operator<=(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<Bool> operator>(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<Bool> operator>=(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<Bool> operator!=(RValue<UInt> lhs, RValue<UInt> rhs);
	RValue<Bool> operator==(RValue<UInt> lhs, RValue<UInt> rhs);

	RValue<UInt> Max(RValue<UInt> x, RValue<UInt> y);
	RValue<UInt> Min(RValue<UInt> x, RValue<UInt> y);
	RValue<UInt> Clamp(RValue<UInt> x, RValue<UInt> min, RValue<UInt> max);

	RValue<UInt> AddAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> SubAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> AndAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> OrAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> XorAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<Int> MinAtomic(RValue<Pointer<Int>> x, RValue<Int> y, std::memory_order memoryOrder);
	RValue<Int> MaxAtomic(RValue<Pointer<Int>> x, RValue<Int> y, std::memory_order memoryOrder);
	RValue<UInt> MinAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> MaxAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> ExchangeAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, std::memory_order memoryOrder);
	RValue<UInt> CompareExchangeAtomic(RValue<Pointer<UInt>> x, RValue<UInt> y, RValue<UInt> compare, std::memory_order memoryOrderEqual, std::memory_order memoryOrderUnequal);

//	RValue<UInt> RoundUInt(RValue<Float> cast);

	class Int2 : public LValue<Int2>
	{
	public:
	//	explicit Int2(RValue<Int> cast);
		explicit Int2(RValue<Int4> cast);

		Int2() = default;
		Int2(int x, int y);
		Int2(RValue<Int2> rhs);
		Int2(const Int2 &rhs);
		Int2(const Reference<Int2> &rhs);
		Int2(RValue<Int> lo, RValue<Int> hi);

		RValue<Int2> operator=(RValue<Int2> rhs);
		RValue<Int2> operator=(const Int2 &rhs);
		RValue<Int2> operator=(const Reference<Int2> &rhs);

		static Type *getType();
	};

	RValue<Int2> operator+(RValue<Int2> lhs, RValue<Int2> rhs);
	RValue<Int2> operator-(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Int2> operator*(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Int2> operator/(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Int2> operator%(RValue<Int2> lhs, RValue<Int2> rhs);
	RValue<Int2> operator&(RValue<Int2> lhs, RValue<Int2> rhs);
	RValue<Int2> operator|(RValue<Int2> lhs, RValue<Int2> rhs);
	RValue<Int2> operator^(RValue<Int2> lhs, RValue<Int2> rhs);
	RValue<Int2> operator<<(RValue<Int2> lhs, unsigned char rhs);
	RValue<Int2> operator>>(RValue<Int2> lhs, unsigned char rhs);
	RValue<Int2> operator+=(Int2 &lhs, RValue<Int2> rhs);
	RValue<Int2> operator-=(Int2 &lhs, RValue<Int2> rhs);
//	RValue<Int2> operator*=(Int2 &lhs, RValue<Int2> rhs);
//	RValue<Int2> operator/=(Int2 &lhs, RValue<Int2> rhs);
//	RValue<Int2> operator%=(Int2 &lhs, RValue<Int2> rhs);
	RValue<Int2> operator&=(Int2 &lhs, RValue<Int2> rhs);
	RValue<Int2> operator|=(Int2 &lhs, RValue<Int2> rhs);
	RValue<Int2> operator^=(Int2 &lhs, RValue<Int2> rhs);
	RValue<Int2> operator<<=(Int2 &lhs, unsigned char rhs);
	RValue<Int2> operator>>=(Int2 &lhs, unsigned char rhs);
//	RValue<Int2> operator+(RValue<Int2> val);
//	RValue<Int2> operator-(RValue<Int2> val);
	RValue<Int2> operator~(RValue<Int2> val);
//	RValue<Int2> operator++(Int2 &val, int);   // Post-increment
//	const Int2 &operator++(Int2 &val);   // Pre-increment
//	RValue<Int2> operator--(Int2 &val, int);   // Post-decrement
//	const Int2 &operator--(Int2 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Bool> operator<=(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Bool> operator>(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Bool> operator>=(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Bool> operator!=(RValue<Int2> lhs, RValue<Int2> rhs);
//	RValue<Bool> operator==(RValue<Int2> lhs, RValue<Int2> rhs);

//	RValue<Int2> RoundInt(RValue<Float4> cast);
	RValue<Short4> UnpackLow(RValue<Int2> x, RValue<Int2> y);
	RValue<Short4> UnpackHigh(RValue<Int2> x, RValue<Int2> y);
	RValue<Int> Extract(RValue<Int2> val, int i);
	RValue<Int2> Insert(RValue<Int2> val, RValue<Int> element, int i);

	class UInt2 : public LValue<UInt2>
	{
	public:
		UInt2() = default;
		UInt2(unsigned int x, unsigned int y);
		UInt2(RValue<UInt2> rhs);
		UInt2(const UInt2 &rhs);
		UInt2(const Reference<UInt2> &rhs);

		RValue<UInt2> operator=(RValue<UInt2> rhs);
		RValue<UInt2> operator=(const UInt2 &rhs);
		RValue<UInt2> operator=(const Reference<UInt2> &rhs);

		static Type *getType();
	};

	RValue<UInt2> operator+(RValue<UInt2> lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator-(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator*(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator/(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator%(RValue<UInt2> lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator&(RValue<UInt2> lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator|(RValue<UInt2> lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator^(RValue<UInt2> lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator<<(RValue<UInt2> lhs, unsigned char rhs);
	RValue<UInt2> operator>>(RValue<UInt2> lhs, unsigned char rhs);
	RValue<UInt2> operator+=(UInt2 &lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator-=(UInt2 &lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator*=(UInt2 &lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator/=(UInt2 &lhs, RValue<UInt2> rhs);
//	RValue<UInt2> operator%=(UInt2 &lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator&=(UInt2 &lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator|=(UInt2 &lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator^=(UInt2 &lhs, RValue<UInt2> rhs);
	RValue<UInt2> operator<<=(UInt2 &lhs, unsigned char rhs);
	RValue<UInt2> operator>>=(UInt2 &lhs, unsigned char rhs);
//	RValue<UInt2> operator+(RValue<UInt2> val);
//	RValue<UInt2> operator-(RValue<UInt2> val);
	RValue<UInt2> operator~(RValue<UInt2> val);
//	RValue<UInt2> operator++(UInt2 &val, int);   // Post-increment
//	const UInt2 &operator++(UInt2 &val);   // Pre-increment
//	RValue<UInt2> operator--(UInt2 &val, int);   // Post-decrement
//	const UInt2 &operator--(UInt2 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<Bool> operator<=(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<Bool> operator>(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<Bool> operator>=(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<Bool> operator!=(RValue<UInt2> lhs, RValue<UInt2> rhs);
//	RValue<Bool> operator==(RValue<UInt2> lhs, RValue<UInt2> rhs);

//	RValue<UInt2> RoundInt(RValue<Float4> cast);
	RValue<UInt> Extract(RValue<UInt2> val, int i);
	RValue<UInt2> Insert(RValue<UInt2> val, RValue<UInt> element, int i);

	template<class T>
	struct Scalar;

	template<class Vector4>
	struct XYZW;

	template<class Vector4, int T>
	class Swizzle2
	{
		friend Vector4;

	public:
		operator RValue<Vector4>() const;

	private:
		Vector4 *parent;
	};

	template<class Vector4, int T>
	class Swizzle4
	{
	public:
		operator RValue<Vector4>() const;

	private:
		Vector4 *parent;
	};

	template<class Vector4, int T>
	class SwizzleMask4
	{
		friend XYZW<Vector4>;

	public:
		operator RValue<Vector4>() const;

		RValue<Vector4> operator=(RValue<Vector4> rhs);
		RValue<Vector4> operator=(RValue<typename Scalar<Vector4>::Type> rhs);

	private:
		Vector4 *parent;
	};

	template<>
	struct Scalar<Float4>
	{
		using Type = Float;
	};

	template<>
	struct Scalar<Int4>
	{
		using Type = Int;
	};

	template<>
	struct Scalar<UInt4>
	{
		using Type = UInt;
	};

	template<class Vector4, int T>
	class SwizzleMask1
	{
	public:
		operator RValue<typename Scalar<Vector4>::Type>() const;
		operator RValue<Vector4>() const;

		RValue<Vector4> operator=(float x);
		RValue<Vector4> operator=(RValue<Vector4> rhs);
		RValue<Vector4> operator=(RValue<typename Scalar<Vector4>::Type> rhs);

	private:
		Vector4 *parent;
	};

	template<class Vector4, int T>
	class SwizzleMask2
	{
		friend class Float4;

	public:
		operator RValue<Vector4>() const;

		RValue<Vector4> operator=(RValue<Vector4> rhs);

	private:
		Float4 *parent;
	};

	template<class Vector4>
	struct XYZW
	{
		friend Vector4;

	private:
		XYZW(Vector4 *parent)
		{
			xyzw.parent = parent;
		}

	public:
		union
		{
			SwizzleMask1<Vector4, 0x00> x;
			SwizzleMask1<Vector4, 0x55> y;
			SwizzleMask1<Vector4, 0xAA> z;
			SwizzleMask1<Vector4, 0xFF> w;
			Swizzle2<Vector4, 0x00>     xx;
			Swizzle2<Vector4, 0x01>     yx;
			Swizzle2<Vector4, 0x02>     zx;
			Swizzle2<Vector4, 0x03>     wx;
			SwizzleMask2<Vector4, 0x54> xy;
			Swizzle2<Vector4, 0x55>     yy;
			Swizzle2<Vector4, 0x56>     zy;
			Swizzle2<Vector4, 0x57>     wy;
			SwizzleMask2<Vector4, 0xA8> xz;
			SwizzleMask2<Vector4, 0xA9> yz;
			Swizzle2<Vector4, 0xAA>     zz;
			Swizzle2<Vector4, 0xAB>     wz;
			SwizzleMask2<Vector4, 0xFC> xw;
			SwizzleMask2<Vector4, 0xFD> yw;
			SwizzleMask2<Vector4, 0xFE> zw;
			Swizzle2<Vector4, 0xFF>     ww;
			Swizzle4<Vector4, 0x00>     xxx;
			Swizzle4<Vector4, 0x01>     yxx;
			Swizzle4<Vector4, 0x02>     zxx;
			Swizzle4<Vector4, 0x03>     wxx;
			Swizzle4<Vector4, 0x04>     xyx;
			Swizzle4<Vector4, 0x05>     yyx;
			Swizzle4<Vector4, 0x06>     zyx;
			Swizzle4<Vector4, 0x07>     wyx;
			Swizzle4<Vector4, 0x08>     xzx;
			Swizzle4<Vector4, 0x09>     yzx;
			Swizzle4<Vector4, 0x0A>     zzx;
			Swizzle4<Vector4, 0x0B>     wzx;
			Swizzle4<Vector4, 0x0C>     xwx;
			Swizzle4<Vector4, 0x0D>     ywx;
			Swizzle4<Vector4, 0x0E>     zwx;
			Swizzle4<Vector4, 0x0F>     wwx;
			Swizzle4<Vector4, 0x50>     xxy;
			Swizzle4<Vector4, 0x51>     yxy;
			Swizzle4<Vector4, 0x52>     zxy;
			Swizzle4<Vector4, 0x53>     wxy;
			Swizzle4<Vector4, 0x54>     xyy;
			Swizzle4<Vector4, 0x55>     yyy;
			Swizzle4<Vector4, 0x56>     zyy;
			Swizzle4<Vector4, 0x57>     wyy;
			Swizzle4<Vector4, 0x58>     xzy;
			Swizzle4<Vector4, 0x59>     yzy;
			Swizzle4<Vector4, 0x5A>     zzy;
			Swizzle4<Vector4, 0x5B>     wzy;
			Swizzle4<Vector4, 0x5C>     xwy;
			Swizzle4<Vector4, 0x5D>     ywy;
			Swizzle4<Vector4, 0x5E>     zwy;
			Swizzle4<Vector4, 0x5F>     wwy;
			Swizzle4<Vector4, 0xA0>     xxz;
			Swizzle4<Vector4, 0xA1>     yxz;
			Swizzle4<Vector4, 0xA2>     zxz;
			Swizzle4<Vector4, 0xA3>     wxz;
			SwizzleMask4<Vector4, 0xA4> xyz;
			Swizzle4<Vector4, 0xA5>     yyz;
			Swizzle4<Vector4, 0xA6>     zyz;
			Swizzle4<Vector4, 0xA7>     wyz;
			Swizzle4<Vector4, 0xA8>     xzz;
			Swizzle4<Vector4, 0xA9>     yzz;
			Swizzle4<Vector4, 0xAA>     zzz;
			Swizzle4<Vector4, 0xAB>     wzz;
			Swizzle4<Vector4, 0xAC>     xwz;
			Swizzle4<Vector4, 0xAD>     ywz;
			Swizzle4<Vector4, 0xAE>     zwz;
			Swizzle4<Vector4, 0xAF>     wwz;
			Swizzle4<Vector4, 0xF0>     xxw;
			Swizzle4<Vector4, 0xF1>     yxw;
			Swizzle4<Vector4, 0xF2>     zxw;
			Swizzle4<Vector4, 0xF3>     wxw;
			SwizzleMask4<Vector4, 0xF4> xyw;
			Swizzle4<Vector4, 0xF5>     yyw;
			Swizzle4<Vector4, 0xF6>     zyw;
			Swizzle4<Vector4, 0xF7>     wyw;
			SwizzleMask4<Vector4, 0xF8> xzw;
			SwizzleMask4<Vector4, 0xF9> yzw;
			Swizzle4<Vector4, 0xFA>     zzw;
			Swizzle4<Vector4, 0xFB>     wzw;
			Swizzle4<Vector4, 0xFC>     xww;
			Swizzle4<Vector4, 0xFD>     yww;
			Swizzle4<Vector4, 0xFE>     zww;
			Swizzle4<Vector4, 0xFF>     www;
			Swizzle4<Vector4, 0x00>     xxxx;
			Swizzle4<Vector4, 0x01>     yxxx;
			Swizzle4<Vector4, 0x02>     zxxx;
			Swizzle4<Vector4, 0x03>     wxxx;
			Swizzle4<Vector4, 0x04>     xyxx;
			Swizzle4<Vector4, 0x05>     yyxx;
			Swizzle4<Vector4, 0x06>     zyxx;
			Swizzle4<Vector4, 0x07>     wyxx;
			Swizzle4<Vector4, 0x08>     xzxx;
			Swizzle4<Vector4, 0x09>     yzxx;
			Swizzle4<Vector4, 0x0A>     zzxx;
			Swizzle4<Vector4, 0x0B>     wzxx;
			Swizzle4<Vector4, 0x0C>     xwxx;
			Swizzle4<Vector4, 0x0D>     ywxx;
			Swizzle4<Vector4, 0x0E>     zwxx;
			Swizzle4<Vector4, 0x0F>     wwxx;
			Swizzle4<Vector4, 0x10>     xxyx;
			Swizzle4<Vector4, 0x11>     yxyx;
			Swizzle4<Vector4, 0x12>     zxyx;
			Swizzle4<Vector4, 0x13>     wxyx;
			Swizzle4<Vector4, 0x14>     xyyx;
			Swizzle4<Vector4, 0x15>     yyyx;
			Swizzle4<Vector4, 0x16>     zyyx;
			Swizzle4<Vector4, 0x17>     wyyx;
			Swizzle4<Vector4, 0x18>     xzyx;
			Swizzle4<Vector4, 0x19>     yzyx;
			Swizzle4<Vector4, 0x1A>     zzyx;
			Swizzle4<Vector4, 0x1B>     wzyx;
			Swizzle4<Vector4, 0x1C>     xwyx;
			Swizzle4<Vector4, 0x1D>     ywyx;
			Swizzle4<Vector4, 0x1E>     zwyx;
			Swizzle4<Vector4, 0x1F>     wwyx;
			Swizzle4<Vector4, 0x20>     xxzx;
			Swizzle4<Vector4, 0x21>     yxzx;
			Swizzle4<Vector4, 0x22>     zxzx;
			Swizzle4<Vector4, 0x23>     wxzx;
			Swizzle4<Vector4, 0x24>     xyzx;
			Swizzle4<Vector4, 0x25>     yyzx;
			Swizzle4<Vector4, 0x26>     zyzx;
			Swizzle4<Vector4, 0x27>     wyzx;
			Swizzle4<Vector4, 0x28>     xzzx;
			Swizzle4<Vector4, 0x29>     yzzx;
			Swizzle4<Vector4, 0x2A>     zzzx;
			Swizzle4<Vector4, 0x2B>     wzzx;
			Swizzle4<Vector4, 0x2C>     xwzx;
			Swizzle4<Vector4, 0x2D>     ywzx;
			Swizzle4<Vector4, 0x2E>     zwzx;
			Swizzle4<Vector4, 0x2F>     wwzx;
			Swizzle4<Vector4, 0x30>     xxwx;
			Swizzle4<Vector4, 0x31>     yxwx;
			Swizzle4<Vector4, 0x32>     zxwx;
			Swizzle4<Vector4, 0x33>     wxwx;
			Swizzle4<Vector4, 0x34>     xywx;
			Swizzle4<Vector4, 0x35>     yywx;
			Swizzle4<Vector4, 0x36>     zywx;
			Swizzle4<Vector4, 0x37>     wywx;
			Swizzle4<Vector4, 0x38>     xzwx;
			Swizzle4<Vector4, 0x39>     yzwx;
			Swizzle4<Vector4, 0x3A>     zzwx;
			Swizzle4<Vector4, 0x3B>     wzwx;
			Swizzle4<Vector4, 0x3C>     xwwx;
			Swizzle4<Vector4, 0x3D>     ywwx;
			Swizzle4<Vector4, 0x3E>     zwwx;
			Swizzle4<Vector4, 0x3F>     wwwx;
			Swizzle4<Vector4, 0x40>     xxxy;
			Swizzle4<Vector4, 0x41>     yxxy;
			Swizzle4<Vector4, 0x42>     zxxy;
			Swizzle4<Vector4, 0x43>     wxxy;
			Swizzle4<Vector4, 0x44>     xyxy;
			Swizzle4<Vector4, 0x45>     yyxy;
			Swizzle4<Vector4, 0x46>     zyxy;
			Swizzle4<Vector4, 0x47>     wyxy;
			Swizzle4<Vector4, 0x48>     xzxy;
			Swizzle4<Vector4, 0x49>     yzxy;
			Swizzle4<Vector4, 0x4A>     zzxy;
			Swizzle4<Vector4, 0x4B>     wzxy;
			Swizzle4<Vector4, 0x4C>     xwxy;
			Swizzle4<Vector4, 0x4D>     ywxy;
			Swizzle4<Vector4, 0x4E>     zwxy;
			Swizzle4<Vector4, 0x4F>     wwxy;
			Swizzle4<Vector4, 0x50>     xxyy;
			Swizzle4<Vector4, 0x51>     yxyy;
			Swizzle4<Vector4, 0x52>     zxyy;
			Swizzle4<Vector4, 0x53>     wxyy;
			Swizzle4<Vector4, 0x54>     xyyy;
			Swizzle4<Vector4, 0x55>     yyyy;
			Swizzle4<Vector4, 0x56>     zyyy;
			Swizzle4<Vector4, 0x57>     wyyy;
			Swizzle4<Vector4, 0x58>     xzyy;
			Swizzle4<Vector4, 0x59>     yzyy;
			Swizzle4<Vector4, 0x5A>     zzyy;
			Swizzle4<Vector4, 0x5B>     wzyy;
			Swizzle4<Vector4, 0x5C>     xwyy;
			Swizzle4<Vector4, 0x5D>     ywyy;
			Swizzle4<Vector4, 0x5E>     zwyy;
			Swizzle4<Vector4, 0x5F>     wwyy;
			Swizzle4<Vector4, 0x60>     xxzy;
			Swizzle4<Vector4, 0x61>     yxzy;
			Swizzle4<Vector4, 0x62>     zxzy;
			Swizzle4<Vector4, 0x63>     wxzy;
			Swizzle4<Vector4, 0x64>     xyzy;
			Swizzle4<Vector4, 0x65>     yyzy;
			Swizzle4<Vector4, 0x66>     zyzy;
			Swizzle4<Vector4, 0x67>     wyzy;
			Swizzle4<Vector4, 0x68>     xzzy;
			Swizzle4<Vector4, 0x69>     yzzy;
			Swizzle4<Vector4, 0x6A>     zzzy;
			Swizzle4<Vector4, 0x6B>     wzzy;
			Swizzle4<Vector4, 0x6C>     xwzy;
			Swizzle4<Vector4, 0x6D>     ywzy;
			Swizzle4<Vector4, 0x6E>     zwzy;
			Swizzle4<Vector4, 0x6F>     wwzy;
			Swizzle4<Vector4, 0x70>     xxwy;
			Swizzle4<Vector4, 0x71>     yxwy;
			Swizzle4<Vector4, 0x72>     zxwy;
			Swizzle4<Vector4, 0x73>     wxwy;
			Swizzle4<Vector4, 0x74>     xywy;
			Swizzle4<Vector4, 0x75>     yywy;
			Swizzle4<Vector4, 0x76>     zywy;
			Swizzle4<Vector4, 0x77>     wywy;
			Swizzle4<Vector4, 0x78>     xzwy;
			Swizzle4<Vector4, 0x79>     yzwy;
			Swizzle4<Vector4, 0x7A>     zzwy;
			Swizzle4<Vector4, 0x7B>     wzwy;
			Swizzle4<Vector4, 0x7C>     xwwy;
			Swizzle4<Vector4, 0x7D>     ywwy;
			Swizzle4<Vector4, 0x7E>     zwwy;
			Swizzle4<Vector4, 0x7F>     wwwy;
			Swizzle4<Vector4, 0x80>     xxxz;
			Swizzle4<Vector4, 0x81>     yxxz;
			Swizzle4<Vector4, 0x82>     zxxz;
			Swizzle4<Vector4, 0x83>     wxxz;
			Swizzle4<Vector4, 0x84>     xyxz;
			Swizzle4<Vector4, 0x85>     yyxz;
			Swizzle4<Vector4, 0x86>     zyxz;
			Swizzle4<Vector4, 0x87>     wyxz;
			Swizzle4<Vector4, 0x88>     xzxz;
			Swizzle4<Vector4, 0x89>     yzxz;
			Swizzle4<Vector4, 0x8A>     zzxz;
			Swizzle4<Vector4, 0x8B>     wzxz;
			Swizzle4<Vector4, 0x8C>     xwxz;
			Swizzle4<Vector4, 0x8D>     ywxz;
			Swizzle4<Vector4, 0x8E>     zwxz;
			Swizzle4<Vector4, 0x8F>     wwxz;
			Swizzle4<Vector4, 0x90>     xxyz;
			Swizzle4<Vector4, 0x91>     yxyz;
			Swizzle4<Vector4, 0x92>     zxyz;
			Swizzle4<Vector4, 0x93>     wxyz;
			Swizzle4<Vector4, 0x94>     xyyz;
			Swizzle4<Vector4, 0x95>     yyyz;
			Swizzle4<Vector4, 0x96>     zyyz;
			Swizzle4<Vector4, 0x97>     wyyz;
			Swizzle4<Vector4, 0x98>     xzyz;
			Swizzle4<Vector4, 0x99>     yzyz;
			Swizzle4<Vector4, 0x9A>     zzyz;
			Swizzle4<Vector4, 0x9B>     wzyz;
			Swizzle4<Vector4, 0x9C>     xwyz;
			Swizzle4<Vector4, 0x9D>     ywyz;
			Swizzle4<Vector4, 0x9E>     zwyz;
			Swizzle4<Vector4, 0x9F>     wwyz;
			Swizzle4<Vector4, 0xA0>     xxzz;
			Swizzle4<Vector4, 0xA1>     yxzz;
			Swizzle4<Vector4, 0xA2>     zxzz;
			Swizzle4<Vector4, 0xA3>     wxzz;
			Swizzle4<Vector4, 0xA4>     xyzz;
			Swizzle4<Vector4, 0xA5>     yyzz;
			Swizzle4<Vector4, 0xA6>     zyzz;
			Swizzle4<Vector4, 0xA7>     wyzz;
			Swizzle4<Vector4, 0xA8>     xzzz;
			Swizzle4<Vector4, 0xA9>     yzzz;
			Swizzle4<Vector4, 0xAA>     zzzz;
			Swizzle4<Vector4, 0xAB>     wzzz;
			Swizzle4<Vector4, 0xAC>     xwzz;
			Swizzle4<Vector4, 0xAD>     ywzz;
			Swizzle4<Vector4, 0xAE>     zwzz;
			Swizzle4<Vector4, 0xAF>     wwzz;
			Swizzle4<Vector4, 0xB0>     xxwz;
			Swizzle4<Vector4, 0xB1>     yxwz;
			Swizzle4<Vector4, 0xB2>     zxwz;
			Swizzle4<Vector4, 0xB3>     wxwz;
			Swizzle4<Vector4, 0xB4>     xywz;
			Swizzle4<Vector4, 0xB5>     yywz;
			Swizzle4<Vector4, 0xB6>     zywz;
			Swizzle4<Vector4, 0xB7>     wywz;
			Swizzle4<Vector4, 0xB8>     xzwz;
			Swizzle4<Vector4, 0xB9>     yzwz;
			Swizzle4<Vector4, 0xBA>     zzwz;
			Swizzle4<Vector4, 0xBB>     wzwz;
			Swizzle4<Vector4, 0xBC>     xwwz;
			Swizzle4<Vector4, 0xBD>     ywwz;
			Swizzle4<Vector4, 0xBE>     zwwz;
			Swizzle4<Vector4, 0xBF>     wwwz;
			Swizzle4<Vector4, 0xC0>     xxxw;
			Swizzle4<Vector4, 0xC1>     yxxw;
			Swizzle4<Vector4, 0xC2>     zxxw;
			Swizzle4<Vector4, 0xC3>     wxxw;
			Swizzle4<Vector4, 0xC4>     xyxw;
			Swizzle4<Vector4, 0xC5>     yyxw;
			Swizzle4<Vector4, 0xC6>     zyxw;
			Swizzle4<Vector4, 0xC7>     wyxw;
			Swizzle4<Vector4, 0xC8>     xzxw;
			Swizzle4<Vector4, 0xC9>     yzxw;
			Swizzle4<Vector4, 0xCA>     zzxw;
			Swizzle4<Vector4, 0xCB>     wzxw;
			Swizzle4<Vector4, 0xCC>     xwxw;
			Swizzle4<Vector4, 0xCD>     ywxw;
			Swizzle4<Vector4, 0xCE>     zwxw;
			Swizzle4<Vector4, 0xCF>     wwxw;
			Swizzle4<Vector4, 0xD0>     xxyw;
			Swizzle4<Vector4, 0xD1>     yxyw;
			Swizzle4<Vector4, 0xD2>     zxyw;
			Swizzle4<Vector4, 0xD3>     wxyw;
			Swizzle4<Vector4, 0xD4>     xyyw;
			Swizzle4<Vector4, 0xD5>     yyyw;
			Swizzle4<Vector4, 0xD6>     zyyw;
			Swizzle4<Vector4, 0xD7>     wyyw;
			Swizzle4<Vector4, 0xD8>     xzyw;
			Swizzle4<Vector4, 0xD9>     yzyw;
			Swizzle4<Vector4, 0xDA>     zzyw;
			Swizzle4<Vector4, 0xDB>     wzyw;
			Swizzle4<Vector4, 0xDC>     xwyw;
			Swizzle4<Vector4, 0xDD>     ywyw;
			Swizzle4<Vector4, 0xDE>     zwyw;
			Swizzle4<Vector4, 0xDF>     wwyw;
			Swizzle4<Vector4, 0xE0>     xxzw;
			Swizzle4<Vector4, 0xE1>     yxzw;
			Swizzle4<Vector4, 0xE2>     zxzw;
			Swizzle4<Vector4, 0xE3>     wxzw;
			SwizzleMask4<Vector4, 0xE4> xyzw;
			Swizzle4<Vector4, 0xE5>     yyzw;
			Swizzle4<Vector4, 0xE6>     zyzw;
			Swizzle4<Vector4, 0xE7>     wyzw;
			Swizzle4<Vector4, 0xE8>     xzzw;
			Swizzle4<Vector4, 0xE9>     yzzw;
			Swizzle4<Vector4, 0xEA>     zzzw;
			Swizzle4<Vector4, 0xEB>     wzzw;
			Swizzle4<Vector4, 0xEC>     xwzw;
			Swizzle4<Vector4, 0xED>     ywzw;
			Swizzle4<Vector4, 0xEE>     zwzw;
			Swizzle4<Vector4, 0xEF>     wwzw;
			Swizzle4<Vector4, 0xF0>     xxww;
			Swizzle4<Vector4, 0xF1>     yxww;
			Swizzle4<Vector4, 0xF2>     zxww;
			Swizzle4<Vector4, 0xF3>     wxww;
			Swizzle4<Vector4, 0xF4>     xyww;
			Swizzle4<Vector4, 0xF5>     yyww;
			Swizzle4<Vector4, 0xF6>     zyww;
			Swizzle4<Vector4, 0xF7>     wyww;
			Swizzle4<Vector4, 0xF8>     xzww;
			Swizzle4<Vector4, 0xF9>     yzww;
			Swizzle4<Vector4, 0xFA>     zzww;
			Swizzle4<Vector4, 0xFB>     wzww;
			Swizzle4<Vector4, 0xFC>     xwww;
			Swizzle4<Vector4, 0xFD>     ywww;
			Swizzle4<Vector4, 0xFE>     zwww;
			Swizzle4<Vector4, 0xFF>     wwww;
		};
	};

	class Int4 : public LValue<Int4>, public XYZW<Int4>
	{
	public:
		explicit Int4(RValue<Byte4> cast);
		explicit Int4(RValue<SByte4> cast);
		explicit Int4(RValue<Float4> cast);
		explicit Int4(RValue<Short4> cast);
		explicit Int4(RValue<UShort4> cast);

		Int4();
		Int4(int xyzw);
		Int4(int x, int yzw);
		Int4(int x, int y, int zw);
		Int4(int x, int y, int z, int w);
		Int4(RValue<Int4> rhs);
		Int4(const Int4 &rhs);
		Int4(const Reference<Int4> &rhs);
		Int4(RValue<UInt4> rhs);
		Int4(const UInt4 &rhs);
		Int4(const Reference<UInt4> &rhs);
		Int4(RValue<Int2> lo, RValue<Int2> hi);
		Int4(RValue<Int> rhs);
		Int4(const Int &rhs);
		Int4(const Reference<Int> &rhs);

		RValue<Int4> operator=(RValue<Int4> rhs);
		RValue<Int4> operator=(const Int4 &rhs);
		RValue<Int4> operator=(const Reference<Int4> &rhs);

		static Type *getType();

	private:
		void constant(int x, int y, int z, int w);
	};

	RValue<Int4> operator+(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator-(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator*(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator/(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator%(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator&(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator|(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator^(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator<<(RValue<Int4> lhs, unsigned char rhs);
	RValue<Int4> operator>>(RValue<Int4> lhs, unsigned char rhs);
	RValue<Int4> operator<<(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator>>(RValue<Int4> lhs, RValue<Int4> rhs);
	RValue<Int4> operator+=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator-=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator*=(Int4 &lhs, RValue<Int4> rhs);
//	RValue<Int4> operator/=(Int4 &lhs, RValue<Int4> rhs);
//	RValue<Int4> operator%=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator&=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator|=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator^=(Int4 &lhs, RValue<Int4> rhs);
	RValue<Int4> operator<<=(Int4 &lhs, unsigned char rhs);
	RValue<Int4> operator>>=(Int4 &lhs, unsigned char rhs);
	RValue<Int4> operator+(RValue<Int4> val);
	RValue<Int4> operator-(RValue<Int4> val);
	RValue<Int4> operator~(RValue<Int4> val);
//	RValue<Int4> operator++(Int4 &val, int);   // Post-increment
//	const Int4 &operator++(Int4 &val);   // Pre-increment
//	RValue<Int4> operator--(Int4 &val, int);   // Post-decrement
//	const Int4 &operator--(Int4 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<Int4> lhs, RValue<Int4> rhs);
//	RValue<Bool> operator<=(RValue<Int4> lhs, RValue<Int4> rhs);
//	RValue<Bool> operator>(RValue<Int4> lhs, RValue<Int4> rhs);
//	RValue<Bool> operator>=(RValue<Int4> lhs, RValue<Int4> rhs);
//	RValue<Bool> operator!=(RValue<Int4> lhs, RValue<Int4> rhs);
//	RValue<Bool> operator==(RValue<Int4> lhs, RValue<Int4> rhs);

	inline RValue<Int4> operator+(RValue<Int> lhs, RValue<Int4> rhs)
	{
		return Int4(lhs) + rhs;
	}

	inline RValue<Int4> operator+(RValue<Int4> lhs, RValue<Int> rhs)
	{
		return lhs + Int4(rhs);
	}

	RValue<Int4> CmpEQ(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> CmpLT(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> CmpLE(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> CmpNEQ(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> CmpNLT(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> CmpNLE(RValue<Int4> x, RValue<Int4> y);
	inline RValue<Int4> CmpGT(RValue<Int4> x, RValue<Int4> y) { return CmpNLE(x, y); }
	inline RValue<Int4> CmpGE(RValue<Int4> x, RValue<Int4> y) { return CmpNLT(x, y); }
	RValue<Int4> Max(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> Min(RValue<Int4> x, RValue<Int4> y);
	RValue<Int4> RoundInt(RValue<Float4> cast);
	RValue<Short8> PackSigned(RValue<Int4> x, RValue<Int4> y);
	RValue<UShort8> PackUnsigned(RValue<Int4> x, RValue<Int4> y);
	RValue<Int> Extract(RValue<Int4> val, int i);
	RValue<Int4> Insert(RValue<Int4> val, RValue<Int> element, int i);
	RValue<Int> SignMask(RValue<Int4> x);
	RValue<Int4> Swizzle(RValue<Int4> x, unsigned char select);
	RValue<Int4> MulHigh(RValue<Int4> x, RValue<Int4> y);

	class UInt4 : public LValue<UInt4>, public XYZW<UInt4>
	{
	public:
		explicit UInt4(RValue<Float4> cast);

		UInt4();
		UInt4(int xyzw);
		UInt4(int x, int yzw);
		UInt4(int x, int y, int zw);
		UInt4(int x, int y, int z, int w);
		UInt4(RValue<UInt4> rhs);
		UInt4(const UInt4 &rhs);
		UInt4(const Reference<UInt4> &rhs);
		UInt4(RValue<Int4> rhs);
		UInt4(const Int4 &rhs);
		UInt4(const Reference<Int4> &rhs);
		UInt4(RValue<UInt2> lo, RValue<UInt2> hi);
		UInt4(RValue<UInt> rhs);
		UInt4(const UInt &rhs);
		UInt4(const Reference<UInt> &rhs);

		RValue<UInt4> operator=(RValue<UInt4> rhs);
		RValue<UInt4> operator=(const UInt4 &rhs);
		RValue<UInt4> operator=(const Reference<UInt4> &rhs);

		static Type *getType();

	private:
		void constant(int x, int y, int z, int w);
	};

	RValue<UInt4> operator+(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator-(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator*(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator/(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator%(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator&(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator|(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator^(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator<<(RValue<UInt4> lhs, unsigned char rhs);
	RValue<UInt4> operator>>(RValue<UInt4> lhs, unsigned char rhs);
	RValue<UInt4> operator<<(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator>>(RValue<UInt4> lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator+=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator-=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator*=(UInt4 &lhs, RValue<UInt4> rhs);
//	RValue<UInt4> operator/=(UInt4 &lhs, RValue<UInt4> rhs);
//	RValue<UInt4> operator%=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator&=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator|=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator^=(UInt4 &lhs, RValue<UInt4> rhs);
	RValue<UInt4> operator<<=(UInt4 &lhs, unsigned char rhs);
	RValue<UInt4> operator>>=(UInt4 &lhs, unsigned char rhs);
	RValue<UInt4> operator+(RValue<UInt4> val);
	RValue<UInt4> operator-(RValue<UInt4> val);
	RValue<UInt4> operator~(RValue<UInt4> val);
//	RValue<UInt4> operator++(UInt4 &val, int);   // Post-increment
//	const UInt4 &operator++(UInt4 &val);   // Pre-increment
//	RValue<UInt4> operator--(UInt4 &val, int);   // Post-decrement
//	const UInt4 &operator--(UInt4 &val);   // Pre-decrement
//	RValue<Bool> operator<(RValue<UInt4> lhs, RValue<UInt4> rhs);
//	RValue<Bool> operator<=(RValue<UInt4> lhs, RValue<UInt4> rhs);
//	RValue<Bool> operator>(RValue<UInt4> lhs, RValue<UInt4> rhs);
//	RValue<Bool> operator>=(RValue<UInt4> lhs, RValue<UInt4> rhs);
//	RValue<Bool> operator!=(RValue<UInt4> lhs, RValue<UInt4> rhs);
//	RValue<Bool> operator==(RValue<UInt4> lhs, RValue<UInt4> rhs);

	RValue<UInt4> CmpEQ(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> CmpLT(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> CmpLE(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> CmpNEQ(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> CmpNLT(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> CmpNLE(RValue<UInt4> x, RValue<UInt4> y);
	inline RValue<UInt4> CmpGT(RValue<UInt4> x, RValue<UInt4> y) { return CmpNLE(x, y); }
	inline RValue<UInt4> CmpGE(RValue<UInt4> x, RValue<UInt4> y) { return CmpNLT(x, y); }
	RValue<UInt4> Max(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> Min(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt4> MulHigh(RValue<UInt4> x, RValue<UInt4> y);
	RValue<UInt> Extract(RValue<UInt4> val, int i);
	RValue<UInt4> Insert(RValue<UInt4> val, RValue<UInt> element, int i);
//	RValue<UInt4> RoundInt(RValue<Float4> cast);
	RValue<UInt4> Swizzle(RValue<UInt4> x, unsigned char select);

	class Half : public LValue<Half>
	{
	public:
		explicit Half(RValue<Float> cast);

		static Type *getType();
	};

	class Float : public LValue<Float>
	{
	public:
		explicit Float(RValue<Int> cast);
		explicit Float(RValue<UInt> cast);
		explicit Float(RValue<Half> cast);

		Float() = default;
		Float(float x);
		Float(RValue<Float> rhs);
		Float(const Float &rhs);
		Float(const Reference<Float> &rhs);
		Float(Argument<Float> argument);

		template<int T>
		Float(const SwizzleMask1<Float4, T> &rhs);

	//	RValue<Float> operator=(float rhs);   // FIXME: Implement
		RValue<Float> operator=(RValue<Float> rhs);
		RValue<Float> operator=(const Float &rhs);
		RValue<Float> operator=(const Reference<Float> &rhs);

		template<int T>
		RValue<Float> operator=(const SwizzleMask1<Float4, T> &rhs);

		static Type *getType();
	};

	RValue<Float> operator+(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Float> operator-(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Float> operator*(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Float> operator/(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Float> operator+=(Float &lhs, RValue<Float> rhs);
	RValue<Float> operator-=(Float &lhs, RValue<Float> rhs);
	RValue<Float> operator*=(Float &lhs, RValue<Float> rhs);
	RValue<Float> operator/=(Float &lhs, RValue<Float> rhs);
	RValue<Float> operator+(RValue<Float> val);
	RValue<Float> operator-(RValue<Float> val);
	RValue<Bool> operator<(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Bool> operator<=(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Bool> operator>(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Bool> operator>=(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Bool> operator!=(RValue<Float> lhs, RValue<Float> rhs);
	RValue<Bool> operator==(RValue<Float> lhs, RValue<Float> rhs);

	RValue<Float> Abs(RValue<Float> x);
	RValue<Float> Max(RValue<Float> x, RValue<Float> y);
	RValue<Float> Min(RValue<Float> x, RValue<Float> y);
	RValue<Float> Rcp_pp(RValue<Float> val, bool exactAtPow2 = false);
	RValue<Float> RcpSqrt_pp(RValue<Float> val);
	RValue<Float> Sqrt(RValue<Float> x);

//	RValue<Int4> IsInf(RValue<Float> x);
//	RValue<Int4> IsNan(RValue<Float> x);
	RValue<Float> Round(RValue<Float> x);
	RValue<Float> Trunc(RValue<Float> x);
	RValue<Float> Frac(RValue<Float> x);
	RValue<Float> Floor(RValue<Float> x);
	RValue<Float> Ceil(RValue<Float> x);

	// Trigonometric functions
	// TODO: Currently unimplemented for Subzero.
//	RValue<Float> Sin(RValue<Float> x);
//	RValue<Float> Cos(RValue<Float> x);
//	RValue<Float> Tan(RValue<Float> x);
//	RValue<Float> Asin(RValue<Float> x);
//	RValue<Float> Acos(RValue<Float> x);
//	RValue<Float> Atan(RValue<Float> x);
//	RValue<Float> Sinh(RValue<Float> x);
//	RValue<Float> Cosh(RValue<Float> x);
//	RValue<Float> Tanh(RValue<Float> x);
//	RValue<Float> Asinh(RValue<Float> x);
//	RValue<Float> Acosh(RValue<Float> x);
//	RValue<Float> Atanh(RValue<Float> x);
//	RValue<Float> Atan2(RValue<Float> x, RValue<Float> y);

	// Exponential functions
	// TODO: Currently unimplemented for Subzero.
//	RValue<Float> Pow(RValue<Float> x, RValue<Float> y);
//	RValue<Float> Exp(RValue<Float> x);
//	RValue<Float> Log(RValue<Float> x);
	RValue<Float> Exp2(RValue<Float> x);
	RValue<Float> Log2(RValue<Float> x);

	class Float2 : public LValue<Float2>
	{
	public:
	//	explicit Float2(RValue<Byte2> cast);
	//	explicit Float2(RValue<Short2> cast);
	//	explicit Float2(RValue<UShort2> cast);
	//	explicit Float2(RValue<Int2> cast);
	//	explicit Float2(RValue<UInt2> cast);
		explicit Float2(RValue<Float4> cast);

		Float2() = default;
	//	Float2(float x, float y);
	//	Float2(RValue<Float2> rhs);
	//	Float2(const Float2 &rhs);
	//	Float2(const Reference<Float2> &rhs);
	//	Float2(RValue<Float> rhs);
	//	Float2(const Float &rhs);
	//	Float2(const Reference<Float> &rhs);

	//	template<int T>
	//	Float2(const SwizzleMask1<T> &rhs);

	//	RValue<Float2> operator=(float replicate);
	//	RValue<Float2> operator=(RValue<Float2> rhs);
	//	RValue<Float2> operator=(const Float2 &rhs);
	//	RValue<Float2> operator=(const Reference<Float2> &rhs);
	//	RValue<Float2> operator=(RValue<Float> rhs);
	//	RValue<Float2> operator=(const Float &rhs);
	//	RValue<Float2> operator=(const Reference<Float> &rhs);

	//	template<int T>
	//	RValue<Float2> operator=(const SwizzleMask1<T> &rhs);

		static Type *getType();
	};

//	RValue<Float2> operator+(RValue<Float2> lhs, RValue<Float2> rhs);
//	RValue<Float2> operator-(RValue<Float2> lhs, RValue<Float2> rhs);
//	RValue<Float2> operator*(RValue<Float2> lhs, RValue<Float2> rhs);
//	RValue<Float2> operator/(RValue<Float2> lhs, RValue<Float2> rhs);
//	RValue<Float2> operator%(RValue<Float2> lhs, RValue<Float2> rhs);
//	RValue<Float2> operator+=(Float2 &lhs, RValue<Float2> rhs);
//	RValue<Float2> operator-=(Float2 &lhs, RValue<Float2> rhs);
//	RValue<Float2> operator*=(Float2 &lhs, RValue<Float2> rhs);
//	RValue<Float2> operator/=(Float2 &lhs, RValue<Float2> rhs);
//	RValue<Float2> operator%=(Float2 &lhs, RValue<Float2> rhs);
//	RValue<Float2> operator+(RValue<Float2> val);
//	RValue<Float2> operator-(RValue<Float2> val);

//	RValue<Float2> Abs(RValue<Float2> x);
//	RValue<Float2> Max(RValue<Float2> x, RValue<Float2> y);
//	RValue<Float2> Min(RValue<Float2> x, RValue<Float2> y);
//	RValue<Float2> Swizzle(RValue<Float2> x, unsigned char select);
//	RValue<Float2> Mask(Float2 &lhs, RValue<Float2> rhs, unsigned char select);

	class Float4 : public LValue<Float4>, public XYZW<Float4>
	{
	public:
		explicit Float4(RValue<Byte4> cast);
		explicit Float4(RValue<SByte4> cast);
		explicit Float4(RValue<Short4> cast);
		explicit Float4(RValue<UShort4> cast);
		explicit Float4(RValue<Int4> cast);
		explicit Float4(RValue<UInt4> cast);

		Float4();
		Float4(float xyzw);
		Float4(float x, float yzw);
		Float4(float x, float y, float zw);
		Float4(float x, float y, float z, float w);
		Float4(RValue<Float4> rhs);
		Float4(const Float4 &rhs);
		Float4(const Reference<Float4> &rhs);
		Float4(RValue<Float> rhs);
		Float4(const Float &rhs);
		Float4(const Reference<Float> &rhs);

		template<int T>
		Float4(const SwizzleMask1<Float4, T> &rhs);
		template<int T>
		Float4(const Swizzle4<Float4, T> &rhs);
		template<int X, int Y>
		Float4(const Swizzle2<Float4, X> &x, const Swizzle2<Float4, Y> &y);
		template<int X, int Y>
		Float4(const SwizzleMask2<Float4, X> &x, const Swizzle2<Float4, Y> &y);
		template<int X, int Y>
		Float4(const Swizzle2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y);
		template<int X, int Y>
		Float4(const SwizzleMask2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y);

		RValue<Float4> operator=(float replicate);
		RValue<Float4> operator=(RValue<Float4> rhs);
		RValue<Float4> operator=(const Float4 &rhs);
		RValue<Float4> operator=(const Reference<Float4> &rhs);
		RValue<Float4> operator=(RValue<Float> rhs);
		RValue<Float4> operator=(const Float &rhs);
		RValue<Float4> operator=(const Reference<Float> &rhs);

		template<int T>
		RValue<Float4> operator=(const SwizzleMask1<Float4, T> &rhs);
		template<int T>
		RValue<Float4> operator=(const Swizzle4<Float4, T> &rhs);

		static Type *getType();

	private:
		void constant(float x, float y, float z, float w);
	};

	RValue<Float4> operator+(RValue<Float4> lhs, RValue<Float4> rhs);
	RValue<Float4> operator-(RValue<Float4> lhs, RValue<Float4> rhs);
	RValue<Float4> operator*(RValue<Float4> lhs, RValue<Float4> rhs);
	RValue<Float4> operator/(RValue<Float4> lhs, RValue<Float4> rhs);
	RValue<Float4> operator%(RValue<Float4> lhs, RValue<Float4> rhs);
	RValue<Float4> operator+=(Float4 &lhs, RValue<Float4> rhs);
	RValue<Float4> operator-=(Float4 &lhs, RValue<Float4> rhs);
	RValue<Float4> operator*=(Float4 &lhs, RValue<Float4> rhs);
	RValue<Float4> operator/=(Float4 &lhs, RValue<Float4> rhs);
	RValue<Float4> operator%=(Float4 &lhs, RValue<Float4> rhs);
	RValue<Float4> operator+(RValue<Float4> val);
	RValue<Float4> operator-(RValue<Float4> val);

	RValue<Float4> Abs(RValue<Float4> x);
	RValue<Float4> Max(RValue<Float4> x, RValue<Float4> y);
	RValue<Float4> Min(RValue<Float4> x, RValue<Float4> y);
	RValue<Float4> Rcp_pp(RValue<Float4> val, bool exactAtPow2 = false);
	RValue<Float4> RcpSqrt_pp(RValue<Float4> val);
	RValue<Float4> Sqrt(RValue<Float4> x);
	RValue<Float4> Insert(RValue<Float4> val, RValue<Float> element, int i);
	RValue<Float> Extract(RValue<Float4> x, int i);
	RValue<Float4> Swizzle(RValue<Float4> x, unsigned char select);
	RValue<Float4> ShuffleLowHigh(RValue<Float4> x, RValue<Float4> y, unsigned char imm);
	RValue<Float4> UnpackLow(RValue<Float4> x, RValue<Float4> y);
	RValue<Float4> UnpackHigh(RValue<Float4> x, RValue<Float4> y);
	RValue<Float4> Mask(Float4 &lhs, RValue<Float4> rhs, unsigned char select);
	RValue<Int> SignMask(RValue<Float4> x);

	// Ordered comparison functions
	RValue<Int4> CmpEQ(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpLT(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpLE(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpNEQ(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpNLT(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpNLE(RValue<Float4> x, RValue<Float4> y);
	inline RValue<Int4> CmpGT(RValue<Float4> x, RValue<Float4> y) { return CmpNLE(x, y); }
	inline RValue<Int4> CmpGE(RValue<Float4> x, RValue<Float4> y) { return CmpNLT(x, y); }

	// Unordered comparison functions
	RValue<Int4> CmpUEQ(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpULT(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpULE(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpUNEQ(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpUNLT(RValue<Float4> x, RValue<Float4> y);
	RValue<Int4> CmpUNLE(RValue<Float4> x, RValue<Float4> y);
	inline RValue<Int4> CmpUGT(RValue<Float4> x, RValue<Float4> y) { return CmpUNLE(x, y); }
	inline RValue<Int4> CmpUGE(RValue<Float4> x, RValue<Float4> y) { return CmpUNLT(x, y); }

	RValue<Int4> IsInf(RValue<Float4> x);
	RValue<Int4> IsNan(RValue<Float4> x);
	RValue<Float4> Round(RValue<Float4> x);
	RValue<Float4> Trunc(RValue<Float4> x);
	RValue<Float4> Frac(RValue<Float4> x);
	RValue<Float4> Floor(RValue<Float4> x);
	RValue<Float4> Ceil(RValue<Float4> x);

	// Trigonometric functions
	// TODO: Currently unimplemented for Subzero.
	RValue<Float4> Sin(RValue<Float4> x);
	RValue<Float4> Cos(RValue<Float4> x);
	RValue<Float4> Tan(RValue<Float4> x);
	RValue<Float4> Asin(RValue<Float4> x);
	RValue<Float4> Acos(RValue<Float4> x);
	RValue<Float4> Atan(RValue<Float4> x);
	RValue<Float4> Sinh(RValue<Float4> x);
	RValue<Float4> Cosh(RValue<Float4> x);
	RValue<Float4> Tanh(RValue<Float4> x);
	RValue<Float4> Asinh(RValue<Float4> x);
	RValue<Float4> Acosh(RValue<Float4> x);
	RValue<Float4> Atanh(RValue<Float4> x);
	RValue<Float4> Atan2(RValue<Float4> x, RValue<Float4> y);

	// Exponential functions
	// TODO: Currently unimplemented for Subzero.
	RValue<Float4> Pow(RValue<Float4> x, RValue<Float4> y);
	RValue<Float4> Exp(RValue<Float4> x);
	RValue<Float4> Log(RValue<Float4> x);
	RValue<Float4> Exp2(RValue<Float4> x);
	RValue<Float4> Log2(RValue<Float4> x);

	// Bit Manipulation functions.
	// TODO: Currently unimplemented for Subzero.

	// Count leading zeros.
	// Returns 32 when: isZeroUndef && x == 0.
	// Returns an undefined value when: !isZeroUndef && x == 0.
	RValue<UInt> Ctlz(RValue<UInt> x, bool isZeroUndef);
	RValue<UInt4> Ctlz(RValue<UInt4> x, bool isZeroUndef);

	// Count trailing zeros.
	// Returns 32 when: isZeroUndef && x == 0.
	// Returns an undefined value when: !isZeroUndef && x == 0.
	RValue<UInt> Cttz(RValue<UInt> x, bool isZeroUndef);
	RValue<UInt4> Cttz(RValue<UInt4> x, bool isZeroUndef);

	template<class T>
	class Pointer : public LValue<Pointer<T>>
	{
	public:
		template<class S>
		Pointer(RValue<Pointer<S>> pointerS, int alignment = 1) : alignment(alignment)
		{
			Value *pointerT = Nucleus::createBitCast(pointerS.value, Nucleus::getPointerType(T::getType()));
			LValue<Pointer<T>>::storeValue(pointerT);
		}

		template<class S>
		Pointer(const Pointer<S> &pointer, int alignment = 1) : alignment(alignment)
		{
			Value *pointerS = pointer.loadValue();
			Value *pointerT = Nucleus::createBitCast(pointerS, Nucleus::getPointerType(T::getType()));
			LValue<Pointer<T>>::storeValue(pointerT);
		}

		Pointer(Argument<Pointer<T>> argument);

		Pointer();
		Pointer(RValue<Pointer<T>> rhs);
		Pointer(const Pointer<T> &rhs);
		Pointer(const Reference<Pointer<T>> &rhs);
		Pointer(std::nullptr_t);

		RValue<Pointer<T>> operator=(RValue<Pointer<T>> rhs);
		RValue<Pointer<T>> operator=(const Pointer<T> &rhs);
		RValue<Pointer<T>> operator=(const Reference<Pointer<T>> &rhs);
		RValue<Pointer<T>> operator=(std::nullptr_t);

		Reference<T> operator*();
		Reference<T> operator[](int index);
		Reference<T> operator[](unsigned int index);
		Reference<T> operator[](RValue<Int> index);
		Reference<T> operator[](RValue<UInt> index);

		static Type *getType();

	private:
		const int alignment;
	};

	RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, int offset);
	RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<Int> offset);
	RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<UInt> offset);
	RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, int offset);
	RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, RValue<Int> offset);
	RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, RValue<UInt> offset);

	RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, int offset);
	RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, RValue<Int> offset);
	RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, RValue<UInt> offset);
	RValue<Pointer<Byte>> operator-=(Pointer<Byte> &lhs, int offset);
	RValue<Pointer<Byte>> operator-=(Pointer<Byte> &lhs, RValue<Int> offset);
	RValue<Pointer<Byte>> operator-=(Pointer<Byte> &lhs, RValue<UInt> offset);

	template <typename T>
	RValue<Bool> operator==(const Pointer<T> &lhs, const Pointer<T> &rhs)
	{
		return RValue<Bool>(Nucleus::createPtrEQ(lhs.loadValue(), rhs.loadValue()));
	}

	template<typename T>
	RValue<T> Load(RValue<Pointer<T>> pointer, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
	{
		return RValue<T>(Nucleus::createLoad(pointer.value, T::getType(), false, alignment, atomic, memoryOrder));
	}

	template<typename T>
	RValue<T> Load(Pointer<T> pointer, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
	{
		return Load(RValue<Pointer<T>>(pointer), alignment, atomic, memoryOrder);
	}

	// TODO: Use SIMD to template these.
	RValue<Float4> MaskedLoad(RValue<Pointer<Float4>> base, RValue<Int4> mask, unsigned int alignment, bool zeroMaskedLanes = false);
	RValue<Int4> MaskedLoad(RValue<Pointer<Int4>> base, RValue<Int4> mask, unsigned int alignment, bool zeroMaskedLanes = false);
	void MaskedStore(RValue<Pointer<Float4>> base, RValue<Float4> val, RValue<Int4> mask, unsigned int alignment);
	void MaskedStore(RValue<Pointer<Int4>> base, RValue<Int4> val, RValue<Int4> mask, unsigned int alignment);

	RValue<Float4> Gather(RValue<Pointer<Float>> base, RValue<Int4> offsets, RValue<Int4> mask, unsigned int alignment, bool zeroMaskedLanes = false);
	RValue<Int4> Gather(RValue<Pointer<Int>> base, RValue<Int4> offsets, RValue<Int4> mask, unsigned int alignment, bool zeroMaskedLanes = false);
	void Scatter(RValue<Pointer<Float>> base, RValue<Float4> val, RValue<Int4> offsets, RValue<Int4> mask, unsigned int alignment);
	void Scatter(RValue<Pointer<Int>> base, RValue<Int4> val, RValue<Int4> offsets, RValue<Int4> mask, unsigned int alignment);

	template<typename T>
	void Store(RValue<T> value, RValue<Pointer<T>> pointer, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
	{
		Nucleus::createStore(value.value, pointer.value, T::getType(), false, alignment, atomic, memoryOrder);
	}

	template<typename T>
	void Store(RValue<T> value, Pointer<T> pointer, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
	{
		Store(value, RValue<Pointer<T>>(pointer), alignment, atomic, memoryOrder);
	}

	template<typename T>
	void Store(T value, Pointer<T> pointer, unsigned int alignment, bool atomic, std::memory_order memoryOrder)
	{
		Store(RValue<T>(value), RValue<Pointer<T>>(pointer), alignment, atomic, memoryOrder);
	}

	// Fence adds a memory barrier that enforces ordering constraints on memory
	// operations. memoryOrder can only be one of:
	// std::memory_order_acquire, std::memory_order_release,
	// std::memory_order_acq_rel, or std::memory_order_seq_cst.
	void Fence(std::memory_order memoryOrder);

	template<class T, int S = 1>
	class Array : public LValue<T>
	{
	public:
		Array(int size = S);

		Reference<T> operator[](int index);
		Reference<T> operator[](unsigned int index);
		Reference<T> operator[](RValue<Int> index);
		Reference<T> operator[](RValue<UInt> index);

		// self() returns the this pointer to this Array object.
		// This function exists because operator&() is overloaded by LValue<T>.
		inline Array* self() { return this; }
	};

//	RValue<Array<T>> operator++(Array<T> &val, int);   // Post-increment
//	const Array<T> &operator++(Array<T> &val);   // Pre-increment
//	RValue<Array<T>> operator--(Array<T> &val, int);   // Post-decrement
//	const Array<T> &operator--(Array<T> &val);   // Pre-decrement

	void branch(RValue<Bool> cmp, BasicBlock *bodyBB, BasicBlock *endBB);

	// ValueOf returns a rr::Value* for the given C-type, RValue<T>, LValue<T>
	// or Reference<T>.
	template <typename T>
	inline Value* ValueOf(const T &v)
	{
		return ReactorType<T>(v).loadValue();
	}

	void Return();

	template<class T>
	void Return(const T &ret)
	{
		static_assert(CanBeUsedAsReturn< ReactorType<T> >::value, "Unsupported type for Return()");
		Nucleus::createRet(ValueOf<T>(ret));
		// Place any unreachable instructions in an unreferenced block.
		Nucleus::setInsertBlock(Nucleus::createBasicBlock());
	}

	// Generic template, leave undefined!
	template<typename FunctionType>
	class Function;

	// Specialized for function types
	template<typename Return, typename... Arguments>
	class Function<Return(Arguments...)>
	{
		// Static assert that the function signature is valid.
		static_assert(sizeof(AssertFunctionSignatureIsValid<Return(Arguments...)>) >= 0, "Invalid function signature");

	public:
		Function();

		virtual ~Function();

		template<int index>
		Argument<typename std::tuple_element<index, std::tuple<Arguments...>>::type> Arg() const
		{
			Value *arg = Nucleus::getArgument(index);
			return Argument<typename std::tuple_element<index, std::tuple<Arguments...>>::type>(arg);
		}

		std::shared_ptr<Routine> operator()(const char *name, ...);
		std::shared_ptr<Routine> operator()(const Config::Edit &cfg, const char *name, ...);

	protected:
		Nucleus *core;
		std::vector<Type*> arguments;
	};

	template<typename Return>
	class Function<Return()> : public Function<Return(Void)>
	{
	};

	RValue<Long> Ticks();
}

namespace rr
{
	template<class T>
	LValue<T>::LValue(int arraySize) : Variable(T::getType(), arraySize)
	{
#ifdef ENABLE_RR_DEBUG_INFO
		materialize();
#endif // ENABLE_RR_DEBUG_INFO
	}

	inline void Variable::materialize() const
	{
		if(!address)
		{
			address = Nucleus::allocateStackVariable(type, arraySize);
			RR_DEBUG_INFO_EMIT_VAR(address);

			if(rvalue)
			{
				storeValue(rvalue);
				rvalue = nullptr;
			}
		}
	}

	inline Value *Variable::loadValue() const
	{
		if(rvalue)
		{
			return rvalue;
		}

		if(!address)
		{
			// TODO: Return undef instead.
			materialize();
		}

		return Nucleus::createLoad(address, type, false, 0);
	}

	inline Value *Variable::storeValue(Value *value) const
	{
		if(address)
		{
			return Nucleus::createStore(value, address, type, false, 0);
		}

		rvalue = value;

		return value;
	}

	inline Value *Variable::getBaseAddress() const
	{
		materialize();

		return address;
	}

	inline Value *Variable::getElementPointer(Value *index, bool unsignedIndex) const
	{
		return Nucleus::createGEP(getBaseAddress(), type, index, unsignedIndex);
	}

	template<class T>
	RValue<Pointer<T>> LValue<T>::operator&()
	{
		return RValue<Pointer<T>>(getBaseAddress());
	}

	template<class T>
	Reference<T>::Reference(Value *pointer, int alignment) : alignment(alignment)
	{
		address = pointer;
	}

	template<class T>
	RValue<T> Reference<T>::operator=(RValue<T> rhs) const
	{
		Nucleus::createStore(rhs.value, address, T::getType(), false, alignment);

		return rhs;
	}

	template<class T>
	RValue<T> Reference<T>::operator=(const Reference<T> &ref) const
	{
		Value *tmp = Nucleus::createLoad(ref.address, T::getType(), false, ref.alignment);
		Nucleus::createStore(tmp, address, T::getType(), false, alignment);

		return RValue<T>(tmp);
	}

	template<class T>
	RValue<T> Reference<T>::operator+=(RValue<T> rhs) const
	{
		return *this = *this + rhs;
	}

	template<class T>
	Value *Reference<T>::loadValue() const
	{
		return Nucleus::createLoad(address, T::getType(), false, alignment);
	}

	template<class T>
	int Reference<T>::getAlignment() const
	{
		return alignment;
	}

#ifdef ENABLE_RR_DEBUG_INFO
	template<class T>
	RValue<T>::RValue(const RValue<T> &rvalue) : value(rvalue.value)
	{
		RR_DEBUG_INFO_EMIT_VAR(value);
	}
#endif // ENABLE_RR_DEBUG_INFO

	template<class T>
	RValue<T>::RValue(Value *rvalue)
	{
		assert(Nucleus::createBitCast(rvalue, T::getType()) == rvalue);   // Run-time type should match T, so bitcast is no-op.

		value = rvalue;
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class T>
	RValue<T>::RValue(const T &lvalue)
	{
		value = lvalue.loadValue();
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class T>
	RValue<T>::RValue(typename BoolLiteral<T>::type i)
	{
		value = Nucleus::createConstantBool(i);
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class T>
	RValue<T>::RValue(typename IntLiteral<T>::type i)
	{
		value = Nucleus::createConstantInt(i);
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class T>
	RValue<T>::RValue(typename FloatLiteral<T>::type f)
	{
		value = Nucleus::createConstantFloat(f);
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class T>
	RValue<T>::RValue(const Reference<T> &ref)
	{
		value = ref.loadValue();
		RR_DEBUG_INFO_EMIT_VAR(value);
	}

	template<class Vector4, int T>
	Swizzle2<Vector4, T>::operator RValue<Vector4>() const
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *vector = parent->loadValue();

		return Swizzle(RValue<Vector4>(vector), T);
	}

	template<class Vector4, int T>
	Swizzle4<Vector4, T>::operator RValue<Vector4>() const
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *vector = parent->loadValue();

		return Swizzle(RValue<Vector4>(vector), T);
	}

	template<class Vector4, int T>
	SwizzleMask4<Vector4, T>::operator RValue<Vector4>() const
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *vector = parent->loadValue();

		return Swizzle(RValue<Vector4>(vector), T);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask4<Vector4, T>::operator=(RValue<Vector4> rhs)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return Mask(*parent, rhs, T);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask4<Vector4, T>::operator=(RValue<typename Scalar<Vector4>::Type> rhs)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return Mask(*parent, Vector4(rhs), T);
	}

	template<class Vector4, int T>
	SwizzleMask1<Vector4, T>::operator RValue<typename Scalar<Vector4>::Type>() const   // FIXME: Call a non-template function
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return Extract(*parent, T & 0x3);
	}

	template<class Vector4, int T>
	SwizzleMask1<Vector4, T>::operator RValue<Vector4>() const
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *vector = parent->loadValue();

		return Swizzle(RValue<Vector4>(vector), T);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask1<Vector4, T>::operator=(float x)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return *parent = Insert(*parent, Float(x), T & 0x3);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask1<Vector4, T>::operator=(RValue<Vector4> rhs)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return Mask(*parent, Float4(rhs), T);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask1<Vector4, T>::operator=(RValue<typename Scalar<Vector4>::Type> rhs)   // FIXME: Call a non-template function
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return *parent = Insert(*parent, rhs, T & 0x3);
	}

	template<class Vector4, int T>
	SwizzleMask2<Vector4, T>::operator RValue<Vector4>() const
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *vector = parent->loadValue();

		return Swizzle(RValue<Float4>(vector), T);
	}

	template<class Vector4, int T>
	RValue<Vector4> SwizzleMask2<Vector4, T>::operator=(RValue<Vector4> rhs)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return Mask(*parent, Float4(rhs), T);
	}

	template<int T>
	Float::Float(const SwizzleMask1<Float4, T> &rhs)
	{
		*this = rhs.operator RValue<Float>();
	}

	template<int T>
	RValue<Float> Float::operator=(const SwizzleMask1<Float4, T> &rhs)
	{
		return *this = rhs.operator RValue<Float>();
	}

	template<int T>
	Float4::Float4(const SwizzleMask1<Float4, T> &rhs) : XYZW(this)
	{
		*this = rhs.operator RValue<Float4>();
	}

	template<int T>
	Float4::Float4(const Swizzle4<Float4, T> &rhs) : XYZW(this)
	{
		*this = rhs.operator RValue<Float4>();
	}

	template<int X, int Y>
	Float4::Float4(const Swizzle2<Float4, X> &x, const Swizzle2<Float4, Y> &y) : XYZW(this)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		*this = ShuffleLowHigh(*x.parent, *y.parent, (X & 0xF) | (Y & 0xF) << 4);
	}

	template<int X, int Y>
	Float4::Float4(const SwizzleMask2<Float4, X> &x, const Swizzle2<Float4, Y> &y) : XYZW(this)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		*this = ShuffleLowHigh(*x.parent, *y.parent, (X & 0xF) | (Y & 0xF) << 4);
	}

	template<int X, int Y>
	Float4::Float4(const Swizzle2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y) : XYZW(this)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		*this = ShuffleLowHigh(*x.parent, *y.parent, (X & 0xF) | (Y & 0xF) << 4);
	}

	template<int X, int Y>
	Float4::Float4(const SwizzleMask2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y) : XYZW(this)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		*this = ShuffleLowHigh(*x.parent, *y.parent, (X & 0xF) | (Y & 0xF) << 4);
	}

	template<int T>
	RValue<Float4> Float4::operator=(const SwizzleMask1<Float4, T> &rhs)
	{
		return *this = rhs.operator RValue<Float4>();
	}

	template<int T>
	RValue<Float4> Float4::operator=(const Swizzle4<Float4, T> &rhs)
	{
		return *this = rhs.operator RValue<Float4>();
	}

	template<class T>
	Pointer<T>::Pointer(Argument<Pointer<T>> argument) : alignment(1)
	{
		LValue<Pointer<T>>::materialize();  // FIXME(b/129757459)
		LValue<Pointer<T>>::storeValue(argument.value);
	}

	template<class T>
	Pointer<T>::Pointer() : alignment(1) {}

	template<class T>
	Pointer<T>::Pointer(RValue<Pointer<T>> rhs) : alignment(1)
	{
		LValue<Pointer<T>>::storeValue(rhs.value);
	}

	template<class T>
	Pointer<T>::Pointer(const Pointer<T> &rhs) : alignment(rhs.alignment)
	{
		Value *value = rhs.loadValue();
		LValue<Pointer<T>>::storeValue(value);
	}

	template<class T>
	Pointer<T>::Pointer(const Reference<Pointer<T>> &rhs) : alignment(rhs.getAlignment())
	{
		Value *value = rhs.loadValue();
		LValue<Pointer<T>>::storeValue(value);
	}

	template<class T>
	Pointer<T>::Pointer(std::nullptr_t) : alignment(1)
	{
		Value *value = Nucleus::createNullPointer(T::getType());
		LValue<Pointer<T>>::storeValue(value);
	}

	template<class T>
	RValue<Pointer<T>> Pointer<T>::operator=(RValue<Pointer<T>> rhs)
	{
		LValue<Pointer<T>>::storeValue(rhs.value);

		return rhs;
	}

	template<class T>
	RValue<Pointer<T>> Pointer<T>::operator=(const Pointer<T> &rhs)
	{
		Value *value = rhs.loadValue();
		LValue<Pointer<T>>::storeValue(value);

		return RValue<Pointer<T>>(value);
	}

	template<class T>
	RValue<Pointer<T>> Pointer<T>::operator=(const Reference<Pointer<T>> &rhs)
	{
		Value *value = rhs.loadValue();
		LValue<Pointer<T>>::storeValue(value);

		return RValue<Pointer<T>>(value);
	}

	template<class T>
	RValue<Pointer<T>> Pointer<T>::operator=(std::nullptr_t)
	{
		Value *value = Nucleus::createNullPointer(T::getType());
		LValue<Pointer<T>>::storeValue(value);

		return RValue<Pointer<T>>(this);
	}

	template<class T>
	Reference<T> Pointer<T>::operator*()
	{
		return Reference<T>(LValue<Pointer<T>>::loadValue(), alignment);
	}

	template<class T>
	Reference<T> Pointer<T>::operator[](int index)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *element = Nucleus::createGEP(LValue<Pointer<T>>::loadValue(), T::getType(), Nucleus::createConstantInt(index), false);

		return Reference<T>(element, alignment);
	}

	template<class T>
	Reference<T> Pointer<T>::operator[](unsigned int index)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *element = Nucleus::createGEP(LValue<Pointer<T>>::loadValue(), T::getType(), Nucleus::createConstantInt(index), true);

		return Reference<T>(element, alignment);
	}

	template<class T>
	Reference<T> Pointer<T>::operator[](RValue<Int> index)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *element = Nucleus::createGEP(LValue<Pointer<T>>::loadValue(), T::getType(), index.value, false);

		return Reference<T>(element, alignment);
	}

	template<class T>
	Reference<T> Pointer<T>::operator[](RValue<UInt> index)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *element = Nucleus::createGEP(LValue<Pointer<T>>::loadValue(), T::getType(), index.value, true);

		return Reference<T>(element, alignment);
	}

	template<class T>
	Type *Pointer<T>::getType()
	{
		return Nucleus::getPointerType(T::getType());
	}

	template<class T, int S>
	Array<T, S>::Array(int size) : LValue<T>(size)
	{
	}

	template<class T, int S>
	Reference<T> Array<T, S>::operator[](int index)
	{
		assert(index < this->arraySize);
		Value *element = LValue<T>::getElementPointer(Nucleus::createConstantInt(index), false);

		return Reference<T>(element);
	}

	template<class T, int S>
	Reference<T> Array<T, S>::operator[](unsigned int index)
	{
		assert(index < static_cast<unsigned int>(this->arraySize));
		Value *element = LValue<T>::getElementPointer(Nucleus::createConstantInt(index), true);

		return Reference<T>(element);
	}

	template<class T, int S>
	Reference<T> Array<T, S>::operator[](RValue<Int> index)
	{
		Value *element = LValue<T>::getElementPointer(index.value, false);

		return Reference<T>(element);
	}

	template<class T, int S>
	Reference<T> Array<T, S>::operator[](RValue<UInt> index)
	{
		Value *element = LValue<T>::getElementPointer(index.value, true);

		return Reference<T>(element);
	}

//	template<class T>
//	RValue<Array<T>> operator++(Array<T> &val, int)
//	{
//		// FIXME: Requires storing the address of the array
//	}

//	template<class T>
//	const Array<T> &operator++(Array<T> &val)
//	{
//		// FIXME: Requires storing the address of the array
//	}

//	template<class T>
//	RValue<Array<T>> operator--(Array<T> &val, int)
//	{
//		// FIXME: Requires storing the address of the array
//	}

//	template<class T>
//	const Array<T> &operator--(Array<T> &val)
//	{
//		// FIXME: Requires storing the address of the array
//	}

	template<class T>
	RValue<T> IfThenElse(RValue<Bool> condition, RValue<T> ifTrue, RValue<T> ifFalse)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return RValue<T>(Nucleus::createSelect(condition.value, ifTrue.value, ifFalse.value));
	}

	template<class T>
	RValue<T> IfThenElse(RValue<Bool> condition, const T &ifTrue, RValue<T> ifFalse)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *trueValue = ifTrue.loadValue();

		return RValue<T>(Nucleus::createSelect(condition.value, trueValue, ifFalse.value));
	}

	template<class T>
	RValue<T> IfThenElse(RValue<Bool> condition, RValue<T> ifTrue, const T &ifFalse)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *falseValue = ifFalse.loadValue();

		return RValue<T>(Nucleus::createSelect(condition.value, ifTrue.value, falseValue));
	}

	template<class T>
	RValue<T> IfThenElse(RValue<Bool> condition, const T &ifTrue, const T &ifFalse)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *trueValue = ifTrue.loadValue();
		Value *falseValue = ifFalse.loadValue();

		return RValue<T>(Nucleus::createSelect(condition.value, trueValue, falseValue));
	}

	template<typename Return, typename... Arguments>
	Function<Return(Arguments...)>::Function()
	{
		core = new Nucleus();

		Type *types[] = {Arguments::getType()...};
		for(Type *type : types)
		{
			if(type != Void::getType())
			{
				arguments.push_back(type);
			}
		}

		Nucleus::createFunction(Return::getType(), arguments);
	}

	template<typename Return, typename... Arguments>
	Function<Return(Arguments...)>::~Function()
	{
		delete core;
	}

	template<typename Return, typename... Arguments>
	std::shared_ptr<Routine> Function<Return(Arguments...)>::operator()(const char *name, ...)
	{
		char fullName[1024 + 1];

		va_list vararg;
		va_start(vararg, name);
		vsnprintf(fullName, 1024, name, vararg);
		va_end(vararg);

		return core->acquireRoutine(fullName, Config::Edit::None);
	}

	template<typename Return, typename... Arguments>
	std::shared_ptr<Routine> Function<Return(Arguments...)>::operator()(const Config::Edit &cfg, const char *name, ...)
	{
		char fullName[1024 + 1];

		va_list vararg;
		va_start(vararg, name);
		vsnprintf(fullName, 1024, name, vararg);
		va_end(vararg);

		return core->acquireRoutine(fullName, cfg);
	}

	template<class T, class S>
	RValue<T> ReinterpretCast(RValue<S> val)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return RValue<T>(Nucleus::createBitCast(val.value, T::getType()));
	}

	template<class T, class S>
	RValue<T> ReinterpretCast(const LValue<S> &var)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		Value *val = var.loadValue();

		return RValue<T>(Nucleus::createBitCast(val, T::getType()));
	}

	template<class T, class S>
	RValue<T> ReinterpretCast(const Reference<S> &var)
	{
		return ReinterpretCast<T>(RValue<S>(var));
	}

	template<class T>
	RValue<T> As(Value *val)
	{
		RR_DEBUG_INFO_UPDATE_LOC();
		return RValue<T>(Nucleus::createBitCast(val, T::getType()));
	}

	template<class T, class S>
	RValue<T> As(RValue<S> val)
	{
		return ReinterpretCast<T>(val);
	}

	template<class T, class S>
	RValue<T> As(const LValue<S> &var)
	{
		return ReinterpretCast<T>(var);
	}

	template<class T, class S>
	RValue<T> As(const Reference<S> &val)
	{
		return ReinterpretCast<T>(val);
	}

	// Returns a reactor pointer to the fixed-address ptr.
	RValue<Pointer<Byte>> ConstantPointer(void const * ptr);

	// Calls the function pointer fptr with the given arguments, return type
	// and parameter types. Returns the call's return value if the function has
	// a non-void return type.
	Value* Call(RValue<Pointer<Byte>> fptr, Type* retTy, std::initializer_list<Value*> args, std::initializer_list<Type*> paramTys);

	template <typename F>
	class CallHelper {};

	template<typename Return, typename ... Arguments>
	class CallHelper<Return(Arguments...)>
	{
	public:
		using RReturn = CToReactor<Return>;

		static inline RReturn Call(Return(fptr)(Arguments...), CToReactor<Arguments>... args)
		{
			return RValue<RReturn>(rr::Call(
				ConstantPointer(reinterpret_cast<void*>(fptr)),
				RReturn::getType(),
				{ ValueOf(args) ... },
				{ CToReactor<Arguments>::getType() ... }));
		}

		static inline RReturn Call(Pointer<Byte> fptr, CToReactor<Arguments>... args)
		{
			return RValue<RReturn>(rr::Call(
				fptr,
				RReturn::getType(),
				{ ValueOf(args) ... },
				{ CToReactor<Arguments>::getType() ... }));
		}
	};

	template<typename ... Arguments>
	class CallHelper<void(Arguments...)>
	{
	public:
		static inline void Call(void(fptr)(Arguments...), CToReactor<Arguments>... args)
		{
			rr::Call(ConstantPointer(reinterpret_cast<void*>(fptr)),
				Void::getType(),
				{ ValueOf(args) ... },
				{ CToReactor<Arguments>::getType() ... });
		}

		static inline void Call(Pointer<Byte> fptr, CToReactor<Arguments>... args)
		{
			rr::Call(fptr,
				Void::getType(),
				{ ValueOf(args) ... },
				{ CToReactor<Arguments>::getType() ... });
		}
	};

	// Calls the function pointer fptr with the given arguments args.
	template<typename Return, typename ... Arguments>
	inline CToReactor<Return> Call(Return(fptr)(Arguments...), CToReactor<Arguments>... args)
	{
		return CallHelper<Return(Arguments...)>::Call(fptr, args...);
	}

	// Calls the function pointer fptr with the signature FUNCTION_SIGNATURE and
	// arguments.
	template<typename FUNCTION_SIGNATURE, typename ... Arguments>
	inline void Call(Pointer<Byte> fptr, Arguments ... args)
	{
		CallHelper<FUNCTION_SIGNATURE>::Call(fptr, args...);
	}

	// Breakpoint emits an instruction that will cause the application to trap.
	// This can be used to stop an attached debugger at the given call.
	void Breakpoint();

#ifdef ENABLE_RR_PRINT
	// PrintValue holds the printf format and value(s) for a single argument
	// to Print(). A single argument can be expanded into multiple printf
	// values - for example a Float4 will expand to "%f %f %f %f" and four
	// scalar values.
	// The PrintValue constructor accepts the following:
	//   * Reactor LValues, RValues, Pointers.
	//   * Standard Plain-Old-Value types (int, float, bool, etc)
	//   * Custom types that specialize the PrintValue::Ty template struct.
	//   * Static arrays in the form T[N] where T can be any of the above.
	class PrintValue
	{
		// Ty is a template that can be specialized for printing type T.
		// Each specialization must expose:
		//  * A 'static constexpr const char* fmt' field that provides the
		//    printf format specifier.
		//  * A 'static std::vector<rr::Value*> val(const T& v)' method that
		//    returns all the printf format values.
		template <typename T> struct Ty
		{
			// static constexpr const char* fmt;
			// static std::vector<rr::Value*> val(const T& v)
		};

		// returns the printf value(s) for the given LValue.
		template <typename T>
		static std::vector<Value*> val(const LValue<T>& v) { return val(RValue<T>(v.loadValue())); }

		// returns the printf value(s) for the given RValue.
		template <typename T>
		static std::vector<Value*> val(const RValue<T>& v) { return Ty<T>::val(v); }

		// returns the printf value from for the given type with a
		// PrintValue::Ty<T> specialization.
		template <typename T>
		static std::vector<Value*> val(const T& v) { return Ty<T>::val(v); }

		// returns the printf values for all the values in the given array.
		template <typename T>
		static std::vector<Value*> val(const T* list, int count) {
			std::vector<Value*> values;
			values.reserve(count);
			for (int i = 0; i < count; i++)
			{
				auto v = val(list[i]);
				values.insert(values.end(), v.begin(), v.end());
			}
			return values;
		}

		// fmt returns a comma-delimited list of the string el repeated count
		// times enclosed in square brackets.
		static std::string fmt(const char* el, int count)
		{
			std::string out = "[";
			for (int i = 0; i < count; i++)
			{
				if (i > 0) { out += ", "; }
				out += el;
			}
			return out + "]";
		}

		static std::string addr(const void* ptr)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%p", ptr);
			return buf;
		}

	public:
		const std::string format;
		const std::vector<Value*> values;

		// Constructs a PrintValue for the given value.
		template <typename T>
		PrintValue(const T& v) : format(Ty<T>::fmt), values(val(v)) {}

		// Constructs a PrintValue for the given static array.
		template <typename T, int N>
		PrintValue(const T (&v)[N]) : format(fmt(Ty<T>::fmt, N)), values(val(&v[0], N)) {}

		// Constructs a PrintValue for the given array starting at arr of length
		// len.
		template <typename T>
		PrintValue(const T* arr, int len) : format(fmt(Ty<T>::fmt, len)), values(val(arr, len)) {}

		// PrintValue constructors for plain-old-data values.
		PrintValue(bool v) : format(v ? "true" : "false") {}
		PrintValue(int8_t v) : format(std::to_string(v)) {}
		PrintValue(uint8_t v) : format(std::to_string(v)) {}
		PrintValue(int16_t v) : format(std::to_string(v)) {}
		PrintValue(uint16_t v) : format(std::to_string(v)) {}
		PrintValue(int32_t v) : format(std::to_string(v)) {}
		PrintValue(uint32_t v) : format(std::to_string(v)) {}
		PrintValue(int64_t v) : format(std::to_string(v)) {}
		PrintValue(uint64_t v) : format(std::to_string(v)) {}
		PrintValue(float v) : format(std::to_string(v)) {}
		PrintValue(double v) : format(std::to_string(v)) {}

		template <typename T>
		PrintValue(const T* v) : format(addr(v)) {}

		// vals is a helper to build composite value lists.
		// vals returns the full, sequential list of printf argument values used
		// to print all the provided variadic values.
		// vals() is intended to be used by implementations of
		// PrintValue::Ty<>::vals() to help declare aggregate types.
		// For example, if you were declaring a PrintValue::Ty<> specialization
		// for a custom Mat4x4 matrix formed from four Vector4 values, you'd
		// write:
		//
		// namespace rr
		// {
		//		template <> struct PrintValue::Ty<Mat4x4>
		//		{
		//			static constexpr const char* fmt =
		//				"[a: <%f, %f, %f, %f>,"
		//				" b: <%f, %f, %f, %f>,"
		//				" c: <%f, %f, %f, %f>,"
		//				" d: <%f, %f, %f, %f>]";
		//			static std::vector<rr::Value*> val(const Mat4x4& v)
		//			{
		//				return PrintValue::vals(v.a, v.b, v.c, v.d);
		//			}
		//		};
		//	}
		template<typename ... ARGS>
		static std::vector<Value*> vals(ARGS... v)
		{
			std::vector< std::vector<Value*> > lists = {val(v)...};
			std::vector<Value*> joined;
			for (const auto& list : lists)
			{
				joined.insert(joined.end(), list.begin(), list.end());
			}
			return joined;
		}
	};

	// PrintValue::Ty<T> specializations for basic types.
	template <> struct PrintValue::Ty<const char*>
	{
		static constexpr const char* fmt = "%s";
		static std::vector<Value*> val(const char* v);
	};
	template <> struct PrintValue::Ty<std::string>
	{
		static constexpr const char* fmt = PrintValue::Ty<const char*>::fmt;
		static std::vector<Value*> val(const std::string& v) { return PrintValue::Ty<const char*>::val(v.c_str()); }
	};

	// PrintValue::Ty<T> specializations for standard Reactor types.
	template <> struct PrintValue::Ty<Bool>
	{
		static constexpr const char* fmt = "%d";
		static std::vector<Value*> val(const RValue<Bool>& v) { return {v.value}; }
	};
	template <> struct PrintValue::Ty<Byte>
	{
		static constexpr const char* fmt = "%d";
		static std::vector<Value*> val(const RValue<Byte>& v) { return {v.value}; }
	};
	template <> struct PrintValue::Ty<Byte4>
	{
		static constexpr const char* fmt = "[%d, %d, %d, %d]";
		static std::vector<Value*> val(const RValue<Byte4>& v);
	};
	template <> struct PrintValue::Ty<Int>
	{
		static constexpr const char* fmt = "%d";
		static std::vector<Value*> val(const RValue<Int>& v);
	};
	template <> struct PrintValue::Ty<Int2>
	{
		static constexpr const char* fmt = "[%d, %d]";
		static std::vector<Value*> val(const RValue<Int2>& v);
	};
	template <> struct PrintValue::Ty<Int4>
	{
		static constexpr const char* fmt = "[%d, %d, %d, %d]";
		static std::vector<Value*> val(const RValue<Int4>& v);
	};
	template <> struct PrintValue::Ty<UInt>
	{
		static constexpr const char* fmt = "%u";
		static std::vector<Value*> val(const RValue<UInt>& v);
	};
	template <> struct PrintValue::Ty<UInt2>
	{
		static constexpr const char* fmt = "[%u, %u]";
		static std::vector<Value*> val(const RValue<UInt2>& v);
	};
	template <> struct PrintValue::Ty<UInt4>
	{
		static constexpr const char* fmt = "[%u, %u, %u, %u]";
		static std::vector<Value*> val(const RValue<UInt4>& v);
	};
	template <> struct PrintValue::Ty<Short>
	{
		static constexpr const char* fmt = "%d";
		static std::vector<Value*> val(const RValue<Short>& v) { return {v.value}; }
	};
	template <> struct PrintValue::Ty<Short4>
	{
		static constexpr const char* fmt = "[%d, %d, %d, %d]";
		static std::vector<Value*> val(const RValue<Short4>& v);
	};
	template <> struct PrintValue::Ty<UShort>
	{
		static constexpr const char* fmt = "%u";
		static std::vector<Value*> val(const RValue<UShort>& v) { return {v.value}; }
	};
	template <> struct PrintValue::Ty<UShort4>
	{
		static constexpr const char* fmt = "[%u, %u, %u, %u]";
		static std::vector<Value*> val(const RValue<UShort4>& v);
	};
	template <> struct PrintValue::Ty<Float>
	{
		static constexpr const char* fmt = "[%f]";
		static std::vector<Value*> val(const RValue<Float>& v);
	};
	template <> struct PrintValue::Ty<Float4>
	{
		static constexpr const char* fmt = "[%f, %f, %f, %f]";
		static std::vector<Value*> val(const RValue<Float4>& v);
	};
	template <> struct PrintValue::Ty<Long>
	{
		static constexpr const char* fmt = "%lld";
		static std::vector<Value*> val(const RValue<Long>& v) { return {v.value}; }
	};
	template <typename T> struct PrintValue::Ty< Pointer<T> >
	{
		static constexpr const char* fmt = "%p";
		static std::vector<Value*> val(const RValue<Pointer<T>>& v) { return {v.value}; }
	};
	template <typename T> struct PrintValue::Ty< Reference<T> >
	{
		static constexpr const char* fmt = PrintValue::Ty<T>::fmt;
		static std::vector<Value*> val(const Reference<T>& v) { return PrintValue::Ty<T>::val(v); }
	};
	template <typename T> struct PrintValue::Ty< RValue<T> >
	{
		static constexpr const char* fmt = PrintValue::Ty<T>::fmt;
		static std::vector<Value*> val(const RValue<T>& v) { return PrintValue::Ty<T>::val(v); }
	};

	// Printv emits a call to printf() using the function, file and line,
	// message and optional values.
	// See Printv below.
	void Printv(const char* function, const char* file, int line, const char* msg, std::initializer_list<PrintValue> vals);

	// Printv emits a call to printf() using the provided message and optional
	// values.
	// Printf replaces any bracketed indices in the message with string
	// representations of the corresponding value in vals.
	// For example:
	//   Printv("{0} and {1}", "red", "green");
	// Would print the string:
	//   "red and green"
	// Arguments can be indexed in any order.
	// Invalid indices are not substituted.
	inline void Printv(const char* msg, std::initializer_list<PrintValue> vals)
	{
		Printv(nullptr, nullptr, 0, msg, vals);
	}

	// Print is a wrapper over Printv that wraps the variadic arguments into an
	// initializer_list before calling Printv.
	template <typename ... ARGS>
	void Print(const char* msg, const ARGS& ... vals) { Printv(msg, {vals...}); }

	// Print is a wrapper over Printv that wraps the variadic arguments into an
	// initializer_list before calling Printv.
	template <typename ... ARGS>
	void Print(const char* function, const char* file, int line, const char* msg, const ARGS& ... vals)
	{
		Printv(function, file, line, msg, {vals...});
	}

	// RR_LOG is a macro that calls Print(), automatically populating the
	// function, file and line parameters and appending a newline to the string.
	//
	// RR_LOG() is intended to be used for debugging JIT compiled code, and is
	// not intended for production use.
	#if defined(_WIN32)
		#define RR_LOG(msg, ...) Print(__FUNCSIG__, __FILE__, static_cast<int>(__LINE__), msg "\n", ##__VA_ARGS__)
	#else
		#define RR_LOG(msg, ...) Print(__PRETTY_FUNCTION__, __FILE__, static_cast<int>(__LINE__), msg "\n", ##__VA_ARGS__)
	#endif

	// Macro magic to perform variadic dispatch.
	// See: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
	// Note, this doesn't attempt to use the ##__VA_ARGS__ trick to handle 0
	#define RR_MSVC_EXPAND_BUG(X) X // Helper macro to force expanding __VA_ARGS__ to satisfy MSVC compiler.
	#define RR_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
	#define RR_COUNT_ARGUMENTS(...) RR_MSVC_EXPAND_BUG(RR_GET_NTH_ARG(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))
	static_assert(1 == RR_COUNT_ARGUMENTS(a), "RR_COUNT_ARGUMENTS broken"); // Sanity checks.
	static_assert(2 == RR_COUNT_ARGUMENTS(a, b), "RR_COUNT_ARGUMENTS broken");
	static_assert(3 == RR_COUNT_ARGUMENTS(a, b, c), "RR_COUNT_ARGUMENTS broken");

	// RR_WATCH_FMT(...) resolves to a string literal that lists all the
	// arguments by name. This string can be passed to LOG() to print each of
	// the arguments with their name and value.
	//
	// RR_WATCH_FMT(...) uses the RR_COUNT_ARGUMENTS helper macro to delegate to a
	// corresponding RR_WATCH_FMT_n specialization macro below.
	#define RR_WATCH_CONCAT(a, b) a ## b
	#define RR_WATCH_CONCAT2(a, b) RR_WATCH_CONCAT(a, b)
	#define RR_WATCH_FMT(...) RR_MSVC_EXPAND_BUG(RR_WATCH_CONCAT2(RR_WATCH_FMT_, RR_COUNT_ARGUMENTS(__VA_ARGS__))(__VA_ARGS__))
	#define RR_WATCH_FMT_1(_1) "\n  " #_1 ": {0}"
	#define RR_WATCH_FMT_2(_1, _2)                                             RR_WATCH_FMT_1(_1) "\n  " #_2 ": {1}"
	#define RR_WATCH_FMT_3(_1, _2, _3)                                         RR_WATCH_FMT_2(_1, _2) "\n  " #_3 ": {2}"
	#define RR_WATCH_FMT_4(_1, _2, _3, _4)                                     RR_WATCH_FMT_3(_1, _2, _3) "\n  " #_4 ": {3}"
	#define RR_WATCH_FMT_5(_1, _2, _3, _4, _5)                                 RR_WATCH_FMT_4(_1, _2, _3, _4) "\n  " #_5 ": {4}"
	#define RR_WATCH_FMT_6(_1, _2, _3, _4, _5, _6)                             RR_WATCH_FMT_5(_1, _2, _3, _4, _5) "\n  " #_6 ": {5}"
	#define RR_WATCH_FMT_7(_1, _2, _3, _4, _5, _6, _7)                         RR_WATCH_FMT_6(_1, _2, _3, _4, _5, _6) "\n  " #_7 ": {6}"
	#define RR_WATCH_FMT_8(_1, _2, _3, _4, _5, _6, _7, _8)                     RR_WATCH_FMT_7(_1, _2, _3, _4, _5, _6, _7) "\n  " #_8 ": {7}"
	#define RR_WATCH_FMT_9(_1, _2, _3, _4, _5, _6, _7, _8, _9)                 RR_WATCH_FMT_8(_1, _2, _3, _4, _5, _6, _7, _8) "\n  " #_9 ": {8}"
	#define RR_WATCH_FMT_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10)           RR_WATCH_FMT_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) "\n  " #_10 ": {9}"
	#define RR_WATCH_FMT_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11)      RR_WATCH_FMT_10(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10) "\n  " #_11 ": {10}"
	#define RR_WATCH_FMT_12(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) RR_WATCH_FMT_11(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) "\n  " #_12 ": {11}"

	// RR_WATCH() is a helper that prints the name and value of all the supplied
	// arguments.
	// For example, if you had the Int and bool variables 'foo' and 'bar' that
	// you want to print, you can simply write:
	//    RR_WATCH(foo, bar)
	// When this JIT compiled code is executed, it will print the string
	// "foo: 1, bar: true" to stdout.
	//
	// RR_WATCH() is intended to be used for debugging JIT compiled code, and
	// is not intended for production use.
	#define RR_WATCH(...) RR_LOG(RR_WATCH_FMT(__VA_ARGS__), __VA_ARGS__)
#endif // ENABLE_RR_PRINT

	class ForData
	{
	public:
		ForData(bool init) : loopOnce(init)
		{
		}

		operator bool()
		{
			return loopOnce;
		}

		bool operator=(bool value)
		{
			return loopOnce = value;
		}

		bool setup()
		{
			RR_DEBUG_INFO_FLUSH();
			if(Nucleus::getInsertBlock() != endBB)
			{
				testBB = Nucleus::createBasicBlock();

				Nucleus::createBr(testBB);
				Nucleus::setInsertBlock(testBB);

				return true;
			}

			return false;
		}

		bool test(RValue<Bool> cmp)
		{
			BasicBlock *bodyBB = Nucleus::createBasicBlock();
			endBB = Nucleus::createBasicBlock();

			Nucleus::createCondBr(cmp.value, bodyBB, endBB);
			Nucleus::setInsertBlock(bodyBB);

			return true;
		}

		void end()
		{
			Nucleus::createBr(testBB);
			Nucleus::setInsertBlock(endBB);
		}

	private:
		BasicBlock *testBB = nullptr;
		BasicBlock *endBB = nullptr;
		bool loopOnce = true;
	};

	class IfElseData
	{
	public:
		IfElseData(RValue<Bool> cmp) : iteration(0)
		{
			condition = cmp.value;

			beginBB = Nucleus::getInsertBlock();
			trueBB = Nucleus::createBasicBlock();
			falseBB = nullptr;
			endBB = Nucleus::createBasicBlock();

			Nucleus::setInsertBlock(trueBB);
		}

		~IfElseData()
		{
			Nucleus::createBr(endBB);

			Nucleus::setInsertBlock(beginBB);
			Nucleus::createCondBr(condition, trueBB, falseBB ? falseBB : endBB);

			Nucleus::setInsertBlock(endBB);
		}

		operator int()
		{
			return iteration;
		}

		IfElseData &operator++()
		{
			++iteration;

			return *this;
		}

		void elseClause()
		{
			Nucleus::createBr(endBB);

			falseBB = Nucleus::createBasicBlock();
			Nucleus::setInsertBlock(falseBB);
		}

	private:
		Value *condition;
		BasicBlock *beginBB;
		BasicBlock *trueBB;
		BasicBlock *falseBB;
		BasicBlock *endBB;
		int iteration;
	};

	#define For(init, cond, inc) \
	for(ForData for__ = true; for__; for__ = false) \
	for(init; for__.setup() && for__.test(cond); inc, for__.end())

	#define While(cond) For((void)0, cond, (void)0)

	#define Do                                            \
	{                                                     \
		BasicBlock *body__ = Nucleus::createBasicBlock(); \
		Nucleus::createBr(body__);                        \
		Nucleus::setInsertBlock(body__);

	#define Until(cond)                                     \
		BasicBlock *end__ = Nucleus::createBasicBlock();    \
		Nucleus::createCondBr((cond).value, end__, body__); \
		Nucleus::setInsertBlock(end__);                     \
	}

	enum {IF_BLOCK__, ELSE_CLAUSE__, ELSE_BLOCK__, IFELSE_NUM__};

	#define If(cond)                                                    \
	for(IfElseData ifElse__(cond); ifElse__ < IFELSE_NUM__; ++ifElse__) \
	if(ifElse__ == IF_BLOCK__)

	#define Else                       \
	else if(ifElse__ == ELSE_CLAUSE__) \
	{                                  \
		 ifElse__.elseClause();        \
	}                                  \
	else   // ELSE_BLOCK__
}

#endif   // rr_Reactor_hpp
