/***************************************************************************
                          location.cpp  -  description
                             -------------------
    begin                : Wed Nov 6 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: location.cpp,v 1.14 2013/03/01 13:20:30 k1mu Exp $
 ***************************************************************************/


// #define DXCC_TEST

#define TQSLLIB_DEF

#include "location.h"

#include <errno.h>
#include <stdlib.h>
#include <zlib.h>
#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#endif
#include <cstring>
#include <fstream>
#include <algorithm>
#include <vector>
#include <iostream>
#include <utility>
#include <map>
#include <string>
#include <cctype>
#include <functional>
#include "tqsllib.h"
#include "tqslerrno.h"
#include "xml.h"
#include "openssl_cert.h"
#ifdef _WIN32
	#include "windows.h"
	#include <fcntl.h>
#endif

#include "winstrdefs.h"

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::ofstream;
using std::ios;
using std::endl;
using std::exception;

static int init_adif_map(void);


namespace tqsllib {

class TQSL_LOCATION_ITEM {
 public:
	TQSL_LOCATION_ITEM() : ivalue(0) {}
	string text;
	string label;
	string zonemap;
	int ivalue;
};

class TQSL_LOCATION_FIELD {
 public:
	TQSL_LOCATION_FIELD() {}
	TQSL_LOCATION_FIELD(string i_gabbi_name, const char *i_label, int i_data_type, int i_data_len,
		int i_input_type, int i_flags = 0);
	string label;
	string gabbi_name;
	int data_type;
	int data_len;
	string cdata;
	vector<TQSL_LOCATION_ITEM> items;
	int idx;
	int idata;
	int input_type;
	int flags;
	bool changed;
	string dependency;
};

TQSL_LOCATION_FIELD::TQSL_LOCATION_FIELD(string i_gabbi_name, const char *i_label, int i_data_type,
	int i_data_len, int i_input_type, int i_flags) : data_type(i_data_type), data_len(i_data_len), cdata(""),
	input_type(i_input_type), flags(i_flags) {
		if (!i_gabbi_name.empty())
		gabbi_name = i_gabbi_name;
	if (i_label)
		label = i_label;
	idx = idata = 0;
}

typedef vector<TQSL_LOCATION_FIELD> TQSL_LOCATION_FIELDLIST;

class TQSL_LOCATION_PAGE {
 public:
	TQSL_LOCATION_PAGE() : complete(false), prev(0), next(0) {}
	bool complete;
	int prev, next;
	string dependentOn, dependency;
	map<string, vector<string> > hash;
	TQSL_LOCATION_FIELDLIST fieldlist;
};

typedef vector<TQSL_LOCATION_PAGE> TQSL_LOCATION_PAGELIST;

class TQSL_NAME {
 public:
	explicit TQSL_NAME(string n = "", string c = "") : name(n), call(c) {}
	string name;
	string call;
};

class TQSL_LOCATION {
 public:
	TQSL_LOCATION() : sentinel(0x5445), page(0), cansave(false), sign_clean(false), cert_flags(TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED), newflags(false), newDXCC(-1) {}

	~TQSL_LOCATION() { sentinel = 0; }
	int sentinel;
	int page;
	bool cansave;
	string name;
	TQSL_LOCATION_PAGELIST pagelist;
	vector<TQSL_NAME> names;
	string signdata;
	string loc_details;
	string qso_details;
	bool sign_clean;
	string tSTATION;
	string tCONTACT;
	string sigspec;
	char data_errors[512];
	int cert_flags;
	bool newflags;
	int newDXCC;
};

class Band {
 public:
	string name, spectrum;
	int low, high;
};

class Mode {
 public:
	string mode, group;
};

class PropMode {
 public:
	string descrip, name;
};

class Satellite {
 public:
	Satellite() {
		start.year = start.month = start.day = 0;
		end.year = end.month = end.day = 0;
	}
	string descrip, name;
	tQSL_Date start, end;
};

bool
operator< (const Band& o1, const Band& o2) {
	static const char *suffixes[] = { "M", "CM", "MM"};
	static const char *prefix_chars = "0123456789.";
	// get suffixes
	string b1_suf = o1.name.substr(o1.name.find_first_not_of(prefix_chars));
	string b2_suf = o2.name.substr(o2.name.find_first_not_of(prefix_chars));
	if (b1_suf != b2_suf) {
		// Suffixes differ -- compare suffixes
		int b1_idx = (sizeof suffixes / sizeof suffixes[0]);
		int b2_idx = b1_idx;
		for (int i = 0; i < static_cast<int>(sizeof suffixes / sizeof suffixes[0]); i++) {
			if (b1_suf == suffixes[i])
				b1_idx = i;
			if (b2_suf == suffixes[i])
				b2_idx = i;
		}
		return b1_idx < b2_idx;
	}
	return atof(o1.name.c_str()) > atof(o2.name.c_str());
}

bool
operator< (const PropMode& o1, const PropMode& o2) {
	if (o1.descrip < o2.descrip)
		return true;
	if (o1.descrip == o2.descrip)
		return (o1.name < o2.name);
	return false;
}

bool
operator< (const Satellite& o1, const Satellite& o2) {
	if (o1.descrip < o2.descrip)
		return true;
	if (o1.descrip == o2.descrip)
		return (o1.name < o2.name);
	return false;
}

bool
operator< (const Mode& o1, const Mode& o2) {
	static const char *groups[] = { "CW", "PHONE", "IMAGE", "DATA" };
	// m1 < m2 if m1 is a modegroup and m2 is not
	if (o1.mode == o1.group) {
		if (o2.mode != o2.group)
			return true;
	} else if (o2.mode == o2.group) {
		return false;
	}
	// If groups are same, compare modes
	if (o1.group == o2.group)
		return o1.mode < o2.mode;
	int m1_g = (sizeof groups / sizeof groups[0]);
	int m2_g = m1_g;
	for (int i = 0; i < static_cast<int>(sizeof groups / sizeof groups[0]); i++) {
		if (o1.group == groups[i])
			m1_g = i;
		if (o2.group == groups[i])
			m2_g = i;
	}
	return m1_g < m2_g;
}

}	// namespace tqsllib

using tqsllib::XMLElement;
using tqsllib::XMLElementList;
using tqsllib::Band;
using tqsllib::Mode;
using tqsllib::PropMode;
using tqsllib::Satellite;
using tqsllib::TQSL_LOCATION;
using tqsllib::TQSL_LOCATION_PAGE;
using tqsllib::TQSL_LOCATION_PAGELIST;
using tqsllib::TQSL_LOCATION_FIELD;
using tqsllib::TQSL_LOCATION_FIELDLIST;
using tqsllib::TQSL_LOCATION_ITEM;
using tqsllib::TQSL_NAME;
using tqsllib::ROOTCERT;
using tqsllib::CACERT;
using tqsllib::USERCERT;
using tqsllib::tqsl_get_pem_serial;

#define CAST_TQSL_LOCATION(x) (reinterpret_cast<TQSL_LOCATION *>((x)))

typedef map<int, string> IntMap;
typedef map<int, bool> BoolMap;
typedef map<int, tQSL_Date> DateMap;

static int num_entities = 0;
static bool _ent_init = false;

static struct _dxcc_entity {
	int number;
	const char* name;
	const char *zonemap;
	tQSL_Date start, end;
} *entity_list = 0;

template<typename T1, typename T2, typename T3>
struct triplet {
	T1 first;
	T2 middle;
	T3 last;
};

template<typename T1, typename T2, typename T3>
triplet<T1, T2, T3>  make_triplet(const T1 &f, const T2 &m, const T3 &l) {
	triplet<T1, T2, T3> trip;
	trip.first = f;
	trip.middle = m;
	trip.last = l;
	return trip;
}

// config data

static XMLElement tqsl_xml_config;
static int tqsl_xml_config_major = -1;
static int tqsl_xml_config_minor = 0;
static IntMap DXCCMap;
static BoolMap DeletedMap;
static IntMap DXCCZoneMap;
static DateMap DXCCStartMap;
static DateMap DXCCEndMap;
static vector< pair<int, string> > DXCCList;
static vector<Band> BandList;
static vector<Mode> ModeList;
static vector<PropMode> PropModeList;
static vector<Satellite> SatelliteList;
static map<int, XMLElement> tqsl_page_map;
static map<string, XMLElement> tqsl_field_map;
static map<string, string> tqsl_adif_map;
static vector<string> tqsl_adif_mode_map;
static map<string, string> tqsl_adif_submode_map;
static map<string, triplet<int, int, TQSL_CABRILLO_FREQ_TYPE> > tqsl_cabrillo_map;
static map<string, pair<int, int> > tqsl_cabrillo_user_map;


static char
char_toupper(char c) {
	return toupper(c);
}

static string
string_toupper(const string& in) {
	string out = in;
	transform(out.begin(), out.end(), out.begin(), char_toupper);
	return out;
}
// isspace() called on extended chars in UTF-8 raises asserts in
// the windows C++ libs. Don't call isspace() if out of range.
//
static inline int isspc(int c) {
	if (c < 0 || c > 255)
		return 0;
	return isspace(c);
}

// trim from start
static inline std::string &ltrim(std::string &s) {
#if __cplusplus > 199711L
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int c) {return !std::isspace(c);}));
#else
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(isspc))));
#endif
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
#if __cplusplus > 199711L
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch);}).base(), s.end());
#else
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(isspc))).base(), s.end());
#endif
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

#define TQSL_NPAGES 4

static TQSL_LOCATION *
check_loc(tQSL_Location loc, bool unclean = true) {
	if (tqsl_init())
		return 0;
	if (loc == 0)
		return 0;
	if (unclean)
		CAST_TQSL_LOCATION(loc)->sign_clean = false;
	return CAST_TQSL_LOCATION(loc);
}

static int
tqsl_load_xml_config() {
	if (tqsl_xml_config.getElementList().size() > 0)	// Already init'd
		return 0;
	XMLElement default_config;
	XMLElement user_config;
	tqslTrace("tqsl_load_xml_config", NULL);

#ifdef _WIN32
	string default_path = string(tQSL_RsrcDir) + "\\config.xml";
	string user_path = string(tQSL_BaseDir) + "\\config.xml";
#else
	string default_path = string(tQSL_RsrcDir) + "/config.xml";
	string user_path = string(tQSL_BaseDir) + "/config.xml";
#endif

	tqslTrace("tqsl_load_xml_config", "user_path=%s", user_path.c_str());
	int default_status = default_config.parseFile(default_path.c_str());
	int user_status = user_config.parseFile(user_path.c_str());
	tqslTrace("tqsl_load_xml_config", "default_status=%d, user_status=%d", default_status, user_status);
	if (default_status != XML_PARSE_NO_ERROR && user_status != XML_PARSE_NO_ERROR) {
		if (user_status == XML_PARSE_SYSTEM_ERROR)
			tQSL_Error = TQSL_CONFIG_ERROR;
		else
			tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}

	int default_major = -1;
	int default_minor = 0;
	int user_major = -1;
	int user_minor = 0;

	XMLElement top;
	if (default_config.getFirstElement("tqslconfig", top)) {
		default_major = strtol(top.getAttribute("majorversion").first.c_str(), NULL, 10);
		default_minor = strtol(top.getAttribute("minorversion").first.c_str(), NULL, 10);
	}
	if (user_config.getFirstElement("tqslconfig", top)) {
		user_major = strtol(top.getAttribute("majorversion").first.c_str(), NULL, 10);
		user_minor = strtol(top.getAttribute("minorversion").first.c_str(), NULL, 10);
	}

	if (default_major > user_major
		|| (default_major == user_major && default_minor > user_minor)) {
			tqsl_xml_config = default_config;
			tqsl_xml_config_major = default_major;
			tqsl_xml_config_minor = default_minor;
			return 0;
	}
	if (user_major < 0) {
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		tqslTrace("tqsl_load_xml_config", "Syntax error");
		return 1;
	}
	tqsl_xml_config	= user_config;
	tqsl_xml_config_major = user_major;
	tqsl_xml_config_minor = user_minor;
	return 0;
}

static int
tqsl_get_xml_config_section(const string& section, XMLElement& el) {
	if (tqsl_load_xml_config())
		return 1;
	XMLElement top;
	if (!tqsl_xml_config.getFirstElement("tqslconfig", top)) {
		tqsl_xml_config.clear();
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}
	if (!top.getFirstElement(section, el)) {
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}
	return 0;
}

static int
tqsl_load_provider_list(vector<TQSL_PROVIDER> &plist) {
	plist.clear();
	XMLElement providers;
	if (tqsl_get_xml_config_section("providers", providers))
		return 1;
	tqslTrace("tqsl_load_provider_list", NULL);
	XMLElement provider;
	bool gotit = providers.getFirstElement("provider", provider);
	while (gotit) {
		TQSL_PROVIDER pdata;
		memset(&pdata, 0, sizeof pdata);
		pair<string, bool> rval = provider.getAttribute("organizationName");
		if (!rval.second) {
			tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
			tqslTrace("tqsl_load_provider_list", "Providers not found");
			return 1;
		}
		strncpy(pdata.organizationName, rval.first.c_str(), sizeof pdata.organizationName);
		XMLElement item;
		if (provider.getFirstElement("organizationalUnitName", item))
			strncpy(pdata.organizationalUnitName, item.getText().c_str(),
				sizeof pdata.organizationalUnitName);
		if (provider.getFirstElement("emailAddress", item))
			strncpy(pdata.emailAddress, item.getText().c_str(),
				sizeof pdata.emailAddress);
		if (provider.getFirstElement("url", item))
			strncpy(pdata.url, item.getText().c_str(),
				sizeof pdata.url);
		plist.push_back(pdata);
		gotit = providers.getNextElement(provider);
		if (gotit && provider.getElementName() != "provider")
			break;
	}
	return 0;
}

static XMLElement tCONTACT_sign;

