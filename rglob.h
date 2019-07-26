/*
	rglob.h - interface of the RGlob "glob" pattern-matcher

	Copyright(c) 2016,2019, Robert Roessler
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <string>
#include <bitset>

/*
	The rglob namespace contains the classes compiler, matcher, and glob, the
	last of which inherits from the first two to provide complete "glob"-style
	pattern matching over UTF-8 -encoded text.

	In addition, an iterator template class (basic_utf8iterator) plus a handful
	of helper functions for assisting with processing of UTF-8 -encoded Unicode
	text are made available.
*/
namespace rglob {

/*
	Make sure we have this - Just In Case(tm).
*/
#ifndef isascii
#define isascii(c) ((int)(c) < 128)
#endif

constexpr size_t LengthSize = 2;		// # of chars in [base64] encoded length

/*
	sizeOfUTF8CodePoint returns the length in bytes of a UTF-8 code point, based
	on being passed the [presumed] first byte.

	N.B. - if the passed value does NOT represent [the start of] a well-formed
	UTF-8 code point, the returned length is ZERO, which means this should most
	likely be used at least initially in a "validation" capacity.

	Conceptually, this is the logic:

	return
		isascii(c)                     ? 1 :
		(c & 0b11100000) == 0b11000000 ? 2 :
		(c & 0b11110000) == 0b11100000 ? 3 :
		(c & 0b11111000) == 0b11110000 ? 4 :
		0; // (caller(s) should NOTICE this)
*/
constexpr size_t sizeOfUTF8CodePoint(char32_t c)
{
	return
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 00-0f 1-byte UTF-8/ASCII
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 10-1f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 20-2f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 30-3f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 40-4f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 50-5f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 60-6f
		"\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"	// 70-7f

		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"	// 80-8f <illegal>
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"	// 90-9f
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"	// a0-af
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"	// b0-bf

		"\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2"	// c0-cf 2-byte UTF-8
		"\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2\2"	// d0-df

		"\3\3\3\3\3\3\3\3\3\3\3\3\3\3\3\3"	// e0-ef 3-byte UTF-8

		"\4\4\4\4\4\4\4\4"					// f0-f7 4-byte UTF-8

		"\0\0\0\0\0\0\0\0"					// f8-ff <illegal>
		[c & 0xff];
}

/*
	codePointToUTF8 is a template function providing flexible output options for
	the encoded UTF-8 chars representing the supplied Unicode code point.

	It is a template function so you can choose to store the output UTF-8 stream
	either like this

	char buf[80], * s = buf;
	codePointToUTF8(c, [&](char x) { *s++ = x; })

	or this

	std::string buf;
	codePointToUTF8(c, [&](char x) { buf.push_back(x); })

	... where c is a Unicode code point in a char32_t.
*/
template<class CharOutput>
inline void codePointToUTF8(char32_t c, CharOutput f)
{
	if (c < 0x80)
		f((char)c);
	else if (c < 0x800)
		f((char)(0b11000000 | (c >> 6))),
		f((char)((c & 0b111111) | 0b10000000));
	else if (c < 0x10000)
		f((char)(0b11100000 | (c >> 12))),
		f((char)(((c >> 6) & 0b111111) | 0b10000000)),
		f((char)((c & 0b111111) | 0b10000000));
	else
		f((char)(0b11110000 | (c >> 18))),
		f((char)(((c >> 12) & 0b111111) | 0b10000000)),
		f((char)(((c >> 6) & 0b111111) | 0b10000000)),
		f((char)((c & 0b111111) | 0b10000000));
}

/*
	basic_utf8iterator is an iterator adaptor template class providing read-only
	operations over a "base" character iterator type on text containing Unicode
	represented in the UTF-8 encoding - typically a std::string::const_iterator
	or a "bare" const char* is used as this underlying character iterator type.

	While this is a functional "bidirectional iterator" - and is almost able to
	be a "random access iterator" (BUT, no subscripting), it must be noted that

	1) The "difference type" supplied to the arithmetic operators represents a
	BYTE offset in "UTF-8 space" - it is NOT in "Unicode space", i.e., it is NOT
	true that It++ is guaranteed to result in the same iterator value as the
	result of It += 1... the former WILL advance to the next code point, while
	the latter [in general] will not.

	In practice, this isn't a problem or limitation at all, since the difference
	values being used in the arithmetic operators should be the result of other
	operations that yield them, or "known" BYTE lengths of UTF-8 sequences.

	2) A possibly trivial issue is that, due to fundamental limitations of the
	UTF-8 encoding, the "operator--" implementation is not [strictly speaking]
	quite "constant time" - for those worried about such things.
*/
template<class BaseType>
class basic_utf8iterator : public std::iterator<std::bidirectional_iterator_tag, char32_t>
{
	// provide [internal] short-hand access to our instantiated type
	using T = basic_utf8iterator<BaseType>;

