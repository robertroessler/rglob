/*
	RGlobCore.cpp - "core" functionality of the RGlob "glob" pattern-matcher

	Copyright(c) 2016-2023, Robert Roessler
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

#include <algorithm>
#include <exception>
#include <iostream>
#include <iomanip>
#include "rglob.h"

using std::string;
using namespace rglob;

/*
	validateUTF8String evaluates the [NUL-terminated] sequence of chars supplied
	for "valid" UTF-8 encoding - structurally, NOT in terms of specific values
	of code points / combinations.

	Returns the result of this evaluation.

	N.B. - a "false" return should probably NOT be ignored.
*/
constexpr auto validateUTF8String(std::string_view v)
{
	for (auto i = v.cbegin(); i != v.cend();)
		switch (auto n = sizeOfUTF8CodePoint(*i++); n) {
		case 0:
			// invalid "lead char" of UTF-8 Unicode sequence
			return false;
		case 1:
			// ASCII char
			break;
		default:
			// multi-byte UTF-8 Unicode sequence...
			while (--n && i != v.cend())
				if (const auto c = *i++; (c & 0b11000000) != 0b10000000)
					// invalid "following char" of UTF-8 Unicode sequence
					return false;
			if (n && i == v.cend())
				// you are NOT paranoid if the UTF-8 really IS malformed!
				return false;
		}
	return true;
}

/*
	compileClass processes a single "character class" string from a glob pattern
	- after first determining whether the sequence is well-formed - an exception
	(invalid_argument) will be thrown if it fails this test.

	While evaluating the legality of the character class, two "special cases" of
	leading character class metachars are checked, and then the presence of non-
	ASCII is tested... if none are found, then the entire class will be handled
	by "fast path" logic, and represented as a "bitset" - in which case, class
	membership (matching) can be tested by a single "lookup" per target char.

	In the general [non-ASCII] case, the match-time evaluation of matches in the
	character class will be done by evaluating a number of either single-char or
	char-range expressions serially... if at least one matches the "target"/test
	char, then the class is matched - otherwise, the class match fails.

	The number of chars/BYTEs consumed is returned.
*/
auto compiler::compileClass(std::string_view pattern, std::string_view::const_iterator p)
{
	const auto base = p++;
	const auto pos = emitted();
	// check for "inversion" of character class metacharacter
	auto invert = false;
	if (*p == '!' || *p == '^')
		invert = true, ++p;
	// NOW check for "close" metacharacter as the first class member
	auto leadingCloseBracket = false;
	// NOW look for the end of the character class specification...
	if (*p == ']')
		leadingCloseBracket = true, ++p;
	const auto o = p - pattern.cbegin();
	const auto close = pattern.find_first_of(']', o);
	// ... and throw if we don't see one
	if (close == string::npos)
		throw std::invalid_argument(string("Missing terminating ']' for character class @ ") + string(pattern.substr(base - pattern.cbegin())));
	if (all_of(p, p + (close - o), [](char c) { return isascii(c); })) {
		// the character class is ALL ASCII chars, so we can use the "fast path"
		emit('{');
		// (neither "invert" flag nor "length" field are needed for "fast path")
		std::bitset<128> b;
		// "fast path" (bitset) invert is easy
		if (invert)
			b.set();
		if (leadingCloseBracket)
			b.flip(']');
		// process all class members by "flipping" corresponding bits...
		while (*p != ']')
			if (const auto c1 = *p++, c2 = *p; c2 == '-' && peek(p) != ']') {
				const auto c3 = *++p;
				for (auto c = c1; c <= c3; c++)
					b.flip(c);
				++p;
			} else
				b.flip(c1);
		// ... finish up by copying the [packed] bitset to finite state machine
		emitPackedBitset(b), ++p;
		return p - base;
	} else {
		// "general case" character class, output single and range match exprs
		emit('[');
		emit(hexDigit(invert ? 1 : 0));
		// initialize and "remember" location of length (to be filled in later)
		const auto lenPos = emitted();
		emitPadding(LengthSize);
		if (leadingCloseBracket)
			emit('+'), emit(']');
		// NOW switch to full UTF-8 (Unicode) processing...
		utf8iterator u = p;
		// ... and process all class members by outputting match-time operators
		while (*u != ']')
			if (const auto c1 = *u++, c2 = *u; c2 == '-' && peek(u) != ']') {
				// (generate "char range" matching operator)
				const auto c3 = *++u;
				emit('-'), emitUTF8CodePoint(c1), emitUTF8CodePoint(c3), ++u;
			} else
				// (generate "single char" matching operator)
				emit('+'), emitUTF8CodePoint(c1);
		// finish up by generating the "NO match" operator...
		emit(']'), ++u;
		// ... and output the length of the character class "interpreter" logic
		emitLengthAt(lenPos, emitted() - pos - (1 + 1 + LengthSize + 1));
		return u - base;
	}
}

