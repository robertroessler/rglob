#include <iostream>
#include "rglob.h"

using namespace std;
using namespace rglob;

int main(int argc, char* argv[])
{
	glob g;
	try {
		//g.compile("a?c*def*[^]ABx-z]*");
		//g.compile("[at]*");
		//g.compile(u8"[\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F][\u0410-\u042F \u0430-\u044F]");
		g.compile(u8"*[А-Я а-я][А-Я а-я][А-Я а-я]XYZ");
		g.pretty_print(cout);
		//g.match("abcYdefABBAw");
		//g.match("a");
		//g.match(u8"\U0000041F");
		cout << g.match(u8"fuП фXYZ") << endl;
	}
	catch (invalid_argument& e)
	{
		cerr << e.what() << endl;
	}
	catch (length_error& e)
	{
		cerr << e.what() << endl;
	}
	return 0;
}

