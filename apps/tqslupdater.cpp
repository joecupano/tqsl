/***************************************************************************
                                  tqslupdater.cpp
                             -------------------
    begin                : Fri 28 Apr 2017
    copyright            : (C) 2017 by the TrustedQSL Developers
    author               : Rick Murphy
    email                : k1mu@arrl.net
 ***************************************************************************/

#include <curl/curl.h> // has to be before something else in this list
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "tqsl_prefs.h"

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/wxchar.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/app.h>
#include <wx/stdpaths.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#ifdef _MSC_VER //could probably do a more generic check here...
// stdint exists on vs2012 and (I think) 2010, but not 2008 or its platform
  #define uint8_t unsigned char
#else
#include <stdint.h> //for uint8_t; should be cstdint but this is C++11 and not universally supported
#endif

#include <io.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslpaths.h"
#include "tqslapp.h"

static wchar_t* utf8_to_Wchar(const char* str);

using std::string;

/// GEOMETRY

#define LABEL_HEIGHT 20

static MyFrame *frame = 0;

static bool quiet = false;

CURL* curlReq = NULL;

/////////// Application //////////////

class QSLApp : public wxApp {
 public:
	QSLApp();
	virtual ~QSLApp();
	class MyFrame *GUIinit(bool checkUpdates, bool quiet = false);
	virtual bool OnInit();
};

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

IMPLEMENT_APP(QSLApp)

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_CLOSE(MyFrame::OnExit)
END_EVENT_TABLE()

void
MyFrame::OnExit(TQ_WXCLOSEEVENT& WXUNUSED(event)) {
	Destroy();		// close the window
}

void
MyFrame::DoExit(wxCommandEvent& WXUNUSED(event)) {
	Close();
	Destroy();
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates, bool quiet, wxLocale* loca)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)), locale(loca) {
	_quiet = quiet;

	// File menu
	file_menu = new wxMenu;
	file_menu->Append(tm_f_exit, _("E&xit TQSL\tAlt-X"));

	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, wxT("&File"));

	SetMenuBar(menu_bar);

	wxPanel* topPanel = new wxPanel(this);
	wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
	topPanel->SetSizer(topSizer);
}

static CURL*
tqsl_curl_init(const char *logTitle, const char *url, bool newFile) {
	curlReq = curl_easy_init();
	if (!curlReq) {
		return NULL;
	}
        DocPaths docpaths(wxT("tqslapp"));

	wxString filename;
	//set up options
	curl_easy_setopt(curlReq, CURLOPT_URL, url);
    curl_easy_setopt(curlReq, CURLOPT_SSL_VERIFYPEER, false);

	wxString exePath;
	wxFileName::SplitPath(wxStandardPaths::Get().GetExecutablePath(), &exePath, 0, 0);
	docpaths.Add(exePath);
	wxString caBundlePath = docpaths.FindAbsoluteValidPath(wxT("ca-bundle.crt"));
	if (!caBundlePath.IsEmpty()) {
		char caBundle[256];
		strncpy(caBundle, caBundlePath.ToUTF8(), sizeof caBundle);
		curl_easy_setopt(curlReq, CURLOPT_CAINFO, caBundle);
	}
	return curlReq;
}

