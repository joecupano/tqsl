/***************************************************************************
                          tqsl.cpp  -  description
                             -------------------
    begin                : Mon Nov 4 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqsl.cpp,v 1.12 2010/03/11 19:14:21 k1mu Exp $
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif
#include <stdlib.h>

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/wxchar.h>
#include <wx/config.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include <wx/wxhtml.h>
#include <wx/wfstream.h>
#include <wx/cmdline.h>
#include "wxutil.h"

#include <iostream>
#include <fstream>
#include <memory>
#ifdef _WIN32
	#include <io.h>
#endif
#include <zlib.h>
#include "tqslwiz.h"
#include "tqslerrno.h"
#include "tqslexcept.h"
#include "tqslpaths.h"
#include "stationdial.h"
#include "tqslconvert.h"
#include "dxcc.h"
#include "tqsl_prefs.h"

#undef ALLOW_UNCOMPRESSED

#include "tqslbuild.h"

using std::ofstream;
using std::cerr;
using std::endl;
using std::ios;

enum {
	tm_f_import = 7000,
	tm_f_import_compress,
	tm_f_compress,
	tm_f_uncompress,
	tm_f_preferences,
	tm_f_new,
	tm_f_edit,
	tm_f_exit,
	tm_s_add,
	tm_s_edit,
	tm_h_contents,
	tm_h_about,
};

enum {
	TQSL_ACTION_ASK = 0,
	TQSL_ACTION_ABORT = 1,
	TQSL_ACTION_NEW = 2,
	TQSL_ACTION_ALL = 3,
	TQSL_ACTION_UNSPEC = 4
};

#define TQSL_CD_MSG TQSL_ID_LOW
#define TQSL_CD_CANBUT TQSL_ID_LOW+1

static wxString ErrorTitle(wxT("TQSL Error"));

/////////// Application //////////////

class QSLApp : public wxAppConsole {
 public:
	QSLApp();
	virtual ~QSLApp();
	bool OnInit();
	int  OnRun();
	bool ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile, bool compress = false, bool suppressdate = false, int action = TQSL_ACTION_UNSPEC, const char *password = NULL);
 private:
	tQSL_Location loc;
	char *password;
	bool suppressdate;
	int action;
	wxString infile;
	wxString outfile;
};

IMPLEMENT_APP_CONSOLE(QSLApp)

#define TQSL_DR_START TQSL_ID_LOW
#define TQSL_DR_END TQSL_ID_LOW+1
#define TQSL_DR_OK TQSL_ID_LOW+2
#define TQSL_DR_CAN TQSL_ID_LOW+3
#define TQSL_DR_MSG TQSL_ID_LOW+4

static void
init_modes() {
	tqsl_clearADIFModes();
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	long cookie;
	wxString key, value;
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		tqsl_setADIFMode(key.ToUTF8(), value.ToUTF8());
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
}

static void
init_contests() {
	tqsl_clearCabrilloMap();
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	long cookie;
	wxString key, value;
	config->SetPath(wxT("/cabrilloMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		int contest_type = strtol(value.ToUTF8(), NULL, 10);
		int callsign_field = strtol(value.AfterFirst(';').ToUTF8(), NULL, 10);
		tqsl_setCabrilloMapEntry(key.ToUTF8(), callsign_field, contest_type);
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
}

static void
check_tqsl_error(int rval) {
	if (rval == 0)
		return;
	const char *msg = tqsl_getErrorString();
	throw TQSLException(msg);
}

static tQSL_Cert *certlist = 0;
static int ncerts;

static void
free_certlist() {
	if (certlist) {
		for (int i = 0; i < ncerts; i++)
			tqsl_freeCertificate(certlist[i]);
		certlist = 0;
	}
}

static void
get_certlist(string callsign, int dxcc) {
	free_certlist();
	tqsl_selectCertificates(&certlist, &ncerts,
		(callsign == "") ? 0 : callsign.c_str(), dxcc, 0, 0, 0);
}

class QSLApp;

bool
QSLApp::ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile,
	bool compressed, bool suppressdate, int action, const char *password) {
	static const char *iam = "TQSL V" VERSION;
	const char *cp;
	tQSL_Converter conv = 0;
	char callsign[40];
	int dxcc;
	wxString name, ext;
	bool allow_dupes = false;
	gzFile gout = 0;
	ofstream out;
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());

	if (action == TQSL_ACTION_ALL)
		allow_dupes = true;

	check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
	check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxcc));

	get_certlist(callsign, dxcc);
	if (ncerts == 0) {
		cerr << "There are no valid certificates for callsign " << callsign << ". Signing aborted" << endl;
		return false;
	}

	cout << wxString::Format(wxT("Signing using CALL=%hs, DXCC=%d"), callsign, dxcc) << endl;

	init_modes();
	init_contests();

	if (compressed)
		gout = gzopen(outfile.ToUTF8(), "wb9");
	else
		out.open(outfile.ToUTF8(), ios::out|ios::trunc|ios::binary);

	if ((compressed && !gout) || (!compressed && !out)) {
		cerr << "Unable to open " << outfile << endl;
		return false;
	}

	int n = 0;
	int lineno = 0;
	int out_of_range = 0;
	int duplicates = 0;
	int processed = 0;
	bool cancelled = false;
	try {
		if (tqsl_beginCabrilloConverter(&conv, infile.ToUTF8(), certlist, ncerts, loc)) {
			if (tQSL_Error != TQSL_CABRILLO_ERROR || tQSL_Cabrillo_Error != TQSL_CABRILLO_NO_START_RECORD)
				check_tqsl_error(1);	// A bad error
			lineno = 0;
	   		check_tqsl_error(tqsl_beginADIFConverter(&conv, infile.ToUTF8(), certlist, ncerts, loc));
		}
		bool range = true;
		config->Read(wxT("DateRange"), &range);
		bool allow = false;
		config->Read(wxT("BadCalls"), &allow);
		tqsl_setConverterAllowBadCall(conv, allow);
		tqsl_setConverterAllowDuplicates(conv, allow_dupes);
		wxSplitPath(infile, 0, &name, &ext);
		if (!ext.IsEmpty())
			name += wxT(".") + ext;
		// Only display windows if notin batch mode -- KD6PAG
		bool ignore_err = false;
		int major = 0, minor = 0, config_major = 0, config_minor = 0;
		tqsl_getVersion(&major, &minor);
		tqsl_getConfigVersion(&config_major, &config_minor);
		wxString ident = wxString::Format(wxT("%hs Lib: V%d.%d Config: V%d.%d AllowDupes: %hs"), iam,
			major, minor, config_major, config_minor,
			allow_dupes ? "true" : "false");
		wxString gabbi_ident = wxString::Format(wxT("<TQSL_IDENT:%d>%s"), static_cast<int>(ident.length()), ident.c_str());
		gabbi_ident += wxT("\n");
		if (compressed)
			gzwrite(gout, (const char *)gabbi_ident.ToUTF8(), gabbi_ident.length());
		else
			out << gabbi_ident << endl;
		do {
	   		while ((cp = tqsl_getConverterGABBI(conv)) != 0) {
					// Only count QSO records
					if (strstr(cp, "tCONTACT")) {
						++n;
						++processed;
					}
					if (compressed) {
	  					gzwrite(gout, const_cast<char *>(cp), strlen(cp));
						gzputs(gout, "\n");
					} else {
						out << cp << endl;
					}
			}
			if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
				tQSL_Cert cert;
				int rval;
				check_tqsl_error(tqsl_getConverterCert(conv, &cert));
				do {
	   				if ((rval = tqsl_beginSigning(cert, const_cast<char *>(password), 0, cert)) == 0)
						break;
					if (tQSL_Error == TQSL_PASSWORD_ERROR) {
						if (password)
							free(reinterpret_cast<void *>(password));
						password = NULL;
					}
				} while (tQSL_Error == TQSL_PASSWORD_ERROR);
				check_tqsl_error(rval);
				continue;
			}
			if (tQSL_Error == TQSL_DATE_OUT_OF_RANGE) {
				processed++;
				out_of_range++;
				continue;
			}
			if (tQSL_Error == TQSL_DUPLICATE_QSO) {
				processed++;
				duplicates++;
				continue;
			}
			bool has_error = (tQSL_Error != TQSL_NO_ERROR);
			if (has_error) {
				try {
					check_tqsl_error(1);
				} catch(TQSLException& x) {
					tqsl_getConverterLine(conv, &lineno);
					wxString msg = wxString::FromUTF8(x.what());
					if (lineno)
						msg += wxString::Format(wxT(" on line %d"), lineno);
					const char *bad_text = tqsl_getConverterRecordText(conv);
					if (bad_text)
						msg += wxString(wxT("\n")) + wxString::FromUTF8(bad_text);
					if (!ignore_err)
						cerr << msg << endl;
					// Only ask if not in batch mode or ignoring errors - KD6PAG
					switch (action) {
                                                case TQSL_ACTION_ABORT:
							cancelled = true;
							ignore_err = true;
							goto abortSigning;
                                                case TQSL_ACTION_NEW:
                                                case TQSL_ACTION_ALL:
							ignore_err = true;
							break;
                                                case TQSL_ACTION_ASK:
                                                case TQSL_ACTION_UNSPEC:
							break;			// error message already displayed
					}
				}
			}
			tqsl_getErrorString();	// Clear error
			if (has_error && ignore_err)
				continue;
			break;
		} while (1);

 abortSigning:

		if (cancelled)
			cerr << "Signing cancelled" << endl;
		if (compressed)
			gzclose(gout);
		else
			out.close();
		if (tQSL_Error != TQSL_NO_ERROR) {
			check_tqsl_error(1);
		}
		if (cancelled)
			tqsl_converterRollBack(conv);
		else
			tqsl_converterCommit(conv);
		tqsl_endConverter(&conv);
	} catch(TQSLException& x) {
		if (compressed)
			gzclose(gout);
		else
			out.close();
		string msg = x.what();
		tqsl_getConverterLine(conv, &lineno);
		tqsl_converterRollBack(conv);
		tqsl_endConverter(&conv);
		if (lineno)
			msg += wxString::Format(wxT(" on line %d"), lineno).ToUTF8();
#ifdef _WIN32
		_wunlink(utf8_to_wchar(outfile.ToUTF8()));
#else
		unlink(outfile.ToUTF8());
#endif
		cerr << "Signing aborted due to errors" << endl;
		throw TQSLException(msg.c_str());
	}
	if (out_of_range > 0)
		cerr << wxString::Format(wxT("%s: %d QSO records were outside the selected date range"),
			infile.c_str(), out_of_range) << endl;
	if (duplicates > 0) {
		if (cancelled) {
			return cancelled;
		}
		cerr << wxString::Format(wxT("%s: %d QSO records were duplicates"),
			infile.c_str(), duplicates) << endl;
	}
	if (n == 0)
		cerr << "No records output" << endl;
	else
	   	cout << wxString::Format(wxT("%s: wrote %d records to %s"), infile.c_str(), n,
			outfile.c_str()) << endl;
	if (n > 0)
		cout << outfile << " is ready to be emailed or uploaded" << endl;
	else
#ifdef _WIN32
		_wunlink(utf8_to_wchar(outfile.ToUTF8()));
#else
		unlink(outfile.ToUTF8());
#endif
	return 0;
}

QSLApp::QSLApp() : wxAppConsole() {
	wxConfigBase::Set(new wxConfig(wxT("tqslapp")));
	DocPaths docpaths(wxT("tqslapp"));
}

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

bool
QSLApp::OnInit() {
	suppressdate = false;
	password = NULL;
	loc = NULL;
	action = TQSL_ACTION_NEW;

	wxCmdLineParser parser;

	static const wxCmdLineEntryDesc cmdLineDesc[] = {
	        { wxCMD_LINE_OPTION, wxT("a"), wxT("action"),	wxT("Specify dialog action - abort, all, compliant or ask") },
	        { wxCMD_LINE_SWITCH, wxT("d"), wxT("nodate"),	wxT("Suppress date range dialog") },
	        { wxCMD_LINE_OPTION, wxT("l"), wxT("location"),	wxT("Selects Station Location") },
	        { wxCMD_LINE_OPTION, wxT("o"), wxT("output"),	wxT("Output file name (defaults to input name minus extension plus .tq8") },
	        { wxCMD_LINE_SWITCH, wxT("u"), wxT("upload"),	wxT("Upload after signing instead of saving") },
	        { wxCMD_LINE_OPTION, wxT("p"), wxT("password"),	wxT("Passphrase for the signing key") },
	        { wxCMD_LINE_SWITCH, wxT("v"), wxT("version"),  wxT("Display the version information and exit") },
	        { wxCMD_LINE_SWITCH, wxT("h"), wxT("help"),	wxT("Display command line help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	        { wxCMD_LINE_PARAM,  NULL,     NULL,		wxT("Input ADIF or Cabrillo log file to sign"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	        { wxCMD_LINE_NONE }
	};

	parser.SetCmdLine(argc, argv);
	parser.SetDesc(cmdLineDesc);

	if (parser.Parse(true) != 0) return false; // exit if help or syntax error

	// print version and exit
	if (parser.Found(wxT("v"))) {
		cout << "TQSL Version" VERSION " " BUILD  << endl;
		return false;
	}

	wxString locname;
	if (parser.Found(wxT("l"), &locname)) {
		if (loc)
			tqsl_endStationLocationCapture(&loc);
		if (tqsl_getStationLocation(&loc, locname.ToUTF8())) {
			cerr << tqsl_getErrorString() << endl;
			return false;
		}
	}
	wxString pwd;
	if (parser.Found(wxT("p"), &pwd))
		password = strdup(pwd.ToUTF8());

	if (parser.Found(wxT("d")))
		suppressdate = true;

	parser.Found(wxT("o"), &outfile);

	wxString act;
	if (parser.Found(wxT("a"), &act)) {
		if (!act.CmpNoCase(wxT("abort"))) {
			action = TQSL_ACTION_ABORT;
		} else if (!act.CmpNoCase(wxT("compliant"))) {
			action = TQSL_ACTION_NEW;
		} else if (!act.CmpNoCase(wxT("all"))) {
			action = TQSL_ACTION_ALL;
		} else if (!act.CmpNoCase(wxT("ask"))) {
			action = TQSL_ACTION_ASK;
		} else {
			char tmp[100];
			strncpy(tmp, (const char *)act.ToUTF8(), sizeof tmp);
			tmp[sizeof tmp -1] = '\0';
			cerr << "The action parameter " << tmp << " is not recognized" << endl;
			exit(1);
		}
	}

	if (parser.GetParamCount() > 0) {
		infile = parser.GetParam(0);
		if (wxIsEmpty(infile)) {	// Nothing to sign
			cerr << "No logfile to sign!" << endl;
			return false;
		}
	} else {
		cerr << "No input file specified - nothing to sign, exiting." << endl;
	}
	return true;
}

int
QSLApp::OnRun() {
	if (loc == 0)
		return 1;
	wxString path, name, ext;
	wxSplitPath(infile, &path, &name, &ext);
	if (outfile.IsEmpty()) {
		if (!path.IsEmpty())
			path += wxT("/");
		path += name + wxT(".tq8");
		outfile = path;
	}
	try {
		if (ConvertLogFile(loc, infile, outfile, true, suppressdate, action, password))
			return 0;
	} catch(TQSLException& x) {
		wxString s;
		if (infile)
			s = infile + wxT(": ");
		s += wxString::FromUTF8(x.what());
		cerr << s << endl;
		return 1;
	}
	return 0;
}