static int
make_sign_data(TQSL_LOCATION *loc) {
	map<string, string> field_data;

	// Loop through the location pages, getting field data
	//
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size()))
					s = "";
				else
					s = f.items[f.idx].text;
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				snprintf(buf, sizeof buf, "%d", f.idata);
				s = buf;
			} else {
				s = f.cdata;
			}
			field_data[f.gabbi_name] = s;
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);

	loc->signdata = "";
	loc->loc_details = "";
	loc->sign_clean = false;
	XMLElement sigspecs;
	if (tqsl_get_xml_config_section("sigspecs", sigspecs)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - it does not have a sigspecs section",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	XMLElement sigspec;
	XMLElement ss;
	if (!sigspecs.getFirstElement("sigspec", sigspec)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - it does not have a sigspec section",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	ss = sigspec;
	bool ok;
	do {
		if (sigspec.getAttribute("status").first == "deprecated") {
			ok = sigspecs.getNextElement(sigspec);
			continue;
		}
		double ssver = atof(ss.getAttribute("version").first.c_str());
		double newver = atof(sigspec.getAttribute("version").first.c_str());
		if (newver > ssver)
			ss = sigspec;
		ok = sigspecs.getNextElement(sigspec);
	} while (ok);
	sigspec = ss;

	loc->sigspec = "SIGN_";
	loc->sigspec += sigspec.getAttribute("name").first;
	loc->sigspec += "_V";
	loc->sigspec += sigspec.getAttribute("version").first;

	tCONTACT_sign.clear();
	if (!sigspec.getFirstElement("tCONTACT", tCONTACT_sign)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing sigspec.tCONTACT",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	if (tCONTACT_sign.getElementList().size() == 0) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - empty sigspec.tCONTACT",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	XMLElement tSTATION;
	if (!sigspec.getFirstElement("tSTATION", tSTATION)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing sigspec.tSTATION",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	XMLElement specfield;

	if (!(ok = tSTATION.getFirstElement(specfield))) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing tSTATION.specfield",
			sizeof tQSL_CustomError);
		tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
		return 1;
	}
	do {
		string value = field_data[specfield.getElementName()];
		value = trim(value);
		if (value == "") {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by ";
				attr = sigspec.getAttribute("name");
				if (attr.second)
					err += attr.first + " ";
				attr = sigspec.getAttribute("version");
				if (attr.second)
					err += "V" + attr.first + " ";
				err += "signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				tqslTrace("make_sign_data", "Error %s", tQSL_CustomError);
				return 1;
			}
		} else {
			loc->signdata += value;
			if (loc->loc_details != "") {
				loc->loc_details += ", ";
			}
			loc->loc_details += specfield.getElementName() + ": " + value;
		}
		ok = tSTATION.getNextElement(specfield);
	} while (ok);
	loc->sign_clean = true;
	return 0;
}

static int
init_dxcc() {
	if (DXCCMap.size() > 0)
		return 0;
	tqslTrace("init_dxcc", NULL);
	XMLElement dxcc;
	if (tqsl_get_xml_config_section("dxcc", dxcc)) {
		tqslTrace("init_dxcc", "Error %d getting dxcc config section", tQSL_Error);
		return 1;
	}
	XMLElement dxcc_entity;
	bool ok = dxcc.getFirstElement("entity", dxcc_entity);
	while (ok) {
		pair<string, bool> rval = dxcc_entity.getAttribute("arrlId");
		pair<string, bool> zval = dxcc_entity.getAttribute("zonemap");
		pair<string, bool> strdate = dxcc_entity.getAttribute("valid");
		pair<string, bool> enddate = dxcc_entity.getAttribute("invalid");
		pair<string, bool> deleted = dxcc_entity.getAttribute("deleted");
		if (rval.second) {
			int num = strtol(rval.first.c_str(), NULL, 10);
			DXCCMap[num] = dxcc_entity.getText();
			DeletedMap[num] = false;
			if (deleted.second) {
				DeletedMap[num] = (deleted.first == "1");
			}
			if (zval.second) {
				DXCCZoneMap[num] = zval.first;
			}
			tQSL_Date d;
			d.year = 1945;
			d.month = 11;
			d.day = 15;
			DXCCStartMap[num] = d;
			if (strdate.second) {
				if (!tqsl_initDate(&d, strdate.first.c_str())) {
					DXCCStartMap[num] = d;
				}
			}
			d.year = 0;
			d.month = 0;
			d.day = 0;
			DXCCEndMap[num] = d;
			if (enddate.second) {
				if (!tqsl_initDate(&d, enddate.first.c_str())) {
					DXCCEndMap[num] = d;
				}
			}
			DXCCList.push_back(make_pair(num, dxcc_entity.getText()));
		}
		ok = dxcc.getNextElement(dxcc_entity);
	}
	return 0;
}

