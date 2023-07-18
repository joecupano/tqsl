/***************************************************************************
                          tqsl_prefs.h  -  description
                             -------------------
    begin                : Sun Jan 05 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __TQSL_PREFS_H
#define __TQSL_PREFS_H

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wx/wxprec.h"
#include "wx/object.h"
#include "wx/config.h"
#include "wx/odcombo.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "wx/dialog.h"
#include "wx/notebook.h"
#include "wx/checkbox.h"
#include "wx/grid.h"
#include "wx/wxhtml.h"
#if defined(__APPLE__) || defined(_WIN32)
#include "wx/filepicker.h"
#endif

#include "tqslctrls.h"

#include <map>
#include <vector>

using std::map;
using std::pair;
using std::vector;

#define DEFAULT_CABRILLO_FILES wxT("log cbr")
#if !defined(__APPLE__) && !defined(_WIN32)
#define DEFAULT_ADIF_FILES wxT("adi adif ADI ADIF")
#else
#define DEFAULT_ADIF_FILES wxT("adi adif")
#endif
#define DEFAULT_AUTO_BACKUP true
#define DEFAULT_BACKUP_VERSIONS 10
#define DEFAULT_CERT_WARNING 60
#define DEFAULT_ADIF_EDIT false
#define DEFAULT_DISP_DUPES true
#define DEFAULT_IGNORE_SECONDS false
#define DEFAULT_LOG_TAB false
#define DEFAULT_CERTPWD false
//online
//#define ENABLE_ONLINE_PREFS
#define DEFAULT_UPL_URL wxT("https://lotw.arrl.org/lotw/upload")
#define DEFAULT_UPL_FIELD wxT("upfile")
#define DEFAULT_UPL_STATUSRE wxT("<!-- .UPL.\\s*([^-]+)\\s*-->")
#define DEFAULT_UPL_STATUSOK wxT("accepted")
#define DEFAULT_UPL_MESSAGERE wxT("<!-- .UPLMESSAGE.\\s*(.+)\\s*-->")
#define DEFAULT_UPL_VERIFYCA true

#define DEFAULT_UPD_URL wxT("https://lotw.arrl.org/lotw/tqslupdate")
#define DEFAULT_UPD_CONFIG_URL wxT("https://lotw.arrl.org/lotw/config_xml_version")
#define DEFAULT_CONFIG_FILE_URL wxT("https://lotw.arrl.org/lotw/config.tq6")

#define DEFAULT_CERT_CHECK_URL wxT("https://lotw.arrl.org/lotw/crl?serial=")
#define DEFAULT_CERT_CHECK_RE wxT(".*<Status>(.*)</Status>.*")

#define DEFAULT_LOTW_LOGIN_URL wxT("https://lotw.arrl.org/lotwuser/default")

enum {		// Window IDs
	ID_OK_BUT,
	ID_CAN_BUT,
	ID_HELP_BUT,
	ID_PREF_FILE_CABRILLO = (wxID_HIGHEST+100),
	ID_PREF_FILE_ADIF,
	ID_PREF_FILE_AUTO_BACKUP,
	ID_PREF_FILE_BACKUP,
	ID_PREF_FILE_BACKUP_VERSIONS,
	ID_PREF_FILE_BADCALLS,
	ID_PREF_FILE_DATERANGE,
	ID_PREF_FILE_EDIT_ADIF,
	ID_PREF_FILE_DISPLAY_DUPES,
	ID_PREF_FILE_LOG_TAB,
	ID_PREF_FILE_CERTPWD,
	ID_PREF_FILE_LOGVFY,
	ID_PREF_IGNORE_SECONDS,
	ID_PREF_MODE_MAP,
	ID_PREF_MODE_ADIF,
	ID_PREF_MODE_DELETE,
	ID_PREF_MODE_ADD,
	ID_PREF_ADD_ADIF,
	ID_PREF_ADD_MODES,
	ID_PREF_CAB_DELETE,
	ID_PREF_CAB_ADD,
	ID_PREF_CAB_EDIT,
	ID_PREF_CAB_MODEMAP,
	ID_PREF_ONLINE_DEFAULT,
	ID_PREF_ONLINE_URL,
	ID_PREF_ONLINE_FIELD,
	ID_PREF_ONLINE_STATUSRE,
	ID_PREF_ONLINE_STATUSOK,
	ID_PREF_ONLINE_MESSAGERE,
	ID_PREF_ONLINE_VERIFYCA,
	ID_PREF_ONLINE_UPD_CONFIGURL,
        ID_PREF_ONLINE_UPD_CONFIGFILE,
	ID_PREF_ONLINE_CERT_CHECK,
	ID_PREF_PROXY_ENABLED,
	ID_PREF_PROXY_HOST,
	ID_PREF_PROXY_PORT,
	ID_PREF_PROXY_TYPE
};

class PrefsPanel : public wxPanel {
 public:
	explicit PrefsPanel(wxWindow *parent, const wxString& helpfile = wxT("prefs.htm"))
		: wxPanel(parent), _helpfile(helpfile) {}
	wxString HelpFile() { return _helpfile; }
 private:
	wxString _helpfile;
};

class FilePrefs : public PrefsPanel {
 public:
	explicit FilePrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	void ShowHide();
 private:
	wxTextCtrl *versions;
	wxCheckBox *autobackup, *adifedit, *logtab, *certpwd;
#if !defined(__APPLE__) && !defined(_WIN32)
	wxTextCtrl *dirPick;
#else
	wxDirPickerCtrl *dirPick;
#endif
	DECLARE_EVENT_TABLE()
};

class LogPrefs : public PrefsPanel {
 public:
	explicit LogPrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	void ShowHide();
 private:
	wxTextCtrl *cabrillo, *adif;
	wxCheckBox *badcalls, *daterange, *dispdupes, *ignoresecs;
	wxRadioBox *handleQTH;
};

#if defined(ENABLE_ONLINE_PREFS)
class OnlinePrefs : public PrefsPanel {
 public:
	explicit OnlinePrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void ShowHide();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	DECLARE_EVENT_TABLE()
 private:
	wxTextCtrl *uploadURL, *postField, *statusRegex, *statusSuccess, *messageRegex;
	wxTextCtrl *updConfigURL, *configFileURL, *certCheckURL;
	wxCheckBox *verifyCA, *useDefaults;
	bool defaults;
};
#endif

typedef map <wxString, wxString> ModeSet;

class ModeMap : public PrefsPanel {
 public:
	explicit ModeMap(wxWindow *parent);
	virtual bool TransferDataFromWindow();
 private:
	void SetModeList();
	void OnDelete(wxCommandEvent &);
	void OnAdd(wxCommandEvent &);
	wxButton *delete_but;
	wxListBox *map;
	ModeSet modemap;
	DECLARE_EVENT_TABLE()
};

typedef map <wxString, pair <int, int> > ContestSet;

class ContestMap : public PrefsPanel {
 public:
	explicit ContestMap(wxWindow *parent);
	virtual bool TransferDataFromWindow();
 private:
	void SetContestList();
	void OnDelete(wxCommandEvent &);
	void OnAdd(wxCommandEvent &);
	void OnEdit(wxCommandEvent &);
	void Buttons();
	void DoUpdateInfo(wxCommandEvent &);

	wxButton *delete_but, *edit_but;
	wxGrid *grid;
	wxOwnerDrawnComboBox *dgmodes;
	vector <const char *> modes;
	ContestSet contestmap;
	DECLARE_EVENT_TABLE()
};

class ProxyPrefs : public PrefsPanel {
 public:
	explicit ProxyPrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void ShowHide();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	DECLARE_EVENT_TABLE()
 private:
	wxCheckBox *proxyEnabled;
	wxTextCtrl *proxyHost, *proxyPort;
	wxChoice *proxyType;
	bool enabled;
};

typedef map <wxString, wxString> ModeSet;
class Preferences : public wxFrame {
 public:
	explicit Preferences(wxWindow *parent, wxHtmlHelpController *help = 0);
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &);
	void OnHelp(wxCommandEvent &);
	void OnClose(wxCloseEvent&);
	DECLARE_EVENT_TABLE()
 private:
	wxNotebook *notebook;
	FilePrefs *fileprefs;
	LogPrefs *logprefs;
	ModeMap *modemap;
	ContestMap *contestmap;
	ProxyPrefs *proxyPrefs;
#if defined(ENABLE_ONLINE_PREFS)
	OnlinePrefs *onlinePrefs;
#endif
	wxHtmlHelpController *_help;
};

class AddMode : public wxDialog {
 public:
	explicit AddMode(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &) { Close(true); }
	wxString key, value;
	DECLARE_EVENT_TABLE()
 private:
	wxTextCtrl *adif;
	wxListBox *modelist;
};

class EditContest : public wxDialog {
 public:
	EditContest(wxWindow *parent, wxString ctype = _("Edit"), wxString _contest = wxT(""),
		int _contest_type = 0, int _callsign_field = 5);
	void OnOK(wxCommandEvent&);
	void OnCancel(wxCommandEvent &) { Close(true); }
	virtual bool TransferDataFromWindow();
	wxString contest;
	int contest_type, callsign_field;
 private:
	wxTextCtrl *name;
	wxRadioBox *type;
	wxTextCtrl *fieldnum;
	DECLARE_EVENT_TABLE()
};

#endif	// __TQSL_PREFS_H