	BaseType base;						// our actual "base" iterator

	// codePointFromUTF8 assembles and returns a full [32-bit] Unicode code point
	// from a UTF-8 encoded sequence of bytes
	//
	// N.B. - as it computes the length of the encoded representation from the data
	// itself - as well as "trusting" the bit patterns contained therein - it will
	// ONLY work with well-formed UTF-8 encoded data!
	value_type codePointFromUTF8() const {
		const value_type c = base[0];
		switch (sizeOfUTF8CodePoint(c)) {
		case 1: return c;
		case 2: return (c & 0b11111) << 6 | (base[1] & 0b111111);
		case 3: return (c & 0b1111) << 12 | (base[1] & 0b111111) << 6 | (base[2] & 0b111111);
		case 4: return (c & 0b111) << 18 | (base[1] & 0b111111) << 12 | (base[2] & 0b111111) << 6 | (base[3] & 0b111111);
		}
		return 0; // ("can't happen")
	}

	// sizeOfPreviousUTF8CodePoint is the basis for our not-quite-constant-time
	//	"decrement" operator
	//
	// N.B. - depends on BOTH well-formed UTF-8 encoded data AND caller not
	// attempting to position iterator prior to beginning of container!
	size_t sizeOfPreviousUTF8CodePoint() const {
		return
			(*(base - 1) & 0b11000000) != 0b10000000 ? 1 :
			(*(base - 2) & 0b11000000) != 0b10000000 ? 2 :
			(*(base - 3) & 0b11000000) != 0b10000000 ? 3 :
			(*(base - 4) & 0b11111000) == 0b11110000 ? 4 :
			0; // happens only for ILLEGAL UTF-8 encoding!
	}

public:
	// provide public access to our instantiated base type
	using base_type = BaseType;

	// define "standard" constructors/destructor for iterators... note that as
	// there isn't a good, generic default "uninitialized" value for iterators
	// - this is a C++ language/stdlib issue - we delete the default ctor, and
	// require apps to explictily employ only valid copy-constructor exprs
	basic_utf8iterator() = delete;
	basic_utf8iterator(const T& u) : base(u.base) {}
	basic_utf8iterator(BaseType i) : base(i) {}
	~basic_utf8iterator() {}

	// provide [expert] access to "base" iterator member
	operator BaseType() const { return base; }

	// define "copy assignment" operator for iterators
	T& operator=(const T& u) { base = u.base; return *this; }

	// define "dereferencing" operator for iterators
	value_type operator*() const { return codePointFromUTF8(); }

	// define "pre- and post- increment/decrement" operators for iterators
	T& operator++() { base += sizeOfUTF8CodePoint(*base); return *this; }
	T operator++(int) { auto u = *this; ++(*this); return u; }
	T& operator--() { base -= sizeOfPreviousUTF8CodePoint(); return *this; }
	T operator--(int) { auto u = *this; --(*this); return u; }

	// define "arithmetic" operators for iterators
	//
	// N.B. - based on char/byte ptrdiff_t, NOT code point "distance"!
	T operator+(difference_type d) const { return T(base + d); }
	T operator-(difference_type d) const { return T(base - d); }
	T& operator+=(difference_type d) { base += d; return *this; }
	T& operator-=(difference_type d) { base -= d; return *this; }

