/***************************************************************************
                          tqslexc.h  -  description
                             -------------------
    begin                : Sat Dec 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslexc_h
#define __tqslexc_h

#include <string>
#include <exception>
#include "tqsllib.h"

using std::exception;
using std::string;

class myexc : public exception {
 public:
	explicit myexc(const string& err) : exception() { _err = err; }
	myexc(const myexc& x) { _err = x._err; }
	virtual const char *what() const throw () { return _err.c_str(); }
	virtual ~myexc() throw() {}
 private:
	string _err;
};

class tqslexc : public myexc {
 public:
	tqslexc() : myexc(tqsl_getErrorString()) {}
};

#endif	// __tqslexc_h
