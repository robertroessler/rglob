# RGlob

[![Build status](https://ci.appveyor.com/api/projects/status/github/robertroessler/rglob?svg=true)](https://ci.appveyor.com/project/robertroessler/rglob)

The RGlob (rglob) project provides a simple pattern-matcher supporting patterns
constructed in the familiar "glob" style - but separated from any of the file or
folder matching operations - JUST the patterns.

It is written in fairly idiomatic modern C++ 20, and in addition to the main
pattern compiling/matching functionality, also includes a few "helper" functions
for working with UTF-8 -encoded Unicode text - and a template "adaptor" class
that augments both std::string iterators and "bare" const char* text pointers to
enable natural processing of UTF-8 textual data.

__Note that the new version of rglob has been re-implemented as a C++ "header-only library",
so that to be used, you only need to reference / include the "rglob.h" file... to
re-iterate, rglob is no longer a traditional [binary artifact] "library", static or otherwise.__

The primary "user" (as well as "developer") documentation for rglob is present
in the rglob.h header file, while examples and a *test harness* are provided in "t0.cpp".

Besides being "pure" C++, the code is believed to be both 32/64 -bit "safe", and
to contain no dependencies (overtly or lurking) on Windows.

## ToDo

Possible items to work on - for myself or collaborators include

* additional (and properly laid out) test cases, both to serve as actual tests
and to show examples of usage... DONE!

* compiling character classes as "subroutines" - this could provide significant
space savings in more complex/repetitive patterns

## ProbablyNot

Things that most likely should NOT happen include

* any attempt to "extend" rglob so that it supports a more powerful "pattern
language" - globs are globs, and if your needs are for something more like real
regular expressions... then use real regular expressions