	// define "differencing" operators for iterators in same container
	difference_type operator-(const T& u) const { return base - u.base; }
	difference_type operator-(const BaseType& b) const { return base - b; }

	// define "relational" operators for iterators in same container
	bool operator==(const T& u) const { return base == u.base; }
	bool operator==(const BaseType& b) const { return base == b; }
	bool operator!=(const T& u) const { return base != u.base; }
	bool operator!=(const BaseType& b) const { return base != b; }

	bool operator>(const T& u) const { return base > u.base; }
	bool operator>(const BaseType& i) const { return base > i; }
	bool operator<(const T& u) const { return base < u.base; }
	bool operator<(const BaseType& i) const { return base < i; }

	bool operator>=(const T& u) const { return base >= u.base; }
	bool operator>=(const BaseType& i) const { return base >= i; }
	bool operator<=(const T& u) const { return base <= u.base; }
	bool operator<=(const BaseType& i) const { return base <= i; }
};

/*
	Create the 2 UTF-8 iterators used by the rglob compiler and matcher classes.
*/
typedef basic_utf8iterator<std::string::const_iterator> utf8iterator;
typedef basic_utf8iterator<const char*> utf8iteratorBare;

/*
	The compiler class is composed of a primary function - compile - which takes
	a "pattern" specification in the style of the "glob" patterns of Unix/Linux,
	and machine, a "payload" function that returns the now-compiled pattern for
	subsequent display or execution by the matcher class.

	All text is expected to be in UTF-8 representation, which is usable and at
	least minimally supported by modern C++ compilers... without attempting to
	be a tutorial on the UTF-8 Unicode encoding, we can observe the following:

	* Unicode is a set of over a million characters ("everything"), and UTF-8
	is a way of representing Unicode "code points" as 1, 2, 3, or 4 bytes

	* 7-bit ASCII is the first 128 characters of Unicode, and appears unchanged
	in UTF-8... another way to say this is that ASCII chars ARE UTF-8 chars

	* 2nd, 3rd, or 4th chars in Unicode code points will NEVER look like an
	ASCII char, so one can still perform "normal" text comparisons or even
	make use of the standard C++ library with UTF-8 text

	* if you want to include non-ASCII text literals in C++ source, you have
	the choice of doing it the hard way with hex/binary escaped sequences in
	strings, \unnnn sequences in strings for 2/3-byte code points, \Unnnnnnnn
	sequences in strings for 3-byte or 4-byte code points, or [easiest] using
	the new UTF-8 string literals: u8"This is a UTF-8 string!"

	* to see FAR more detail on this, visit http://utf8everywhere.org/

	Patterns supported by the rglob::compiler and rglob::matcher classes are
	made up of combinations of the following elements:

	?		any SINGLE UTF-8 code point (again, this could be an ASCII char)

	*		any sequence of ZERO OR MORE UTF-8 code points

	[abc]	"ONE OF" the supplied set of UTF-8 code points

	[a-c]	"ONE OF" the specified range of UTF-8 code points

	[a-cYZ]	"ONE OF" either the range OR set of UTF-8 code points

	abcdef	the EXACT SEQUENCE of UTF-8 code points

	More details on patterns:

	* mixing and matching is fine, so "*[abc]?[A-Z]hello" is a valid pattern
	that matches
		1 ZERO OR MORE UTF-8 code points, followed by
		2 ONE OF a, b, or c, followed by
		3 any SINGLE UTF-8 code point, followed by
		4 ONE OF any character in the range A-Z, followed by
		5 the EXACT SEQUENCE hello

	* [...] pattern elements are basically simplified versions of the "character
	classes" found in regular expressions

	* if the FIRST char in the class is '!' or '^', then that changes that class
	to mean "any UTF-8 code point EXCEPT for the ones specified in this class"

	* to include the "special" characters ']', '-', '!', or '^' IN a character
	class, do the following
		]	use as the FIRST char (but AFTER either '!' or '^')
		-	use as the LAST char
		!	use as anything BUT the first char
		^	use as anything BUT the first char
*/
class compiler
{
	enum {
		AllowedMaxFSM = 4096 - 1		// limit [compiled] finite state machine
	};

