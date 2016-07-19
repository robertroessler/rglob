RGlob
=====
The RGlob (rglob) project provides a simple pattern-matcher supporting patterns
constructed in the familiar "glob" style - but separated from any of the file or
folder matching operations - JUST the patterns.

It is written in fairly idiomatic modern C++ 11/14, and in addition to the main
pattern compiling/matching functionality, also includes a few "helper" functions
for working with UTF-8 -encoded Unicode text - and a template "adaptor" class
that augments both std::string iterators and "bare" const char* text pointers to
enable natural processing of UTF-8 textual data.

The primary "user" (as well as "developer") documentation for rglob is present
in the rglob.h header file.

Besides being "pure" C++, the code is believed to be both 32/64 -bit "safe", and
to contain no dependencies (overtly or lurking) on Windows.

ToDo
====
Possible items to work on - for myself or collaborators include

* additonal (and properly laid out) test cases, both to serve as actual tests
and to show examples of usage

* compiling character classes as "subroutines" - this could provide significant
space savings in more complex/repetitive patterns

* as only Visual Studio 2015 project and solution files are initially present,
control files for building in non-Windows environments could be useful

ProbablyNot
===========
Things that most likely should NOT happen include

* any attempt to "extend" rglob so that it supports a more powerful "pattern
language" - globs are globs, and if your needs are for something more like real
regular expressions... then use real regular expressions

[![Build status](https://ci.appveyor.com/api/projects/status/github/robertroessler/rglob?svg=true)](https://ci.appveyor.com/project/robertroessler/rglob)
