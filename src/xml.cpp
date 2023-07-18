/***************************************************************************
                          xml.cpp  -  description
                             -------------------
    begin                : Fri Aug 9 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#define TQSLLIB_DEF

#include "xml.h"
#include "tqsllib.h"
#ifdef _WIN32
#include <fcntl.h>
#endif
#include <string.h>
#include <zlib.h>
#include <stack>
#include <fstream>
#include <utility>
#include <string>

using std::pair;
using std::string;
using std::ostream;
using std::map;

namespace tqsllib {

pair<string, bool>
XMLElement::getAttribute(const string& key) {
	string s;
	XMLElementAttributeList::iterator pos;
	pos = _attributes.find(key);
	pair<string, bool> rval;
	if (pos == _attributes.end()) {
		rval.second = false;
	} else {
		rval.first = pos->second;
		rval.second = true;
	}
	return rval;
}

void
XMLElement::xml_start(void *data, const XML_Char *name, const XML_Char **atts) {
	XMLElement *el = reinterpret_cast<XMLElement *>(data);
	XMLElement *new_el = new XMLElement(name);
//cout << "Element: " << name << endl;
	for (int i = 0; atts[i]; i += 2) {
		new_el->setAttribute(atts[i], atts[i+1]);
	}
	if (el->_parsingStack.empty()) {
		el->_parsingStack.push_back(el->addElement(new_el));
	} else {
		new_el->setPretext(el->_parsingStack.back()->second->getText());
		el->_parsingStack.back()->second->setText("");
		el->_parsingStack.push_back(el->_parsingStack.back()->second->addElement(new_el));
	}
}

void
XMLElement::xml_end(void *data, const XML_Char *name) {
	XMLElement *el = reinterpret_cast<XMLElement *>(data);
	if (!(el->_parsingStack.empty()))
		el->_parsingStack.pop_back();
}

void
XMLElement::xml_text(void *data, const XML_Char *text, int len) {
	XMLElement *el = reinterpret_cast<XMLElement *>(data);
	el->_parsingStack.back()->second->_text.append(text, len);
}

int
XMLElement::parseString(const char *xmlstring) {
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, reinterpret_cast<void *>(this));
	XML_SetStartElementHandler(xp, &XMLElement::xml_start);
	XML_SetEndElementHandler(xp, &XMLElement::xml_end);
	XML_SetCharacterDataHandler(xp, &XMLElement::xml_text);

	_parsingStack.clear();
	// Process the XML
	if (XML_Parse(xp, xmlstring, strlen(xmlstring), 1) == 0) {
		XML_ParserFree(xp);
		strncpy(tQSL_CustomError, xmlstring, 80);
		tQSL_CustomError[79] = '\0';
		return XML_PARSE_SYNTAX_ERROR;
	}
	XML_ParserFree(xp);
	return XML_PARSE_NO_ERROR;
}

int
XMLElement::parseFile(const char *filename) {
	gzFile in = NULL;
#ifdef _WIN32
	wchar_t* fn = utf8_to_wchar(filename);
	int fd = _wopen(fn, _O_RDONLY|_O_BINARY);
	free_wchar(fn);
	if (fd != -1)
		in = gzdopen(fd, "rb");
#else
	in = gzopen(filename, "rb");
#endif

	if (!in)
		return XML_PARSE_SYSTEM_ERROR;	// Failed to open file
	char buf[256];
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, reinterpret_cast<void *>(this));
	XML_SetStartElementHandler(xp, &XMLElement::xml_start);
	XML_SetEndElementHandler(xp, &XMLElement::xml_end);
	XML_SetCharacterDataHandler(xp, &XMLElement::xml_text);

	_parsingStack.clear();
	int rcount;
	while ((rcount = gzread(in, buf, sizeof buf)) > 0) {
		// Process the XML
		if (XML_Parse(xp, buf, rcount, 0) == 0) {
			gzclose(in);
			strncpy(tQSL_CustomError, buf, 80);
			tQSL_CustomError[79] = '\0';
			XML_ParserFree(xp);
			return XML_PARSE_SYNTAX_ERROR;
		}
	}
	gzclose(in);
	bool rval = (rcount == 0);
	if (rval)
		rval = (XML_Parse(xp, "", 0, 1) != 0);
	XML_ParserFree(xp);
	return (rval ? XML_PARSE_NO_ERROR : XML_PARSE_SYNTAX_ERROR);
}


static struct {
	char c;
	const char *ent;
} xml_entity_table[] = {
	{ '"', "&quot;" },
	{ '\'', "&apos;" },
	{ '>', "&gt;" },
	{ '<', "&lt;" }
};

static string
xml_entities(const string& s) {
	string ns = s;
	string::size_type idx = 0;
	while ((idx = ns.find('&', idx)) != string::npos) {
		ns.replace(idx, 1, "&amp;");
		idx++;
	}
	for (int i = 0; i < static_cast<int>((sizeof xml_entity_table / sizeof xml_entity_table[0])); i++) {
		while ((idx = ns.find(xml_entity_table[i].c)) != string::npos)
			ns.replace(idx, 1, xml_entity_table[i].ent);
	}
	return ns;
}

/* Stream out an XMLElement as XML text */
ostream&
operator<< (ostream& stream, XMLElement& el) {
	bool ok;
	XMLElement subel;
	if (el.getElementName() != "") {
		stream << "<" << el.getElementName();
		string key, val;
		bool ok = el.getFirstAttribute(key, val);
		while (ok) {
			stream << " " << key << "=\"" << xml_entities(val) << "\"";
			ok = el.getNextAttribute(key, val);
		}
		if (el.getText() == "" && !el.getFirstElement(subel)) {
			stream << " />";
			return stream;
		} else {
			stream << ">";
		}
	}
	ok = el.getFirstElement(subel);
	while (ok) {
		string s = subel.getPretext();
		if (s != "")
			stream << xml_entities(s);
		stream << subel;
		ok = el.getNextElement(subel);
	}
	if (el.getText() != "")
		stream << xml_entities(el.getText());
	if (el.getElementName() != "")
		stream << "</" << el.getElementName() << ">";
	return stream;
}

}	// namespace tqsllib