	std::string fsm;					// compiled fsm for current glob pattern

	auto compileClass(const std::string& pattern, std::string::const_iterator p);
	auto compileString(const std::string& pattern, std::string::const_iterator p);

public:
	compiler() {
		// N.B. - it is IMPORTANT to do this in the constructor!
		// (needed when composed with the matcher class in rglob::glob class)
		fsm.reserve(AllowedMaxFSM);
	}

	/*
		compile accepts a pattern following the rules detailed in the class
		documentation, and "compiles" it to a representation enabling faster
		subsequent matching; possible exceptions include

		invalid_argument if the pattern string is NOT valid UTF-8

		invalid_argument if pattern string has an unterminated character class

		length_error if the compiled pattern is > 4 KB (implementation limit)

		In all cases, an explanatory text message is included, with position
		information if applicable.
	*/
	void compile(const std::string& pattern);

	/*
		machine returns the compiled form of the [valid] glob pattern supplied
		to compile... note that while this is "human-readable", the matcher
		class's pretty_print does a better job of displaying this information.
	*/
	const std::string& machine() const { return fsm; }

private:
	constexpr auto base64Digit(int n) const {
		return								// RFCs 2045/3548/4648/4880 et al
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"	//  0-25
			"abcdefghijklmnopqrstuvwxyz"	// 26-51
			"0123456789"					// 52-61
			"+/"							// 62-63
			[n & 0x3f];
	}
	void emit(char c) { fsm.push_back(c); }
	void emit(std::string s) { fsm.append(s); }
	void emit(std::string::const_iterator i, std::string::const_iterator j) { fsm.append(i, j); }
	void emitAt(size_t i, char c) { fsm[i] = c; }
	void emitPackedBitset(const std::bitset<128>& b);
	void emitLengthAt(size_t i, size_t n)
	{
		emitAt(i + 0, base64Digit((n & 0xfc) >> 6)), emitAt(i + 1, base64Digit((n & 0x3f)));
	}
	void emitPadding(int n, char c = '_') { while (n-- > 0) emit(c); }
	auto emitted() const { return fsm.size(); }
	void emitUTF8CodePoint(char32_t c) { codePointToUTF8(c, [this](char x) { emit(x); }); }
	constexpr auto hexDigit(int n) const { return "0123456789abcdef"[n & 0xf]; }
	auto peek(std::string::const_iterator i) const { return *++i; }
	auto peek(utf8iterator u) const { return *++u; }
};

/*
	The matcher class accepts (via its constructor) the compiled representation
	of a "glob" pattern from the compiler class above, and can then be used to
	match targets against this pattern with its match function, or to output it
	in "pretty-printed" form to a supplied stream with pretty_print.
*/
class matcher
{
	const char* fsm;

public:
	/*
		Using the rglob::matcher constructor is considered an "expert" level of
		use of the rglob system... it is far more likely that you will be using
		the rglob::glob class - it's easier and really made for most situations.

		That said, if you DO choose to access rglob functionality at the lower
		level of using the compiler and matcher classes directly, note that the
		ONLY supported values for the matcher constructor(s) are those returned
		from the compiler::machine function... which by definition only returns
		well-formed finite state machines, composed of valid sequences.

		This last bit is really just a disclaimer saying "we trust our own data
		and may therefore be a bit relaxed in our internal error-checking".
	*/
	matcher() = delete;
	matcher(const std::string& m) : fsm(m.c_str()) {}
	matcher(const char* s) : fsm(s) {}

	/*
		match accepts a "target" string and attempts to match it to the pattern
		that was previously processed by compiler::compile, reflecting the match
		success/failure as its return value; possible exceptions include

		invalid_argument if the target string is NOT valid UTF-8
	*/
	bool match(const std::string& target) const;