/*
	compileString processes a single "exact match" string from a glob pattern...
	this will extend until either the next glob metacharacter or the pattern end
	- there is no "invalid" case.

	The number of chars/BYTEs consumed is returned.
*/
auto compiler::compileString(std::string_view pattern, std::string_view::const_iterator p)
{
	emit('=');
	// initialize and "remember" location of length (to be filled in later)
	const auto lenPos = emitted();
	emitPadding(LengthSize);
	// determine length...
	const auto o = p - pattern.cbegin();
	const auto i = pattern.find_first_of("?*[", o);
	const auto n = i != string::npos ? i - o : pattern.size() - o;
	// ... and copy "exact match" string to finite state machine
	emit(pattern.substr(p - pattern.cbegin(), n));
	emitLengthAt(lenPos, n);
	return n;
}

/*
	compile generates a "finite state machine" able to recognize text matching
	the supplied [UTF-8] pattern.
*/
void compiler::compile(std::string_view pattern)
{
	// make SURE pattern is *structurally* valid UTF8
	if (!validateUTF8String(pattern))
		throw std::invalid_argument("Pattern string is not valid UTF-8.");
	fsm.clear();
	// prep for filling in compiled length of pattern later
	emit('#'), emitPadding(2);
	ptrdiff_t incr = 1;
	// iterate over, compile, and consume pattern elements
	for (auto pi = pattern.cbegin(); pi != pattern.cend(); pi += incr) {
		switch (*pi) {
		case '?':
			emit(*pi), incr = 1;
			break;
		case '*':
			emit(*pi), incr = 1;
			break;
		case '[':
			incr = compileClass(pattern, pi);
			break;
		default:
			incr = compileString(pattern, pi);
		}
		if (emitted() > AllowedMaxFSM)
			throw std::length_error(string("Exceeded allowed compiled pattern size @ ") + string(pattern.substr(pi - pattern.cbegin())));
	}
	// NOW fill in length of compiled pattern... IFF there is any actual pattern
	if (const auto n = emitted(); n > 1 + LengthSize)
		emitLengthAt(1, n - (1 + LengthSize));
	else
		fsm.clear();
}

/*
	emitPackedBitset inserts a representation of the just-processed "fast path"
	character class into the current finite state machine definition.

	The actual form of this data is dictated by two considerations:

	1) The stdlib bitset implementation only has convenient "[de-]serialization"
	options for up-to 64-element sets - the "1 character per bit" form is just a
	bit too voluminous for our purposes, so we use our own 32 "ASCII/hex" char
	string for the 128-bit sets used by the "fast path" logic.

	2) Additionally, it was desirable to employ a format that permits fairly
	efficient queries of individual bits WITHOUT having to "de-serialize" the
	entire bitset.
*/
void compiler::emitPackedBitset(const std::bitset<128>& b)
{
	// output the 128-bit bitset in a 4-bits-per-ASCII/hex-character format.
	for (auto c = 128 - 4; c >= 0; c -= 4)
		emit(hexDigit(
			(b.test((size_t)c + 0) ? 1 : 0) |
			(b.test((size_t)c + 1) ? 2 : 0) |
			(b.test((size_t)c + 2) ? 4 : 0) |
			(b.test((size_t)c + 3) ? 8 : 0)));
}

