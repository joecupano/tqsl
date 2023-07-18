/***************************************************************************
                          xml.h  -  description
                             -------------------
    begin                : Fri Aug 9 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __xml_h
#define __xml_h

#include <string>
#include <map>
#include <vector>
#include <utility>
#include <expat.h>

using std::pair;
using std::string;
using std::ostream;
using std::map;
using std::multimap;
using std::vector;

namespace tqsllib {

class XMLElement;

typedef multimap<string, XMLElement*> XMLElementList;
typedef map<string, string> XMLElementAttributeList;

/** Encapsulates an XML element
  *
  * An XMLElement comprises a name, the enclosed text, an optional set of
  * attributes, and an optional list of contained elements.
  *
  * Having a list of contained elements allows construction of the XML
  * document tree structure. In most cases, the structure will be populated
  * by a call to parseFile().
  */
class XMLElement {
 public:
	XMLElement() : _name(""), _text(""), _pretext("") {}
	/// Constructor initializes element name
	explicit XMLElement(const string& name) : _text(""), _pretext("") { _name = name; }
	/// Clear the element of all data
	void clear();
	/// Set the element name
	void setElementName(const string& name) { _name = name; }
	/// Get the element name
	string getElementName() const { return _name; }
	/// Set an attribute.
    /** Attributes are treated as unique key/value pairs. */
	void setAttribute(const string& key, const string& value);
	/// Get an attribute by its key.
	/** Returns a pair where:
      *
      * \li \c getAttribute().first := the attribute's value
      * \li \c getAttribute().second := a bool, true if the attribute key exists
      */
	pair<string, bool> getAttribute(const string& key);
	/// Add an element to the list of contained subelements
	XMLElementList::iterator addElement(XMLElement* element);
	XMLElementAttributeList& getAttributeList() { return _attributes; }
	XMLElementList& getElementList() { return _elements; }
	/// Parse an XML file and add its element tree to this element
	int parseFile(const char *filename);
#define XML_PARSE_NO_ERROR 0
#define XML_PARSE_SYSTEM_ERROR 1
#define XML_PARSE_SYNTAX_ERROR 2
	/// Parse an XML string and add its element tree to this element
	int parseString(const char *xmlstring);
	/// Get the first attribute of the element
    /** Provides the attribute key and value. Returns \c false if the
      * element contains no attributes */
	bool getFirstAttribute(string& key, string& attr);
	/// Get the next attribute of the element
    /** Should be called only after calling getFirstAttribute and getting
      * a return value of \c true.
      * Provides the attribute key and value. Returns \c false if the
      * element contains no more attributes */
	bool getNextAttribute(string& key, string& attr);
	/// Get the first contained element named \c name.
    /** Returns \c false if the element contains no elements named \c name */
	bool getFirstElement(const string& name, XMLElement&);
	/// Get the first contained element.
    /** Returns \c false if the element contains no elements */
	bool getFirstElement(XMLElement&);
	/// Get the next contained element.
    /** Should be called only after calling getFirstElement and getting
      * a return value of \c true. If the getFirstElement that takes an
      * element name was called, getNextElement will return \c false when there
      * are no more elements of that name in the element list.
	  *
      * Returns \c false if the element contains no more elements */
	bool getNextElement(XMLElement&);
	/// Set the contained text string
	void setText(const string& s) { _text = s; }
	/// Get the contained text string.
	/** Note that this string comprises the text contained in this
      * element only, not any text contained in elements on the
      * element list; they each have their own contained text.
      */
	string getText() const { return _text; }
	void setPretext(const string& s) { _pretext = s; }
	string getPretext() const { return _pretext; }

 private:
	static void xml_start(void *data, const XML_Char *name, const XML_Char **atts);
	static void xml_end(void *data, const XML_Char *name);
	static void xml_text(void *data, const XML_Char *text, int len);
	string _name, _text, _pretext;
	XMLElementAttributeList _attributes;
	XMLElementList _elements;
	vector<XMLElementList::iterator> _parsingStack;
	XMLElementList::iterator _iter;
	bool _iterByName;
	string _iterName;
	XMLElementAttributeList::iterator _aiter;
};

inline void XMLElement::clear() {
	_name = _text = _pretext = _iterName = "";
	_attributes.clear();
	_elements.clear();
	_parsingStack.clear();
}

inline void
XMLElement::setAttribute(const string& key, const string& value) {
	_attributes[key] = value;
}

inline XMLElementList::iterator
XMLElement::addElement(XMLElement* element) {
	XMLElementList::iterator it = _elements.insert(make_pair(element->getElementName(), element));
	return it;
}

inline bool
XMLElement::getFirstElement(XMLElement& element) {
	_iterByName = false;
	_iter = _elements.begin();
	return getNextElement(element);
}

inline bool
XMLElement::getFirstElement(const string& name, XMLElement& element) {
	_iterName = name;
	_iterByName = true;
	_iter = _elements.find(_iterName);
	return getNextElement(element);
}

inline bool
XMLElement::getNextElement(XMLElement& element) {
	if (_iter == _elements.end())
		return false;
	if (_iterByName && _iter->second->getElementName() != _iterName)
		return false;
	element = *_iter->second;
	++_iter;
	return true;
}

inline bool
XMLElement::getFirstAttribute(string& key, string& attr) {
	_aiter = _attributes.begin();
	return getNextAttribute(key, attr);
}

inline bool
XMLElement::getNextAttribute(string& key, string& attr) {
	if (_aiter == _attributes.end())
		return false;
	key = _aiter->first;
	attr = _aiter->second;
	++_aiter;
	return true;
}

ostream& operator<< (ostream& stream, XMLElement& el);

}	// namespace tqsllib

#endif // __xml_h
