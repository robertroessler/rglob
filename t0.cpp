#include <iostream>
#include "rglob.h"

using namespace std;
using namespace rglob;

static void validate(string p, string t, bool x = true, bool pp = false);

int main(int argc, char* argv[])
{
	validate("a?c*def*[^]ABx-z]*", "abcYdefABBAw");
	validate("[at*]", "a");
	validate(u8"*[\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F]XYZ", u8"fu\u041f \u0444XYZ");
	validate(u8"*[А-Я а-я][А-Я а-я][А-Я а-я]XYZ", u8"fuП фXYZ");
	return 0;
}

static void validate(string p, string t, bool x, bool pp)
{
	bool goodCompile = true;
	auto mf = [](bool tf) { return tf ? "MATCH" : "FAIL!"; };
	glob g;
	try {
		g.compile(p);
	} catch (invalid_argument& e) {
		cerr << "*** Compiling " << p << " => std::invalid_argument: " << e.what() << endl, goodCompile = false;
	} catch (length_error& e) {
		cerr << "*** Compiling " << p << " => std::length_error: " << e.what() << endl, goodCompile = false;
	}
	if (goodCompile && pp)
		cout << "Pretty_print of " << p << ':' << endl, g.pretty_print(cout);
	try {
		auto r = g.match(t);
		cout << "Wanted "
			<< mf(x) << ", got "
			<< mf(r) << " ("
			<< ((r ^ x) ? "BZZZT!" : "OK") << ") with "
			<< t << " -> "
			<< p << endl;
	} catch (invalid_argument& e) {
		cerr << "*** Matching " << t << " => std::invalid_argument: " << e.what() << endl;
	}
}