static int
init_band() {
	if (BandList.size() > 0)
		return 0;
	tqslTrace("init_band", NULL);
	XMLElement bands;
	if (tqsl_get_xml_config_section("bands", bands)) {
		tqslTrace("init_band", "Error %d getting bands", tQSL_Error);
		return 1;
	}
	XMLElement config_band;
	bool ok = bands.getFirstElement("band", config_band);
	while (ok) {
		Band b;
		b.name = config_band.getText();
		b.spectrum = config_band.getAttribute("spectrum").first;
		b.low = strtol(config_band.getAttribute("low").first.c_str(), NULL, 10);
		b.high = strtol(config_band.getAttribute("high").first.c_str(), NULL, 10);
		BandList.push_back(b);
		ok = bands.getNextElement(config_band);
	}
	sort(BandList.begin(), BandList.end());
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getConfigVersion(int *major, int *minor) {
	if (tqsl_init())
		return 1;
	if (tqsl_load_xml_config()) {
		tqslTrace("tqsl_getConfigVersion", "Error %d from tqsl_load_xml_config", tQSL_Error);
		return 1;
	}
	tqslTrace("tqsl_getConfigVersion", "major=%d, minor=%d", tqsl_xml_config_major, tqsl_xml_config_minor);
	if (major)
		*major = tqsl_xml_config_major;
	if (minor)
		*minor = tqsl_xml_config_minor;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumBand(int *number) {
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tqslTrace("tqsl_getNumBand", NULL);
	if (init_band()) {
		tqslTrace("tqsl_getNumBand", "init_band error=%d", tQSL_Error);
		return 1;
	}
	*number = BandList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getBand(int index, const char **name, const char **spectrum, int *low, int *high) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_band()) {
		tqslTrace("tqsl_getBand", "init_band error=%d", tQSL_Error);
		return 1;
	}
	if (index >= static_cast<int>(BandList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_getBand", "init_band arg error - index %d", index);
		return 1;
	}
	*name = BandList[index].name.c_str();
	if (spectrum)
		*spectrum = BandList[index].spectrum.c_str();
	if (low)
		*low = BandList[index].low;
	if (high)
		*high = BandList[index].high;
	return 0;
}

static int
init_mode() {
	if (ModeList.size() > 0)
		return 0;
	XMLElement modes;
	if (tqsl_get_xml_config_section("modes", modes)) {
		tqslTrace("init_mode", "Error from tqsl_get_xml_config_section %d", tQSL_Error);
		return 1;
	}
	XMLElement config_mode;
	bool ok = modes.getFirstElement("mode", config_mode);
	while (ok) {
		Mode m;
		m.mode = config_mode.getText();
		m.group = config_mode.getAttribute("group").first;
		ModeList.push_back(m);
		ok = modes.getNextElement(config_mode);
	}
	sort(ModeList.begin(), ModeList.end());

	return 0;
}

static int
init_propmode() {
	if (PropModeList.size() > 0)
		return 0;
	XMLElement propmodes;
	if (tqsl_get_xml_config_section("propmodes", propmodes)) {
		tqslTrace("init_propmode", "Error getting config section %d", tQSL_Error);
		return 1;
	}
	XMLElement config_mode;
	bool ok = propmodes.getFirstElement("propmode", config_mode);
	while (ok) {
		PropMode p;
		p.descrip = config_mode.getText();
		p.name = config_mode.getAttribute("name").first;
		PropModeList.push_back(p);
		ok = propmodes.getNextElement(config_mode);
	}
	sort(PropModeList.begin(), PropModeList.end());
	return 0;
}

static int
init_satellite() {
	if (SatelliteList.size() > 0)
		return 0;
	XMLElement satellites;
	if (tqsl_get_xml_config_section("satellites", satellites)) {
		tqslTrace("init_satellite", "Error getting config section %d", tQSL_Error);
		return 1;
	}
	XMLElement config_sat;
	bool ok = satellites.getFirstElement("satellite", config_sat);
	while (ok) {
		Satellite s;
		s.descrip = config_sat.getText();
		s.name = config_sat.getAttribute("name").first;
		tQSL_Date d;
		if (!tqsl_initDate(&d, config_sat.getAttribute("startDate").first.c_str()))
			s.start = d;
		if (!tqsl_initDate(&d, config_sat.getAttribute("endDate").first.c_str()))
			s.end = d;
		SatelliteList.push_back(s);
		ok = satellites.getNextElement(config_sat);
	}
	sort(SatelliteList.begin(), SatelliteList.end());
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumMode(int *number) {
	if (tqsl_init())
		return 1;
	if (number == NULL) {
		tqslTrace("tqsl_getNumMode", "Argument error, number = 0x%lx", number);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_mode()) {
		tqslTrace("tqsl_getNumMode", "init_mode error %d", tQSL_Error);
		return 1;
	}
	*number = ModeList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getMode(int index, const char **mode, const char **group) {
	if (index < 0 || mode == NULL) {
		tqslTrace("tqsl_getMode", "Arg error index=%d, mode=0x%lx, group=0x%lx", index, mode, group);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_mode()) {
		tqslTrace("tqsl_getMode", "init_mode error %d", tQSL_Error);
		return 1;
	}
	if (index >= static_cast<int>(ModeList.size())) {
		tqslTrace("tqsl_getMode", "Argument error: %d", index);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*mode = ModeList[index].mode.c_str();
	if (group)
		*group = ModeList[index].group.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumADIFMode(int *number) {
	if (tqsl_init())
		return 1;
	if (number == NULL) {
		tqslTrace("tqsl_getNumADIFMode", "Argument error, number = 0x%lx", number);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tqslTrace("tqsl_getNumADIFMode", "init_mode error %d", tQSL_Error);
		return 1;
	}
	*number = tqsl_adif_mode_map.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFModeEntry(int index, const char **mode) {
	if (tqsl_init())
		return 1;
	if (mode == NULL) {
		tqslTrace("tqsl_getADIFMode", "Argument error, mode = 0x%lx", mode);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tqslTrace("tqsl_getADIFMode", "init_mode error %d", tQSL_Error);
		return 1;
	}
	if (index < 0 || index > static_cast<int> (tqsl_adif_mode_map.size())) {
		tqslTrace("tqsl_getADIFMode", "Argument error, index = %d", index);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*mode = tqsl_adif_mode_map[index].c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumDXCCEntity(int *number) {
	if (number == NULL) {
		tqslTrace("tqsl_getNumDXCCEntity", "Arg error - number=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getNumDXCCEntity", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	*number = DXCCList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCEntity(int index, int *number, const char **name) {
	if (index < 0 || name == NULL || number == NULL) {
		tqslTrace("tqsl_getDXCCEntity", "arg error index=%d, number = 0x%lx, name=0x%lx", index, number, name);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCEntity", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	if (index >= static_cast<int>(DXCCList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_getDXCCEntity", "index range %d", index);
		return 1;
	}
	*number = DXCCList[index].first;
	*name = DXCCList[index].second.c_str();
	return 0;
}


DLLEXPORT int CALLCONVENTION
tqsl_getDXCCEntityName(int number, const char **name) {
	if (name == NULL) {
		tqslTrace("tqsl_getDXCCEntityName", "Name=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCEntityName", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	IntMap::const_iterator it;
	it = DXCCMap.find(number);
	if (it == DXCCMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	*name = it->second.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCZoneMap(int number, const char **zonemap) {
	if (zonemap == NULL) {
		tqslTrace("tqsl_getDXCCZoneMap", "zonemap ptr null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCZoneMap", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	IntMap::const_iterator it;
	it = DXCCZoneMap.find(number);
	if (it == DXCCZoneMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	const char *map = it->second.c_str();
	if (!map || map[0] == '\0') {
		*zonemap = NULL;
	} else {
		*zonemap = map;
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCStartDate(int number, tQSL_Date *d) {
	if (d == NULL) {
		tqslTrace("tqsl_getDXCCStartDate", "date ptr null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCStartDate", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	DateMap::const_iterator it;
	it = DXCCStartMap.find(number);
	if (it == DXCCStartMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	tQSL_Date newdate = it->second;
	*d = newdate;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCEndDate(int number, tQSL_Date *d) {
	if (d == NULL) {
		tqslTrace("tqsl_getDXCCEndDate", "date ptr null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCEndDate", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	DateMap::const_iterator it;
	it = DXCCEndMap.find(number);
	if (it == DXCCEndMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	tQSL_Date newdate = it->second;
	*d = newdate;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCDeleted(int number, int *deleted) {
	if (deleted == NULL) {
		tqslTrace("tqsl_getDXCCDeleted", "Name=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc()) {
		tqslTrace("tqsl_getDXCCDeleted", "init_dxcc error %d", tQSL_Error);
		return 1;
	}
	*deleted = 0;
	BoolMap::const_iterator it;
	it = DeletedMap.find(number);
	if (it == DeletedMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	*deleted = it->second;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumPropagationMode(int *number) {
	if (tqsl_init())
		return 1;
	if (number == NULL) {
		tqslTrace("tqsl_getNumPropagationMode", "number=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_propmode()) {
		tqslTrace("tqsl_getNumPropagationMode", "init_propmode error %d", tQSL_Error);
		return 1;
	}
	*number = PropModeList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getPropagationMode(int index, const char **name, const char **descrip) {
	if (index < 0 || name == NULL) {
		tqslTrace("tqsl_getPropagationMode", "arg error index=%d name=0x%lx descrip=0x%lx", index, name, descrip);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_propmode()) {
		tqslTrace("tqsl_getPropagationMode", "init_propmode error %d",  tQSL_Error);
		return 1;
	}
	if (index >= static_cast<int>(PropModeList.size())) {
		tqslTrace("tqsl_getPropagationMode", "index out of range: %d", index);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = PropModeList[index].name.c_str();
	if (descrip)
		*descrip = PropModeList[index].descrip.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumSatellite(int *number) {
	if (tqsl_init())
		return 1;
	if (number == NULL) {
		tqslTrace("tqsl_getNumSatellite", "arg error number = null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_satellite()) {
		tqslTrace("tqsl_getNumSatellite", "init_satellite error %d", tQSL_Error);
		return 1;
	}
	*number = SatelliteList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getSatellite(int index, const char **name, const char **descrip,
	tQSL_Date *start, tQSL_Date *end) {
	if (index < 0 || name == NULL) {
		tqslTrace("tqsl_getSatellite", "arg error index=%d name=0x%lx", index, name);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_satellite()) {
		tqslTrace("tqsl_getSatellite", "init_satellite error %d", tQSL_Error);
		return 1;
	}
	if (index >= static_cast<int>(SatelliteList.size())) {
		tqslTrace("tqsl_getSatellite", "index error %d", index);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = SatelliteList[index].name.c_str();
	if (descrip)
		*descrip = SatelliteList[index].descrip.c_str();
	if (start)
		*start = SatelliteList[index].start;
	if (end)
		*end = SatelliteList[index].end;
	return 0;
}

static int
init_cabrillo_map() {
	if (tqsl_cabrillo_map.size() > 0)
		return 0;
	XMLElement cabrillo_map;
	if (tqsl_get_xml_config_section("cabrillomap", cabrillo_map)) {
		tqslTrace("init_cabrillo_map", "get_xml_config_section error %d", tQSL_Error);
		return 1;
	}
	XMLElement cabrillo_item;
	bool ok = cabrillo_map.getFirstElement("cabrillocontest", cabrillo_item);
	int call_field = 0;
	int grid_field = 0;
	while (ok) {
		if (cabrillo_item.getText() != "") {
			call_field = strtol(cabrillo_item.getAttribute("field").first.c_str(), NULL, 10);
			grid_field = strtol(cabrillo_item.getAttribute("gridsquare").first.c_str(), NULL, 10);
			if (call_field > TQSL_MIN_CABRILLO_MAP_FIELD) {
				tqsl_cabrillo_map[cabrillo_item.getText()] =
					make_triplet(call_field - 1, grid_field - 1,
						(cabrillo_item.getAttribute("type").first == "VHF") ? TQSL_CABRILLO_VHF : TQSL_CABRILLO_HF);
			}
		}
		ok = cabrillo_map.getNextElement(cabrillo_item);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_clearCabrilloMap() {
	tqslTrace("tqsl_clearCabrilloMap", NULL);
	tqsl_cabrillo_user_map.clear();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setCabrilloMapEntry(const char *contest, int field, int contest_type) {
	if (contest == NULL || field <= TQSL_MIN_CABRILLO_MAP_FIELD ||
		(contest_type != TQSL_CABRILLO_HF && contest_type != TQSL_CABRILLO_VHF)) {
		tqslTrace("tqsl_setCabrilloMapEntry", "arg error contest=0x%lx field = %d", contest, field);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tqsl_cabrillo_user_map[string_toupper(contest)] = make_pair(field-1, contest_type);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloMapEntry(const char *contest, int *fieldnum, int *contest_type) {
	if (contest == NULL || fieldnum == NULL) {
		tqslTrace("tqsl_getCabrilloMapEntry", "arg error contest=0x%lx fieldnum = 0x%lx", contest, fieldnum);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_cabrillo_map()) {
		tqslTrace("tqsl_getCabrilloMapEntry", "init_cabrillo_map error %d", tQSL_Error);
		return 1;
	}
	map<string, triplet<int, int, TQSL_CABRILLO_FREQ_TYPE> >::iterator it;
	map<string, pair<int, int> >::iterator uit;
	if ((uit = tqsl_cabrillo_user_map.find(string_toupper(contest))) == tqsl_cabrillo_user_map.end()) {
		if ((it = tqsl_cabrillo_map.find(string_toupper(contest))) == tqsl_cabrillo_map.end()) {
			*fieldnum = 0;
			return 0;
		} else {
			*fieldnum = it->second.first + 1 + ((it->second.middle + 1) * 1000);
		}
		if (contest_type)
			*contest_type = it->second.last;
	} else {
		*fieldnum = uit->second.first + 1;
		if (contest_type)
			*contest_type = uit->second.second;
	}
	return 0;
}

static int
init_adif_map() {
	if (tqsl_adif_map.size() > 0)
		return 0;
	XMLElement adif_map;
	if (tqsl_get_xml_config_section("adifmap", adif_map)) {
		tqslTrace("init_adif_map", "tqsl_get_xml_config_section error %d", tQSL_Error);
		return 1;
	}
	XMLElement adif_item;
	bool ok = adif_map.getFirstElement("adifmode", adif_item);
	while (ok) {
		string adifmode = adif_item.getAttribute("adif-mode").first;
		string submode = adif_item.getAttribute("adif-submode").first;
		// Prefer the "mode=" attribute of the mode definition, else get the item value.
		string gabbi = adif_item.getAttribute("mode").first;
		string melem = adif_item.getText();

		if (adifmode != "" && submode != "") {
			tqsl_adif_submode_map[melem] = adifmode + "%" + submode;
		}
		if (adifmode == "") { 		// Handle entries with just a mode element
			adifmode = melem;
		}
		bool found = false;
		for (unsigned int i = 0; i < tqsl_adif_mode_map.size(); i++) {
			if (tqsl_adif_mode_map[i] == melem) {
				found = true;
			}
		}
		if (!found) {
			tqsl_adif_mode_map.push_back(melem);
		}

		if (gabbi != "") {		// There should always be one
			if (adifmode != "") {
				tqsl_adif_map[adifmode] = gabbi;
			}
			// Map this gabbi mode from submode
			if (submode != "" && submode != adifmode) {
				tqsl_adif_map[submode] = gabbi;
			}
			if (melem != "" && melem != adifmode) {
				tqsl_adif_map[melem] = gabbi;
			}
			// Add a mode%submode lookup too
			if (adifmode != "" && submode != "") {
				tqsl_adif_map[adifmode + "%" + submode] = gabbi;
			}
		}
		ok = adif_map.getNextElement(adif_item);
	}
	sort(tqsl_adif_mode_map.begin(), tqsl_adif_mode_map.end());
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_clearADIFModes() {
	tqsl_adif_map.clear();
	tqsl_adif_mode_map.clear();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setADIFMode(const char *adif_item, const char *mode) {
	if (adif_item == NULL || mode == NULL) {
		tqslTrace("tqsl_setADIFMode", "arg error adif_item=0x%lx mode=0x%lx", adif_item, mode);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - ADIF map invalid",
			sizeof tQSL_CustomError);
		tqslTrace("tqslSetADIFMode", "Error %s", tQSL_CustomError);
		return 1;
	}
	string umode = string_toupper(mode);
	tqsl_adif_map[string_toupper(adif_item)] = umode;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFMode(const char *adif_item, char *mode, int nmode) {
	if (adif_item == NULL || mode == NULL) {
		tqslTrace("tqsl_getADIFMode", "arg error adif_item=0x%lx, mode=0x%lx", adif_item, mode);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - ADIF map invalid",
			sizeof tQSL_CustomError);
		tqslTrace("tqsl_getADIFMode", "init_adif error %s", tQSL_CustomError);
		return 1;
	}
	string orig = adif_item;
	orig = string_toupper(orig);
	string amode;
	if (tqsl_adif_map.find(orig) != tqsl_adif_map.end()) {
		amode = tqsl_adif_map[orig];
	} else {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}

	if (nmode < static_cast<int>(amode.length())+1) {
		tqslTrace("tqsl_getAdifMode", "buffer error %s %s", nmode, amode.length());
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strncpy(mode, amode.c_str(), nmode);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFSubMode(const char *adif_item, char *mode, char *submode, int nmode) {
	if (adif_item == NULL || mode == NULL) {
		tqslTrace("tqsl_getADIFSubMode", "arg error adif_item=0x%lx, mode=0x%lx", adif_item, mode);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - ADIF map invalid",
			sizeof tQSL_CustomError);
		tqslTrace("tqsl_getADIFSubMode", "init_adif error %s", tQSL_CustomError);
		return 1;
	}
	string orig = adif_item;
	orig = string_toupper(orig);
	string amode;
	if (tqsl_adif_submode_map.find(orig) != tqsl_adif_submode_map.end()) {
		amode = tqsl_adif_submode_map[orig];
	} else {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}

	string adifmode = amode.substr(0, amode.find("%"));
	string adifsubmode = amode.substr(amode.find("%")+1);
	if (nmode < static_cast<int>(amode.length())+1) {
		tqslTrace("tqsl_getAdifSubMode", "buffer error %s %s", nmode, amode.length());
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strncpy(mode, adifmode.c_str(), nmode);
	strncpy(submode, adifsubmode.c_str(), nmode);
	return 0;
}

static int
init_loc_maps() {
	if (tqsl_field_map.size() > 0)
		return 0;
	XMLElement config_pages;
	if (tqsl_get_xml_config_section("locpages", config_pages)) {
		tqslTrace("init_loc_maps", "get_xml_config_section error %d", tQSL_Error);
		return 1;
	}
	XMLElement config_page;
	tqsl_page_map.clear();
	bool ok;
	for (ok = config_pages.getFirstElement("page", config_page); ok; ok = config_pages.getNextElement(config_page)) {
		pair <string, bool> Id = config_page.getAttribute("Id");
		int page_num = strtol(Id.first.c_str(), NULL, 10);
		if (!Id.second || page_num < 1) {	// Must have the Id!
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page missing ID",
				sizeof tQSL_CustomError);
			tqslTrace("init_loc_maps", "error %s", tQSL_CustomError);
			return 1;
		}
		tqsl_page_map[page_num] = config_page;
	}

	XMLElement config_fields;
	if (tqsl_get_xml_config_section("locfields", config_fields)) {
		tqslTrace("init_loc_maps", "get_xml_config_section locfields error %d", tQSL_Error);
		return 1;
	}
	XMLElement config_field;
	for (ok = config_fields.getFirstElement("field", config_field); ok; ok = config_fields.getNextElement(config_field)) {
		pair <string, bool> Id = config_field.getAttribute("Id");
		if (!Id.second) {	// Must have the Id!
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field missing ID",
				sizeof tQSL_CustomError);
			tqslTrace("init_loc_maps", "config field error %s", tQSL_CustomError);
			return 1;
		}
		tqsl_field_map[Id.first] = config_field;
	}

	return 0;
}

static bool inMap(int cqvalue, int ituvalue, bool cqz, bool ituz, const char *map) {
/*
 * Parse the zone map and return true if the value is a valid zone number
 * The maps are colon-separated number pairs, with a list of pairs comma separated.
 */
	int cq, itu;
	bool result = false;

	// No map or empty string -> all match
	if (!map || map[0] == '\0') {
		return true;
	}

	char *mapcopy = strdup(map);
	char *mapPart = strtok(mapcopy, ",");
	while (mapPart) {
		sscanf(mapPart, "%d:%d", &itu, &cq);
		if (cqz && ituz) {
			if ((cq == cqvalue || cqvalue == 0) && (itu == ituvalue || ituvalue == 0)) {
				result = true;
				break;
			}
		} else if (cqz && (cq == cqvalue || cqvalue == 0)) {
			result = true;
			break;
		} else if (ituz && (itu == ituvalue || ituvalue == 0)) {
			result = true;
			break;
		}
		mapPart = strtok(NULL, ",");
	}
	free(mapcopy);
	return result;
}

static int
_ent_cmp(const void *a, const void *b) {
	return strcasecmp(((struct _dxcc_entity *)a)->name, ((struct _dxcc_entity *)b)->name);
}

static TQSL_LOCATION_FIELD *
get_location_field_page(const string& gabbi, TQSL_LOCATION *loc, int* page = NULL) {
	for (int mypage = 1; mypage > 0; mypage = loc->pagelist[mypage-1].next) {
		TQSL_LOCATION_FIELDLIST& fl = loc->pagelist[mypage-1].fieldlist;
		for (int j = 0; j < static_cast<int>(fl.size()); j++) {
			if (fl[j].gabbi_name == gabbi) {
				if (page) {
					*page = mypage;
				}
				return &(fl[j]);
			}
		}
	}
	return 0;
}

struct sasMap {
	const char *gabbi;
	const char *errstr;
};

static struct sasMap sasMapping[] = {
	{ "US_STATE", "Invalid zone selections for state" },
	{ "CA_PROVINCE", "Invalid zone selections for province" },
	{ "RU_OBLAST", "Invalid zone selections for oblast" },
	{ "CN_PROVINCE", "Invalid zone selections for province" },
	{ "AU_STATE", "Invalid zone selections for state" },
	{ "JA_PREFECTURE", "Invalid zone selections for prefecture" },
	{ "FI_KUNTA", "Invalid zone selections for kunta" },
	{ NULL, NULL }
};

static TQSL_LOCATION_FIELD*
get_primary_sub(TQSL_LOCATION* loc, string* errstr) {
	for (int i = 0; sasMapping[i].gabbi; i++) {
		TQSL_LOCATION_FIELD* temp = get_location_field_page(sasMapping[i].gabbi, loc);
		if (temp) {
			if (errstr)
				*errstr = sasMapping[i].errstr;
			return temp;
		}
	}
	return NULL;
}

static int find_next_page(TQSL_LOCATION *loc);

static int
update_page(int page, TQSL_LOCATION *loc) {
	TQSL_LOCATION_PAGE& p = loc->pagelist[page-1];
	int dxcc;
	int current_entity = -1;
	int loaded_cqz = -1;
	int loaded_ituz = -1;
	TQSL_LOCATION_FIELD *cqz = get_location_field_page("CQZ", loc);
	TQSL_LOCATION_FIELD *ituz = get_location_field_page("ITUZ", loc);
	tqslTrace("update_page", "page=%d, loc=0x%lx", page, loc);
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD& field = p.fieldlist[i];
		field.changed = false;
		if (field.gabbi_name == "CALL") {
			if (field.items.size() == 0 || loc->newflags) {
				// Build list of call signs from available certs
				field.changed = true;
				field.items.clear();
				field.idx = 0;
				loc->newflags = false;
				field.flags = TQSL_LOCATION_FIELD_SELNXT;	// Must be selected
				p.hash.clear();
				tQSL_Cert *certlist;
				int ncerts;
				tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, loc->cert_flags);
				for (int i = 0; i < ncerts; i++) {
					char callsign[40];
					tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign);
					tqsl_getCertificateDXCCEntity(certlist[i], &dxcc);

					char ibuf[10];
					snprintf(ibuf, sizeof ibuf, "%d", dxcc);
					bool found = false;
					// Only add a given DXCC entity to a call once.
					map<string, vector<string> >::iterator call_p;
					for (call_p = p.hash.begin(); call_p != p.hash.end(); call_p++) {
						if (call_p->first == callsign && call_p->second[0] == ibuf) {
							found = true;
							break;
						}
					}
					if (!found)
						p.hash[callsign].push_back(ibuf);
					tqsl_freeCertificate(certlist[i]);
				}
				free(certlist);
				// Fill the call sign list
				map<string, vector<string> >::iterator call_p;
				field.idx = 0;
				TQSL_LOCATION_ITEM none;
				none.text = "[None]";
				field.items.push_back(none);
				for (call_p = p.hash.begin(); call_p != p.hash.end(); call_p++) {
					TQSL_LOCATION_ITEM item;
					item.text = call_p->first;
					if (item.text == field.cdata)
						field.idx = static_cast<int>(field.items.size());
					field.items.push_back(item);
				}
				if (field.idx == 0 && field.items.size() == 2) {
					field.idx = 1;
				}
				if (field.idx >= 0) {
					field.cdata = field.items[field.idx].text;
				}
			}
		} else if (field.gabbi_name == "DXCC") {
			// Note: Expects CALL to be field 0 of this page.
			string call = p.fieldlist[0].cdata;
			if (field.items.size() == 0 || call != field.dependency) {
				// rebuild list
				field.changed = true;
				init_dxcc();
				int olddxcc = strtol(field.cdata.c_str(), NULL, 10);
				if (loc->newDXCC != -1) {
					olddxcc = loc->newDXCC;
					loc->newDXCC = -1;
				}

				field.items.clear();
				field.idx = 0;
#ifdef DXCC_TEST
				const char *dxcc_test = getenv("TQSL_DXCC");
				if (dxcc_test) {
					vector<string> &entlist = p.hash[call];
					char *parse_dxcc = strdup(dxcc_test);
					char *cp = strtok(parse_dxcc, ",");
					while (cp) {
						if (find(entlist.begin(), entlist.end(), string(cp)) == entlist.end())
							entlist.push_back(cp);
						cp = strtok(0, ",");
					}
					free(parse_dxcc);
				}
#endif
				if (call == "[None]") {
					int i;
					if (!_ent_init) {
						num_entities = DXCCMap.size();
						entity_list = new struct _dxcc_entity[num_entities];
						IntMap::const_iterator it;
						for (it = DXCCMap.begin(), i = 0; it != DXCCMap.end(); it++, i++) {
							entity_list[i].number = it->first;
							entity_list[i].name = it->second.c_str();
							entity_list[i].zonemap = DXCCZoneMap[it->first].c_str();
							entity_list[i].start = DXCCStartMap[it->first];
							entity_list[i].end = DXCCEndMap[it->first];
						}
						qsort(entity_list, num_entities, sizeof(struct _dxcc_entity), &_ent_cmp);
						_ent_init = true;
					}
					for (i = 0; i < num_entities; i++) {
						TQSL_LOCATION_ITEM item;
						item.ivalue = entity_list[i].number;
						char buf[10];
						snprintf(buf, sizeof buf, "%d", item.ivalue);
						item.text = buf;
						item.label = entity_list[i].name;
						item.zonemap = entity_list[i].zonemap;
						if (item.ivalue == olddxcc) {
							field.idx = field.items.size();
						}
						field.items.push_back(item);
					}
					field.idx = 0;
				} else {
					vector<string>::iterator ip;
					// Always have the "-NONE-" entity.
					TQSL_LOCATION_ITEM item;
					item.label = "-NONE-";
					item.zonemap = "";
					// This iterator walks the list of DXCC entities associated
					// with this callsign
					field.items.push_back(item);
					bool setIndex = false;

					for (ip = p.hash[call].begin(); ip != p.hash[call].end(); ip++) {
						item.text = *ip;
						item.ivalue = strtol(ip->c_str(), NULL, 10);
						IntMap::iterator dxcc_it = DXCCMap.find(item.ivalue);
						if (dxcc_it != DXCCMap.end()) {
							item.label = dxcc_it->second;
							item.zonemap = DXCCZoneMap[item.ivalue];
						}
						if (item.ivalue == olddxcc) {
							field.idx = field.items.size();
							setIndex = true;
						}
						field.items.push_back(item);
					}
					if (!setIndex) {
						field.idx = field.items.size()-1;
					}
				}
				if (field.items.size() > 0) {
					field.cdata = field.items[field.idx].text;
				}
				field.dependency = call;
			} // rebuild list
		} else {
			if (tqsl_field_map.find(field.gabbi_name) == tqsl_field_map.end()) {
				// Shouldn't happen!
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field map mismatch.",
					sizeof tQSL_CustomError);
				tqslTrace("update_page", "field map error %s", field.gabbi_name.c_str());
				return 1;
			}
			XMLElement config_field = tqsl_field_map.find(field.gabbi_name)->second;
			pair<string, bool> attr = config_field.getAttribute("dependsOn");
			if (attr.first != "") {
				// Items list depends on other field
				TQSL_LOCATION_FIELD *fp = get_location_field_page(attr.first, loc);
				if (fp) {
					// Found the dependency field. Now find the enums to use
					string val = fp->cdata;
					if (fp->items.size() > 0)
						val = fp->items[fp->idx].text;
					if (val == field.dependency)
						continue;
					field.dependency = val;
					field.changed = true;
					field.items.clear();
					string oldcdata = field.cdata;
					field.idx = 0;
					XMLElement enumlist;
					bool ok = config_field.getFirstElement("enums", enumlist);
					while (ok) {
						pair<string, bool> dependency = enumlist.getAttribute("dependency");
						if (dependency.second && dependency.first == val) {
							if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
								TQSL_LOCATION_ITEM item;
								item.label = "[None]";
								field.items.push_back(item);
							}
							XMLElement enumitem;
							bool iok = enumlist.getFirstElement("enum", enumitem);
							while (iok) {
								TQSL_LOCATION_ITEM item;
								item.text = enumitem.getAttribute("value").first;
								item.label = enumitem.getText();
								item.zonemap = enumitem.getAttribute("zonemap").first;
								field.items.push_back(item);
								if (item.text == oldcdata) {
									field.idx = field.items.size() - 1;
								}
								iok = enumlist.getNextElement(enumitem);
							}
						}
						ok = config_field.getNextElement(enumlist);
					} // enum loop
				} else {
					tQSL_Error = TQSL_CUSTOM_ERROR;
					strncpy(tQSL_CustomError, "TQSL Configuration file invalid - dependent field not found.",
						sizeof tQSL_CustomError);
					tqslTrace("update_page", "error %s", tQSL_CustomError);
					return 1;
				}
			} else {
				// No dependencies
				TQSL_LOCATION_FIELD *ent = get_location_field_page("DXCC", loc);
				current_entity = strtol(ent->cdata.c_str(), NULL, 10);
				bool isCQZ = field.gabbi_name == "CQZ";
				bool isITUZ = field.gabbi_name == "ITUZ";
				if (field.items.size() == 0 || (isCQZ && current_entity != loaded_cqz) || (isITUZ && current_entity != loaded_ituz)) {
					XMLElement enumlist;
					if (config_field.getFirstElement("enums", enumlist)) {
						field.items.clear();
						field.idx = 0;
						string oldcdata = field.cdata;
						field.changed = true;
						if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
							TQSL_LOCATION_ITEM item;
							item.label = "[None]";
							field.items.push_back(item);
						}
						XMLElement enumitem;
						bool iok = enumlist.getFirstElement("enum", enumitem);
						while (iok) {
							TQSL_LOCATION_ITEM item;
							item.text = enumitem.getAttribute("value").first;
							item.label = enumitem.getText();
							item.zonemap = enumitem.getAttribute("zonemap").first;
							field.items.push_back(item);
							if (item.text == oldcdata) {
								field.idx = field.items.size() - 1;
							}
							iok = enumlist.getNextElement(enumitem);
						}
					} else {
						// No enums supplied
						int ftype = strtol(config_field.getAttribute("intype").first.c_str(), NULL, 10);
						if (ftype == TQSL_LOCATION_FIELD_LIST || ftype == TQSL_LOCATION_FIELD_DDLIST) {
							// This a list field
							int lower = strtol(config_field.getAttribute("lower").first.c_str(), NULL, 10);
							int upper = strtol(config_field.getAttribute("upper").first.c_str(), NULL, 10);
							const char *zoneMap;
							/* Get the map */
							if (tqsl_getDXCCZoneMap(current_entity, &zoneMap)) {
								zoneMap = NULL;
							}
							// Try for a zonemap from the primary subdivision
							TQSL_LOCATION_FIELD* pas = NULL;

							if (find_next_page(loc)) {
								pas = get_primary_sub(loc, NULL);
							}
							if (pas != NULL && pas->items.size() > 0 && (unsigned int) pas->idx < pas->items.size() && pas->items[pas->idx].zonemap != "")
								zoneMap = pas->items[pas->idx].zonemap.c_str();
							if (upper < lower) {
								tQSL_Error = TQSL_CUSTOM_ERROR;
								strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field range order incorrect.",
									sizeof tQSL_CustomError);
								tqslTrace("update_page", "error %s", tQSL_CustomError);
								return 1;
							}
							field.items.clear();
							field.idx = 0;
							string oldcdata = field.cdata;
							field.changed = true;
							int currentCQ = cqz->idata;
							int currentITU = ituz->idata;
							if (isCQZ) {
								loaded_cqz = current_entity;
								if (!inMap(0, currentITU, false, true, zoneMap)) {
									currentITU = 0;		// Not valid here, ignore
								}
							}
							if (isITUZ) {
								loaded_ituz = current_entity;
								if (!inMap(currentCQ, 0, true, false, zoneMap)) {
									currentCQ = 0;		// Not valid here, ignore
								}
							}
							if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
								TQSL_LOCATION_ITEM item;
								item.label = "[None]";
								field.items.push_back(item);
							}
							char buf[40];
							for (int j = lower; j <= upper; j++) {
								bool zoneOK = true;
								if (zoneMap) {
									if (isCQZ) {
										zoneOK = inMap(j, currentITU, true, true, zoneMap);
									}
									if (isITUZ) {
										zoneOK = inMap(currentCQ, j, true, true, zoneMap);
									}
								}
								if (zoneOK) {
									snprintf(buf, sizeof buf, "%d", j);
									TQSL_LOCATION_ITEM item;
									item.text = buf;
									item.ivalue = j;
									field.items.push_back(item);
									if (item.text == oldcdata) {
										field.idx = field.items.size() - 1;
									}
								}
							}
						} // intype != TEXT
					} // enums supplied
				} // itemlist not empty and current entity
			} // no dependencies
		} // field name not CALL|DXCC
	} // field loop

	/* Sanity check zones */
	bool zonesok = true;
	string zone_error = "";

	int currentCQ = cqz->idata;
	int currentITU = ituz->idata;
	// Check each division, start from entity, then division
	TQSL_LOCATION_FIELD *entity = get_location_field_page("DXCC", loc);
	if (entity) {
		zone_error = "Invalid zone selections for DXCC entity";
		if (entity && entity->idx >=0 && entity->items.size() > 0) {
			string dxzm = entity->items[entity->idx].zonemap;
			const char* dxccZoneMap = dxzm.c_str();
			if (!inMap(currentCQ, currentITU, true, true, dxccZoneMap)) {
				zonesok = false;
			}
		}
	}

	// Entity is OK, try for the state/province/oblast
	TQSL_LOCATION_FIELD *state = get_primary_sub(loc, &zone_error);

	if (state && state->idx >=0 && state->items.size() > 0) {
		string szm = state->items[state->idx].zonemap;
		const char* stateZoneMap = szm.c_str();

		if (!inMap(currentCQ, currentITU, true, true, stateZoneMap)) {
			zonesok = false;
		}
	}
	if (zonesok) {
		tQSL_CustomError[0] = '\0';
	} else {
		strncpy(tQSL_CustomError, zone_error.c_str(), sizeof tQSL_CustomError);
	}
	p.complete = true;
	return 0;
}

static int
make_page(TQSL_LOCATION_PAGELIST& pagelist, int page_num) {
	if (init_loc_maps()) {
		tqslTrace("make_page", "init_loc_maps error %d", tQSL_Error);
		return 1;
	}
	if (tqsl_page_map.find(page_num) == tqsl_page_map.end()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page reference could not be found.",
			sizeof tQSL_CustomError);
		tqslTrace("make_page", "Error %d %s", page_num, tQSL_CustomError);
		return 1;
	}

	TQSL_LOCATION_PAGE p;
	pagelist.push_back(p);

	XMLElement& config_page = tqsl_page_map[page_num];

	pagelist.back().prev = strtol(config_page.getAttribute("follows").first.c_str(), NULL, 10);
	XMLElement config_pageField;
	bool field_ok = config_page.getFirstElement("pageField", config_pageField);
	while (field_ok) {
		string field_name = config_pageField.getText();
		if (field_name == "" || tqsl_field_map.find(field_name) == tqsl_field_map.end()) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page references undefined field.",
				sizeof tQSL_CustomError);
			tqslTrace("make_page", "Error %s", tQSL_CustomError);
			return 1;
		}
		XMLElement& config_field = tqsl_field_map[field_name];
		TQSL_LOCATION_FIELD loc_field(
			field_name,
			config_field.getAttribute("label").first.c_str(),
			(config_field.getAttribute("type").first == "C") ? TQSL_LOCATION_FIELD_CHAR : TQSL_LOCATION_FIELD_INT,
			strtol(config_field.getAttribute("len").first.c_str(), NULL, 10),
			strtol(config_field.getAttribute("intype").first.c_str(), NULL, 10),
			strtol(config_field.getAttribute("flags").first.c_str(), NULL, 10)
		);	// NOLINT(whitespace/parens)
		pagelist.back().fieldlist.push_back(loc_field);
		field_ok = config_page.getNextElement(config_pageField);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_initStationLocationCapture(tQSL_Location *locp) {
	if (tqsl_init())
		return 1;
	if (locp == NULL) {
		tqslTrace("tqsl_initStationLocationCapture", "Arg error locp=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION *loc = new TQSL_LOCATION;
	*locp = loc;
	if (init_loc_maps()) {
		tqslTrace("tqsl_initStationLocationCapture", "init_loc_maps error %d", tQSL_Error);
		return 1;
	}
	map<int, XMLElement>::iterator pit;
	for (pit = tqsl_page_map.begin(); pit != tqsl_page_map.end(); pit++) {
		if (make_page(loc->pagelist, pit->first)) {
			tqslTrace("tqsl_initStationLocationCapture", "make_page error %d", tQSL_Error);
			return 1;
		}
	}

	loc->page = 1;
	if (update_page(1, loc)) {
		tqslTrace("tqsl_initStationLocationCapture", "updatePage error %d", tQSL_Error);
		return 1;
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_endStationLocationCapture(tQSL_Location *locp) {
	if (tqsl_init())
		return 1;
	if (locp == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_endStationLocationCapture", "arg error locp=NULL");
		return 1;
	}
	if (*locp == 0)
		return 0;
	if (CAST_TQSL_LOCATION(*locp)->sentinel == 0x5445)
		delete CAST_TQSL_LOCATION(*locp);
	*locp = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_updateStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_updateStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
//	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	return update_page(loc->page, loc);
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumStationLocationCapturePages(tQSL_Location locp, int *npages) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getNumStationLocationCapturePages", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (npages == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_getNumStationLocationCapturePages", "arg error npages=NULL");
		return 1;
	}
	*npages = loc->pagelist.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCapturePage(tQSL_Location locp, int *page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocationCapturePage", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (page == NULL) {
		tqslTrace("tqsl_getStationLocationCapturePage", "arg error page=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*page = loc->page;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCapturePage(tQSL_Location locp, int page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setStationLocationCapturePage", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (page < 1 || page > static_cast<int>(loc->pagelist.size())) {
		tqslTrace("tqsl_setStationLocationCapturePage", "Page %d out of range", page);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->page = page;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCertFlags(tQSL_Location locp, int flags) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setStationLocationCertFlags", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (loc->cert_flags != flags) {
		loc->cert_flags = flags;
		loc->newflags = true;
		loc->page = 1;
		if (update_page(1, loc)) {
			tqslTrace("tqsl_setStationLocationCertFlags", "update_page error %d", tQSL_Error);
			return 1;
		}
	}
	return 0;
}


static int
find_next_page(TQSL_LOCATION *loc) {
	// Set next page based on page dependencies
	TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
	map<int, XMLElement>::iterator pit;
	p.next = 0;
	for (pit = tqsl_page_map.begin(); pit != tqsl_page_map.end(); pit++) {
		if (strtol(pit->second.getAttribute("follows").first.c_str(), NULL, 10) == loc->page) {
			string dependsOn = pit->second.getAttribute("dependsOn").first;
			string dependency = pit->second.getAttribute("dependency").first;
			if (dependsOn == "") {
				p.next = pit->first;
				return 1;	// Found next page
			}
			TQSL_LOCATION_FIELD *fp = get_location_field_page(dependsOn, loc);
			if (static_cast<int>(fp->items.size()) > fp->idx && fp->idx >= 0 && fp->items[fp->idx].text == dependency) {
				p.next = pit->first;
				return 1;	// Found next page
			}
		}
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_nextStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_nextStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (!find_next_page(loc))
		return 0;
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	if (p.next > 0)
		loc->page = p.next;
	update_page(loc->page, loc);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNextStationLocationCapturePage(tQSL_Location locp, int *page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)) || page == NULL) {
		tqslTrace("tqsl_nextStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (!find_next_page(loc))
		return 1;
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	if (p.next > 0) {
		*page = p.next;
		return 0;
	}
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_prevStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_prevStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	if (p.prev > 0)
		loc->page = p.prev;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getPrevStationLocationCapturePage(tQSL_Location locp, int *page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)) || page == NULL) {
		tqslTrace("tqsl_getPrevStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	if (p.prev > 0) {
		*page = p.prev;
		return 0;
	}
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCurrentStationLocationCapturePage(tQSL_Location locp, int *page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)) || page == NULL) {
		tqslTrace("tqsl_getPrevStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	*page = loc->page;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_hasNextStationLocationCapture(tQSL_Location locp, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_hasNextStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (rval == NULL) {
		tqslTrace("tqsl_hasNextStationLocationCapture", "Arg error rval=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (!find_next_page(loc)) {
		tqslTrace("tqsl_hasNextStationLocationCapture", "find_next_page error %d", tQSL_Error);
		return 1;
	}
	*rval = (loc->pagelist[loc->page-1].next > 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_hasPrevStationLocationCapture(tQSL_Location locp, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_hasPrevStationLocationCapture", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (rval == NULL) {
		tqslTrace("tqsl_hasPrevStationLocationCapture", "arg error rval=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = (loc->pagelist[loc->page-1].prev > 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumLocationField(tQSL_Location locp, int *numf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getNumLocationField", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (numf == NULL) {
		tqslTrace("tqsl_getNumLocationField", "arg error numf=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	*numf = fl.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLabelSize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataLabelSize", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataLabelSize", "arg error rval=0x%lx, field_num=%d", rval, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].label.size()+1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLabel(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataLabel", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataLabel", "arg error buf=0x%lx, field_num=%d", buf, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].label.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataGABBISize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataGABBISize", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataGABBISize", "arg error rval=0x%lx, field_num=%d", rval, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].gabbi_name.size()+1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataGABBI(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataGABBI", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataGABBI", "arg error buf=0x%lx, field_num=%d", buf, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].gabbi_name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldInputType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldInputType", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldInputType", "arg error type=0x%lx, field_num=%d", type, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].input_type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldChanged(tQSL_Location locp, int field_num, int *changed) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldChanged", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (changed == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldChanged", "arg error changed=0x%lx, field_num=%d", changed, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*changed = fl[field_num].changed;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataType", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataType", "arg error type=0x%lx, field_num=%d", type, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].data_type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldFlags(tQSL_Location locp, int field_num, int *flags) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldFlags", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (flags == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldFlags", "arg error flags=0x%lx, field_num=%d", flags, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*flags = fl[field_num].flags;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLength(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldDataLength", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldDataLength", "arg error rval=0x%lx, field_num=%d", rval, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].data_len;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldCharData(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldCharData", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldCharData", "arg error buf=0x%lx, field_num=%d", buf, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (fl[field_num].flags & TQSL_LOCATION_FIELD_UPPER)
		strncpy(buf, string_toupper(fl[field_num].cdata).c_str(), bufsiz);
	else
		strncpy(buf, fl[field_num].cdata.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldIntData(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldIntData", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldIntData", "arg error dat=0x%lx, field_num=%d", dat, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*dat = fl[field_num].idata;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldIndex(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldIndex", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_getLocationFieldIndex", "arg error dat=0x%lx, field_num=%d", dat, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (fl[field_num].input_type != TQSL_LOCATION_FIELD_DDLIST
		&& fl[field_num].input_type != TQSL_LOCATION_FIELD_LIST) {
		tqslTrace("tqsl_getLocationFieldIndex", "arg error input type mismatch");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*dat = fl[field_num].idx;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldCharData(tQSL_Location locp, int field_num, const char *buf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setLocationFieldCharData", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_setLocationFieldCharData", "arg error buf=0x%lx, field_num=%d", buf, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].cdata = string(buf).substr(0, fl[field_num].data_len);
	if (fl[field_num].flags & TQSL_LOCATION_FIELD_UPPER)
		fl[field_num].cdata = string_toupper(fl[field_num].cdata);


	if (fl[field_num].input_type == TQSL_LOCATION_FIELD_DDLIST
		|| fl[field_num].input_type == TQSL_LOCATION_FIELD_LIST) {
		if (fl[field_num].cdata == "") {
			fl[field_num].idx = 0;
			fl[field_num].idata = fl[field_num].items[0].ivalue;
		} else {
			bool found = false;
			for (int i = 0; i < static_cast<int>(fl[field_num].items.size()); i++) {
				if (fl[field_num].items[i].text == fl[field_num].cdata) {
					fl[field_num].idx = i;
					fl[field_num].idata = fl[field_num].items[i].ivalue;
					found = true;
					break;
				}
			}
			if (!found) { 	// There's no entry in the list that matches!
				fl[field_num].cdata = "";
				fl[field_num].idx = 0;
				fl[field_num].idata = 0;
			}
		}
	}
	return 0;
}

/* Set the field's index. For pick lists, this is the index into
 * 'items'. In that case, also set the field's data to the picked value.
 */
DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldIndex(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setLocationFieldIndex", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_setLocationFieldIndex", "arg error index out of range page %d size %d - field_num=%d, dat=%d", loc->page, fl.size(), field_num, dat);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idx = dat;
	if (fl[field_num].input_type == TQSL_LOCATION_FIELD_DDLIST
		|| fl[field_num].input_type == TQSL_LOCATION_FIELD_LIST) {
		if (dat >= 0 && dat < static_cast<int>(fl[field_num].items.size())) {
			fl[field_num].idx = dat;
			fl[field_num].cdata = fl[field_num].items[dat].text;
			fl[field_num].idata = fl[field_num].items[dat].ivalue;
		} else {
			tqslTrace("tqsl_setLocationFieldIndex", "arg error page %d field_num=%d dat=%d", loc->page, field_num, dat);
			tQSL_Error = TQSL_ARGUMENT_ERROR;
			return 1;
		}
	}
	return 0;
}

/* Set the field's integer data.
 */
DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldIntData(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setLocationFieldIntData", "check_loc error %d", tQSL_Error);
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tqslTrace("tqsl_setLocationFieldIntData", "arg error field_num=%d, dat=%d", field_num, dat);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idata = dat;
	return 0;
}

/* For pick lists, this is the index into
 * 'items'. In that case, also set the field's char data to the picked value.
 */
DLLEXPORT int CALLCONVENTION
tqsl_getNumLocationFieldListItems(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getNumLocationFieldListItems", "check_loc error %d", tQSL_Error);
		return 1;
	}
	if (rval == NULL) {
		tqslTrace("tqsl_getNumLocationFieldListItems", "arg error rval=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	*rval = fl[field_num].items.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldListItem(tQSL_Location locp, int field_num, int item_idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getLocationFieldListItem", "check_loc error %d", tQSL_Error);
		return 1;
	}
	bool findKey = false;
	if (item_idx & 0x10000) {
		findKey = true;
		item_idx &= 0xffff;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == NULL || field_num < 0 || field_num >= static_cast<int>(fl.size())
		|| (fl[field_num].input_type != TQSL_LOCATION_FIELD_LIST
		&& fl[field_num].input_type != TQSL_LOCATION_FIELD_DDLIST)) {
		tqslTrace("tqsl_getLocationFieldListItem", "arg error buf=0x%lx, field_num=%d", buf, field_num);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (item_idx < 0 || item_idx >= static_cast<int>(fl[field_num].items.size())) {
		tqslTrace("tqsl_getLocationFieldListItem", "arg error item_idx=%d", item_idx);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (findKey) {
		strncpy(buf, fl[field_num].items[item_idx].text.c_str(), bufsiz);
	} else {
		string& str = (fl[field_num].items[item_idx].label == "")
			? fl[field_num].items[item_idx].text
			: fl[field_num].items[item_idx].label;
		strncpy(buf, str.c_str(), bufsiz);
	}
	buf[bufsiz - 1] = '\0';
	return 0;
}

static string
tqsl_station_data_filename(bool deleted = false) {
	const char *f;
	if (deleted)
		f = "station_data_trash";
	else
		f = "station_data";

	string s = tQSL_BaseDir;
#ifdef _WIN32
	s += "\\";
#else
	s += "/";
#endif
	s += f;
	return s;
}

static int
tqsl_load_station_data(XMLElement &xel, bool deleted = false) {
	int status = xel.parseFile(tqsl_station_data_filename(deleted).c_str());
	tqslTrace("tqsl_load_station_data", "file %s parse status %d", tqsl_station_data_filename(deleted).c_str(), status);
	if (status) {
		if (errno == ENOENT) {		// If there's no file, no error.
			tqslTrace("tqsl_load_station_data", "File does not exist");
			return 0;
		}
		strncpy(tQSL_ErrorFile, tqsl_station_data_filename(deleted).c_str(), sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_load_station_data", "parse error, errno=%d", tQSL_Errno);
		} else {
			tqslTrace("tqsl_load_station_data", "syntax error");
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	return status;
}

static int
tqsl_dump_station_data(XMLElement &xel, bool deleted = false) {
	ofstream out;
	string fn = tqsl_station_data_filename(deleted);

	out.exceptions(ios::failbit | ios::eofbit | ios::badbit);
	try {
#ifdef _WIN32
		wchar_t* wfn = utf8_to_wchar(fn.c_str());
		out.open(wfn);
		free_wchar(wfn);
#else
		out.open(fn.c_str());
#endif
		out << xel << endl;
		out.close();
	}
	catch(exception& x) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unable to save new station location file (%s): %s/%s",
				fn.c_str(), x.what(), strerror(errno));
		tqslTrace("tqsl_dump_station_data", "file error %s %s", fn.c_str(), tQSL_CustomError);
		return 1;
	}
	return 0;
}

static int
tqsl_load_loc(TQSL_LOCATION *loc, XMLElementList::iterator ep, bool ignoreZones) {
	bool exists;
	loc->page = 1;
	loc->data_errors[0] = '\0';
	int bad_ituz = 0;
	int bad_cqz = 0;
	tqslTrace("tqsl_load_loc", NULL);
	while(1) {
		TQSL_LOCATION_PAGE& page = loc->pagelist[loc->page-1];
		for (int fidx = 0; fidx < static_cast<int>(page.fieldlist.size()); fidx++) {
			TQSL_LOCATION_FIELD& field = page.fieldlist[fidx];
			if (field.gabbi_name != "") {
				// A field that may exist
				XMLElement el;
				if (ep->second->getFirstElement(field.gabbi_name, el)) {
					field.cdata = el.getText();
					switch (field.input_type) {
						case TQSL_LOCATION_FIELD_DDLIST:
						case TQSL_LOCATION_FIELD_LIST:
							exists = false;
							for (int i = 0; i < static_cast<int>(field.items.size()); i++) {
								string cp = field.items[i].text;
								int q = strcasecmp(field.cdata.c_str(), cp.c_str());
								if (q == 0) {
									field.idx = i;
									field.cdata = cp;
									field.idata = field.items[i].ivalue;
									exists = true;
									break;
								}
							}
							if (!exists) {
								if (field.gabbi_name == "CQZ")
									bad_cqz = strtol(field.cdata.c_str(), NULL, 10);
								else if (field.gabbi_name == "ITUZ")
									bad_ituz = strtol(field.cdata.c_str(), NULL, 10);
								else if (field.gabbi_name == "CALL" || field.gabbi_name == "DXCC")
									field.idx = -1;
							}
							break;
						case TQSL_LOCATION_FIELD_TEXT:
							field.cdata = trim(field.cdata);
							if (field.data_type == TQSL_LOCATION_FIELD_INT)
								field.idata = strtol(field.cdata.c_str(), NULL, 10);
							break;
					}
				}
			}
			if (update_page(loc->page, loc))
				return 1;
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	}
	if (ignoreZones)
		return 0;
	if (bad_cqz && bad_ituz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors),
			"This station location is configured with invalid CQ zone %d and invalid ITU zone %d.", bad_cqz, bad_ituz);
	} else if (bad_cqz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors), "This station location is configured with invalid CQ zone %d.", bad_cqz);
	} else if (bad_ituz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors), "This station location is configured with invalid ITU zone %d.", bad_ituz);
	}
	tqslTrace("tqsl_load_loc", "data_errors=%s", loc->data_errors);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationDataEnc(tQSL_StationDataEnc *sdata) {
	char *dbuf = NULL;
	size_t dlen = 0;
	gzFile in = NULL;
#ifdef _WIN32
	wchar_t *fn = utf8_to_wchar(tqsl_station_data_filename().c_str());
	int fd = _wopen(fn, _O_RDONLY|_O_BINARY);
	free_wchar(fn);
	if (fd != -1)
		in = gzdopen(fd, "rb");
#else
	in = gzopen(tqsl_station_data_filename().c_str(), "rb");
#endif

	if (!in) {
		if (errno == ENOENT) {
			*sdata = NULL;
			tqslTrace("tqsl_getStationDataEnc", "File %s does not exist", tqsl_station_data_filename().c_str());
			return 0;
		}
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_ErrorFile, tqsl_station_data_filename().c_str(), sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_getStationDataEnc", "File %s open error %s", tqsl_station_data_filename().c_str(), strerror(tQSL_Error)
);
		return 1;
	}
	char buf[2048];
	int rcount;
	while ((rcount = gzread(in, buf, sizeof buf)) > 0) {
		dlen += rcount;
	}
	dbuf = reinterpret_cast<char *>(malloc(dlen + 2));
	if (!dbuf) {
		tqslTrace("tqsl_getStationDataEnc", "memory allocation error %d", dlen+2);
		return 1;
	}
	*sdata = dbuf;

	gzrewind(in);
	while ((rcount = gzread(in, dbuf, sizeof buf)) > 0) {
		dbuf += rcount;
	}
	*dbuf = '\0';
	gzclose(in);
	return 0;
}

DLLEXPORT int CALLCONVENTION
	tqsl_freeStationDataEnc(tQSL_StationDataEnc sdata) {
	if (sdata)
		free(sdata);
	return 0; //can never fail
}

DLLEXPORT int CALLCONVENTION
tqsl_mergeStationLocations(const char *locdata) {
	XMLElement new_data;
	XMLElement old_data;
	XMLElement new_top_el;
	XMLElement old_top_el;
	vector<string> locnames;

	tqslTrace("tqsl_mergeStationLocations", NULL);
	// Load the current station data
	if (tqsl_load_station_data(old_top_el)) {
		tqslTrace("tqsl_mergeStationLocations", "error loading station data");
		return 1;
	}
	// Parse the data to be merged
	new_top_el.parseString(locdata);

	if (!new_top_el.getFirstElement(new_data))
		new_data.setElementName("StationDataFile");

	if (!old_top_el.getFirstElement(old_data))
		old_data.setElementName("StationDataFile");

	// Build a list of existing station locations
	XMLElementList& namelist = old_data.getElementList();
	XMLElementList::iterator nameiter;
	XMLElement locname;
	for (nameiter = namelist.find("StationData"); nameiter != namelist.end(); nameiter++) {
		if (nameiter->first != "StationData")
			break;
		pair<string, bool> rval = nameiter->second->getAttribute("name");
		if (rval.second) {
			locnames.push_back(rval.first);
		}
	}

	// Iterate the new locations
	XMLElementList& ellist = new_data.getElementList();
	XMLElementList::iterator ep;
	old_data.setPretext(old_data.getPretext() + "  ");
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second->getAttribute("name");
		bool found = false;
		if (rval.second) {
			for (size_t j = 0; j < locnames.size(); j++) {
				if (locnames[j] == rval.first) {
					found = true;
					break;
				}
			}
		}
		if (!found) {
			// Add this one to the station data file
			XMLElement *newtop = new XMLElement("StationData");
			newtop->setPretext("\n  ");
			newtop->setAttribute("name", rval.first);
			newtop->setText("\n  ");
			XMLElement el;
			bool elok = ep->second->getFirstElement(el);
			while (elok) {
				XMLElement *sub = new XMLElement;
				sub->setPretext(newtop->getPretext() + "  ");
				sub->setElementName(el.getElementName());
				sub->setText(el.getText());
				newtop->addElement(sub);
				elok = ep->second->getNextElement(el);
			}
			old_data.addElement(newtop);
			old_data.setText("\n");
		}
	}
	return tqsl_dump_station_data(old_data);
}

// Move a station location to or from the trash
static int
tqsl_move_station_location(const char *name, bool fromtrash) {
	tqslTrace("tqsl_move_station_location", "name=%s, fromtrash=%d", name, fromtrash);
	XMLElement from_top_el;
	XMLElement to_top_el;

	if (tqsl_load_station_data(from_top_el, fromtrash)) {
		tqslTrace("tqsl_move_station_location", "error %d loading data", tQSL_Error);
		return 1;
	}

	if (tqsl_load_station_data(to_top_el, !fromtrash)) {
		tqslTrace("tqsl_move_station_location", "error %d loading data", tQSL_Error);
		return 1;
	}

	XMLElement from_sfile;
	XMLElement to_sfile;
	if (!from_top_el.getFirstElement(from_sfile))
		from_sfile.setElementName("StationDataFile");

	if (!to_top_el.getFirstElement(to_sfile))
		to_sfile.setElementName("StationDataFile");

	XMLElementList& from_ellist = from_sfile.getElementList();
	XMLElementList::iterator from_ep;
	for (from_ep = from_ellist.find("StationData"); from_ep != from_ellist.end(); from_ep++) {
		if (from_ep->first != "StationData")
			break;
		pair<string, bool> from_rval = from_ep->second->getAttribute("name");
		if (from_rval.second && !strcasecmp(from_rval.first.c_str(), name)) {
			// Match, move it.
			// First, delete any old backup for this station location
			XMLElementList& to_ellist = to_sfile.getElementList();
			XMLElementList::iterator to_ep;
			for (to_ep = to_ellist.find("StationData"); to_ep != to_ellist.end(); to_ep++) {
				if (to_ep->first != "StationData")
					break;
				pair<string, bool> to_rval = to_ep->second->getAttribute("name");
				if (to_rval.second && !strcasecmp(to_rval.first.c_str(), name)) {
					to_ellist.erase(to_ep);
					break;
				}
			}
			// Now add it to the target
			XMLElement *newtop = new XMLElement("StationData");
			newtop->setPretext("\n  ");
			newtop->setAttribute("name", from_rval.first);
			newtop->setText("\n  ");
			XMLElement el;
			bool elok = from_ep->second->getFirstElement(el);
			while (elok) {
				XMLElement *sub = new XMLElement;
				sub->setPretext(newtop->getPretext() + "  ");
				sub->setElementName(el.getElementName());
				sub->setText(el.getText());
				newtop->addElement(sub);
				elok = from_ep->second->getNextElement(el);
			}
			to_sfile.addElement(newtop);
			to_sfile.setText("\n");
			tqsl_dump_station_data(to_sfile, !fromtrash);
			from_ellist.erase(from_ep);
			return tqsl_dump_station_data(from_sfile, fromtrash);
		}
	}
	tqslTrace("tqsl_move_station_location", "location not found");
	tQSL_Error = TQSL_LOCATION_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_deleteStationLocation(const char *name) {
	tqslTrace("tqsl_deleteStationLocation", "name=%s", name);

	return tqsl_move_station_location(name, false);
}

DLLEXPORT int CALLCONVENTION
tqsl_restoreStationLocation(const char *name) {
	tqslTrace("tqsl_restoreStationLocation", "name=%s", name);

	return tqsl_move_station_location(name, true);
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocation(tQSL_Location *locp, const char *name) {
	if (tqsl_initStationLocationCapture(locp)) {
		tqslTrace("tqsl_getStationLocation", "name=%s error=%d", name, tQSL_Error);
		return 1;
	}
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(*locp))) {
		tqslTrace("tqsl_getStationLocation", "loc error %d", tQSL_Error);
		return 1;
	}
	loc->name = name;
	XMLElement top_el;
	if (tqsl_load_station_data(top_el)) {
		tqslTrace("tqsl_getStationLocation", "load station data error %d", tQSL_Error);
		return 1;
	}
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	XMLElementList& ellist = sfile.getElementList();

	bool exists = false;
	XMLElementList::iterator ep;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second->getAttribute("name");
		if (rval.second && !strcasecmp(trim(rval.first).c_str(), trim(loc->name).c_str())) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		tQSL_Error = TQSL_LOCATION_NOT_FOUND;
		tqslTrace("tqsl_getStationLocation", "location %s does not exist", name);
		return 1;
	}
	return tqsl_load_loc(loc, ep, false);
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationErrors(tQSL_Location locp, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocation", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_getStationLocation", "buf = NULL");
		return 1;
	}
	strncpy(buf, loc->data_errors, bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumStationLocations(tQSL_Location locp, int *nloc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getNumStationLocations", "loc error %d", tQSL_Error);
		return 1;
	}
	if (nloc == NULL) {
		tqslTrace("tqsl_getNumStationLocations", "arg error nloc=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->names.clear();
	XMLElement top_el;
	if (tqsl_load_station_data(top_el)) {
		tqslTrace("tqsl_getNumStationLocations", "error %d loading station data", tQSL_Error);
		return 1;
	}
	XMLElement sfile;
	if (top_el.getFirstElement(sfile)) {
		XMLElement sd;
		bool ok = sfile.getFirstElement("StationData", sd);
		while (ok && sd.getElementName() == "StationData") {
			pair<string, bool> name = sd.getAttribute("name");
			if (name.second) {
				XMLElement xc;
				string call;
				if (sd.getFirstElement("CALL", xc))
					call = xc.getText();
				loc->names.push_back(TQSL_NAME(name.first, call));
			}
			ok = sfile.getNextElement(sd);
		}
	}
	*nloc = loc->names.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationName(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocationName", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL || idx < 0 || idx >= static_cast<int>(loc->names.size())) {
		tqslTrace("tqsl_getStationLocationName", "arg error buf=0x%lx, idx=%d", buf, idx);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCallSign(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocationCallSign", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL || idx < 0 || idx >= static_cast<int>(loc->names.size())) {
		tqslTrace("tqsl_getStationLocationCallSign", "arg error buf=0x%lx, idx=%d", buf, idx);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].call.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationField(tQSL_Location locp, const char *name, char *namebuf, int bufsize) {
	int old_page;
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocationField", "loc error %d", tQSL_Error);
		return 1;
	}
	if (name == NULL || namebuf == NULL) {
		tqslTrace("tqsl_getStationLocationField", "arg error name=0x%lx, namebuf=0x%lx", name, namebuf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_getStationLocationCapturePage(loc, &old_page)) {
		tqslTrace("tqsl_getStationLocationField", "get cap page error %d", tQSL_Error);
		return 1;
	}
	string find = name;

	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		int numf;
		if (tqsl_getNumLocationField(loc, &numf)) {
			tqslTrace("tqsl_getStationLocationField", "error getting num fields %d", tQSL_Error);
			return 1;
		}
		for (int i = 0; i < numf; i++) {
			TQSL_LOCATION_FIELD& field = loc->pagelist[loc->page-1].fieldlist[i];
			if (find == field.gabbi_name) {	// Found it
				switch (field.input_type) {
					case TQSL_LOCATION_FIELD_DDLIST:
					case TQSL_LOCATION_FIELD_LIST:
						if (field.data_type == TQSL_LOCATION_FIELD_INT) {
							char numbuf[20];
							if (static_cast<int>(field.items.size()) <= field.idx) {
								strncpy(namebuf, field.cdata.c_str(), bufsize);
							} else if (field.idx == 0 && field.items[field.idx].label == "[None]") {
								strncpy(namebuf, "", bufsize);
							} else {
								snprintf(numbuf, sizeof numbuf, "%d", field.items[field.idx].ivalue);
								strncpy(namebuf, numbuf, bufsize);
							}
						} else if (field.idx < 0 || field.idx >= static_cast<int>(field.items.size())) {
							// Allow CALL to not be in the items list
							if (field.idx == -1 && i == 0)
								strncpy(namebuf, field.cdata.c_str(), bufsize);
							else
								strncpy(namebuf, "", bufsize);
						} else {
							if (field.items[field.idx].label == "") {
								strncpy(namebuf, field.items[field.idx].text.c_str(), bufsize);
							} else {
								strncpy(namebuf, field.items[field.idx].label.c_str(), bufsize);
							}
						}
						break;
					case TQSL_LOCATION_FIELD_TEXT:
						field.cdata = trim(field.cdata);
						if (field.flags & TQSL_LOCATION_FIELD_UPPER)
							field.cdata = string_toupper(field.cdata);
						strncpy(namebuf, field.cdata.c_str(), bufsize);
						break;
				}
				goto done;
			}
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		if (tqsl_nextStationLocationCapture(loc)) {
			tqslTrace("tqsl_getStationLocationField", "error in nextStationLocationCapture %d", tQSL_Error);
			return 1;
		}
	} while (1);
	strncpy(namebuf, "", bufsize);		// Did not find it
 done:
	tqsl_setStationLocationCapturePage(loc, old_page);
	return 0;
}

static int
tqsl_location_to_xml(TQSL_LOCATION *loc, XMLElement& sd) {
	int old_page;
	if (tqsl_getStationLocationCapturePage(loc, &old_page)) {
		tqslTrace("tqsl_location_to_xml",  "get_sta_loc_cap_page error %d", tQSL_Error);
		return 1;
	}
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		int numf;
		if (tqsl_getNumLocationField(loc, &numf)) {
			tqslTrace("tqsl_location_to_xml", "get num loc field error %d", tQSL_Error);
			return 1;
		}
		for (int i = 0; i < numf; i++) {
			TQSL_LOCATION_FIELD& field = loc->pagelist[loc->page-1].fieldlist[i];
			XMLElement *fd = new XMLElement;
			fd->setPretext(sd.getPretext() + "  ");
			fd->setElementName(field.gabbi_name);
			switch (field.input_type) {
				case TQSL_LOCATION_FIELD_DDLIST:
				case TQSL_LOCATION_FIELD_LIST:
					if (field.idx < 0 || field.idx >= static_cast<int>(field.items.size())) {
						fd->setText("");
						if (field.gabbi_name == "CALL") {
							fd->setText("NONE");
						}
					} else if (field.data_type == TQSL_LOCATION_FIELD_INT) {
						char numbuf[20];
						snprintf(numbuf, sizeof numbuf, "%d", field.items[field.idx].ivalue);
						fd->setText(numbuf);
					} else {
						fd->setText(field.items[field.idx].text);
					}
					break;
				case TQSL_LOCATION_FIELD_TEXT:
					field.cdata = trim(field.cdata);
					if (field.flags & TQSL_LOCATION_FIELD_UPPER)
						field.cdata = string_toupper(field.cdata);
					fd->setText(field.cdata);
					break;
			}
			if (strcmp(fd->getText().c_str(), ""))
				sd.addElement(fd);
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		if (tqsl_nextStationLocationCapture(loc))
			return 1;
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCaptureName(tQSL_Location locp, const char *name) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_setStationLocationCaptureName", "loc error %d", tQSL_Error);
		return 1;
	}
	if (name == NULL) {
		tqslTrace("tqsl_setStationLocationCaptureName", "arg error name=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->name = name;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCaptureName(tQSL_Location locp, char *namebuf, int bufsize) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_getStationLocationCaptureName", "loc error %d", tQSL_Error);
		return 1;
	}
	if (namebuf == NULL) {
		tqslTrace("tqsl_getStationLocationCaptureName", "arg error namebuf=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(namebuf, loc->name.c_str(), bufsize);
	namebuf[bufsize-1] = 0;
	return 0;
}


DLLEXPORT int CALLCONVENTION
tqsl_saveStationLocationCapture(tQSL_Location locp, int overwrite) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp))) {
		tqslTrace("tqsl_saveStationLocationCaptureName", "loc error %d", tQSL_Error);
		return 1;
	}
	if (loc->name == "") {
		tqslTrace("tqsl_saveStationLocationCaptureName", "name empty");
		tQSL_Error = TQSL_EXPECTED_NAME;
		return 1;
	}
	XMLElement top_el;
	if (tqsl_load_station_data(top_el)) {
		tqslTrace("tqsl_saveStationLocationCaptureName", "error %d loading station data", tQSL_Error);
		return 1;
	}
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	XMLElementList& ellist = sfile.getElementList();
	bool exists = false;
	XMLElementList::iterator ep;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second->getAttribute("name");
		if (rval.second && !strcasecmp(rval.first.c_str(), loc->name.c_str())) {
			exists = true;
			break;
		}
	}
	if (exists && !overwrite) {
		tqslTrace("tqsl_saveStationLocationCaptureName", "exists, no overwrite");
		tQSL_Error = TQSL_NAME_EXISTS;
		return 1;
	}
	XMLElement *sd = new XMLElement("StationData");
	sd->setPretext("\n  ");
	if (tqsl_location_to_xml(loc, *sd)) {
		tqslTrace("tqsl_saveStationLocationCaptureName", "error in loc_to_xml %d", tQSL_Error);
		return 1;
	}
	sd->setAttribute("name", loc->name);
	sd->setText("\n  ");

	// If 'exists', ep points to the existing station record
	if (exists)
		ellist.erase(ep);

	sfile.addElement(sd);
	sfile.setText("\n");
	return tqsl_dump_station_data(sfile);
}


DLLEXPORT int CALLCONVENTION
tqsl_signQSORecord(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *rec, unsigned char *sig, int *siglen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_signQSORecord", "loc error %d", tQSL_Error);
		return 1;
	}
	if (make_sign_data(loc)) {
		tqslTrace("tqsl_signQSORecord", "error %d making sign data", tQSL_Error);
		return 1;
	}
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	while (ok) {
		string eln = specfield.getElementName();
		const char *elname = eln.c_str();
		const char *value = 0;
		char buf[100];
		if (!strcmp(elname, "CALL")) {
			value = rec->callsign;
		} else if (!strcmp(elname, "BAND")) {
			value = rec->band;
		} else if (!strcmp(elname, "BAND_RX")) {
			value = rec->rxband;
		} else if (!strcmp(elname, "MODE")) {
			value = rec->mode;
		} else if (!strcmp(elname, "FREQ")) {
			value = rec->freq;
		} else if (!strcmp(elname, "FREQ_RX")) {
			value = rec->rxfreq;
		} else if (!strcmp(elname, "PROP_MODE")) {
			value = rec->propmode;
		} else if (!strcmp(elname, "SAT_NAME")) {
			value = rec->satname;
		} else if (!strcmp(elname, "QSO_DATE")) {
			if (tqsl_isDateValid(&(rec->date)))
				value = tqsl_convertDateToText(&(rec->date), buf, sizeof buf);
		} else if (!strcmp(elname, "QSO_TIME")) {
			if (tqsl_isTimeValid(&(rec->time)))
				value = tqsl_convertTimeToText(&(rec->time), buf, sizeof buf);
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unknown field in signing specification: %s", elname);
			tqslTrace("tqsl_signQSORecord", "field err %s", tQSL_CustomError);
			return 1;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				tqslTrace("tqsl_signQSORecord", "val err %s", tQSL_CustomError);
				return 1;
			}
		} else {
			string v(value);
			rec_sign_data += trim(v);
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	}
	return tqsl_signDataBlock(cert, (const unsigned char *)rec_sign_data.c_str(), rec_sign_data.size(), sig, siglen);
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCERT(tQSL_Cert cert, int uid) {
	static string s;

	s = "";
	char buf[3000];
	if (tqsl_getCertificateEncoded(cert, buf, sizeof buf))
		return 0;
	char *cp = strstr(buf, "-----END CERTIFICATE-----");
	if (cp)
		*cp = 0;
	if ((cp = strstr(buf, "\n")))
		cp++;
	else
		cp = buf;
	s = "<Rec_Type:5>tCERT\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", uid);
	snprintf(lbuf, sizeof lbuf, "<CERT_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	s += lbuf;
	snprintf(lbuf, sizeof lbuf, "<CERTIFICATE:%d>", static_cast<int>(strlen(cp)));
	s += lbuf;
	s += cp;
	s += "<eor>\n";
	return s.c_str(); //KC2YWE 1/26 - dangerous but might work since s is static
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItSTATION(tQSL_Location locp, int uid, int certuid) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getGABBItSTATION", "loc error %d", tQSL_Error);
		return 0;
	}
	unsigned char *buf = 0;
	int bufsiz = 0;
	loc->tSTATION = "<Rec_Type:8>tSTATION\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", uid);
	snprintf(lbuf, sizeof lbuf, "<STATION_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tSTATION += lbuf;
	snprintf(sbuf, sizeof sbuf, "%d", certuid);
	snprintf(lbuf, sizeof lbuf, "<CERT_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tSTATION += lbuf;
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_BADZONE)	// Don't output these to tSTATION
				continue;

			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size())) {
					s = "";
				} else {
					s = f.items[f.idx].text;
				}
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				snprintf(buf, sizeof buf, "%d", f.idata);
				s = buf;
			} else {
				s = f.cdata;
			}
			if (s.size() == 0)
				continue;
			int wantsize = s.size() + f.gabbi_name.size() + 20;
			if (buf == 0 || bufsiz < wantsize) {
				if (buf != 0)
					delete[] buf;
				buf = new unsigned char[wantsize];
				bufsiz = wantsize;
			}
			if (tqsl_adifMakeField(f.gabbi_name.c_str(), 0, (unsigned char *)s.c_str(), s.size(), buf, bufsiz)) {
				delete[] buf;
				return 0;
			}
			loc->tSTATION += (const char *)buf;
			loc->tSTATION += "\n";
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);
	if (buf != 0)
		delete[] buf;
	loc->tSTATION += "<eor>\n";
	return loc->tSTATION.c_str();
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCONTACTData(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *qso, int stationuid, char* signdata, int sdlen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getGABBItCONTACTData", "loc error %d", tQSL_Error);
		return 0;
	}
	if (make_sign_data(loc)) {
		tqslTrace("tqsl_getGABBItCONTACTData", "make_sign_data error %d", tQSL_Error);
		return 0;
	}
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	loc->qso_details = "";
	while(ok) {
		string en = specfield.getElementName();
		const char *elname = en.c_str();
		const char *value = 0;
		char buf[100];
		if (!strcmp(elname, "CALL")) {
			value = qso->callsign;
		} else if (!strcmp(elname, "BAND")) {
			value = qso->band;
		} else if (!strcmp(elname, "BAND_RX")) {
			value = qso->rxband;
		} else if (!strcmp(elname, "MODE")) {
			value = qso->mode;
		} else if (!strcmp(elname, "FREQ")) {
			value = qso->freq;
		} else if (!strcmp(elname, "FREQ_RX")) {
			value = qso->rxfreq;
		} else if (!strcmp(elname, "PROP_MODE")) {
			value = qso->propmode;
		} else if (!strcmp(elname, "SAT_NAME")) {
			value = qso->satname;
		} else if (!strcmp(elname, "QSO_DATE")) {
			if (tqsl_isDateValid(&(qso->date)))
				value = tqsl_convertDateToText(&(qso->date), buf, sizeof buf);
		} else if (!strcmp(elname, "QSO_TIME")) {
			if (tqsl_isTimeValid(&(qso->time)))
				value = tqsl_convertTimeToText(&(qso->time), buf, sizeof buf);
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unknown field in signing specification: %s", elname);
			tqslTrace("tqsl_getGABBItCONTACTData", "field err %s", tQSL_CustomError);
			return 0;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				tqslTrace("tqsl_getGABBItCONTACTData", "field err %s", tQSL_CustomError);
				return 0;
			}
		} else {
			string v(value);
			rec_sign_data += trim(v);
			loc->qso_details += trim(v);
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	}
	unsigned char sig[129];
	int siglen = sizeof sig;
	rec_sign_data = string_toupper(rec_sign_data);
	if (tqsl_signDataBlock(cert, (const unsigned char *)rec_sign_data.c_str(), rec_sign_data.size(), sig, &siglen))
		return 0;
	char b64[512];
	if (tqsl_encodeBase64(sig, siglen, b64, sizeof b64))
		return 0;
	loc->tCONTACT = "<Rec_Type:8>tCONTACT\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", stationuid);
	snprintf(lbuf, sizeof lbuf, "<STATION_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tCONTACT += lbuf;
	char buf[256];
	tqsl_adifMakeField("CALL", 0, (const unsigned char *)qso->callsign, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField("BAND", 0, (const unsigned char *)qso->band, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField("MODE", 0, (const unsigned char *)qso->mode, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	// Optional fields
	if (qso->freq[0] != 0) {
		tqsl_adifMakeField("FREQ", 0, (const unsigned char *)qso->freq, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->rxfreq[0] != 0) {
		tqsl_adifMakeField("FREQ_RX", 0, (const unsigned char *)qso->rxfreq, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->propmode[0] != 0) {
		tqsl_adifMakeField("PROP_MODE", 0, (const unsigned char *)qso->propmode, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->satname[0] != 0) {
		tqsl_adifMakeField("SAT_NAME", 0, (const unsigned char *)qso->satname, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->rxband[0] != 0) {
		tqsl_adifMakeField("BAND_RX", 0, (const unsigned char *)qso->rxband, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	// Date and Time
	char date_buf[40] = "";
	tqsl_convertDateToText(&(qso->date), date_buf, sizeof date_buf);
	tqsl_adifMakeField("QSO_DATE", 0, (const unsigned char *)date_buf, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	date_buf[0] = 0;
	tqsl_convertTimeToText(&(qso->time), date_buf, sizeof date_buf);
	tqsl_adifMakeField("QSO_TIME", 0, (const unsigned char *)date_buf, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField(loc->sigspec.c_str(), '6', (const unsigned char *)b64, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	// Signature
	tqsl_adifMakeField("SIGNDATA", 0, (const unsigned char *)rec_sign_data.c_str(), -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	loc->tCONTACT += "<eor>\n";
	if (signdata)
		strncpy(signdata, rec_sign_data.c_str(), sdlen);
	return loc->tCONTACT.c_str();
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCONTACT(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *qso, int stationuid) {
	return tqsl_getGABBItCONTACTData(cert, locp, qso, stationuid, NULL, 0);
}


DLLEXPORT int CALLCONVENTION
tqsl_getLocationCallSign(tQSL_Location locp, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationCallSign", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL || bufsiz <= 0) {
		tqslTrace("tqsl_getLocationCallSign", "arg error buf=0x%lx, bufsiz=%d", buf, bufsiz);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "CALL") {
			strncpy(buf, f.cdata.c_str(), bufsiz);
			buf[bufsiz-1] = 0;
			if (static_cast<int>(f.cdata.size()) >= bufsiz) {
				tqslTrace("tqsl_getLocationCallSign", "buf error req=%d avail=%d", static_cast<int>(f.cdata.size()), bufsiz);
				tQSL_Error = TQSL_BUFFER_ERROR;
				return 1;
			}
			return 0;
		}
	}
	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_setLocationCallSign(tQSL_Location locp, const char *buf, int dxcc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_setLocationCallSign", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL) {
		tqslTrace("tqsl_setLocationCallSign", "arg error buf=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "CALL") {
			for (int j = 0; j < static_cast<int>(f.items.size()); j++) {
				if (f.items[j].text == buf) {
					loc->pagelist[0].fieldlist[i].idx = j;
					loc->pagelist[0].fieldlist[i].cdata = buf;
					loc->newflags = true;
					loc->newDXCC = dxcc;
					break;
				}
			}
			return 0;
		}
	}
	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationField(tQSL_Location locp, const char *field, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationField", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL || bufsiz <= 0) {
		tqslTrace("tqsl_getLocationField", "arg error buf=0x%lx, bufsiz=%d", buf, bufsiz);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*buf = '\0';
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);

	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD f = p.fieldlist[i];
			if (f.gabbi_name == field) {
				if ((f.gabbi_name == "ITUZ" || f.gabbi_name == "CQZ") && f.cdata == "0") {
					buf[0] = '\0';
				} else {
					strncpy(buf, f.cdata.c_str(), bufsiz);
				}
				buf[bufsiz-1] = 0;
				if (static_cast<int>(f.cdata.size()) >= bufsiz) {
					tqslTrace("tqsl_getLocationField", "buf error req=%d avail=%d", static_cast<int>(f.cdata.size()), bufsiz);
					tQSL_Error = TQSL_BUFFER_ERROR;
					return 1;
				}
				tqsl_setStationLocationCapturePage(loc, old_page);
				return 0;
			}
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);

	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldLabel(tQSL_Location locp, const char *field, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationFieldLabel", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL || bufsiz <= 0) {
		tqslTrace("tqsl_getLocationFieldLabel", "arg error buf=0x%lx, bufsiz=%d", buf, bufsiz);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*buf = '\0';
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);

	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD f = p.fieldlist[i];
			if (f.gabbi_name == field) {
				if ((f.gabbi_name == "ITUZ" || f.gabbi_name == "CQZ") && f.cdata == "0") {
					buf[0] = '\0';
				} else {
					if (static_cast<int>(f.items.size()) > f.idx)
						strncpy(buf, f.items[f.idx].label.c_str(), bufsiz);
				}
				buf[bufsiz-1] = 0;
				if (static_cast<int>(f.label.size()) >= bufsiz) {
					tqslTrace("tqsl_getLocationFieldLabel", "buf error req=%d avail=%d", static_cast<int>(f.cdata.size()), bufsiz);
					tQSL_Error = TQSL_BUFFER_ERROR;
					return 1;
				}
				tqsl_setStationLocationCapturePage(loc, old_page);
				return 0;
			}
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);

	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}
// Replaces all occurrences of 'from' with 'to' in string 'str'

static void replaceAll(string& str, const string& from, const string& to) {
	if (from.empty()) {
		return;
	}
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        	str.replace(start_pos, from.length(), to);
        	start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

static bool fuzzy_match(string userInput, string field) {
	// First upcase
	string left = string_toupper(userInput);
	string right = string_toupper(field);
	// Strip spaces
	replaceAll(left, " ", "");
	replaceAll(right, " ", "");
	// Strip apostraphes
	replaceAll(left, "'", "");
	replaceAll(right, "'", "");
	// Strip hyphens
	replaceAll(left, "-", "");
	replaceAll(right, "-", "");
	// Alaska fixes
	replaceAll(left, "CITYANDBOROUGH", "");
	replaceAll(right, "CITYANDBOROUGH", "");
	replaceAll(left, "BOROUGH", "");
	replaceAll(right, "BOROUGH", "");
	replaceAll(left, "CENSUSAREA", "");
	replaceAll(right, "CENSUSAREA", "");
	replaceAll(left, "MUNICIPALITY", "");
	replaceAll(right, "MUNICIPALITY", "");
	// Normalize saints
	replaceAll(left, "ST.", "SAINT");
	replaceAll(right, "ST.", "SAINT");
	replaceAll(left, "STE.", "SAINTE");
	replaceAll(right, "STE.", "SAINTE");
	// One-offs
	replaceAll(left, "DOÑAANA", "DONAANA");
	replaceAll(right, "DOÑAANA", "DONAANA");
	replaceAll(left, "BRISTOLCITY", "BRISTOL");
	replaceAll(right, "BRISTOLCITY", "BRISTOL");
	replaceAll(left, "SALEMCITY", "SALEM");
	replaceAll(right, "SALEMCITY", "SALEM");
	return (left == right);
}

DLLEXPORT int CALLCONVENTION
tqsl_setLocationField(tQSL_Location locp, const char *field, const char *buf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_setLocationField", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL) {
		tqslTrace("tqsl_setLocationField", "arg error buf=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);

	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];

		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD *pf = &p.fieldlist[i];
			if (pf->gabbi_name == field) {
				bool found = false;
				bool adifVal = false;
				pf->cdata = string(buf).substr(0, pf->data_len);
				if (pf->flags & TQSL_LOCATION_FIELD_UPPER)
					pf->cdata = string_toupper(pf->cdata);

				if (pf->input_type == TQSL_LOCATION_FIELD_DDLIST || pf->input_type == TQSL_LOCATION_FIELD_LIST) {
					if (pf->cdata == "") {
						pf->idx = 0;
						pf->idata = pf->items[0].ivalue;
					} else {
						for (int i = 0; i < static_cast<int>(pf->items.size()); i++) {
							if (string_toupper(pf->items[i].text) == string_toupper(pf->cdata)) {
								pf->cdata = pf->items[i].text;
								pf->idx = i;
								pf->idata = pf->items[i].ivalue;
								found = true;
								break;
							}

							if (fuzzy_match(pf->items[i].label, pf->cdata)) {
								strncpy(tQSL_CustomError, pf->items[i].text.c_str(), sizeof tQSL_CustomError);
								pf->cdata = pf->items[i].text;
								pf->idx = i;
								pf->idata = pf->items[i].ivalue;
								found = true;
								adifVal = true;
								break;
							}
						}
// This was being used to force-add fields to enumerations, but that's wrong.
// Keeping it around in case it's useful later.
//						if (!found) {
//							TQSL_LOCATION_ITEM item;
//							item.text = buf;
//							item.ivalue = strtol(buf, NULL, 10);
//							pf->items.push_back(item);
//							pf->idx = pf->items.size() - 1;
//							pf->idata = item.ivalue;
//						}
					}
				} else if (pf->data_type == TQSL_LOCATION_FIELD_INT) {
					pf->idata = strtol(buf, NULL, 10);
				}
				tqsl_setStationLocationCapturePage(loc, old_page);
				if (adifVal)
					return -2;
				if (!found)
					return -1;
				return 0;
			}
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);

	tqsl_setStationLocationCapturePage(loc, old_page);
	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationDXCCEntity(tQSL_Location locp, int *dxcc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationDXCCEntity", "loc error %d", tQSL_Error);
		return 1;
	}
	if (dxcc == NULL) {
		tqslTrace("tqsl_getLocationDXCCEntity", "arg err dxcc=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "DXCC") {
			if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size()))
				break;	// No matching DXCC entity
			*dxcc = f.items[f.idx].ivalue;
			return 0;
		}
	}
	tqslTrace("tqsl_getLocationDXCCEntity", "name not found");
	tQSL_Error = TQSL_NAME_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumProviders(int *n) {
	if (n == NULL) {
		tqslTrace("tqsl_getNumProviders", "arg error n=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	vector<TQSL_PROVIDER> plist;
	if (tqsl_load_provider_list(plist)) {
		tqslTrace("tqsl_getNumProviders", "error loading providers %d", tQSL_Error);
		return 1;
	}
	if (plist.size() == 0) {
		tqslTrace("tqsl_getNumProviders", "prov not found");
		tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
		return 1;
	}
	*n = plist.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getProvider(int idx, TQSL_PROVIDER *provider) {
	if (provider == NULL || idx < 0) {
		tqslTrace("tqsl_getProvider", "arg error provider=0x%lx, idx=%d", provider, idx);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	vector<TQSL_PROVIDER> plist;
	if (tqsl_load_provider_list(plist)) {
		tqslTrace("tqsl_getProvider", "err %d loading list", tQSL_Error);
		return 1;
	}
	if (idx >= static_cast<int>(plist.size())) {
		tqslTrace("tqsl_getProvider", "prov not found");
		tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
		return 1;
	}
	*provider = plist[idx];
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_importTQSLFile(const char *file, int(*cb)(int type, const char *, void *), void *userdata) {
	bool foundcerts = false;
	tQSL_ImportCall[0] = '\0';
	tQSL_ImportSerial = 0;
	int rval = 0;

	if (file == NULL) {
		tqslTrace("tqsl_importTQSLFile", "file=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	XMLElement topel;
	int status = topel.parseFile(file);
	if (status) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_importTQSLFile", "system error file=%s err=%s", file, strerror(tQSL_Errno));
		} else {
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
			tqslTrace("tqsl_importTQSLFile", "file %s syntax error", file);
		}
		return 1;
	}
	XMLElement tqsldata;
	if (!topel.getFirstElement("tqsldata", tqsldata)) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		return 1;
	}
	XMLElement section;
	bool stat = tqsldata.getFirstElement("tqslcerts", section);
	if (stat) {
		XMLElement cert;
		bool cstat = section.getFirstElement("rootcert", cert);
		while (cstat) {
			foundcerts = true;
			if (tqsl_import_cert(cert.getText().c_str(), ROOTCERT, cb, userdata)) {
				tqslTrace("tqsl_importTQSLFile", "duplicate/expired root cert");
			}
			cstat = section.getNextElement(cert);
		}
		cstat = section.getFirstElement("cacert", cert);
		while (cstat) {
			foundcerts = true;
			if (tqsl_import_cert(cert.getText().c_str(), CACERT, cb, userdata)) {
				tqslTrace("tqsl_importTQSLFile", "duplicate/expired ca cert");
			}
			cstat = section.getNextElement(cert);
		}
		cstat = section.getFirstElement("usercert", cert);
		while (cstat) {
			foundcerts = true;
			if (tqsl_import_cert(cert.getText().c_str(), USERCERT, cb, userdata)) {
				tqslTrace("tqsl_importTQSLFile", "error importing user cert");
				tQSL_Error = TQSL_CERT_ERROR;
				rval = 1;
			}
			cstat = section.getNextElement(cert);
		}
	}
	// If any of the user certificates failed import, return the error status.
	if (rval) {
		return rval;
	}

	stat = tqsldata.getFirstElement("tqslconfig", section);
	if (stat) {
		// Check to make sure we aren't overwriting newer version
		int major = strtol(section.getAttribute("majorversion").first.c_str(), NULL, 10);
		int minor = strtol(section.getAttribute("minorversion").first.c_str(), NULL, 10);
		int curmajor, curminor;
		if (tqsl_getConfigVersion(&curmajor, &curminor)) {
			tqslTrace("tqsl_importTQSLFile", "Get config ver error %d", tQSL_Error);
			return 1;
		}
		if (major < curmajor) {
			if (foundcerts) {
				tqslTrace("tqsl_importTQSLFile", "Suppressing update from V%d.%d to V%d.%d", curmajor, curminor, major, minor);
				return rval;
			}
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"This configuration file (V%d.%d) is older than the currently installed one (V%d.%d). It will not be installed.",
						major, minor, curmajor, curminor);
			tqslTrace("tqsl_importTQSLFile", "Config update error: %s", tQSL_CustomError);
			return 1;
		}
		if (major == curmajor) {
			if (minor == curminor) {		// Same rev as already installed
				tqslTrace("tqsl_importTQSLFile", "Suppressing update from V%d.%d to V%d.%d", curmajor, curminor, major, minor);
				return rval;
			}
			if (minor < curminor) {
				if (foundcerts) {
					tqslTrace("tqsl_importTQSLFile", "Suppressing update from V%d.%d to V%d.%d", curmajor, curminor, major, minor);
					return rval;
				}
				tQSL_Error = TQSL_CUSTOM_ERROR;
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
					"This configuration file (V%d.%d) is older than the currently installed one (V%d.%d). It will not be installed.",
							major, minor, curmajor, curminor);
				tqslTrace("tqsl_importTQSLFile", "Config update error: %s", tQSL_CustomError);
				return rval;
		 	}
		}
		// Save the configuration file
		ofstream out;
#ifdef _WIN32
		string fn = string(tQSL_BaseDir) + "\\config.xml";
#else
		string fn = string(tQSL_BaseDir) + "/config.xml";
#endif
		out.exceptions(ios::failbit | ios::eofbit | ios::badbit);
		try {
#ifdef _WIN32
			wchar_t *wfn = utf8_to_wchar(fn.c_str());
			out.open(wfn);
			free_wchar(wfn);
#else
			out.open(fn.c_str());
#endif
			out << section << endl;
			out.close();
		}
		catch(exception& x) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Error writing new configuration file (%s): %s/%s",
				fn.c_str(), x.what(), strerror(errno));
			tqslTrace("tqsl_importTQSLFile", "I/O error: %s", tQSL_CustomError);
			if (cb)
				return (*cb)(TQSL_CERT_CB_RESULT | TQSL_CERT_CB_ERROR | TQSL_CERT_CB_CONFIG,
					fn.c_str(), userdata);
			if (tQSL_Error == 0) {
				tQSL_Error = TQSL_CERT_ERROR;
			}
			return 1;
		}
		// Clear stored config data to force re-reading new config
		tqsl_xml_config.clear();
		DXCCMap.clear();
		DXCCList.clear();
		BandList.clear();
		ModeList.clear();
		tqsl_page_map.clear();
		tqsl_field_map.clear();
		tqsl_adif_map.clear();
		tqsl_cabrillo_map.clear();
		string version = "Configuration V" + section.getAttribute("majorversion").first + "."
			+ section.getAttribute("minorversion").first + "\n" + fn;
		if (cb) {
			int cbret = (*cb)(TQSL_CERT_CB_RESULT | TQSL_CERT_CB_LOADED | TQSL_CERT_CB_CONFIG,
				version.c_str(), userdata);
			if (cbret || rval) {
				if (tQSL_Error == 0) {
					tQSL_Error = TQSL_CERT_ERROR;
				}
				return 1;
			}
		}
	}
	if (rval && tQSL_Error == 0) {
		tQSL_Error = TQSL_CERT_ERROR;
	}
	return rval;
}

/*
 * Get the first user certificate from a .tq6 file
*/
DLLEXPORT int CALLCONVENTION
tqsl_getSerialFromTQSLFile(const char *file, long *serial) {
	XMLElement topel;
	if (file == NULL || serial == NULL) {
		tqslTrace("tqsl_getSerialFromTQSLFile", "Arg error file=0x%lx, serial=0x%lx", file, serial);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	int status =  topel.parseFile(file);
	if (status) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_getSerialFromTQSLFile", "parse error %d, error %s", tQSL_Error, strerror(tQSL_Errno));
		} else {
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
			tqslTrace("tqsl_getSerialFromTQSLFile", "parse syntax error %d", tQSL_Error);
		}
		return 1;
	}
	XMLElement tqsldata;
	if (!topel.getFirstElement("tqsldata", tqsldata)) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_getSerialFromTQSLFile", "parse syntax error %d", tQSL_Error);
		tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		return 1;
	}
	XMLElement section;
	bool stat = tqsldata.getFirstElement("tqslcerts", section);
	if (stat) {
		XMLElement cert;
		bool cstat = section.getFirstElement("usercert", cert);
		if (cstat) {
			if (tqsl_get_pem_serial(cert.getText().c_str(), serial)) {
				strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
				tqslTrace("tqsl_getSerialFromTQSLFile", "parse syntax error %d", tQSL_Error);
				tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
				return 1;
			}
			return 0;
		}
	}
	tqslTrace("tqsl_getSerialFromTQSLFile", "no usercert in file %s", file);
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDeletedStationLocations(char ***locp, int *nloc) {
	if (locp == NULL) {
		tqslTrace("tqsl_getDeletedStationLocations", "arg error locp=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (nloc == NULL) {
		tqslTrace("tqsl_getDeletedStationLocations", "arg error nloc=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*locp = NULL;
	*nloc = 0;

	vector<string> namelist;

	XMLElement top_el;
	if (tqsl_load_station_data(top_el, true)) {
		tqslTrace("tqsl_getDeletedStationLocations", "error %d loading station data", tQSL_Error);
		return 1;
	}
	XMLElement sfile;
	if (top_el.getFirstElement(sfile)) {
		XMLElement sd;
		bool ok = sfile.getFirstElement("StationData", sd);
		while (ok && sd.getElementName() == "StationData") {
			pair<string, bool> name = sd.getAttribute("name");
			if (name.second) {
				namelist.push_back(name.first);
			}
			ok = sfile.getNextElement(sd);
		}
	}
	*nloc = namelist.size();
	if (*nloc == 0) {
		*locp = NULL;
		return 0;
	}
	*locp = reinterpret_cast<char **>(calloc(*nloc, sizeof(**locp)));
	vector<string>::iterator it;
	char **p = *locp;
	for (it = namelist.begin(); it != namelist.end(); it++) {
		*p++ = strdup((*it).c_str());
	}
	return 0;
}

DLLEXPORT void CALLCONVENTION
tqsl_freeDeletedLocationList(char** list, int nloc) {
	if (!list) return;
	for (int i = 0; i < nloc; i++)
		if (list[i]) free(list[i]);
	if (list) free(list);
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationQSODetails(tQSL_Location locp, char *buf, int buflen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationQSODetails", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL) {
		tqslTrace("tqsl_getLocationQSODetails", "Argument error, buf = 0x%lx", buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->qso_details.c_str(), buflen);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationStationDetails(tQSL_Location locp, char *buf, int buflen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false))) {
		tqslTrace("tqsl_getLocationStationDetails", "loc error %d", tQSL_Error);
		return 1;
	}
	if (buf == NULL) {
		tqslTrace("tqsl_getLocationStationDetails", "Argument error, buf = 0x%lx", buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->loc_details.c_str(), buflen);
	return 0;
}