/*
	match examines and the supplied [UTF-8] target text and attempts to match
	it against the previously compiled pattern, returning success/failure.
*/
bool matcher::match(std::string_view target) const
{
	// make SURE target is *structurally* valid UTF8
	if (!validateUTF8String(target))
		throw std::invalid_argument("Target string is not valid UTF-8.");
	auto anchored = true, invert = false;
	utf8iteratorBare next = nullptr;
	utf8iterator ti = target.cbegin();
	// iterate over the previously compiled pattern representation, consuming
	// recognized (matched) elements of the target text
	for (auto first = cbegin(), mi = first, last = cend(); mi != last;)
		switch (*mi++) {
		case '#':
			// "no-op" from the perspective of matching
			mi += LengthSize;
			break;
		case '?':
			// accept ("match") single target code point
			anchored = true, ++ti;
			break;
		case '*':
			// set "free" or "floating" match meta state; this MAY involve
			// "skipping over" zero or more target code points
			anchored = false;
			break;
		case '[':
			// prep for full "interpreted" UTF-8 character class recognition
			invert = decodeModifierAt(mi) != 0;
			next = mi + 1 + LengthSize + decodeLengthAt(mi + 1) + 1, mi += 1 + LengthSize;
			break;
		case '{':
			// perform "fast path" (all-ASCII) character class match
			if (anchored) {
				if (const auto tx = *ti; !(isascii(tx) && testPackedBitsetAt(mi, tx)))
					return false;
				// (consume target code point(s) and skip to after the ']')
				++ti, mi += 32;
			} else {
				auto i = find_if(ti, utf8iterator(target.cend()), [=](char32_t tx) { return isascii(tx) && testPackedBitsetAt(mi, tx); });
				if (i == target.cend())
					return false;
				// (consume target code point(s) and skip to after the ']')
				ti = ++i, anchored = true, mi += 32;
			}
			break;
		case '+': {
			// attempt to match single "interpreted" character class code point
			const auto p = *mi++;
			if (anchored) {
				if (const auto tx = *ti; (p == tx) ^ invert)
					// (consume target code point(s) and skip to after the ']')
					++ti, mi = next;
			} else {
				auto i = find_if(ti, utf8iterator(target.cend()), [=](char32_t tx) { return (p == tx) ^ invert; });
				if (i != target.cend())
					// (consume target code point(s) and skip to after the ']')
					ti = ++i, anchored = true, mi = next;
			}
			break;
		}
		case '-': {
			// attempt to match "interpreted" character class "range" code point
			const auto p1 = *mi++, p2 = *mi++;
			if (anchored) {
				const auto tx = *ti;
				if ((p1 <= tx && tx <= p2) ^ invert)
					// (consume target code point(s) and skip to after the ']')
					++ti, mi = next;
			} else {
				auto i = find_if(ti, utf8iterator(target.cend()), [=](char32_t tx) { return (p1 <= tx && tx <= p2) ^ invert; });
				if (i != target.cend())
					// (consume target code point(s) and skip to after the ']')
					ti = ++i, anchored = true, mi = next;
			}
			break;
		}
		case ']':
			// end of "interpreted" UTF-8 character class... if we get here, it
			// means we did NOT match ANY target code point - i.e., "failure"
			return false;
		case '=': {
			// attempt an exact sequence of UTF-8 code points match
			const auto n = decodeLengthAt(mi);
			const auto o = ti - target.cbegin();
			const auto i = target.find(mi + LengthSize, o, n);
			// (below means "not found" OR "found, but not where expected")
			if (i == string::npos || (anchored && i != o))
				return false;
			ti = anchored ? ti + n : utf8iterator(target.cbegin() + i + n), anchored = true, mi += LengthSize + n;
			break;
		}
		}
	// return whether we [successfully] consumed ALL target text OR the pattern
	// ended in a "free" or "floating" match state (e.g.,  "ab*" matches "abZ")
	return ti == target.cend() || !anchored;
}

/*
	pretty_print produces a formatted representation of the compiled form of the
	current finite state machine on the supplied ostream.
*/
void matcher::pretty_print(std::ostream& s, std::string_view pre) const
{
	// (local fn to compute width for Unicode representation)
	auto w = [](char32_t c) { return c < 0x010000 ? 4 : c < 0x100000 ? 5 : 6; };
	// (local fn to show Unicode char as ASCII if we can, else use "U+..." form)
	auto a = [&](char32_t c) -> std::ostream& {
		return
			isascii(c) ?
				s << (char)c :
				s << "U+" << std::hex << std::uppercase << std::setfill('0') << std::setw(w(c))
				  << (int)c
				  << std::dec << std::setfill(' ');
	};
	// iterate over each element of finite state machine...
	for (auto first = cbegin(), mi = first, last = cend(); mi != last;) {
		const auto op = *mi++;
		s << pre << "[" << std::setw(4) << (mi - first - 1) << "] op: " << (char)op;
		switch (op) {
		case '#':
			// display length of compiled pattern
			s << " len: " << decodeLengthAt(mi);
			mi += LengthSize;
			break;
		case '[':
			// display control metadata from "interpreted" character class
			s << " mod: " << (char)*mi << " len: " << decodeLengthAt(mi + 1);
			mi += 1 + LengthSize;
			break;
		case '{':
			// display bitset from "fast path" character class
			s << " val: ";
			std::copy((utf8iteratorBare::base_type)mi, (utf8iteratorBare::base_type)mi + 32, std::ostreambuf_iterator<char>(s));
			mi += 32;
			break;
		case '+':
			// display SINGLE match case from "interpreted" character class
			s << " val: ", a(*mi++);
			break;
		case '-':
			// display RANGE match case from "interpreted" character class
			s << " val: ", a(*mi++) << ' ', a(*mi++);
			break;
		case '=': {
			// display "exact match" string from glob pattern
			const auto n = decodeLengthAt(mi);
			s << " len: " << n << " val:";
			// "leading space" rules: NEVER show ASCII sequences with embedded
			// spaces, ALWAYS show [multi-byte] Unicode code points as " U+..."
			// for each, and ALWAYS insert a space when switching between them.
			enum LeadingSpace { None, Ascii, Unicode } state = None;
			std::for_each(mi + LengthSize, mi + LengthSize + n, [&](char32_t c) {
				if (const auto newState = isascii(c) ? Ascii : Unicode; newState != state || state == Unicode)
					s << ' ', state = newState;
				a(c);
			});
			mi += LengthSize + n;
			break;
		}
		}
		s << std::endl;
	}
}