	/*
		pretty_print outputs a formatted representation of the current finite
		state machine produced by compiler::compile to the supplied ostream.
	*/
	void pretty_print(std::ostream& s) const;

private:
	constexpr auto base64Value(char c) const {
		return int(
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 00-0f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 10-1f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"

			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 20-2f <illegal>,+,/
			"\x00\x00\x00\x3e\x00\x00\x00\x3f"
			"\x34\x35\x36\x37\x38\x39\x3a\x3b"	// 30-3f 0-9,<illegal>
			"\x3c\x3d\x00\x00\x00\x00\x00\x00"

			"\x00\x00\x01\x02\x03\x04\x05\x06"	// 40-4f <illegal>,A-O
			"\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
			"\x0f\x10\x11\x12\x13\x14\x15\x16"	// 50-5f P-Z,<illegal>
			"\x17\x18\x19\x00\x00\x00\x00\x00"

			"\x00\x1a\x1b\x1c\x1d\x1e\x1f\x20"	// 60-6f <illegal>,a-o
			"\x21\x22\x23\x24\x25\x26\x27\x28"
			"\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30"	// 70-7f p-z,<illegal>
			"\x31\x32\x33\x00\x00\x00\x00\x00"
			[c & 0x7f]);
	}
	constexpr auto hexValue(char c) const {
		return int(
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 00-0f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 10-1f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"

			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 20-2f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x01\x02\x03\x04\x05\x06\x07"	// 30-3f 0-9
			"\x08\x09\x00\x00\x00\x00\x00\x00"	//  ...  <illegal>

			"\x00\x0a\x0b\x0c\x0d\x0e\x0f\x00"	// 40-4f <illegal>,A-F
			"\x00\x00\x00\x00\x00\x00\x00\x00"	//  ...  <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 50-5f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"

			"\x00\x0a\x0b\x0c\x0d\x0e\x0f\x00"	// 60-6f <illegal>,a-f
			"\x00\x00\x00\x00\x00\x00\x00\x00"	//  ...  <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"	// 70-7f <illegal>
			"\x00\x00\x00\x00\x00\x00\x00\x00"
			[c & 0x7f]);
	}
	auto opAt(utf8iteratorBare i) const { return *(utf8iteratorBare::base_type)i; }
	auto decodeLengthAt(utf8iteratorBare i) const { return base64Value(opAt(i + 0)) * 64 + base64Value(opAt(i + 1)); }
	auto decodeModifierAt(utf8iteratorBare i) const { return hexValue(opAt(i)); }
	constexpr auto packedBitsetMask(int b) const { return int("\x8\4\2\1"[(127 - b) & 0b11]); }
	auto packedBitsetNibbleAt(utf8iteratorBare i, int b) const { return hexValue(((utf8iteratorBare::base_type)i)[(127 - b) >> 2]); }
	auto testPackedBitsetAt(utf8iteratorBare i, int b) const { return (packedBitsetNibbleAt(i, b) & packedBitsetMask(b)) != 0; }

	const utf8iteratorBare cbegin() const { return fsm; }
	const utf8iteratorBare cend() const { return fsm + (fsm[0] == '#' ? 1 + LengthSize + decodeLengthAt(fsm + 1) : strlen(fsm)); }
};

/*
	The glob class is a "glue" class that composes a compiler and a matcher for
	specifying and subsequently recognizing "glob" -style patterns over text in
	UTF-8 form.

	While certain specialized applications may find it convenient to separately
	compile and match patterns - and will thus directly make use of the compiler
	and matcher classes, the expected typical usage is to use the composed class
	glob to handle all compiling, matching, and pretty-printing functionality.

	Note that when using glob, there is no need to refer to or use the compiler
	or matcher classes (or their constructors) at all: just create a glob object
	and invoke its compile and match (or pretty_print) functions directly.
*/
class glob : public compiler, public matcher
{
public:
	glob() : compiler(), matcher(compiler::machine()) {}
};

}