wxString GetUpdatePlatformString() {
	wxString ret;
	#if defined(_WIN64)
		//this is 64 bit code already; if we are running we support 64
		ret = wxT("win64 win32");

	#else // this is not 64 bit code, but we are on windows
		// are we 64-bit compatible? if so prefer it
		BOOL val = false;
		typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fnIsWow64Process =
			(LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
		if (fnIsWow64Process != NULL) {
			fnIsWow64Process(GetCurrentProcess(), &val);
		}
		if(val) //32 bit running on 64 bit
			ret = wxT("win64 win32");
		else //just 32-bit only
			ret = wxT("win32");
	#endif

	return ret;
}

static size_t file_recv(void *ptr, size_t size, size_t nmemb, void *stream) {
	size_t left = nmemb * size;
	size_t written;

	while (left > 0) {
  		written = fwrite(ptr, size, nmemb, reinterpret_cast <FILE *>(stream));
		if (written == 0)
			return 0;
		left -= (written * size);
	}
	return nmemb * size;
}


void
MyFrame::UpdateTQSL(wxString& url) {
	curlReq = tqsl_curl_init("TQSL Update Download Log", (const char *)url.ToUTF8(), false);

	wxString filename;
	wxString temp;
	wxGetEnv(wxT("TEMP"), &temp);
	filename.Printf(wxT("%hs\\tqslupdate.msi"), temp);
	wchar_t* lfn = utf8_to_Wchar(filename.ToUTF8());
	FILE *updateFile = _wfopen(lfn, L"wb");
	free(lfn);
	if (!updateFile) {
		wxMessageBox(wxString::Format(wxT("Can't open TQSL update file %s: %hs"), filename.c_str(), strerror(errno)), wxT("Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &file_recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, updateFile);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(curlReq);
	if (retval == CURLE_OK) {
		if (fclose(updateFile)) {
			wxMessageBox(wxString::Format(wxT("Error writing new configuration file %s: %hs"), filename.c_str(), strerror(errno)), wxT("Error"), wxOK | wxICON_ERROR, this);
			return;
		}
		wxExecute(wxT("msiexec /i ") + filename, wxEXEC_ASYNC);
		wxExit();
	} else {
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxMessageBox(wxT("Unable to update - either your Internet connection is down or LoTW is unreachable.\nPlease try again later."), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxMessageBox(wxT("Unable to update. The network is down or the LoTW site is too busy\nPlease try again later."), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxMessageBox(wxT("Unable to connect to the update site.\nPlease try again later"), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else { // some other error
			wxString fmt = wxT("Error downloading new file:");
			fmt += wxT("\n%hs");
			wxMessageBox(wxString::Format(fmt, errorbuf), wxT("Update"), wxOK | wxICON_EXCLAMATION, this);
		}
	}
}

class FileUploadHandler {
 public:
	string s;
	FileUploadHandler(): s() { s.reserve(2000); }

	size_t internal_recv(char *ptr, size_t size, size_t nmemb) {
		s.append(ptr, size*nmemb);
		return size*nmemb;
	}

	static size_t recv(char *ptr, size_t size, size_t nmemb, void *userdata) {
		return (reinterpret_cast<FileUploadHandler*>(userdata))->internal_recv(ptr, size, nmemb);
	}
};

void
MyFrame::DoCheckForUpdates(bool silent, bool noGUI) {
	wxString updateURL = DEFAULT_UPD_URL;
	wxString ourPlatURL; //empty by default (we check against this later)

	curlReq = tqsl_curl_init("Version Check Log", (const char*)updateURL.ToUTF8(), true);

	//the following allow us to analyze our file

	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);
	curl_easy_setopt(curlReq, CURLOPT_CONNECTTIMEOUT, 120);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);

	int retval = curl_easy_perform(curlReq);
	if (retval == CURLE_OK) {
		wxString result = wxString::FromAscii(handler.s.c_str());
		wxString url;
		WX_DECLARE_STRING_HASH_MAP(wxString, URLHashMap);
		URLHashMap map;

		wxStringTokenizer urls(result, wxT("\n"));
		wxString onlinever;
		while(urls.HasMoreTokens()) {
			wxString header = urls.GetNextToken().Trim();
			if (header.StartsWith(wxT("TQSLVERSION;"), &onlinever)) {
				continue;
			} else if (header.IsEmpty()) {
				continue; //blank line
			} else if (header[0] == '#') {
				continue; //comments
			} else if (header.StartsWith(wxT("config.xml"), &onlinever)) {
				continue;
			} else {
				int sep = header.Find(';'); //; is invalid in URLs
				if (sep == wxNOT_FOUND) continue; //malformed string
				wxString plat = header.Left(sep);
				wxString url = header.Right(header.size()-sep-1);
				map[plat] = url;
			}
		}

		wxStringTokenizer plats(GetUpdatePlatformString(), wxT(" "));
		while(plats.HasMoreTokens()) {
			wxString tok = plats.GetNextToken();
			//see if this token is here
			if (map.count(tok)) { ourPlatURL=map[tok]; break; }
		}
	} else {
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxMessageBox(wxT("Unable to check for updates - either your Internet connection is down or LoTW is unreachable.\nPlease try again later."), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxMessageBox(wxT("Unable to check for updates. The network is down or the LoTW site is too busy.\nPlease try again later."), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxMessageBox(wxT("Unable to connect to the update site.\nPlease try again later."), wxT("Error"), wxOK | wxICON_ERROR, this);
		} else { // some other error
			wxString fmt = wxT("Error downloading new version information:");
			fmt += wxT("\n%hs");
			wxMessageBox(wxString::Format(fmt, errorbuf), wxT("Error"), wxOK | wxICON_ERROR, this);
		}
	}

	if (curlReq) curl_easy_cleanup(curlReq);
	curlReq = NULL;
	if (!ourPlatURL.empty()) {
		UpdateTQSL(ourPlatURL);
	}
	return;
}

QSLApp::QSLApp() : wxApp() {
}

bool
QSLApp::OnInit() {
	frame = 0;

	frame = new MyFrame(wxT("TQSL"), 0, 0, 0, 0, true, true, NULL);
	frame->Show(false);
	SetTopWindow(frame);
	frame->DoCheckForUpdates(false, false);
	frame->Close();
	return NULL;
}

static wchar_t*
utf8_to_Wchar(const char* str) {
	wchar_t* buffer;
	int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
	buffer = static_cast<wchar_t *>(malloc(needed*sizeof(wchar_t) + 4));
	if (!buffer)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, str, -1, &buffer[0], needed);
	return buffer;
}
