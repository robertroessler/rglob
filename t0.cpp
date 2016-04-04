#include <iostream>
#include "rglob.h"

using namespace std;
using namespace rglob;

static void validate(string p, string t, bool x = true, bool pp = false);

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
	validate(u8"*[\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F]BAR", u8"fu\u041f \u0444BAR", true, true);
	validate(u8"*[А-Я а-я][А-Я а-я][А-Я а-я]BAR", u8"fuП фBAR", true, true);
	return 0;
}

/*
	validate "wraps" pattern compiling, optional pretty-printing, and matching,
	displaying a [generally] single-line/test formatted report, while catching
	and reporting any of the exceptions thrown by rglob.
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
		cout << "Wanted "
			<< mf(x) << ", got "
			<< mf(r) << " ("
			<< ((r != x) ? "BZZZT!" : "OK") << ") with "
			<< t << " -> "
			<< p << endl;
	} catch (invalid_argument& e) {
		cerr << "*** Matching " << t << " => std::invalid_argument: " << e.what() << endl;
	}
}
