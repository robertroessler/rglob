#include <iostream>
#include "rglob.h"

using namespace std;
using namespace rglob;

static void validate(string p, string t, bool x = true, bool pp = false);

/*
	Both the main and validate functions below illustrate some sample patterns
	and targets, as well as providing a useful framework for simple testing.

	N.B. - it should be emphasized that "globs" are NOT "regexps", and in
	particular, a single character class, no matter how complex, will match AT
	MOST a SINGLE [UTF-8] character / "code point" from the target string - no
	number of '+' or '*' chars after the closing ']' will change this, because
	well, "glob" patterns really AREN'T regular expressions (like we said).
*/
int main(int argc, char* argv[])
{
	// validate the simplest patterns...
	validate("abc", "abc");
	validate("abc", "abC", false);
	validate("ab?", "abC");
	validate("*bar", "foobar");
	validate("*ba?", "foobaR");

	// ... now for some character classes
	validate("[A-Z][0-9][^0-9]", "B2B");
	validate("[A-Z][0-9][^0-9]", "B2Bx", false);
	validate("[A-Z][0-9][^0-9]*", "B2Bx-ray");
	validate("[A-Z][0-9][^0-9]", "B23", false);

	// can you spot why this will throw an exception?
	validate("[A-Z][0-9][^0-9*", "B2Bx-ray");

	// how about some fun?
	validate("a?c*def*[^]ABx-z]*", "abcYdefABBA Van Halen");
	validate("a?c*def[^]ABx-z]*", "abcYdefABBA Van Halen", false);
	validate("a?c*def[]ABx-z]*", "abcYdefABBA Van Halen");

	// the next two validations are really about showing the equivalence between
	// two different ways of inserting Unicode chars into strings (hard vs easy)
	// (they really ARE the same pattern, see the pretty_print output yourself!)
	validate(u8"*[\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F]bar\u03B5", u8"fu\u041f \u0444bar\u03B5", true, true);
	validate(u8"*[А-Я а-я][А-Я а-я][А-Я а-я]barε", u8"fuП фbarε", true, true);
	return 0;
}

/*
	validate "wraps" pattern compiling, optional pretty-printing, and matching,
	displaying a [generally] single-line/test formatted report, while catching
	and reporting any of the exceptions thrown by rglob.

	Usage
	=====
	p	pattern to compile and match against
	t	target text for matching
	x	expected result of match (true -> MATCH, false -> FAIL!)
	pp	pretty_print the compiled version of this pattern

	N.B. - whether or not the handy "u8" from of string literals is used, both
	the pattern and target will be interpreted as containing Unicode in UTF-8!
*/
static void validate(string p, string t, bool x, bool pp)
{
	auto mf = [](bool tf) { return tf ? "MATCH" : "FAIL!"; };
	glob g;
	try {
		g.compile(p);
	} catch (invalid_argument& e) {
		cerr << "*** Compiling " << p << " => std::invalid_argument: " << e.what() << endl;
		return; // we're outta here - after a compile fail, "match" is undefined
	} catch (length_error& e) {
		cerr << "*** Compiling " << p << " => std::length_error: " << e.what() << endl;
		return; // we're outta here - after a compile fail, "match" is undefined
	}
	if (pp)
		cout << "Pretty_print of " << p << ':' << endl, g.pretty_print(cout);
	try {
		const auto r = g.match(t);
		cout << "Want "
			<< mf(x) << ", got "
			<< mf(r) << " ("
			<< ((r != x) ? "BZZZT!" : "OK") << ") with "
			<< t << " -> "
			<< p << endl;
	} catch (invalid_argument& e) {
		cerr << "*** Matching " << t << " => std::invalid_argument: " << e.what() << endl;
	}
}
