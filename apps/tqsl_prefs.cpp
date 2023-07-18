/***************************************************************************
                          tqsl_prefs.cpp  -  description
                             -------------------
    begin                : Mon Jul 1 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "tqsl_prefs.h"
#include <stdlib.h>
#include <algorithm>
#include <utility>
#include <curl/curl.h>

#include "wx/sizer.h"
#include "wx/button.h"
#include "wx/stattext.h"
#include "wx/statbox.h"
#include "wx/config.h"


#include "tqsllib.h"
#include "tqsltrace.h"
#include "tqslapp.h"
#include "winstrdefs.h"

#if wxMAJOR_VERSION == 3 && wxMINOR_VERSION > 0
#define WX31
#endif

using std::make_pair;

#if defined(__APPLE__)
  #define HEIGHT_ADJ(x) ((x)*4/2)
#else
  #define HEIGHT_ADJ(x) ((x)*3/2)
#endif

BEGIN_EVENT_TABLE(Preferences, wxFrame)
	EVT_BUTTON(ID_OK_BUT, Preferences::OnOK)
	EVT_BUTTON(ID_CAN_BUT, Preferences::OnCancel)
	EVT_BUTTON(ID_HELP_BUT, Preferences::OnHelp)
	EVT_MENU(wxID_EXIT, Preferences::OnOK)
	EVT_CLOSE(Preferences::OnClose)
END_EVENT_TABLE()

Preferences::Preferences(wxWindow *parent, wxHtmlHelpController *help)
	: wxFrame(parent, -1, wxString(_("Preferences"))), _help(help) {
	tqslTrace("Preferences::Preferences", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	notebook = new wxNotebook(this, -1);
//	topsizer->Add(notebook, 1, wxGROW);
	topsizer->Add(notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, 20);
	fileprefs = new FilePrefs(notebook);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_HELP_BUT, _("Help") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_OK_BUT, _("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, _("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	topsizer->Add(butsizer, 0, wxALIGN_CENTER);

	notebook->AddPage(fileprefs, _("Options"));

	logprefs = new LogPrefs(notebook);

	notebook->AddPage(logprefs, _("Log Handling"));

	modemap = new ModeMap(notebook);
	notebook->AddPage(modemap, _("ADIF Modes"));

	contestmap = new ContestMap(notebook);
	notebook->AddPage(contestmap, _("Cabrillo Specs"));

	proxyPrefs = new ProxyPrefs(notebook);
	notebook->AddPage(proxyPrefs, _("Network Proxy"));
	//don't let the user play with these
#if defined(ENABLE_ONLINE_PREFS)
	onlinePrefs = new OnlinePrefs(notebook);
	notebook->AddPage(onlinePrefs, wxT("Server Setup"));
#endif

#ifdef __WXMAC__
	// You can't have a toplevel window without a menubar.
	wxMenu *file_menu = new wxMenu;
	file_menu->Append(wxID_EXIT, _("Close"));
	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, _("&File"));
	SetMenuBar(menu_bar);
#endif

	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	SetWindowStyle(GetWindowStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
	CenterOnParent();
}

void Preferences::OnOK(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("Preferences::OnOK", NULL);
#if defined(ENABLE_ONLINE_PREFS)
	if (!onlinePrefs->TransferDataFromWindow())
		return;
#endif
	if (!proxyPrefs->TransferDataFromWindow())
		return;
	if (!logprefs->TransferDataFromWindow())
		return;
	if (!fileprefs->TransferDataFromWindow())
		return;
	(reinterpret_cast<MyFrame *>(GetParent()))->file_menu->Enable(tm_f_preferences, true);
	Destroy();
}

void Preferences::OnCancel(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("Preferences::OnOK", NULL);

	(reinterpret_cast<MyFrame *>(GetParent()))->file_menu->Enable(tm_f_preferences, true);
	Destroy();
}

void Preferences::OnClose(wxCloseEvent& WXUNUSED(event)) {
	tqslTrace("Preferences::OnClose", NULL);

	(reinterpret_cast<MyFrame *>(GetParent()))->file_menu->Enable(tm_f_preferences, true);
	Destroy();
}

void Preferences::OnHelp(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("Preferences::OnHelp", NULL);
	if (_help) {
		wxString file(wxT("pref.htm"));
		int idx = notebook->GetSelection();
		if (idx >= 0)
			file = (reinterpret_cast<PrefsPanel *>(notebook->GetPage(idx)))->HelpFile();
		_help->Display(file);
	}
}

BEGIN_EVENT_TABLE(ModeMap, PrefsPanel)
	EVT_BUTTON(ID_PREF_MODE_DELETE, ModeMap::OnDelete)
	EVT_BUTTON(ID_PREF_MODE_ADD, ModeMap::OnAdd)
END_EVENT_TABLE()

#define MODE_TEXT_WIDTH 15

ModeMap::ModeMap(wxWindow *parent) : PrefsPanel(parent, wxT("pref-adi.htm")) {
	tqslTrace("ModeMap::ModeMap", "parent=0x%lx", reinterpret_cast<void *>(parent));
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, _("Custom ADIF mode mappings:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	map = new wxListBox(this, ID_PREF_MODE_MAP, wxDefaultPosition, wxSize(char_width, (char_height*10)));
	hsizer->Add(map, 1, wxEXPAND, 0);

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

	vsizer->Add(new wxButton(this, ID_PREF_MODE_ADD, _("Add...")), 0, wxBOTTOM, 10);
	delete_but = new wxButton(this, ID_PREF_MODE_DELETE, _("Delete"));
	vsizer->Add(delete_but, 0);

	hsizer->Add(vsizer, 0, wxLEFT, 10);

	sizer->Add(hsizer, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 10);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);

	SetModeList();
}

void ModeMap::SetModeList() {
	tqslTrace("ModeMap::SetModeList", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString key, value;
	long cookie;

	modemap.clear();
	map->Clear();
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		key.Replace(wxT("!SLASH!"), wxT("/"), true); // Fix slashes in modes
		modemap.insert(make_pair(key, value));
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
	for (ModeSet::iterator it = modemap.begin(); it != modemap.end(); it++) {
		map->Append(it->first + wxT(" -> ") + it->second, reinterpret_cast<void *>(const_cast<wxString *>(&it->first)));
	}
	if (map->GetCount() > 0)
		map->SetSelection(0);
	delete_but->Enable(map->GetSelection() >= 0);
}

void ModeMap::OnDelete(wxCommandEvent &) {
	tqslTrace("ModeMap::OnDelete", NULL);
	int sel = map->GetSelection();
	if (sel >= 0) {
		wxString* keystr = reinterpret_cast<wxString*>(map->GetClientData(sel));
		keystr->Replace(wxT("/"), wxT("!SLASH!"), true); // Fix slashes in modes
		if (!keystr->IsEmpty()) {
			wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
			config->SetPath(wxT("/modeMap"));
			config->DeleteEntry(*keystr, true);
			config->Flush(false);
			SetModeList();
		}
	}
}

void ModeMap::OnAdd(wxCommandEvent &) {
	tqslTrace("ModeMap::OnAdd", NULL);
	AddMode add_dial(this);
	int val = add_dial.ShowModal();
	if (val == ID_OK_BUT && !add_dial.key.IsEmpty() && !add_dial.value.IsEmpty()) {
		wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
		config->SetPath(wxT("/modeMap"));
		wxString key(add_dial.key);
		key.Replace(wxT("/"), wxT("!SLASH!"), true); // Fix slashes in modes
		config->Write(key, add_dial.value);
		config->Flush(false);
		SetModeList();
	}
}

bool ModeMap::TransferDataFromWindow() {
	tqslTrace("ModeMap::TransferDataFromWindow", NULL);
	return true;
}

BEGIN_EVENT_TABLE(AddMode, wxDialog)
	EVT_BUTTON(ID_OK_BUT, AddMode::OnOK)
	EVT_BUTTON(ID_CAN_BUT, AddMode::OnCancel)
END_EVENT_TABLE()

AddMode::AddMode(wxWindow *parent) : wxDialog(parent, -1, wxString(_("Add ADIF mode"))) {
	tqslTrace("AddMode::AddMode", "parent=0x%lx", reinterpret_cast<void *>(parent));
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, _("Add ADIF mode mapping:")), 0, wxALL, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	hsizer->Add(new wxStaticText(this, -1, _("ADIF Mode:")), 0, wxALIGN_CENTER_VERTICAL);

	adif = new wxTextCtrl(this, ID_PREF_ADD_ADIF, wxT(""), wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	hsizer->Add(adif, 0, wxLEFT, 5);

	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, _("Resulting TQSL mode:")), 0, wxLEFT|wxRIGHT, 10);

	modelist = new wxListBox(this, ID_PREF_ADD_MODES, wxDefaultPosition, wxSize(char_width, (char_height*10)));
	sizer->Add(modelist, 0, wxLEFT|wxRIGHT, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, _("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, _("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	sizer->Add(butsizer, 0, wxALIGN_CENTER);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();

	int n;
	if (tqsl_getNumMode(&n) == 0) {
		for (int i = 0; i < n; i++) {
			const char *modestr;
			if (tqsl_getMode(i, &modestr, 0) == 0) {
				modelist->Append(wxString::FromUTF8(modestr));
			}
		}
	}
}

void AddMode::OnOK(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("AddMode::OnOK", NULL);
	if (!TransferDataFromWindow()) return;
	key = adif->GetValue().Trim(true).Trim(false).MakeUpper();
	int sel = modelist->GetSelection();
	if (sel >= 0)
		value = modelist->GetString(sel);
	EndModal(ID_OK_BUT);
}

bool AddMode::TransferDataFromWindow() {
	tqslTrace("AddMode::TransferDataFromWindow", NULL);
	key = adif->GetValue().Trim(true).Trim(false).MakeUpper();
	if (key.IsEmpty()) return true;
	if (modelist->FindString(key) != wxNOT_FOUND) {	// This duplicates an existing mode
		wxMessageBox(wxString::Format(_("This mode definition conflicts with a standard mode definition for %s"),
				key.c_str()), _("Mode Conflict"), wxOK | wxICON_ERROR, this);
		return false;
	}
	return true;
}

#define FILE_TEXT_WIDTH 30

BEGIN_EVENT_TABLE(FilePrefs, PrefsPanel)
	EVT_CHECKBOX(ID_PREF_FILE_AUTO_BACKUP, FilePrefs::OnShowHide)
END_EVENT_TABLE()

FilePrefs::FilePrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	tqslTrace("FilePrefs::FilePrefs", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	bool ab;
	config->Read(wxT("AutoBackup"), &ab, DEFAULT_AUTO_BACKUP);
	autobackup = new wxCheckBox(this, ID_PREF_FILE_AUTO_BACKUP, _("Allow automatic configuration backup"));
	autobackup->SetValue(ab);
	sizer->Add(autobackup, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	sizer->Add(new wxStaticText(this, -1, _("Backup File Folder:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString bdir = config->Read(wxT("BackupFolder"));
#if !defined(__APPLE__) && !defined(_WIN32)
	dirPick = new wxTextCtrl(this, ID_PREF_FILE_BACKUP, bdir, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
#else
	dirPick = new wxDirPickerCtrl(this, ID_PREF_FILE_BACKUP, bdir, _("Select a Folder"), wxDefaultPosition,
		wxSize(char_width, HEIGHT_ADJ(char_height)), wxDIRP_USE_TEXTCTRL);
#endif
	dirPick->Enable(ab);
	sizer->Add(dirPick, 0, wxEXPAND|wxLEFT|wxRIGHT, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("Number of Backups to retain:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	int bver;
	config->Read(wxT("BackupVersions"), &bver, DEFAULT_BACKUP_VERSIONS);

	versions = new wxTextCtrl(this, ID_PREF_FILE_BACKUP_VERSIONS, wxString::Format(wxT("%d"), bver),
		wxDefaultPosition, wxSize(char_width / (FILE_TEXT_WIDTH / 3), HEIGHT_ADJ(char_height)));
	hsizer->Add(versions, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	sizer->Add(hsizer, 0, wxALL|wxALIGN_LEFT, 10);

	adifedit = new wxCheckBox(this, ID_PREF_FILE_EDIT_ADIF, _("Open ADIF files in ADIF editor"));

	bool allow;
	config->Read(wxT("AdifEdit"), &allow, DEFAULT_ADIF_EDIT);
	adifedit->SetValue(allow);
	sizer->Add(adifedit, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	logtab = new wxCheckBox(this, ID_PREF_FILE_LOG_TAB, _("Display status messages in separate tab"));
	config->Read(wxT("LogTab"), &allow, DEFAULT_LOG_TAB);
	logtab->SetValue(allow);
	sizer->Add(logtab, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	certpwd = new wxCheckBox(this, ID_PREF_FILE_CERTPWD, _("Enable passphrases for Callsign Certificates"));
	bool cp;
	config->Read(wxT("CertPwd"), &cp, DEFAULT_CERTPWD);
	certpwd->SetValue(cp);
	sizer->Add(certpwd, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

void
FilePrefs::ShowHide() {
	dirPick->Enable(autobackup->GetValue());
}

static wxString
fix_ext_str(const wxString& oldexts) {
	static const char *delims = ".,;: ";

	char *str = new char[oldexts.Length() + 1];
	strncpy(str, oldexts.ToUTF8(), oldexts.Length() + 1);
	wxString exts;
	char *tok = strtok(str, delims);
	while (tok) {
		if (!exts.IsEmpty())
			exts += wxT(" ");
		exts += wxString::FromUTF8(tok);
		tok = strtok(NULL, delims);
	}
	return exts;
}

bool FilePrefs::TransferDataFromWindow() {
	tqslTrace("FilePrefs::TransferDataFromWindow", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/"));
	config->Write(wxT("AdifEdit"), adifedit->GetValue());
	config->Write(wxT("CertPwd"), certpwd->GetValue());

	bool oldLog;
	config->Read(wxT("LogTab"), &oldLog, DEFAULT_LOG_TAB);
	if (logtab->GetValue() != oldLog) {
		wxMessageBox(_("Changes to the status message configuration will take affect when TQSL is restarted"), _("Warning"), wxOK | wxICON_INFORMATION, this);
	}
	config->Write(wxT("LogTab"), logtab->GetValue());
	config->Write(wxT("AutoBackup"), autobackup->GetValue());
#if !defined(__APPLE__) && !defined(_WIN32)
	config->Write(wxT("BackupFolder"), dirPick->GetValue());
#else
	config->Write(wxT("BackupFolder"), dirPick->GetPath());
#endif
	long vers = 0;
	vers = strtol(versions->GetValue().ToUTF8(), NULL, 10);
	if (vers <= 0)
		vers = DEFAULT_BACKUP_VERSIONS;
	config->Write(wxT("BackupVersions"), vers);
	return true;
}

LogPrefs::LogPrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	tqslTrace("LogPrefs::LogPrefs", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, _("Cabrillo file extensions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString cab = config->Read(wxT("CabrilloFiles"), DEFAULT_CABRILLO_FILES);
	cabrillo = new wxTextCtrl(this, ID_PREF_FILE_CABRILLO, cab, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(cabrillo, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, _("ADIF file extensions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString adi = config->Read(wxT("ADIFFiles"), DEFAULT_ADIF_FILES);
	adif = new wxTextCtrl(this, ID_PREF_FILE_ADIF, adi, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(adif, 0, wxLEFT|wxRIGHT, 10);

	badcalls = new wxCheckBox(this, ID_PREF_FILE_BADCALLS, _("Allow nonamateur call signs"));
	bool allow;
	config->Read(wxT("BadCalls"), &allow, false);
	badcalls->SetValue(allow);
	sizer->Add(badcalls, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	daterange = new wxCheckBox(this, ID_PREF_FILE_BADCALLS, _("Prompt for QSO Date range when signing"));
	config->Read(wxT("DateRange"), &allow, true);
	daterange->SetValue(allow);
	sizer->Add(daterange, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	dispdupes = new wxCheckBox(this, ID_PREF_FILE_DISPLAY_DUPES, _("Display details of already uploaded QSOs when signing a log"));
	config->Read(wxT("DispDupes"), &allow, DEFAULT_DISP_DUPES);
	dispdupes->SetValue(allow);
	sizer->Add(dispdupes, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	ignoresecs = new wxCheckBox(this, ID_PREF_IGNORE_SECONDS, _("Ignore seconds in QSO times"));
	config->Read(wxT("IgnoreSeconds"), &allow, DEFAULT_IGNORE_SECONDS);
	ignoresecs->SetValue(allow);
	sizer->Add(ignoresecs, 0, wxLEFT|wxRIGHT|wxTOP, 10);

	int logverify;
        config->Read(wxT("LogVerify"), &logverify, TQSL_LOC_REPORT);
	if (logverify != TQSL_LOC_IGNORE && logverify != TQSL_LOC_REPORT && logverify != TQSL_LOC_UPDATE) {
                        logverify = TQSL_LOC_REPORT;
	}

	static wxString choices[] = { _("Ignore QTH details from your log"), _("Report on QTH differences") , _("Override Station Location with QTH details from your log") };

	handleQTH = new wxRadioBox(this, -1, _("Handle QTH information in ADIF logs with what action?"), wxDefaultPosition, wxDefaultSize,
		3, choices, 3, wxRA_SPECIFY_ROWS);
	sizer->Add(handleQTH, 0, wxALL|wxEXPAND, 10);
	handleQTH->SetSelection(logverify);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

bool LogPrefs::TransferDataFromWindow() {
	tqslTrace("LogPrefs::TransferDataFromWindow", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/"));
	config->Write(wxT("CabrilloFiles"), fix_ext_str(cabrillo->GetValue()));
	config->Write(wxT("ADIFFiles"), fix_ext_str(adif->GetValue()));
	config->Write(wxT("BadCalls"), badcalls->GetValue());
	config->Write(wxT("DateRange"), daterange->GetValue());
	config->Write(wxT("DispDupes"), dispdupes->GetValue());
	config->Write(wxT("IgnoreSeconds"), ignoresecs->GetValue());
	config->Write(wxT("LogVerify"), handleQTH->GetSelection());

	return true;
}

#if defined(ENABLE_ONLINE_PREFS)
BEGIN_EVENT_TABLE(OnlinePrefs, PrefsPanel)
	EVT_CHECKBOX(ID_PREF_ONLINE_DEFAULT, OnlinePrefs::OnShowHide)
END_EVENT_TABLE()

OnlinePrefs::OnlinePrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	tqslTrace("OnlinePrefs::OnlinePrefs", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/LogUpload"));
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);

	wxString uplURL = config->Read(wxT("UploadURL"), DEFAULT_UPL_URL);
	wxString uplPOST = config->Read(wxT("PostField"), DEFAULT_UPL_FIELD);
	wxString uplStatusRE = config->Read(wxT("StatusRegex"), DEFAULT_UPL_STATUSRE);
	wxString uplStatOK = config->Read(wxT("StatusSuccess"), DEFAULT_UPL_STATUSOK);
	wxString uplMsgRE = config->Read(wxT("MessageRegex"), DEFAULT_UPL_MESSAGERE);
	wxString cfgUpdURL = config->Read(wxT("ConfigFileVerURL"), DEFAULT_UPD_CONFIG_URL);
	wxString cfgFileUpdURL = config->Read(wxT("NewConfigURL"), DEFAULT_CONFIG_FILE_URL);
	wxString certCheckUpdURL = config->Read(wxT("CertCheckURL"), DEFAULT_CERT_CHECK_URL);

	bool uplVerifyCA;
	config->Read(wxT("VerifyCA"), &uplVerifyCA, DEFAULT_UPL_VERIFYCA);

	defaults = (
		(uplURL == DEFAULT_UPL_URL) &&
		(uplPOST == DEFAULT_UPL_FIELD) &&
		(uplStatusRE == DEFAULT_UPL_STATUSRE) &&
		(uplStatOK == DEFAULT_UPL_STATUSOK) &&
		(uplMsgRE == DEFAULT_UPL_MESSAGERE) &&
		(uplVerifyCA == DEFAULT_UPL_VERIFYCA) &&
		(cfgUpdURL == DEFAULT_UPD_CONFIG_URL) &&
		(cfgFileUpdURL == DEFAULT_CONFIG_FILE_URL) &&
		(certCheckUpdURL == DEFAULT_CERT_CHECK_URL));

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	useDefaults = new wxCheckBox(this, ID_PREF_ONLINE_DEFAULT, wxT("Use Defaults"));
	useDefaults->SetValue(defaults);
	sizer->Add(useDefaults, 0, wxTop|wxCENTER|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Upload URL:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);

	uploadURL = new wxTextCtrl(this, ID_PREF_ONLINE_URL, uplURL, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(uploadURL, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("HTTP POST Field:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);

	postField = new wxTextCtrl(this, ID_PREF_ONLINE_FIELD, uplPOST, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(postField, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Status RegEx:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	statusRegex = new wxTextCtrl(this, ID_PREF_ONLINE_STATUSRE, uplStatusRE, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(statusRegex, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Successful Status Message:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	statusSuccess = new wxTextCtrl(this, ID_PREF_ONLINE_STATUSOK, uplStatOK, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(statusSuccess, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Message RegEx:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	messageRegex = new wxTextCtrl(this, ID_PREF_ONLINE_MESSAGERE, uplMsgRE, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(messageRegex, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Config File Version URL:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	updConfigURL = new wxTextCtrl(this, ID_PREF_ONLINE_UPD_CONFIGURL, cfgUpdURL, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(updConfigURL, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("New Config File URL:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	configFileURL = new wxTextCtrl(this, ID_PREF_ONLINE_UPD_CONFIGFILE, cfgFileUpdURL, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(configFileURL, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Certificate Status Check URL:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	certCheckURL = new wxTextCtrl(this, ID_PREF_ONLINE_CERT_CHECK, certCheckUpdURL, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(certCheckURL, 0, wxLEFT|wxRIGHT, 10);

	verifyCA = new wxCheckBox(this, ID_PREF_ONLINE_VERIFYCA, wxT("Verify server certificate"));
	verifyCA->SetValue(uplVerifyCA);
	sizer->Add(verifyCA, 0, wxLEFT|wxRIGHT|wxTOP|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);

	config->SetPath(wxT("/"));

	SetSizer(sizer);

	sizer->Fit(this);
	sizer->SetSizeHints(this);
	ShowHide();
}

void OnlinePrefs::ShowHide() {
	tqslTrace("OnlinePrefs::ShowHide", NULL);
	defaults = useDefaults->GetValue();
	for (int i = 1; i < 16; i++) GetSizer()->Show(i, !defaults); //16 items in sizer; hide all but checkbox

	Layout();
  //wxNotebook caches best size
	GetParent()->InvalidateBestSize();
	GetParent()->Fit();
	GetGrandParent()->Fit();
}

bool OnlinePrefs::TransferDataFromWindow() {
	tqslTrace("OnlinePrefs::TransferDataFromWindow", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());

	if (defaults) {
		config->DeleteGroup(wxT("/LogUpload"));
	} else {
		config->SetPath(wxT("/LogUpload"));
		config->Write(wxT("UploadURL"), uploadURL->GetValue());
		config->Write(wxT("PostField"), postField->GetValue());
		config->Write(wxT("StatusRegex"), statusRegex->GetValue());
		config->Write(wxT("StatusSuccess"), statusSuccess->GetValue());
		config->Write(wxT("MessageRegex"), messageRegex->GetValue());
		config->Write(wxT("VerifyCA"), verifyCA->GetValue());
		config->Write(wxT("ConfigFileVerURL"), updConfigURL->GetValue());
		config->Write(wxT("NewCOnfigURL"), updConfigURL->GetValue());
		config->SetPath(wxT("/"));
	}

	return true;
}
#endif // ENABLE_ONLINE_PREFS

BEGIN_EVENT_TABLE(ProxyPrefs, PrefsPanel)
	EVT_CHECKBOX(ID_PREF_PROXY_ENABLED, ProxyPrefs::OnShowHide)
END_EVENT_TABLE()

ProxyPrefs::ProxyPrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	tqslTrace("ProxyPrefs::ProxyPrefs", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/Proxy"));
	SetAutoLayout(true);

	wxArrayString ptypes;
	ptypes.Add(wxT("HTTP"));
	ptypes.Add(wxT("Socks4"));
	ptypes.Add(wxT("Socks5"));

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);

	config->Read(wxT("ProxyEnabled"), &enabled, false);
	wxString pHost = config->Read(wxT("proxyHost"));
	wxString pPort = config->Read(wxT("proxyPort"));
	wxString pType = config->Read(wxT("proxyType"));

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxString msg = wxT("\n");
		msg += _("Use these settings to configure a network proxy for Internet uploads and downloads. You should only enable a proxy if directed by your network administrator.");
		msg += wxT("\n");
		msg += _("Incorrect settings can cause TQSL to be unable to upload logs or check for updates.");
	wxStaticText *st = new wxStaticText(this, -1, msg, wxDefaultPosition, wxSize(char_width, char_height *8));
	sizer->Add(st);

	st->Wrap(char_width * 60);

	proxyEnabled = new wxCheckBox(this, ID_PREF_PROXY_ENABLED, _("Enable a Network Proxy"));
	proxyEnabled->SetValue(enabled);
	sizer->Add(proxyEnabled, 0, wxTop|wxCENTER|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, _("Proxy Address:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	proxyHost = new wxTextCtrl(this, ID_PREF_PROXY_HOST, pHost, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(proxyHost, 0, wxLEFT|wxRIGHT, 10);
	proxyHost->Enable(enabled);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("Port Number:")), 0, wxTOP|wxALIGN_LEFT, 4);

	proxyPort = new wxTextCtrl(this, ID_PREF_PROXY_PORT, pPort, wxPoint(0, 0),
		wxSize(char_width/6, HEIGHT_ADJ(char_height)));
	hsizer->Add(proxyPort, 0, wxLEFT|wxRIGHT|wxALIGN_BOTTOM, 0);
	proxyPort->Enable(enabled);

	hsizer->Add(new wxStaticText(this, -1, _("Proxy Type:")), 0, wxTOP|wxALIGN_LEFT, 4);

	proxyType = new wxChoice(this, ID_PREF_PROXY_TYPE, wxPoint(0, 0),
		wxSize(char_width/4, HEIGHT_ADJ(char_height)), ptypes, 0, wxDefaultValidator, _("ProxyType"));
	hsizer->Add(proxyType, 0, wxALIGN_BOTTOM, 0);
	proxyType->Enable(enabled);
	proxyType->SetStringSelection(pType);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_LEFT, 10);

	config->SetPath(wxT("/"));

	SetSizer(sizer);

	sizer->Fit(this);
	sizer->SetSizeHints(this);
	ShowHide();
}

void ProxyPrefs::ShowHide() {
	tqslTrace("ProxyPrefs::ShowHide", NULL);

	enabled = proxyEnabled->GetValue();
	proxyHost->Enable(enabled);
	proxyPort->Enable(enabled);
	proxyType->Enable(enabled);
	for (int i = 2; i < 5; i++) GetSizer()->Show(i, enabled); // 5 items in sizer; hide all but warning and checkbox

	Layout();
  //wxNotebook caches best size
	GetParent()->InvalidateBestSize();
	GetParent()->Fit();
	GetGrandParent()->Fit();
}

bool ProxyPrefs::TransferDataFromWindow() {
	tqslTrace("ProxyPrefs::TransferDataFromWindow", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());

	config->SetPath(wxT("/Proxy"));
	config->Write(wxT("ProxyEnabled"), enabled);
	config->Write(wxT("ProxyHost"), proxyHost->GetValue());
	config->Write(wxT("ProxyPort"), proxyPort->GetValue());
	config->Write(wxT("ProxyType"), proxyType->GetStringSelection());
	config->SetPath(wxT("/"));

	return true;
}

BEGIN_EVENT_TABLE(ContestMap, PrefsPanel)
	EVT_BUTTON(ID_PREF_CAB_DELETE, ContestMap::OnDelete)
	EVT_BUTTON(ID_PREF_CAB_ADD, ContestMap::OnAdd)
	EVT_BUTTON(ID_PREF_CAB_EDIT, ContestMap::OnEdit)
	EVT_COMBOBOX(ID_PREF_CAB_MODEMAP, ContestMap::DoUpdateInfo)
END_EVENT_TABLE()

ContestMap::ContestMap(wxWindow *parent) : PrefsPanel(parent, wxT("pref-cab.htm")) {
	tqslTrace("ContestMap::ContestMap", "parent=0x%lx", parent);
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(new wxStaticText(this, -1, _("Cabrillo CONTEST definitions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	wxBoxSizer *subsizer = new wxBoxSizer(wxHORIZONTAL);
	grid = new wxGrid(this, -1, wxDefaultPosition, wxDefaultSize);

	grid->CreateGrid(10, 3);
#ifndef WX31
	grid->SetLabelSize(wxHORIZONTAL, HEIGHT_ADJ(char_height));
	grid->SetLabelSize(wxVERTICAL, 0);
	grid->SetColumnWidth(0, char_width*15);
	grid->SetColumnWidth(1, char_width*4);
	grid->SetColumnWidth(2, char_width*5);
	grid->SetLabelValue(wxHORIZONTAL, _("CONTEST"), 0);
	grid->SetLabelValue(wxHORIZONTAL, _("Type"), 1);
	grid->SetLabelValue(wxHORIZONTAL, _("Field"), 2);
	grid->SetEditable(false);
	grid->SetDividerPen(wxNullPen);		// No replacement in wx3.1?
#else
	grid->SetColLabelSize(HEIGHT_ADJ(char_height));
	grid->SetRowLabelSize(0);
	grid->SetColSize(0, char_width*15);
	grid->SetColSize(1, char_width*4);
	grid->SetColSize(2, char_width*5);
	grid->SetColLabelValue(0, _("CONTEST"));
	grid->SetColLabelValue(1, _("Type"));
	grid->SetColLabelValue(2, _("Field"));
	grid->EnableEditing(false);
#endif

#ifndef WX31
	grid->SetSize(1, grid->GetRowHeight(0) * grid->GetRows());
#else
	grid->SetSize(1, grid->GetRowHeight(0) * grid->GetNumberRows());
#endif
	subsizer->Add(grid, 1, wxLEFT|wxRIGHT|wxEXPAND, 10);

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);
	vsizer->Add(new wxButton(this, ID_PREF_CAB_ADD, _("Add...")), 0, wxBOTTOM, 10);
	edit_but = new wxButton(this, ID_PREF_CAB_EDIT, _("Edit..."));
	vsizer->Add(edit_but, 0, wxBOTTOM, 10);
	delete_but = new wxButton(this, ID_PREF_CAB_DELETE, _("Delete"));
	vsizer->Add(delete_but, 0);

	subsizer->Add(vsizer, 0, wxRIGHT, 10);

	sizer->Add(subsizer, 1, wxBOTTOM|wxEXPAND, 10);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);

	SetContestList();
}

void ContestMap::Buttons() {
#if wxMAJOR_VERSION > 2
	bool editable = grid->GetGridCursorRow() >= 0;
#else
	bool editable = grid->GetCursorRow() >= 0;
#endif
	delete_but->Enable(editable);
	edit_but->Enable(editable);
}

void ContestMap::SetContestList() {
	tqslTrace("ContestMap::SetContestList", NULL);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString key, value;
	long cookie;

	contestmap.clear();
#ifndef WX31
	if (grid->GetRows() > 0)
		grid->DeleteRows(0, grid->GetRows());
#else
	if (grid->GetNumberRows() > 0)
		grid->DeleteRows(0, grid->GetNumberRows());
#endif

	config->SetPath(wxT("/cabrilloMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		int contest_type = strtol(value.ToUTF8(), NULL, 10);
		int fieldnum = strtol(value.AfterFirst(wxT(';')).ToUTF8(), NULL, 10);
		contestmap.insert(make_pair(key, make_pair(contest_type, fieldnum)));
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
	int vsize = contestmap.size();
//	if (vsize < 10)
//		vsize = 10;
	if (vsize)
		grid->AppendRows(vsize);
	int idx = 0;
	for (ContestSet::iterator it = contestmap.begin(); it != contestmap.end(); it++) {
#ifndef WX31
		grid->SetCellValue(it->first, idx, 0);
		grid->SetCellValue(it->second.first == 1 ? wxT("VHF") : wxT("HF"), idx, 1);
		grid->SetCellValue(wxString::Format(wxT("%d"), it->second.second), idx, 2);
#else
		grid->SetCellValue(idx, 0, it->first);
		grid->SetCellValue(idx, 1, it->second.first == 1 ? wxT("VHF") : wxT("HF"));
		grid->SetCellValue(idx, 2, wxString::Format(wxT("%d"), it->second.second));
#endif
		++idx;
	}
	config->SetPath(wxT("/"));
	Buttons();
}

void ContestMap::DoUpdateInfo(wxCommandEvent&) {
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
        int sel = dgmodes->GetSelection();
	if (sel != wxNOT_FOUND) {
		const char* mapped = modes[sel];
		config->Write(wxT("CabrilloDGMap"), wxString::FromUTF8(mapped));
		config->Flush(false);
	}
	return;
}

bool ContestMap::TransferDataFromWindow() {
	tqslTrace("ContestMap::TransferDataFromWindow", NULL);
	return true;
}

void ContestMap::OnDelete(wxCommandEvent &) {
	tqslTrace("ContestMap::OnDelete", NULL);
#if wxMAJOR_VERSION > 2
	int row = grid->GetGridCursorRow();
#else
	int row = grid->GetCursorRow();
#endif
	if (row >= 0) {
		wxString contest = grid->GetCellValue(row, 0);
		if (!contest.IsEmpty()) {
			wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
			config->SetPath(wxT("/cabrilloMap"));
			config->DeleteEntry(contest, true);
			config->Flush(false);
			SetContestList();
		}
	}
}

void ContestMap::OnAdd(wxCommandEvent &) {
	tqslTrace("ContestMap::OnAdd", NULL);
	EditContest dial(this, wxT("Add"), wxT(""), 0, TQSL_DEF_CABRILLO_MAP_FIELD);
	if (dial.ShowModal() == ID_OK_BUT) {
		wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
		config->SetPath(wxT("/cabrilloMap"));
		config->Write(dial.contest, wxString::Format(wxT("%d;%d"), dial.contest_type, dial.callsign_field));
		config->Flush(false);
		SetContestList();
	}
}

void ContestMap::OnEdit(wxCommandEvent &) {
	tqslTrace("ContestMap::OnEdit", NULL);
	wxString contest;
	int contest_type = 0, callsign_field = TQSL_DEF_CABRILLO_MAP_FIELD;
#if wxMAJOR_VERSION > 2
	int row = grid->GetGridCursorRow();
#else
	int row = grid->GetCursorRow();
#endif
	if (row >= 0) {
		contest = grid->GetCellValue(row, 0);
		if (!contest.IsEmpty()) {
			wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
			config->SetPath(wxT("/cabrilloMap"));
			wxString val;
			if (config->Read(contest, &val)) {
				contest_type = strtol(val.ToUTF8(), NULL, 10);
				callsign_field = strtol(val.AfterFirst(wxT(';')).ToUTF8(), NULL, 10);
			}
			config->SetPath(wxT("/"));
		}
	}
	EditContest dial(this, _("Edit"), contest, contest_type, callsign_field);
	if (dial.ShowModal() == ID_OK_BUT) {
		wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
		config->SetPath(wxT("/cabrilloMap"));
		wxString val = wxString::Format(wxT("%d;%d"), dial.contest_type, dial.callsign_field);
		config->Write(dial.contest, val);
		config->Flush(false);
		if (dial.contest != contest && !contest.IsEmpty()) {
			config->SetPath(wxT("/cabrilloMap"));
			config->DeleteEntry(contest, true);
			config->Flush(false);
		}
		SetContestList();
	}
}

BEGIN_EVENT_TABLE(EditContest, wxDialog)
	EVT_BUTTON(ID_OK_BUT, EditContest::OnOK)
	EVT_BUTTON(ID_CAN_BUT, EditContest::OnCancel)
END_EVENT_TABLE()

EditContest::EditContest(wxWindow *parent, wxString ctype, wxString _contest,
		int _contest_type, int _callsign_field)
		: wxDialog(parent, -1, ctype + wxT(" ") + _("Contest")), contest(_contest),
		contest_type(_contest_type), callsign_field(_callsign_field) {
	tqslTrace("EditContest::EditContest", "parent=0x%lx, ctype=%s, _contest=%s, _contest_type=%d, _callsign_field=%d", reinterpret_cast<void *>(parent), S(ctype), S(_contest), _contest_type, _callsign_field);
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, ctype + wxT(" ") + _("Cabrillo CONTEST")), 0, wxALL, 10);

	sizer->Add(new wxStaticText(this, -1, _("CONTEST Name:")), 0, wxLEFT|wxRIGHT, 10);
	name = new wxTextCtrl(this, -1, contest, wxDefaultPosition, wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(name, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);

	static wxString choices[] = { wxT("HF"), wxT("VHF") };

	type = new wxRadioBox(this, -1, _("Contest type"), wxDefaultPosition, wxDefaultSize,
		2, choices, 2, wxRA_SPECIFY_COLS);
	sizer->Add(type, 0, wxALL|wxEXPAND, 10);
	type->SetSelection(contest_type);

	sizer->Add(new wxStaticText(this, -1, _("Call-Worked Field Number:")), 0, wxLEFT|wxRIGHT, 10);
	fieldnum = new wxTextCtrl(this, -1, wxString::Format(wxT("%d"), callsign_field),
		wxDefaultPosition, wxSize(char_width * 3, HEIGHT_ADJ(char_height)));
	sizer->Add(fieldnum, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, _("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, _("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	sizer->Add(butsizer, 0, wxALIGN_CENTER);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void EditContest::OnOK(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("EditContest::OnOK", NULL);
	if (TransferDataFromWindow())
		EndModal(ID_OK_BUT);
}

bool EditContest::TransferDataFromWindow() {
	tqslTrace("EditContest::TransferDataFromWindow", NULL);
	contest = name->GetValue();
	contest.Trim(false);
	contest.Trim(true);
	contest.MakeUpper();
	if (contest.IsEmpty()) {
		wxMessageBox(_("Contest name cannot be blank"), _("Error"), wxOK | wxICON_ERROR, this);
		return false;
	}
	contest_type = type->GetSelection();
	callsign_field = strtol(fieldnum->GetValue().ToUTF8(), NULL, 10);
	if (callsign_field < TQSL_MIN_CABRILLO_MAP_FIELD) {
		wxMessageBox(wxString::Format(_("Call-worked field must be %d or greater"), TQSL_MIN_CABRILLO_MAP_FIELD),
			_("Error"), wxOK | wxICON_ERROR, this);
		return false;
	}
	return true;
}
