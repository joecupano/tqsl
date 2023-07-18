/***************************************************************************
                                  tqsl.cpp
                             -------------------
    begin                : Mon Nov 4 2002
    copyright            : (C) 2002-2022 by ARRL and the TrustedQSL Developers
    author               : Jon Bloom
    email                : jbloom@arrl.org
 ***************************************************************************/

#include <curl/curl.h> // has to be before something else in this list
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#endif
#include <expat.h>
#include <sys/stat.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <dirent.h>
#else
    #include <direct.h>
    #include "windirent.h"
#endif

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/wxchar.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/hashmap.h>
#include <wx/hyperlink.h>
#include <wx/cmdline.h>
#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/intl.h>
#include <wx/cshelp.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#include <wx/wxhtml.h>
#include <wx/wfstream.h>

#ifdef _MSC_VER //could probably do a more generic check here...
// stdint exists on vs2012 and (I think) 2010, but not 2008 or its platform
  #define uint8_t unsigned char
#else
#include <stdint.h> //for uint8_t; should be cstdint but this is C++11 and not universally supported
#endif

#ifdef _WIN32
	#include <io.h>
	HRESULT IsElevated(BOOL * pbElevated);
	static HRESULT GetElevationType(__out TOKEN_ELEVATION_TYPE * ptet);
#endif
#include <zlib.h>
#include <openssl/opensslv.h> // only for version info!

#ifdef USE_LMDB
#include <lmdb.h> //only for version info!
#else
#include <db.h> //only for version info!
#endif

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <map>

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslhelp.h"

#include "crqwiz.h"
#include "getpassword.h"
#include "loadcertwiz.h"
#include "tqsllib.h"
#include "util.h"
#include "wxutil.h"

#include "tqslwiz.h"
#include "qsodatadialog.h"
#include "tqslerrno.h"
#include "tqslexcept.h"
#include "tqslpaths.h"
#include "stationdial.h"
#include "tqslconvert.h"
#include "dxcc.h"
#include "tqsl_prefs.h"
#include "tqslbuild.h"
#include "tqslapp.h"
#include "tqsltrace.h"

#include "winstrdefs.h"

#include "jsonval.h"
#include "jsonreader.h"

using std::ifstream;
using std::ios;
using std::ofstream;
using std::cerr;
using std::endl;
using std::vector;
using std::string;

#ifdef _WIN32
#include "key.xpm"
#else
#include "key-new.xpm"
#endif
#include "save.xpm"
#include "upload.xpm"
#include "upload_dis.xpm"
#include "file_edit.xpm"
#include "file_edit_dis.xpm"
#include "loc_add.xpm"
#include "loc_add_dis.xpm"
#include "delete.xpm"
#include "delete_dis.xpm"
#include "edit.xpm"
#include "edit_dis.xpm"
#include "download.xpm"
#include "download_dis.xpm"
#include "properties.xpm"
#include "properties_dis.xpm"
#include "import.xpm"
#include "lotw.xpm"

/// GEOMETRY

#define LABEL_HEIGHT 20

#define CERTLIST_FLAGS TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_SUPERCEDED | TQSL_SELECT_CERT_EXPIRED

static wxMenu *stn_menu;

static wxString flattenCallSign(const wxString& call);

static wxString ErrorTitle(_("TQSL Error"));

static bool verify_cert(tQSL_Location loc, bool editing);

static CURL* tqsl_curl_init(const char *logTitle, const char *url, FILE **curlLogFile, bool newFile);

static wxString origCommandLine = wxT("");
static MyFrame *frame = 0;

static char unipwd[64];
static bool quiet = false;
static bool verifyCA = true;

static FILE *curlLogFile;
static CURL *curlReq;

static int lock_db(bool wait);
static void unlock_db(void);
int get_address_field(const char *callsign, const char *field, string& result);

static void cert_cleanup(void);

static void exitNow(int status, bool quiet) {
	const char *errors[] = { __("Success"),
				 __("User Cancelled"),
				 __("Upload Rejected"),
				 __("Unexpected LoTW Response"),
				 __("TQSL Error"),
				 __("TQSLLib Error"),
				 __("Error opening input file"),
				 __("Error opening output file"),
				 __("No QSOs written"),
				 __("Some QSOs suppressed"),
				 __("Command Syntax Error"),
				 __("LoTW Connection Failed"),
				 __("Unknown"),
				 __("The duplicates database is locked")
				};
	int stat = status;
	if (stat > TQSL_EXIT_UNKNOWN || stat < 0) stat = TQSL_EXIT_UNKNOWN;
	wxString msg = wxString::Format(wxT("Final Status: %hs (%d)"), errors[stat], status);
	wxString err = wxGetTranslation(wxString::FromUTF8(errors[stat]));
	wxString localmsg = wxString::Format(_("Final Status: %hs (%d)"), (const char *)err.ToUTF8(), status);

	if (!quiet) {
		if (msg != localmsg) {
			wxLogMessage(localmsg);
		}
		wxLogMessage(msg);
	} else {
		cerr << "Final Status: " << errors[stat] << "(" << status << ")" << endl;
		tqslTrace(NULL, "Final Status: %s", errors[stat]);
	}
	exit(status);
}

/////////// Application //////////////

class QSLApp : public wxApp {
 public:
	QSLApp();
	virtual ~QSLApp();
	class MyFrame *GUIinit(bool checkUpdates, bool quiet = false);
	virtual bool OnInit();
	virtual int OnRun();
	wxLanguage GetLang() {return lang; }
//	virtual wxLog *CreateLogTarget();
 protected:
	wxLanguage lang;		// Language specified by user
	wxLocale* locale;		// Locale we're using
};

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
	tqsl_closeDiagFile();
}

IMPLEMENT_APP(QSLApp)

static int
getCertPassword(char *buf, int bufsiz, tQSL_Cert cert) {
	tqslTrace("getCertPassword", "buf = %lx, bufsiz=%d, cert=%lx", buf, bufsiz, cert);
	char call[TQSL_CALLSIGN_MAX+1] = "";
	int dxcc = 0;
	tqsl_getCertificateCallSign(cert, call, sizeof call);
	tqsl_getCertificateDXCCEntity(cert, &dxcc);
	DXCC dx;
	dx.getByEntity(dxcc);

	// TRANSLATORS: this is followed by the callsign and entity name
	wxString fmt = _("Enter the passphrase to unlock the callsign certificate for %hs -- %hs\n(This is the passphrase you made up when you installed the callsign certificate.)");
	wxString message = wxString::Format(fmt, call, dx.name());

	wxWindow* top = wxGetApp().GetTopWindow();
	if (frame->IsQuiet()) {
		frame->Show(true);
	}
	top->SetFocus();
	top->Raise();

	wxString pwd;
	int ret = getPasswordFromUser(pwd, message, _("Enter passphrase"), wxT(""), top);
	if (ret != wxID_OK)
		return 1;
	strncpy(buf, pwd.ToUTF8(), bufsiz);
	utf8_to_ucs2(buf, unipwd, sizeof unipwd);
	return 0;
}

class ConvertingDialog : public wxDialog {
 public:
	explicit ConvertingDialog(wxWindow *parent, const char *filename = "");
	void OnCancel(wxCommandEvent&);
	bool running;
	wxStaticText *msg;
 private:
	wxButton *canbut;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ConvertingDialog, wxDialog)
	EVT_BUTTON(TQSL_CD_CANBUT, ConvertingDialog::OnCancel)
END_EVENT_TABLE()

void
ConvertingDialog::OnCancel(wxCommandEvent&) {
	running = false;
	canbut->Enable(FALSE);
}

ConvertingDialog::ConvertingDialog(wxWindow *parent, const char *filename)
	: wxDialog(parent, -1, wxString(_("Signing QSO Data"))),
	running(true) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString label = wxString(_("Converting ")) + wxString::FromUTF8(filename) + wxT(" ") + _("to TQSL format");
	sizer->Add(new wxStaticText(this, -1, label), 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_CD_MSG, wxT(" "));
	sizer->Add(msg, 0, wxALL|wxALIGN_LEFT, 10);
	canbut = new wxButton(this, TQSL_CD_CANBUT, _("Cancel"));
	sizer->Add(canbut, 0, wxALL|wxEXPAND, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

#define TQSL_PROG_CANBUT TQSL_ID_LOW+30

DECLARE_EVENT_TYPE(wxEVT_LOGUPLOAD_DONE, -1)
DEFINE_EVENT_TYPE(wxEVT_LOGUPLOAD_DONE)
DECLARE_EVENT_TYPE(wxEVT_LOGUPLOAD_PROGRESS, -1)
DEFINE_EVENT_TYPE(wxEVT_LOGUPLOAD_PROGRESS)

class UploadDialog : public wxDialog {
 public:
	explicit UploadDialog(wxWindow *parent, wxString title = wxString(_("Uploading Signed Data")), wxString label = wxString(_("Uploading signed log data...")));
	void OnCancel(wxCommandEvent&);
	void OnDone(wxCommandEvent&);
	void OnProgress(wxCommandEvent&) { }
	int doUpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow);
	static int UpdateProgress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
		return (reinterpret_cast<UploadDialog*>(clientp))->doUpdateProgress(dltotal, dlnow, ultotal, ulnow);
	}
 private:
	wxButton *canbut;
	wxGauge* progress;
	bool cancelled;
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(UploadDialog, wxDialog)
	EVT_BUTTON(TQSL_PROG_CANBUT, UploadDialog::OnCancel)
	EVT_COMMAND(wxID_ANY, wxEVT_LOGUPLOAD_DONE, UploadDialog::OnDone)
	EVT_COMMAND(wxID_ANY, wxEVT_LOGUPLOAD_PROGRESS, UploadDialog::OnProgress)
END_EVENT_TABLE()

void
UploadDialog::OnCancel(wxCommandEvent&) {
	cancelled = true;
	canbut->Enable(false);
}

UploadDialog::UploadDialog(wxWindow *parent, wxString title, wxString label)
	: wxDialog(parent, -1, title), cancelled(false) {
	tqslTrace("UploadDialog::UploadDialog", "parent = %lx", parent);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, label), 0, wxALL|wxALIGN_CENTER, 10);

	progress = new wxGauge(this, -1, 100);
	progress->SetValue(0);
	sizer->Add(progress, 0, wxALL|wxEXPAND);

	canbut = new wxButton(this, TQSL_PROG_CANBUT, _("Cancel"));
	sizer->Add(canbut, 0, wxALL|wxEXPAND, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void UploadDialog::OnDone(wxCommandEvent&) {
	tqslTrace("UploadDialog::OnDone", "Upload complete");
	EndModal(1);
}

int UploadDialog::doUpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow) {
// This logging is just noise. No reason to bother.
#ifdef LOG_DL_PROGRESS
	static double lastDlnow = 0.0;
	if (dlnow != lastDlnow) {
		tqslTrace("UploadDialog::doUpdateProgresss", "dltotal=%f, dlnow=%f, ultotal=%f, ulnow=%f", dltotal, dlnow, ultotal, ulnow);
		lastDlnow = dlnow;
	}
#endif
	// Post an event to the GUI thread
	wxCommandEvent evt(wxEVT_LOGUPLOAD_PROGRESS, wxID_ANY);
	GetEventHandler()->AddPendingEvent(evt);

	if (cancelled) return 1;
	// Avoid ultotal at zero.
	if (ultotal > 0.0000001) progress->SetValue(static_cast<int>((100*(ulnow/ultotal))));
	return 0;
}

#define TQSL_DR_START TQSL_ID_LOW+10
#define TQSL_DR_END TQSL_ID_LOW+11
#define TQSL_DR_OK TQSL_ID_LOW+12
#define TQSL_DR_CAN TQSL_ID_LOW+13
#define TQSL_DR_MSG TQSL_ID_LOW+14

class DateRangeDialog : public wxDialog {
 public:
	explicit DateRangeDialog(wxWindow *parent = 0);
	tQSL_Date start, end;
 private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	virtual bool TransferDataFromWindow();
	wxTextCtrl *start_tc, *end_tc;
	wxStaticText *msg;
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DateRangeDialog, wxDialog)
	EVT_BUTTON(TQSL_DR_OK, DateRangeDialog::OnOk)
	EVT_BUTTON(TQSL_DR_CAN, DateRangeDialog::OnCancel)
END_EVENT_TABLE()

DateRangeDialog::DateRangeDialog(wxWindow *parent) : wxDialog(parent, -1, wxString(_("QSO Date Range"))) {
	tqslTrace("DateRangeDialog::DateRangeDialog", NULL);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString msgstr = _("You may set the starting and/or ending QSO dates in order to select QSOs from the input file.");
		msgstr += wxT("\n\n");
		msgstr += _("QSOs prior to the starting date or after the ending date will not be signed or included in the output file.");
		msgstr += wxT("\n\n");
		msgstr += _("You may leave either date (or both dates) blank.");
	wxSize sz = getTextSize(this);
	int em_w = sz.GetWidth();
	wxStaticText *st = new wxStaticText(this, -1, msgstr);
	st->Wrap(em_w * 30);
	sizer->Add(st, 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("Start Date (YYYY-MM-DD)")), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	start_tc = new wxTextCtrl(this, TQSL_DR_START);
	hsizer->Add(start_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("End Date (YYYY-MM-DD)")), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	end_tc = new wxTextCtrl(this, TQSL_DR_END);
	hsizer->Add(end_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_DR_MSG, wxT(""));
	sizer->Add(msg, 0, wxALL, 5);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxButton(this, TQSL_DR_OK, _("OK")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_DR_CAN, _("Cancel")), 0, wxLEFT, 10);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

bool
DateRangeDialog::TransferDataFromWindow() {
	tqslTrace("DateRangeDialog::TransferDataFromWindow", NULL);
	wxString text = start_tc->GetValue();
	tqslTrace("DateRangeDialog::TransferDataFromWindow", "start=%s", S(text));
	if (text.Trim().IsEmpty()) {
		start.year = start.month = start.day = 0;
	} else if (tqsl_initDate(&start, text.ToUTF8()) || !tqsl_isDateValid(&start)) {
		msg->SetLabel(_("Start date is invalid"));
		return false;
	}
	text = end_tc->GetValue();
	tqslTrace("DateRangeDialog::TransferDataFromWindow", "end=%s", S(text));
	if (text.Trim().IsEmpty()) {
		end.year = end.month = end.day = 0;
	} else if (tqsl_initDate(&end, text.ToUTF8()) || !tqsl_isDateValid(&end)) {
		msg->SetLabel(_("End date is invalid"));
		return false;
	}
	return true;
}

void
DateRangeDialog::OnOk(wxCommandEvent&) {
	tqslTrace("DateRangeDialog::OnOk", NULL);
	if (TransferDataFromWindow())
		EndModal(wxOK);
}

void
DateRangeDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("DateRangeDialog::OnCancel", NULL);
	EndModal(wxCANCEL);
}

#define TQSL_DP_OK TQSL_ID_LOW+20
#define TQSL_DP_CAN TQSL_ID_LOW+21
#define TQSL_DP_ALLOW TQSL_ID_LOW+22

class DupesDialog : public wxDialog {
 public:
	explicit DupesDialog(wxWindow *parent = 0, int qso_count = 0, int dupes = 0, int action = TQSL_ACTION_ASK);
 private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnAllow(wxCommandEvent&);
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DupesDialog, wxDialog)
	EVT_BUTTON(TQSL_DP_OK, DupesDialog::OnOk)
	EVT_BUTTON(TQSL_DP_CAN, DupesDialog::OnCancel)
	EVT_BUTTON(TQSL_DP_ALLOW, DupesDialog::OnAllow)
END_EVENT_TABLE()

DupesDialog::DupesDialog(wxWindow *parent, int qso_count, int dupes, int action)
		: wxDialog(parent, -1, wxString(_("Already Uploaded QSOs Detected")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	tqslTrace("DupesDialog::DupesDialog", "qso_count = %d, dupes =%d, action= =%d", qso_count, dupes, action);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString message;

	if (qso_count == dupes) {
		wxString fmt = _("This log contains %d QSO(s) which appear to have already been signed for upload to LoTW, and no new QSOs.");
		fmt += wxT("\n\n");
		fmt += _("Click 'Cancel' to abandon processing this log file (Recommended).");
		fmt += wxT("\n");
		fmt += _("Click 'Re-Upload QSOs' to re-process this log while allowing already uploaded QSOs.");
		message = wxString::Format(fmt, qso_count);
	} else {
		int newq = qso_count - dupes;
		wxString fmt = _("This log contains %d QSO(s) which appear to have already been signed for upload to LoTW, and %d QSOs which are new.");
			fmt += wxT("\n\n");
		  	fmt += _("Click 'New QSOs Only' to sign normally, without the already uploaded QSOs (Recommended).");
			fmt += wxT("\n");
			fmt += _("Click 'Cancel' to abandon processing this log file.");
			fmt += wxT("\n");
			fmt += _("Click 'Re-Upload QSOs' to re-process this log while allowing already uploaded QSOs.");
		wxString fmt1 = _("This log contains %d QSO(s) which appear to have already been signed for upload to LoTW, and one QSO which is new.");
			fmt1 += wxT("\n\n");
		  	fmt1 += _("Click 'New QSOs Only' to sign normally, without the already uploaded QSOs (Recommended).");
			fmt1 += wxT("\n");
			fmt1 += _("Click 'Cancel' to abandon processing this log file.");
			fmt1 += wxT("\n");
			fmt1 += _("Click 'Re-Upload QSOs' to re-process this log while allowing already uploaded QSOs.");
		if (newq == 1) {
			message = wxString::Format(fmt1, dupes);
		} else {
			message = wxString::Format(fmt, dupes, newq);
		}
	}

	if (action == TQSL_ACTION_UNSPEC) {
		if (qso_count == dupes) {
			message+= wxT("\n\n");
			message += _("The log file you are uploading using your QSO Logging system consists entirely of previously uploaded QSOs that create unnecessary work for LoTW. There may be a more recent version of your QSO Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.");
			message += wxT("\n");
			message += _("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Re-Upload QSOs' is clicked. Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
		} else {
			message+= wxT("\n\n");
			message += _("The log file you are uploading using your QSO Logging system includes some previously uploaded QSOs that create unnecessary work for LoTW. There may be a more recent version of your QSO Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.");
			message += wxT("\n");
			message += _("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Re-Upload QSOs' is clicked. 'New QSOs Only' is recommended, but a logger that does its own upload tracking may incorrectly set the status in this case. A logger that doesn't track uploads should be unaffected by choosing 'New QSOs Only' and if it tracks 'QSO sent' status, will correctly mark all selected QSOs as sent - they are in your account even though they would not be in this specific batch");
			message += wxT("\n");
			message += _("Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
		}
	}
	wxStaticText* mtext = new wxStaticText(this, -1, message);
	sizer->Add(mtext, 0, wxALL|wxALIGN_CENTER, 10);

	wxSize sz = getTextSize(this);
	int em_w = sz.GetWidth();
	mtext->Wrap(em_w * 50);
	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	if (qso_count != dupes)
		hsizer->Add(new wxButton(this, TQSL_DP_OK, _("New QSOs Only")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_DP_CAN, _("Cancel")), 0, wxLEFT, 10);
	hsizer->Add(new wxButton(this, TQSL_DP_ALLOW, _("Re-Upload QSOs")), 0, wxLEFT, 20);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void
DupesDialog::OnOk(wxCommandEvent&) {
	tqslTrace("DupesDialog::OnOk", NULL);
	EndModal(TQSL_DP_OK);
}

void
DupesDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("DupesDialog::OnCancel", NULL);
	EndModal(TQSL_DP_CAN);
}

void
DupesDialog::OnAllow(wxCommandEvent&) {
	tqslTrace("DupesDialog::OnAllow", NULL);

	wxString msg = _("The only reason to re-sign already uploaded QSOs is if a previous upload was not processed by LoTW, either because it was never uploaded, or there was a server failure");
		msg += wxT("\n\n");
		msg += _("Are you sure you want to proceed? Click 'No' to review the choices");
	if (wxMessageBox(msg, _("Are you sure?"), wxYES_NO|wxICON_EXCLAMATION, this) == wxYES) {
		EndModal(TQSL_DP_ALLOW);
	}
}

#define TQSL_AE_OK TQSL_ID_LOW+40
#define TQSL_AE_CAN TQSL_ID_LOW+41

class ErrorsDialog : public wxDialog {
 public:
	explicit ErrorsDialog(wxWindow *parent = 0, wxString msg = wxT(""));
 private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ErrorsDialog, wxDialog)
	EVT_BUTTON(TQSL_AE_OK, ErrorsDialog::OnOk)
	EVT_BUTTON(TQSL_AE_CAN, ErrorsDialog::OnCancel)
END_EVENT_TABLE()

ErrorsDialog::ErrorsDialog(wxWindow *parent, wxString msg)
		: wxDialog(parent, -1, wxString(_("Errors Detected")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	tqslTrace("ErrorsDialog::ErrorsDialog", "msg=%s", S(msg));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(new wxStaticText(this, -1, msg), 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxButton(this, TQSL_AE_OK, _("Ignore")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_AE_CAN, _("Cancel")), 0, wxLEFT, 10);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void
ErrorsDialog::OnOk(wxCommandEvent&) {
	tqslTrace("ErrorsDialog::OnOk", NULL);
	EndModal(TQSL_AE_OK);
}

void
ErrorsDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("ErrorsDialog::OnCancel", NULL);
	EndModal(TQSL_AE_CAN);
}

static void
init_modes() {
	tqslTrace("init_modes", NULL);
	tqsl_clearADIFModes();
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	long cookie;
	wxString key, value;
	vector<wxString> badModes;
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		key.Replace(wxT("!SLASH!"), wxT("/"), true);	// Fix slashes in modes
		bool newMode = true;
		int numModes;
		if (tqsl_getNumMode(&numModes) == 0) {
			for (int i = 0; i < numModes; i++) {
				const char *modestr;
				if (tqsl_getMode(i, &modestr, NULL) == 0) {
					if (strcasecmp(key.ToUTF8(), modestr) == 0) {
						wxLogWarning(_("Your custom mode map %s conflicts with the standard mode definition for %hs and was deleted."), key.c_str(), modestr);
						newMode = false;
						badModes.push_back(key);
						break;
					}
				}
			}
		}
		if (newMode)
			tqsl_setADIFMode(key.ToUTF8(), value.ToUTF8());
		stat = config->GetNextEntry(key, cookie);
	}
	// Delete the conflicting entries
	for (int i = 0; i < static_cast<int>(badModes.size()); i++) {
		config->DeleteEntry(badModes[i]);
	}
	config->SetPath(wxT("/"));
}

static void
init_contests() {
	tqslTrace("init_contests", NULL);
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
	tqslTrace("check_tqsl_error", "rval=%d", rval);
	wxString msg = getLocalizedErrorString();
	tqslTrace("check_tqsl_error", "msg=%s", S(msg));
	throw TQSLException(S(msg));
}

static tQSL_Cert *certlist = 0;
static int ncerts;

static void
free_certlist() {
	tqslTrace("free_certlist", NULL);
	if (certlist) {
		tqsl_freeCertificateList(certlist, ncerts);
		certlist = 0;
	}
	ncerts = 0;
}

static void
get_certlist(string callsign, int dxcc, bool expired, bool superceded, bool withkeys) {
	tqslTrace("get_certlist", "callsign=%s, dxcc=%d, expired=%d, superceded=%d withkeys=%d", callsign.c_str(), dxcc, expired, superceded, withkeys);
	free_certlist();
	int select = 0;
	if (expired) select |= TQSL_SELECT_CERT_EXPIRED;
	if (superceded) select |= TQSL_SELECT_CERT_SUPERCEDED;
	if (withkeys) select |= TQSL_SELECT_CERT_WITHKEYS;
	tqsl_selectCertificates(&certlist, &ncerts,
		(callsign == "") ? 0 : callsign.c_str(), dxcc, 0, 0, select);
}

#if wxMAJOR_VERSION > 2
class SimpleLogFormatter : public wxLogFormatter {
	virtual wxString Format(wxLogLevel level, const wxString& msg, const wxLogRecordInfo& info) const {
		return msg;
	}
};
#endif

class LogList : public wxLog {
 public:
	explicit LogList(MyFrame *frame) : wxLog(), _frame(frame) {}
#if wxMAJOR_VERSION > 2
	virtual void DoLogText(const wxString& msg);
#else
	virtual void DoLogString(const wxChar *szString, time_t t);
#endif
 private:
	MyFrame *_frame;
};

#if wxMAJOR_VERSION > 2
void LogList::DoLogText(const wxString& msg) {
	const wxChar* szString = msg.wc_str();
#else
void LogList::DoLogString(const wxChar *szString, time_t) {
	static wxString msg(szString);
#endif
	static const char *smsg = msg.ToUTF8();

	tqslTrace(NULL, "%s", smsg);

	wxTextCtrl *_logwin = 0;

	if (msg.StartsWith(wxT("Debug:")))
		return;
	if (msg.StartsWith(wxT("Error: Unable to open requested HTML document:")))
		return;
	if (_frame != 0)
		_logwin = _frame->logwin;
	if (_logwin == 0) {
#ifdef _WIN32
		fwprintf(stderr, L"%ls\n", szString);
#else
		fprintf(stderr, "%ls\n", szString);
#endif
		return;
	}
	_logwin->AppendText(szString);
	_logwin->AppendText(wxT("\n"));
	// Select the log tab when there's some new message
	if (_frame->notebook->GetPageCount() > TQSL_LOG_TAB)
		_frame->notebook->SetSelection(TQSL_LOG_TAB);
}

class LogStderr : public wxLog {
 public:
	LogStderr(void) : wxLog() {}
#if wxMAJOR_VERSION > 2
	virtual void DoLogText(const wxString& msg);
#else
	virtual void DoLogString(const wxChar *szString, time_t t);
#endif
};

#if wxMAJOR_VERSION > 2
void LogStderr::DoLogText(const wxString& msg) {
	const wxChar* szString = msg.wc_str();
#else
void LogStderr::DoLogString(const wxChar *szString, time_t) {
	static wxString msg(szString);
#endif
	static const char *smsg = msg.ToUTF8();

	tqslTrace(NULL, "%s", smsg);
	if (msg.StartsWith(wxT("Debug:")))
		return;
#ifdef _WIN32
	fwprintf(stderr, L"%ls\n", szString);
#else
	fprintf(stderr, "%ls\n", szString);
#endif
	return;
}

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(tm_s_add, MyFrame::AddStationLocation)
	EVT_BUTTON(tl_AddLoc, MyFrame::AddStationLocation)
	EVT_MENU(tm_s_edit, MyFrame::EditStationLocation)
	EVT_BUTTON(tl_EditLoc, MyFrame::EditStationLocation)
	EVT_MENU(tm_f_new, MyFrame::EnterQSOData)
	EVT_BUTTON(tl_Edit, MyFrame::EnterQSOData)
	EVT_MENU(tm_f_edit, MyFrame::EditQSOData)
	EVT_MENU(tm_f_import_compress, MyFrame::ImportQSODataFile)
	EVT_BUTTON(tl_Save, MyFrame::ImportQSODataFile)
	EVT_MENU(tm_f_upload, MyFrame::UploadQSODataFile)
	EVT_BUTTON(tl_Upload, MyFrame::UploadQSODataFile)
	EVT_MENU(tm_f_exit, MyFrame::DoExit)
	EVT_MENU(tm_f_preferences, MyFrame::OnPreferences)
	EVT_MENU(tm_f_loadconfig, MyFrame::OnLoadConfig)
	EVT_MENU(tm_f_saveconfig, MyFrame::OnSaveConfig)
	EVT_MENU(tm_h_contents, MyFrame::OnHelpContents)
	EVT_MENU(tm_h_about, MyFrame::OnHelpAbout)
	EVT_MENU(tm_f_diag, MyFrame::OnHelpDiagnose)
	EVT_MENU(tm_f_lang, MyFrame::OnChooseLanguage)

	EVT_MENU(tm_h_update, MyFrame::CheckForUpdates)

	EVT_CLOSE(MyFrame::OnExit)

	EVT_MENU(tc_CRQWizard, MyFrame::CRQWizard)
	EVT_MENU(tc_c_New, MyFrame::CRQWizard)
	EVT_MENU(tc_c_Load, MyFrame::OnLoadCertificateFile)
	EVT_BUTTON(tc_Load, MyFrame::OnLoadCertificateFile)
	EVT_MENU(tc_c_Properties, MyFrame::OnCertProperties)
	EVT_BUTTON(tc_CertProp, MyFrame::OnCertProperties)
	EVT_MENU(tc_c_Export, MyFrame::OnCertExport)
	EVT_BUTTON(tc_CertSave, MyFrame::OnCertExport)
	EVT_MENU(tc_c_Delete, MyFrame::OnCertDelete)
	EVT_MENU(tc_c_Undelete, MyFrame::OnCertUndelete)
//	EVT_MENU(tc_c_Import, MyFrame::OnCertImport)
//	EVT_MENU(tc_c_Sign, MyFrame::OnSign)
	EVT_MENU(tc_c_Renew, MyFrame::CRQWizardRenew)
	EVT_BUTTON(tc_CertRenew, MyFrame::CRQWizardRenew)
	EVT_MENU(tc_h_Contents, MyFrame::OnHelpContents)
	EVT_MENU(tc_h_About, MyFrame::OnHelpAbout)
	EVT_MENU(tl_c_Properties, MyFrame::OnLocProperties)
	EVT_MENU(tm_s_Properties, MyFrame::OnLocProperties)
	EVT_MENU(tm_s_undelete, MyFrame::OnLocUndelete)
	EVT_BUTTON(tl_PropLoc, MyFrame::OnLocProperties)
	EVT_MENU(tl_c_Delete, MyFrame::OnLocDelete)
	EVT_BUTTON(tl_DeleteLoc, MyFrame::OnLocDelete)
	EVT_MENU(tl_c_Edit, MyFrame::OnLocEdit)
	EVT_BUTTON(tl_Login, MyFrame::OnLoginToLogbook)
	EVT_TREE_SEL_CHANGED(tc_CertTree, MyFrame::OnCertTreeSel)
	EVT_TREE_SEL_CHANGED(tc_LocTree, MyFrame::OnLocTreeSel)

	EVT_MENU(bg_updateCheck, MyFrame::OnUpdateCheckDone)
	EVT_MENU(bg_expiring, MyFrame::OnExpiredCertFound)

END_EVENT_TABLE()

void
MyFrame::AutoBackup(void) {
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString bdir = config->Read(wxT("BackupFolder"), wxString::FromUTF8(tQSL_BaseDir));
	if (bdir.Trim(true).Trim(false).IsEmpty())
		bdir = wxString::FromUTF8(tQSL_BaseDir);
	SaveOldBackups(bdir, wxT("tqslconfig"), wxT("tbk"));
#ifdef _WIN32
	bdir += wxT("\\tqslconfig.tbk");
#else
	bdir += wxT("/tqslconfig.tbk");
#endif
	tqslTrace("MyFrame::OnExit", "Auto Backup %s", S(bdir));
	BackupConfig(bdir, true);
}

void
MyFrame::SaveOldBackups(const wxString& directory, const wxString& filename, const wxString& ext) {
#ifdef _WIN32
	wxString bfile = directory + wxT("\\") + filename + wxT(".") + ext;
	struct _stat32 s;
	wchar_t* lfn = utf8_to_wchar(bfile.ToUTF8());
	int ret = _wstat32(lfn, &s);
	free_wchar(lfn);
	if (ret != 0) {					// Does it exist?
#else
	wxString bfile = directory + wxT("/") + filename + wxT(".") + ext;
	struct stat s;
	if (lstat(bfile.ToUTF8(), &s) != 0) {		// Does it exist?
#endif
		return;					// No. No need to back it up.
	}

	// There's a file with that name already.
	// Rename it for backup purposes

	struct tm *t;

#ifdef _WIN32
	t = _gmtime32(&s.st_mtime);
	wxString newName = directory + wxT("\\") + filename +
#else
	t = gmtime(&s.st_mtime);
	wxString newName = directory + wxT("/") + filename +
#endif
		wxString::Format(wxT("-%4.4d-%2.2d-%2.2d-%2.2d-%2.2d."),
					t->tm_year+1900, t->tm_mon+1, t->tm_mday,
					t->tm_hour, t->tm_min) + ext;
#ifdef _WIN32
	lfn = utf8_to_wchar(bfile.ToUTF8());
	wchar_t* newlfn = utf8_to_wchar(newName.ToUTF8());
	ret = _wrename(lfn, newlfn);
	free_wchar(lfn);
	free_wchar(newlfn);
#else
	int ret = rename(bfile.ToUTF8(), newName.ToUTF8());
#endif
	if (ret && errno != EEXIST) {			// EEXIST means that there's been another backup this minute.
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("MyFrame::SaveOldBackups", "Error renaming: %s", strerror(errno));
		wxLogError(_("Error renaming backup file %s: %hs"), bfile.c_str(), strerror(errno));
		return;
	}

	// Find any backups and delete older ones

	wxArrayString bfiles;

#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(directory.ToUTF8());
	_WDIR *dir = _wopendir(wpath);
	free_wchar(wpath);
#else
	DIR *dir = opendir(directory.ToUTF8());
#endif

	if (dir != NULL) {
#ifdef _WIN32
		struct _wdirent *ent = NULL;
		while ((ent = _wreaddir(dir)) != NULL) {
			if (wcsstr(ent->d_name, L"tqslconfig-") == ent->d_name && wcsstr(ent->d_name, L".tbk")) {
				bfiles.Add(wxString(ent->d_name));
			}
		}
#else
		struct dirent *ent = NULL;
		while ((ent = readdir(dir)) != NULL) {
			if (strstr(ent->d_name, "tqslconfig-") == ent->d_name && strstr(ent->d_name, ".tbk")) {
				bfiles.Add(wxString::FromUTF8(ent->d_name));
			}
		}
#endif
	}
	bfiles.Sort();
	long vlimit = DEFAULT_BACKUP_VERSIONS;
	wxConfig::Get()->Read(wxT("BackupVersions"), &vlimit, DEFAULT_BACKUP_VERSIONS);

	if (vlimit <= 0)
		vlimit = DEFAULT_BACKUP_VERSIONS;
	int toRemove = bfiles.GetCount() - vlimit;
	if (toRemove <= 0)
		return;			// Nothing to remove

	// Remove, starting from the oldest
	for (int i = 0; i < toRemove; i++) {
#ifdef _WIN32
		wxString removeIt = directory + wxT("\\") + bfiles[i];
		wchar_t* wfname = utf8_to_wchar(removeIt.ToUTF8());
		_wunlink(wfname);
		free_wchar(wfname);
#else
		wxString removeIt = directory + wxT("/") + bfiles[i];
		unlink(removeIt.ToUTF8());
#endif
	}
}

void
MyFrame::SaveWindowLayout() {
	int x, y, w, h;
	// Don't save window size/position if minimized or too small
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	if (!IsIconized()) {
		GetPosition(&x, &y);
		GetSize(&w, &h);
		if (w >= MAIN_WINDOW_MIN_WIDTH && h >= MAIN_WINDOW_MIN_HEIGHT) {
			config->Write(wxT("MainWindowX"), x);
			config->Write(wxT("MainWindowY"), y);
			config->Write(wxT("MainWindowWidth"), w);
			config->Write(wxT("MainWindowHeight"), h);
			config->Write(wxT("MainWindowMaximized"), IsMaximized());
			config->Flush(false);
		}
	}
}
void
MyFrame::OnExit(TQ_WXCLOSEEVENT& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnExit", "exiting");
	if (logConv) {
		tqsl_converterRollBack(logConv);
		tqsl_endConverter(&logConv);
	}
	SaveWindowLayout();
	unlock_db();
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	bool ab;
	config->Read(wxT("AutoBackup"), &ab, DEFAULT_AUTO_BACKUP);
	if (ab) {
		AutoBackup();
	}
	tqslTrace("MyFrame::OnExit", "GUI Destroy");
	Destroy();		// close the window
	tqslTrace("MyFrame::OnExit", "Done");
}

void
MyFrame::DoExit(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::DoExit", "About to close");
	Close();
	tqslTrace("MyFrame::DoExit", "About to destroy GUI");
	Destroy();
	tqslTrace("MyFrame::DoExit", "Done");
}

static wxSemaphore *updateLocker;

class UpdateThread : public wxThread {
 public:
	UpdateThread(MyFrame *frame, bool silent, bool noGUI) : wxThread(wxTHREAD_DETACHED) {
		_frame = frame;
		_silent = silent;
		_noGUI = noGUI;
	}
	virtual void *Entry() {
		_frame->DoCheckForUpdates(_silent, _noGUI);
		updateLocker->Post();
		return NULL;
	}
 private:
	MyFrame *_frame;
	bool _silent;
	bool _noGUI;
};

void
MyFrame::DoUpdateCheck(bool silent, bool noGUI) {
	tqslTrace("MyFrame::DoUpdateCheck", "silent=%d noGUI=%d", silent, noGUI);
	//check for updates
	if (!noGUI) {
		wxBeginBusyCursor();
		wxTheApp->Yield(true);
		wxLogMessage(_("Checking for TQSL updates..."));
		wxTheApp->Yield(true);
	}
	updateLocker = new wxSemaphore(0, 1);		// One waiter
	UpdateThread* thread = new UpdateThread(this, silent, noGUI);
	thread->Create();
	wxTheApp->Yield(true);
	thread->Run();
	wxTheApp->Yield(true);
	while (updateLocker->TryWait() == wxSEMA_BUSY) {
		wxTheApp->Yield(true);
#if (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
		wxGetApp().DoIdle();
#endif
		wxMilliSleep(300);
	}
	delete updateLocker;
	if (!noGUI) {
		wxString val = logwin->GetValue();
		wxString chk(_("Checking for TQSL updates..."));
		val.Replace(chk + wxT("\n"), wxT(""));
		val.Replace(chk, wxT(""));
		logwin->SetValue(val);		// Clear the checking message
		notebook->SetSelection(0);
		// Refresh the cert tree in case any new info on expires/supercedes
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
		wxEndBusyCursor();
	}
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates, bool quiet, wxLocale* loca)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)), locale(loca) {
	_quiet = quiet;

#ifdef __WXMAC__
	DocPaths docpaths(wxT("tqsl.app"));
#else
	DocPaths docpaths(wxT("tqslapp"));
#endif
	wxBitmap savebm(save_xpm);
	wxBitmap uploadbm(upload_xpm);
	wxBitmap upload_disbm(upload_dis_xpm);
	wxBitmap file_editbm(file_edit_xpm);
	wxBitmap file_edit_disbm(file_edit_dis_xpm);
	wxBitmap locaddbm(loc_add_xpm);
	wxBitmap locadd_disbm(loc_add_dis_xpm);
	wxBitmap editbm(edit_xpm);
	wxBitmap edit_disbm(edit_dis_xpm);
	wxBitmap deletebm(delete_xpm);
	wxBitmap delete_disbm(delete_dis_xpm);
	wxBitmap downloadbm(download_xpm);
	wxBitmap download_disbm(download_dis_xpm);
	wxBitmap propertiesbm(properties_xpm);
	wxBitmap properties_disbm(properties_dis_xpm);
	wxBitmap importbm(import_xpm);
	wxBitmap lotwbm(lotw_xpm);
	loc_edit_button = NULL;
	cert_save_label = NULL;
	req = NULL;
	curlReq = NULL;
	curlLogFile = NULL;
	logConv = NULL;

	// File menu
	file_menu = new wxMenu;
	file_menu->Append(tm_f_upload, _("Sign and &upload ADIF or Cabrillo File..."));
	file_menu->Append(tm_f_import_compress, _("&Sign and save ADIF or Cabrillo file..."));
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_saveconfig, _("&Backup Station Locations, Certificates, and Preferences..."));
	file_menu->Append(tm_f_loadconfig, _("&Restore Station Locations, Certificates, and Preferences..."));
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_new, _("Create &New ADIF file..."));
	file_menu->Append(tm_f_edit, _("&Edit existing ADIF file..."));
	file_menu->AppendSeparator();
#ifdef __WXMAC__	// On Mac, Preferences not on File menu
	file_menu->Append(tm_f_preferences, _("&Preferences..."));
#else
	file_menu->Append(tm_f_preferences, _("Display or Modify &Preferences..."));
#endif
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_lang, _("Language"));
	file_menu->AppendSeparator();
	file_menu->AppendCheckItem(tm_f_diag, _("Dia&gnostic Mode"));
	file_menu->Check(tm_f_diag, false);
#ifndef __WXMAC__	// On Mac, Exit not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tm_f_exit, _("E&xit TQSL\tAlt-X"));

	cert_menu = makeCertificateMenu(false);
	// Station menu
	stn_menu = new wxMenu;
	stn_menu->Append(tm_s_Properties, _("&Display Station Location Properties"));
	stn_menu->Enable(tm_s_Properties, false);
	stn_menu->Append(tm_s_edit, _("&Edit Station Location"));
	stn_menu->Append(tm_s_add, _("&Add Station Location"));
	stn_menu->AppendSeparator();

	int nloc = 0;
	char **locp = NULL;
	tqsl_getDeletedStationLocations(&locp, &nloc);
	stn_menu->Append(tm_s_undelete, _("&Restore a Deleted Station Location"));
	stn_menu->Enable(tm_s_undelete, nloc > 0);
	if (nloc > 0)
		tqsl_freeDeletedLocationList(locp, nloc);

	// Help menu
	help = new wxHtmlHelpController(wxHF_DEFAULT_STYLE | wxHF_OPEN_FILES);
	help_menu = new wxMenu;
	help->UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath(wxT("tqslapp.hhp"));
	if (!wxFileNameFromPath(hhp).IsEmpty()) {
		if (help->AddBook(hhp))
#ifdef __WXMAC__
		help_menu->Append(tm_h_contents, _("&Contents"));
#else
		help_menu->Append(tm_h_contents, _("Display &Documentation"));
		help_menu->AppendSeparator();
#endif
	}

	help_menu->Append(tm_h_update, _("Check for &Updates..."));

	help_menu->Append(tm_h_about, _("&About"));
	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, _("&File"));
	menu_bar->Append(stn_menu, _("&Station Location"));
	menu_bar->Append(cert_menu, _("Callsign &Certificate"));
	menu_bar->Append(help_menu, _("&Help"));

	SetMenuBar(menu_bar);

	wxPanel* topPanel = new wxPanel(this);
	wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
	topPanel->SetSizer(topSizer);

	bool logTab = DEFAULT_LOG_TAB;
	wxConfig::Get()->Read(wxT("LogTab"), &logTab, DEFAULT_LOG_TAB);

	// Log operations

	topSizer->AddSpacer(2);

	notebook = new wxNotebook(topPanel, -1, wxDefaultPosition, wxSize(400, 300), wxNB_TOP /* | wxNB_FIXEDWIDTH*/, _("Log Operations"));

	wxColor defBkg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
	notebook->SetBackgroundColour(defBkg);
	topSizer->Add(notebook, 1, wxEXPAND | wxALL, 1);

	if (!logTab) {
		topSizer->Add(new wxStaticText(topPanel, -1, _("Status Log")), 0, wxEXPAND | wxALL, 1);

		logwin = new wxTextCtrl(topPanel, -1, wxT(""), wxDefaultPosition, wxSize(400, 300),
			wxTE_MULTILINE|wxTE_READONLY);
		topSizer->Add(logwin, 1, wxEXPAND | wxALL, 1);
	}

	wxPanel* buttons = new wxPanel(notebook, -1);
	buttons->SetBackgroundColour(defBkg);

	wxBoxSizer* bsizer = new wxBoxSizer(wxVERTICAL);
	buttons->SetSizer(bsizer);

	wxPanel* b1Panel = new wxPanel(buttons);
	b1Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* b1sizer = new wxBoxSizer(wxHORIZONTAL);
	b1Panel->SetSizer(b1sizer);

	wxBitmapButton *up = new wxBitmapButton(b1Panel, tl_Upload, uploadbm);
	// Use a really tiny label font on the buttons, as the labels are there
	// for accessibility only.
	wxFont f(1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	up->SetLabel(_("Sign a log and upload it automatically to LoTW"));
	up->SetBitmapDisabled(upload_disbm);
	up->SetFont(f);
	b1sizer->Add(up, 0, wxALL, 1);
	wxString b1lbl = wxT("\n");
	b1lbl += _("Sign a log and upload it automatically to LoTW");
	wxStaticText *b1txt = new wxStaticText(b1Panel, -1, b1lbl);
	b1sizer->Add(b1txt, 1, wxFIXED_MINSIZE | wxALL, 1);
	b1txt->SetLabel(b1lbl);
	up->SetLabel(b1lbl);
	bsizer->Add(b1Panel, 0, wxALL, 1);

	wxPanel* b2Panel = new wxPanel(buttons);
	b2Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* b2sizer = new wxBoxSizer(wxHORIZONTAL);
	b2Panel->SetSizer(b2sizer);
	wxBitmapButton *signsave = new wxBitmapButton(b2Panel, tl_Save, savebm);
	signsave->SetLabel(_("Sign a log and save it for uploading later"));
	signsave->SetFont(f);
	b2sizer->Add(signsave, 0, wxALL, 1);
	wxString b2lbl = wxT("\n");
	b2lbl += _("Sign a log and save it for uploading later");
	wxStaticText *b2txt = new wxStaticText(b2Panel, -1, b2lbl);
	b2txt->SetLabel(b2lbl);
	signsave->SetLabel(b2lbl);
	b2sizer->Add(b2txt, 1, wxALL, 1);
	bsizer->Add(b2Panel, 0, wxALL, 1);

	wxPanel* b3Panel = new wxPanel(buttons);
	b3Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* b3sizer = new wxBoxSizer(wxHORIZONTAL);
	b3Panel->SetSizer(b3sizer);
	wxBitmapButton *fed = new wxBitmapButton(b3Panel, tl_Edit, file_editbm);
	fed->SetBitmapDisabled(file_edit_disbm);
	fed->SetLabel(_("Create an ADIF file for signing and uploading"));
	fed->SetFont(f);
	b3sizer->Add(fed, 0, wxALL, 1);
	wxString b3lbl = wxT("\n");
	b3lbl += _("Create an ADIF file for signing and uploading");
	wxStaticText *b3txt = new wxStaticText(b3Panel, -1, b3lbl);
	b3sizer->Add(b3txt, 1, wxALL, 1);
	b3txt->SetLabel(b3lbl);
	fed->SetLabel(b3lbl);
	bsizer->Add(b3Panel, 0, wxALL, 1);

	wxPanel* b4Panel = new wxPanel(buttons);
	b4Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* b4sizer = new wxBoxSizer(wxHORIZONTAL);
	b4Panel->SetSizer(b4sizer);
	wxBitmapButton *lotw = new wxBitmapButton(b4Panel, tl_Login, lotwbm);
	lotw->SetLabel(_("Log in to the Logbook of the World Site"));
	lotw->SetFont(f);
	b4sizer->Add(lotw, 0, wxALL, 1);
	wxString b4lbl = wxT("\n");
	b4lbl += _("Log in to the Logbook of the World Site");
	wxStaticText *b4txt = new wxStaticText(b4Panel, -1, b4lbl);
	b4sizer->Add(b4txt, 1, wxALL, 1);
	b4txt->SetLabel(b4lbl);
	lotw->SetLabel(b4lbl);
	bsizer->Add(b4Panel, 0, wxALL, 1);

	notebook->AddPage(buttons, _("Log Operations"));

//	notebook->InvalidateBestSize();
//	logwin->FitInside();

	// Location tab

	wxPanel* loctab = new wxPanel(notebook, -1);
	wxBoxSizer* locsizer = new wxBoxSizer(wxHORIZONTAL);
	loctab->SetSizer(locsizer);

	wxPanel* locgrid = new wxPanel(loctab, -1);
	locgrid->SetBackgroundColour(defBkg);
	wxBoxSizer* lgsizer = new wxBoxSizer(wxVERTICAL);
	locgrid->SetSizer(lgsizer);

	loc_tree = new LocTree(locgrid, tc_LocTree, wxDefaultPosition,
		wxDefaultSize, wxTR_DEFAULT_STYLE | wxBORDER_NONE);

	loc_tree->SetBackgroundColour(defBkg);
	loc_tree->Build();
	LocTreeReset();
	lgsizer->Add(loc_tree, 1, wxEXPAND);

	wxString lsl = wxT("\n");
	lsl += _("Select a Station Location to process      ");
	loc_select_label = new wxStaticText(locgrid, -1, lsl);
	lgsizer->Add(loc_select_label, 0, wxALL, 1);

	locsizer->Add(locgrid, 50, wxEXPAND);

	wxStaticLine *locsep = new wxStaticLine(loctab, -1, wxDefaultPosition, wxSize(2, -1), wxLI_VERTICAL);
	locsizer->Add(locsep, 0, wxEXPAND);

	wxPanel* lbuttons = new wxPanel(loctab, -1);
	lbuttons->SetBackgroundColour(defBkg);
	locsizer->Add(lbuttons, 50, wxEXPAND);
	wxBoxSizer* lbsizer = new wxBoxSizer(wxVERTICAL);
	lbuttons->SetSizer(lbsizer);

	wxPanel* lb1Panel = new wxPanel(lbuttons);
	lb1Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* lb1sizer = new wxBoxSizer(wxHORIZONTAL);
	lb1Panel->SetSizer(lb1sizer);

	loc_add_button = new wxBitmapButton(lb1Panel, tl_AddLoc, locaddbm);
	loc_add_button->SetFont(f);
	loc_add_button->SetBitmapDisabled(locadd_disbm);
	lb1sizer->Add(loc_add_button, 0, wxALL, 1);
	// Note - the doubling below is to size the label to allow the control to stretch later
	loc_add_label = new wxStaticText(lb1Panel, -1, wxT("\nCreate a new Station LocationCreate a new Station\n"));
	lb1sizer->Add(loc_add_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb1Panel, 0, wxALL, 1);
	int tw, th;
	loc_add_label->GetSize(&tw, &th);
	wxString lal = wxT("\n");
	lal += _("Create a new Station Location");
	loc_add_label->SetLabel(lal);
	loc_add_button->SetLabel(lal);

	wxPanel* lb2Panel = new wxPanel(lbuttons);
	lb2Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* lb2sizer = new wxBoxSizer(wxHORIZONTAL);
	lb2Panel->SetSizer(lb2sizer);

	loc_edit_button = new wxBitmapButton(lb2Panel, tl_EditLoc, editbm);
	loc_edit_button->SetFont(f);
	loc_edit_button->SetBitmapDisabled(edit_disbm);
	loc_edit_button->Enable(false);
	lb2sizer->Add(loc_edit_button, 0, wxALL, 1);
	wxString lel = wxT("\n");
	lel += _("Edit a Station Location");
	loc_edit_label = new wxStaticText(lb2Panel, -1, lel, wxDefaultPosition, wxSize(tw, th));
	lb2sizer->Add(loc_edit_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb2Panel, 0, wxALL, 1);

	wxPanel* lb3Panel = new wxPanel(lbuttons);
	lb3Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* lb3sizer = new wxBoxSizer(wxHORIZONTAL);
	lb3Panel->SetSizer(lb3sizer);

	loc_delete_button = new wxBitmapButton(lb3Panel, tl_DeleteLoc, deletebm);
	loc_delete_button->SetFont(f);
	loc_delete_button->SetBitmapDisabled(delete_disbm);
	loc_delete_button->Enable(false);
	lb3sizer->Add(loc_delete_button, 0, wxALL, 1);
	wxString ldl = wxT("\n");
	ldl += _("Delete a Station Location");
	loc_delete_label = new wxStaticText(lb3Panel, -1, ldl, wxDefaultPosition, wxSize(tw, th));
	lb3sizer->Add(loc_delete_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb3Panel, 0, wxALL, 1);

	wxPanel* lb4Panel = new wxPanel(lbuttons);
	lb4Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* lb4sizer = new wxBoxSizer(wxHORIZONTAL);
	lb4Panel->SetSizer(lb4sizer);

	loc_prop_button = new wxBitmapButton(lb4Panel, tl_PropLoc, propertiesbm);
	loc_prop_button->SetFont(f);
	loc_prop_button->SetBitmapDisabled(properties_disbm);
	loc_prop_button->Enable(false);
	lb4sizer->Add(loc_prop_button, 0, wxALL, 1);
	wxString lpl = wxT("\n");
	lpl += _("Display Station Location Properties");
	loc_prop_label = new wxStaticText(lb4Panel, -1, lpl, wxDefaultPosition, wxSize(tw, th));
	lb4sizer->Add(loc_prop_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb4Panel, 0, wxALL, 1);

	notebook->AddPage(loctab, _("Station Locations"));

	// Certificates tab

	wxPanel* certtab = new wxPanel(notebook, -1);

	wxBoxSizer* certsizer = new wxBoxSizer(wxHORIZONTAL);
	certtab->SetSizer(certsizer);

	wxPanel* certgrid = new wxPanel(certtab, -1);
	certgrid->SetBackgroundColour(defBkg);
	wxBoxSizer* cgsizer = new wxBoxSizer(wxVERTICAL);
	certgrid->SetSizer(cgsizer);

	cert_tree = new CertTree(certgrid, tc_CertTree, wxDefaultPosition,
		wxDefaultSize, wxTR_DEFAULT_STYLE | wxBORDER_NONE); //wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);

	cert_tree->SetBackgroundColour(defBkg);
	cgsizer->Add(cert_tree, 1, wxEXPAND);

	wxString csq = wxT("\n");
	csq += _("Select a Callsign Certificate to process");
	cert_select_label = new wxStaticText(certgrid, -1, csq);
	cgsizer->Add(cert_select_label, 0, wxALL, 1);

	certsizer->Add(certgrid, 50, wxEXPAND);

	wxStaticLine *certsep = new wxStaticLine(certtab, -1, wxDefaultPosition, wxSize(2, -1), wxLI_VERTICAL);
	certsizer->Add(certsep, 0, wxEXPAND);

	wxPanel* cbuttons = new wxPanel(certtab, -1);
	cbuttons->SetBackgroundColour(defBkg);
	certsizer->Add(cbuttons, 50, wxEXPAND, 0);

	wxBoxSizer* cbsizer = new wxBoxSizer(wxVERTICAL);
	cbuttons->SetSizer(cbsizer);

	wxPanel* cb1Panel = new wxPanel(cbuttons);
	cb1Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* cb1sizer = new wxBoxSizer(wxHORIZONTAL);
	cb1Panel->SetSizer(cb1sizer);

	cert_load_button = new wxBitmapButton(cb1Panel, tc_Load, importbm);
	cert_load_button->SetFont(f);
	cert_load_button->SetBitmapDisabled(delete_disbm);
	cb1sizer->Add(cert_load_button, 0, wxALL, 1);
	wxString lcl = wxT("\n");
	lcl += _("Load a Callsign Certificate");
	cert_load_label = new wxStaticText(cb1Panel, -1, lcl, wxDefaultPosition, wxSize(tw, th));
	cert_load_button->SetLabel(lcl);
	cb1sizer->Add(cert_load_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb1Panel, 0, wxALL, 1);

	wxPanel* cb2Panel = new wxPanel(cbuttons);
	cb2Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* cb2sizer = new wxBoxSizer(wxHORIZONTAL);
	cb2Panel->SetSizer(cb2sizer);

	cert_save_button = new wxBitmapButton(cb2Panel, tc_CertSave, downloadbm);
	cert_save_button->SetFont(f);
	cert_save_button->SetBitmapDisabled(download_disbm);
	cert_save_button->Enable(false);
	cb2sizer->Add(cert_save_button, 0, wxALL, 1);
	wxString csl = wxT("\n");
	csl += _("Save a Callsign Certificate");
	cert_save_label = new wxStaticText(cb2Panel, -1, csl, wxDefaultPosition, wxSize(tw, th));
	cb2sizer->Add(cert_save_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb2Panel, 0, wxALL, 1);

	wxPanel* cb3Panel = new wxPanel(cbuttons);
	cb3Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* cb3sizer = new wxBoxSizer(wxHORIZONTAL);
	cb3Panel->SetSizer(cb3sizer);

	cert_renew_button = new wxBitmapButton(cb3Panel, tc_CertRenew, uploadbm);
	cert_renew_button->SetFont(f);
	cert_renew_button->SetBitmapDisabled(upload_disbm);
	cert_renew_button->Enable(false);
	cb3sizer->Add(cert_renew_button, 0, wxALL, 1);
	wxString crl = wxT("\n");
	crl += _("Renew a Callsign Certificate");
	cert_renew_label = new wxStaticText(cb3Panel, -1, crl, wxDefaultPosition, wxSize(tw, th));
	cb3sizer->Add(cert_renew_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb3Panel, 0, wxALL, 1);

	wxPanel* cb4Panel = new wxPanel(cbuttons);
	cb4Panel->SetBackgroundColour(defBkg);
	wxBoxSizer* cb4sizer = new wxBoxSizer(wxHORIZONTAL);
	cb4Panel->SetSizer(cb4sizer);

	cert_prop_button = new wxBitmapButton(cb4Panel, tc_CertProp, propertiesbm);
	cert_prop_button->SetFont(f);
	cert_prop_button->SetBitmapDisabled(properties_disbm);
	cert_prop_button->Enable(false);
	cb4sizer->Add(cert_prop_button, 0, wxALL, 1);
	wxString dcl = wxT("\n");
	dcl += _("Display a Callsign Certificate's Properties");
	cert_prop_label = new wxStaticText(cb4Panel, -1, dcl, wxDefaultPosition, wxSize(tw, th));
	cb4sizer->Add(cert_prop_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb4Panel, 0, wxALL, 1);

	notebook->AddPage(certtab, _("Callsign Certificates"));

	// Status Log tab (if enabled)
	if (logTab) {
		wxPanel* logtab = new wxPanel(notebook, -1);
		wxBoxSizer* ltsizer = new wxBoxSizer(wxHORIZONTAL);
		logtab->SetSizer(ltsizer);
		logwin = new wxTextCtrl(logtab, -1, wxT(""), wxDefaultPosition, wxSize(800, 600),
			wxTE_MULTILINE|wxTE_READONLY);
		ltsizer->Add(logwin, 1, wxEXPAND);
		notebook->AddPage(logtab, _("Status Log"));
	}

	//app icon
	SetIcon(wxIcon(key_xpm));

	if (checkUpdates) {
		LogList *log = new LogList(this);
#if wxMAJOR_VERSION > 2
		log->SetFormatter(new SimpleLogFormatter);
#endif
		wxLog::SetActiveTarget(log);
	}
}

static wxString
run_station_wizard(wxWindow *parent, tQSL_Location loc, wxHtmlHelpController *help = 0,
	bool expired = false, bool editing = false, wxString title = _("Add Station Location"), wxString dataname = wxT(""), wxString callsign = wxT("")) {
	tqslTrace("run_station_wizard", "loc=%lx, expired=%d, editing=%d, title=%s, dataname=%s, callsign=%s", loc, expired, editing, S(title), S(dataname), S(callsign));
	wxString rval(wxT(""));
	get_certlist("", 0, expired, false, false);
	if (ncerts == 0)
		throw TQSLException("No certificates available");
	TQSLWizard *wiz = new TQSLWizard(loc, parent, help, title, expired);
	wiz->SetDefaultCallsign(callsign);
	wiz->GetPage(true);
	TQSLWizLocPage *cpage = reinterpret_cast<TQSLWizLocPage*>(wiz->GetPage());
	if (cpage && !callsign.IsEmpty())
		cpage->UpdateFields(0);
	TQSLWizPage *page = wiz->GetPage();
	if (page == 0)
		throw TQSLException("Error getting first wizard page");
	wiz->AdjustSize();
	// Note: If dynamically created pages are larger than the pages already
	// created (the initial page and the final page), the wizard will need to
	// be resized, but we don't presently have that happening. (The final
	// page is larger than all expected dynamic pages.)
	bool okay = wiz->RunWizard(page);
	rval = wiz->GetLocationName();
	wiz->Destroy();
	if (!okay)
		return rval;
	wxCharBuffer locname = rval.ToUTF8();
	check_tqsl_error(tqsl_setStationLocationCaptureName(loc, locname.data()));
	check_tqsl_error(tqsl_saveStationLocationCapture(loc, 1));
	return rval;
}

void
MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event)) {
	help->Display(wxT("main.htm"));
}

// Return the "About" string
//
static wxString getAbout() {
	wxString msg = wxT("TQSL V") wxT(TQSL_VERSION) wxT(" build ") wxT(TQSL_BUILD);
#ifdef OSX_PLATFORM
	msg += wxT("\nBuilt for ") wxT(OSX_PLATFORM);
#endif
	msg += wxT("\n(c) 2001-2022 American Radio Relay League\n\n");
	int major, minor;
	if (tqsl_getVersion(&major, &minor))
		wxLogError(getLocalizedErrorString());
	else
		msg += wxString::Format(wxT("TrustedQSL library V%d.%d\n"), major, minor);
	if (tqsl_getConfigVersion(&major, &minor))
		wxLogError(getLocalizedErrorString());
	else
		msg += wxString::Format(wxT("\nConfiguration data V%d.%d\n\n"), major, minor);
	msg += wxVERSION_STRING;
#ifdef wxUSE_UNICODE
	if (wxUSE_UNICODE)
		msg += wxT(" (Unicode)");
#endif
	XML_Expat_Version xv = XML_ExpatVersionInfo();

	msg+=wxString::Format(wxT("\nexpat v%d.%d.%d\n"), xv.major, xv.minor, xv.micro);
	msg+=wxString::Format(wxT("libcurl V%hs\n"), LIBCURL_VERSION);
	msg+=wxString::Format(wxT("%hs\n"), OPENSSL_VERSION_TEXT);
	msg+=wxString::Format(wxT("zlib V%hs\n"), ZLIB_VERSION);
#ifdef USE_LMDB
	msg+=wxString::Format(wxT("%hs"), MDB_VERSION_STRING);
#else
	msg+=wxString::Format(wxT("%hs"), DB_VERSION_STRING);
#endif
	msg+=wxT("\n\n\nTranslators:\n");
	msg+=wxT("Catalan: Xavi, EA3W and Salva, EB3MA\n");
	msg+=wxT("Chinese (Simplified): Caros, BH4TXN\n");
	msg+=wxT("Chinese (Traditional): SZE-TO Wing, VR2UPU\n");
	msg+=wxT("Finnish: Juhani Tapaninen, OH8MXL\n");
	msg+=wxT("French: Laurent Beugnet, F6GOX\n");
	msg+=wxT("German: Andreas Rehberg, DF4WC\n");
	msg+=wxT("Hindi: Manmohan Bhagat, VU3YBH\n");
	msg+=wxT("Italian: Salvatore Besso, I4FYV\n");
	msg+=wxT("Japanese: Akihiro KODA, JL3OXR\n");
	msg+=wxString::FromUTF8("Polish: Roman Bagiński, SP4JEU\n");
	msg+=wxT("Portuguese: Nuno Lopes, CT2IRY\n");
	msg+=wxT("Russian: Vic Goncharsky, US5WE\n");
	msg+=wxT("Spanish: Jordi Quintero, EA3GCV\n");
	msg+=wxT("Swedish: Roger Jonsson, SM0LTV\n");
	msg+=wxT("Turkish: Oguzhan Kayhan, TA2NC\n");
	return msg;
}

void
MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxMessageBox(getAbout(), _("About"), wxOK | wxCENTRE | wxICON_INFORMATION, this);
}

void
MyFrame::OnHelpDiagnose(wxCommandEvent& event) {
	wxString s_fname;

	if (tqsl_diagFileOpen()) {
		file_menu->Check(tm_f_diag, false);
		tqsl_closeDiagFile();
		wxMessageBox(wxT("Diagnostic log closed"), wxT("Diagnostics"), wxOK | wxCENTRE| wxICON_INFORMATION, this);
		return;
	}
	s_fname = wxFileSelector(_("Log File"), wxT(""), wxT("tqsldiag.log"), wxT("log"),
			_("Log files (*.log)|*.log|All files (*.*)|*.*"),
			wxFD_SAVE|wxFD_OVERWRITE_PROMPT, this);
	if (s_fname.IsEmpty()) {
		file_menu->Check(tm_f_diag, false); //would be better to not check at all, but no, apparently that's a crazy thing to want
		return;
	}
	if (tqsl_openDiagFile(s_fname.ToUTF8())) {
		wxString errmsg = wxString::Format(_("Error opening diagnostic log %s: %hs"), s_fname.c_str(), strerror(errno));
		wxMessageBox(errmsg, _("Log File Error"), wxOK | wxICON_EXCLAMATION);
		return;
	}
	file_menu->Check(tm_f_diag, true);
	wxString about = getAbout();
#ifdef _WIN32
	about.Replace(wxT("\\n"), wxT("\\r\\n"));
#endif
	tqslTrace(NULL, "TQSL Diagnostics\r\n%s\r\n\r\n", (const char *)about.ToUTF8());
	tqslTrace(NULL, "Command Line: %s\r\n", (const char *)origCommandLine.ToUTF8());
	tqslTrace(NULL, "Working Directory:%s\r\n", tQSL_BaseDir);
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

static void
AddEditStationLocation(tQSL_Location loc, bool expired = false, const wxString& title = _("Add Station Location"), const wxString& callsign = wxT("")) {
	tqslTrace("AddEditStationLocation", "loc=%lx, expired=%lx, title=%s, callsign=%s", loc, expired, S(title), S(callsign));
	try {
		run_station_wizard(frame, loc, frame->help, expired, true, title, wxT(""), callsign);
		frame->loc_tree->Build();
	}
	catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::AddStationLocation(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::AddStationLocation", NULL);
	wxTreeItemId root = loc_tree->GetRootItem();
	wxTreeItemId id = loc_tree->GetSelection();
	wxString call;
	if (id != root && id.IsOk()) {		// If something selected
		LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(id));
		if (data) {
			call = data->getCallSign();		// Station location
		} else {
			call = loc_tree->GetItemText(id);	// Callsign folder selected
		}
		tqslTrace("MyFrame::AddStationLocation", "Call selected is %s", S(call));
	}
	tQSL_Location loc;
	if (tqsl_initStationLocationCapture(&loc)) {
		wxLogError(getLocalizedErrorString());
	}
	AddEditStationLocation(loc, false, _("Add Station Location"), call);
	if (tqsl_endStationLocationCapture(&loc)) {
		wxLogError(getLocalizedErrorString());
	}
	loc_tree->Build();
	LocTreeReset();
	AutoBackup();
}

void
MyFrame::EditStationLocation(wxCommandEvent& event) {
	tqslTrace("MyFrame::EditStationLocation", NULL);
	if (event.GetId() == tl_EditLoc) {
		try {
			LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(loc_tree->GetSelection()));
			tQSL_Location loc;
			wxString selname;
			char errbuf[512];

			if (data == NULL) return;

			check_tqsl_error(tqsl_getStationLocation(&loc, data->getLocname().ToUTF8()));
			if (!verify_cert(loc, true))	// Check if there is a certificate before editing
				return;
			check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
			if (strlen(errbuf) > 0) {
				wxString fmt = wxT("%hs\n");
				// TRANSLATORS: uncommon error - error in a station location, followed by the ignore message that follows.
				fmt += _("The invalid data was ignored.");
				wxMessageBox(wxString::Format(fmt, errbuf), _("Location data error"), wxOK | wxICON_EXCLAMATION, this);
			}
			char loccall[512];
			check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
			selname = run_station_wizard(this, loc, help, true, true, wxString::Format(_("Edit Station Location : %hs - %s"), loccall, data->getLocname().c_str()), data->getLocname());
			check_tqsl_error(tqsl_endStationLocationCapture(&loc));
			loc_tree->Build();
			LocTreeReset();
			AutoBackup();
			return;
		}
		catch(TQSLException& x) {
			wxLogError(wxT("%hs"), x.what());
			return;
		}
	}
	// How many locations are there?
	try {
		int n;
		tQSL_Location loc;
		check_tqsl_error(tqsl_initStationLocationCapture(&loc));
		check_tqsl_error(tqsl_getNumStationLocations(loc, &n));
		if (n == 1) {
			// There's only one station location. Use that and don't prompt.
			char deflocn[512];
			check_tqsl_error(tqsl_getStationLocationName(loc, 0, deflocn, sizeof deflocn));
			wxString locname = wxString::FromUTF8(deflocn);
			tqsl_endStationLocationCapture(&loc);
			check_tqsl_error(tqsl_getStationLocation(&loc, deflocn));
			char loccall[512];
			check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
			run_station_wizard(this, loc, help, true, true, wxString::Format(_("Edit Station Location : %hs - %s"), loccall, locname.c_str()), locname);
			check_tqsl_error(tqsl_endStationLocationCapture(&loc));
			loc_tree->Build();
			LocTreeReset();
			AutoBackup();
			return;
		}
		// More than one location or not selected in the tree. Prompt for the location.
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
		SelectStationLocation(_("Edit Station Location"), _("Close"), true);
		loc_tree->Build();
		LocTreeReset();
	}
	catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
		return;
	}
}

static tqsl_adifFieldDefinitions fielddefs[] = {
	{ "CALL", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, 0, 0 },
	{ "BAND", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, 0, 0 },
	{ "BAND_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, 0, 0 },
	{ "MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, 0, 0 },
	{ "SUBMODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, 0, 0 },
	{ "FREQ", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, 0, 0 },
	{ "FREQ_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, 0, 0 },
	{ "QSO_DATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 8, 0, 0, 0, 0 },
	{ "TIME_ON", "", TQSL_ADIF_RANGE_TYPE_NONE, 6, 0, 0, 0, 0 },
	{ "SAT_NAME", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_SATNAME_MAX, 0, 0, 0, 0 },
	{ "PROP_MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_PROPMODE_MAX, 0, 0, 0, 0 },
	{ "*", "", TQSL_ADIF_RANGE_TYPE_NONE, 2048, 0, 0, 0, 0 },
	{ "EOR", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, 0, 0 },
	{ "", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, 0, 0 },
};

static const char *defined_types[] = { "T", "D", "M", "C", "N", "6" };

static unsigned char *
adif_alloc(size_t n) {
	return new unsigned char[n];
}

static void
loadQSOfile(wxString& file, QSORecordList& recs) {
	init_modes();
	tqsl_adifFieldResults field;
	TQSL_ADIF_GET_FIELD_ERROR stat;
	tQSL_ADIF adif;
	if (tqsl_beginADIF(&adif, file.ToUTF8())) {
		wxLogError(getLocalizedErrorString());
	}
	QSORecord rec;
	do {
		if (tqsl_getADIFField(adif, &field, &stat, fielddefs, defined_types, adif_alloc)) {
			wxLogError(getLocalizedErrorString());
		}
		if (stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
			if (!strcasecmp(field.name, "CALL")) {
				rec._call = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "BAND")) {
				rec._band = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "BAND_RX")) {
				rec._rxband = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "MODE")) {
				rec._mode = wxString::FromUTF8((const char *)field.data);
				char amode[40];
				if (tqsl_getADIFMode(rec._mode.ToUTF8(), amode, sizeof amode) == 0 && amode[0] != '\0')
					rec._mode = wxString::FromUTF8(amode);
			} else if (!strcasecmp(field.name, "SUBMODE")) {
				char amode[40];
				if (tqsl_getADIFMode((const char *)field.data, amode, sizeof amode) == 0 && amode[0] != '\0')
					rec._mode = wxString::FromUTF8(amode);
			} else if (!strcasecmp(field.name, "FREQ")) {
				rec._freq = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "FREQ_RX")) {
				rec._rxfreq = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "PROP_MODE")) {
				rec._propmode = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "SAT_NAME")) {
				rec._satellite = wxString::FromUTF8((const char *)field.data);
			} else if (!strcasecmp(field.name, "QSO_DATE")) {
				char *cp = reinterpret_cast<char *>(field.data);
				if (strlen(cp) == 8) {
					rec._date.day = strtol(cp+6, NULL, 10);
					*(cp+6) = '\0';
					rec._date.month = strtol(cp+4, NULL, 10);
					*(cp+4) = '\0';
					rec._date.year = strtol(cp, NULL, 10);
				}
			} else if (!strcasecmp(field.name, "TIME_ON")) {
				char *cp = reinterpret_cast<char *>(field.data);
				if (strlen(cp) >= 4) {
					rec._time.second = (strlen(cp) > 4) ? strtol(cp+4, NULL, 10) : 0;
					*(cp+4) = 0;
					rec._time.minute = strtol(cp+2, NULL, 10);
					*(cp+2) = '\0';
					rec._time.hour = strtol(cp, NULL, 10);
				}
			} else if (!strcasecmp(field.name, "EOH")) {
				rec._extraFields.clear();
			} else if (!strcasecmp(field.name, "EOR")) {
				recs.push_back(rec);
				rec = QSORecord();
			} else {
				// Not a field I recognize, add it to the extras
				const char * val = reinterpret_cast<const char *>(field.data);
				if (!val)
					val = "";
				rec._extraFields[field.name] = val;
			}
			delete[] field.data;
		}
	} while (stat == TQSL_ADIF_GET_FIELD_SUCCESS || stat == TQSL_ADIF_GET_FIELD_NO_NAME_MATCH);
	tqsl_endADIF(&adif);
	return;
}

void
MyFrame::EditQSOData(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::EditQSOData", NULL);
	QSORecordList recs;
	wxString file = wxFileSelector(_("Open File"), wxConfig::Get()->Read(wxT("QSODataPath"), wxT("")), wxT(""), wxT("adi"),
#if !defined(__APPLE__) && !defined(_WIN32)
			_("ADIF files (*.adi;*.adif;*.ADI;*.ADIF)|*.adi;*.adif;*.ADI;*.ADIF|All files (*.*)|*.*"),
#else
			_("ADIF files (*.adi;*.adif)|*.adi;*.adif|All files (*.*)|*.*"),
#endif
			wxFD_OPEN|wxFD_FILE_MUST_EXIST, this);
	if (file.IsEmpty())
		return;
	loadQSOfile(file, recs);
	try {
		QSODataDialog dial(this, file, help, &recs);
		dial.ShowModal();
	} catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::EnterQSOData(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::EnterQSOData", NULL);
	QSORecordList recs;
	wxString file = wxT("tqsl.adi");
	try {
		QSODataDialog dial(this, file, help, &recs);
		dial.ShowModal();
	} catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

int MyFrame::ConvertLogToString(tQSL_Location loc, const wxString& infile, wxString& output, int& numrecs, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, int logverify, const char* password, const char* defcall) {
	tqslTrace("MyFrame::ConvertLogToString", "loc = %lx, infile=%s, suppressdate=%d, startdate=0x%lx, enddate=0x%lx, action=%d, logverify=%d defcall=%s", reinterpret_cast<void *>(loc), S(infile), suppressdate, reinterpret_cast<void *>(startdate), reinterpret_cast<void *>(enddate), action, logverify, defcall ? defcall : "");
	static const char *iam = "TQSL V" TQSL_VERSION;
	const char *cp;
	char callsign[40];
	int dxcc = 0;
	wxString name, ext;
	bool allow_dupes = false;
	bool restarting = false;
	bool ignore_err = false;
	bool show_dupes = false;
	int tmp;
	DXCC dx;
	wxString msg;

	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Read(wxT("DispDupes"), &show_dupes, DEFAULT_DISP_DUPES);

	if (logverify == -1) {		// Not on command line
		config->Read(wxT("LogVerify"), &tmp, TQSL_LOC_REPORT);
		if (tmp == TQSL_LOC_IGNORE || tmp == TQSL_LOC_REPORT || tmp == TQSL_LOC_UPDATE) {
			logverify = tmp;
		} else {
			logverify = TQSL_LOC_REPORT;
		}
	} else {			// Command line
		show_dupes = true;	// So always display dupe details
		if (logverify == TQSL_LOC_UPDATE) {
			tqsl_getLocationCallSign(loc, callsign, sizeof callsign);
			if (strcmp(callsign, "[None]") == 0) {
				get_certlist("", 0, false, false, true);	// any call, any DXCC, not expired or superceded
				goto skip_call;
			}
		}
	}

	try {
		if (defcall) {
			strncpy(callsign, defcall, sizeof callsign);
			check_tqsl_error(tqsl_setLocationCallSign(loc, callsign, -1));
		} else {
			check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		}
	} catch(TQSLException &x) {
		wxLogError(wxT("%hs"), x.what());
		return TQSL_EXIT_LIB_ERROR;
	}
	tqsl_getLocationDXCCEntity(loc, &dxcc);
	dx.getByEntity(dxcc);

	get_certlist(callsign, dxcc, false, false, false);
	if (ncerts == 0) {
		// No certificates for this callsign/dxcc pair.
		// If the callsign for this location is set to "[None]", go look
		// for a suitable callsign certificate
		if (strcmp(callsign, "[None]") == 0) {
			// Get all of the certificates for this location's DXCC entity
			get_certlist("", dxcc, false, false, false);
			// If only one callsign matches the dxcc entity, just use that
			if (ncerts == 1) {
				tqsl_getCertificateCallSign(certlist[0], callsign, sizeof callsign);
				tqsl_setLocationCallSign(loc, callsign, -1);
			} else if (ncerts > 1) {
				wxString *choices = new wxString[ncerts];
				// Get today's date
				time_t t = time(0);
				struct tm *tm = gmtime(&t);
				tQSL_Date d;
				d.year = tm->tm_year + 1900;
				d.month = tm->tm_mon + 1;
				d.day = tm->tm_mday;
				tQSL_Date exp;
				int def_index = 0;
				int oldest = -1;
				for (int i = 0; i < ncerts; i++) {
					tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign);
					choices[i] = wxString::FromUTF8(callsign);
					if (0 == tqsl_getCertificateNotAfterDate(certlist[i], &exp)) {
						int days_left;
						tqsl_subtractDates(&d, &exp, &days_left);
						if (days_left > oldest) {
							def_index = i;
							oldest = days_left;
						}
					}
				}
				wxSingleChoiceDialog dialog(this, _("Please choose a callsign for this Station Location"),
							_("Select Callsign"), ncerts, choices);
				dialog.SetSelection(def_index);
				int idx;
				if (dialog.ShowModal() == wxID_OK)
					idx = dialog.GetSelection();
				else
					return TQSL_EXIT_CANCEL;
				tqsl_getCertificateCallSign(certlist[idx], callsign, sizeof callsign);
				get_certlist(callsign, dxcc, false, false, false);
				tqsl_setLocationCallSign(loc, callsign, -1);
			}

			char loc_name[128];
			if (tqsl_getStationLocationCaptureName(loc, loc_name, sizeof loc_name)) {
				strncpy(loc_name, "Unknown", sizeof loc_name);
			}
			LocPropDial dial(wxString::FromUTF8(loc_name), false, infile.ToUTF8(), this);
			int locnOK = dial.ShowModal();
			if (locnOK != wxID_OK)  {
				return TQSL_EXIT_CANCEL;
			}
		}
	} else {
		// The defcall or callsign from the location has matching certificates
		// for this DXCC location. If we used the default call, set that
		// for the location
		if (defcall) {
			tqsl_setLocationCallSign(loc, defcall, -1);
		}
	}
	if (ncerts == 0) {
		wxString fmt = _("There are no valid callsign certificates for callsign");
		fmt += wxT(" %hs ");
		fmt += _("in entity");
		fmt += wxT(" %hs.\n");
		fmt += _("Signing aborted.");
		fmt + wxT("\n");
		msg = wxString::Format(fmt, callsign, dx.name());
		wxLogError(msg);
		return TQSL_EXIT_TQSL_ERROR;
	}

	// Re-load the certificate list with all valid calls
	free_certlist();
	get_certlist("", 0, false, false, false);

	msg = _("Signing using Callsign %hs, DXCC Entity %hs");
	msg += wxT("\n");
	wxLogMessage(msg, callsign, dx.name());

 skip_call:

	init_modes();
	init_contests();

	wxString dgMap;

#ifdef MAP_CABRILLO_MODE
	config->Read(wxT("CabrilloDGMap"), &dgMap, DEFAULT_CABRILLO_DG_MAP);
	tqsl_setCabrilloDGMap(dgMap.ToUTF8());
#endif
	if (lock_db(false) < 0) {
		if (quiet) {			// If the database is locked, don't stall if in batch mode.
			return TQSL_EXIT_BUSY;
		}
		wxSafeYield();
		wxLogMessage(_("TQSL must wait for other running copies of TQSL to exit before signing..."));
		wxSafeYield();
		lock_db(true);
	}

 restart:

	ConvertingDialog *conv_dial = new ConvertingDialog(this, infile.ToUTF8());
	numrecs = 0;
	bool cancelled = false;
	bool aborted = false;
	int lineno = 0;
	int out_of_range = 0;
	int duplicates = 0;
	int processed = 0;
	int errors = 0;
	try {
		if (tqsl_beginCabrilloConverter(&logConv, infile.ToUTF8(), certlist, ncerts, loc)) {
			if (tQSL_Error != TQSL_CABRILLO_ERROR || tQSL_Cabrillo_Error != TQSL_CABRILLO_NO_START_RECORD)
				check_tqsl_error(1);	// A bad error
			lineno = 0;
			check_tqsl_error(tqsl_beginADIFConverter(&logConv, infile.ToUTF8(), certlist, ncerts, loc));
		}
		bool range = true;
		config->Read(wxT("DateRange"), &range);
		if (range && !suppressdate && !restarting) {
			DateRangeDialog dial(this);
			if (dial.ShowModal() != wxOK) {
				wxLogMessage(_("Cancelled"));
				unlock_db();
				return TQSL_EXIT_CANCEL;
			}
			tqsl_setADIFConverterDateFilter(logConv, &dial.start, &dial.end);
			if (this->IsQuiet()) {
				this->Show(false);
				wxSafeYield(this);
			}
		}
		if (startdate || enddate) {
			tqslTrace("MyFrame::ConvertLogToString", "startdate %d/%d/%d, enddate %d/%d/%d",
					startdate ? startdate->year : 0,
					startdate ? startdate->month : 0,
					startdate ? startdate->day : 0,
					enddate ? enddate->year : 0,
					enddate ? enddate->month : 0,
					enddate ? enddate->day : 0);
			tqsl_setADIFConverterDateFilter(logConv, startdate, enddate);
		}
		bool allow = false;
		config->Read(wxT("BadCalls"), &allow);
		tqsl_setConverterAllowBadCall(logConv, allow);
		tqsl_setConverterAllowDuplicates(logConv, allow_dupes);
		config->Read(wxT("IgnoreSeconds"), &allow, DEFAULT_IGNORE_SECONDS);
		tqsl_setConverterIgnoreSeconds(logConv, allow);
		tqsl_setConverterAppName(logConv, iam);
		tqsl_setConverterQTHDetails(logConv, logverify);

		wxFileName::SplitPath(infile, 0, &name, &ext);
		if (!ext.IsEmpty())
			name += wxT(".") + ext;
		// Only display windows if notin batch mode -- KD6PAG
		if (!this->IsQuiet()) {
			conv_dial->Show(TRUE);
		}
		this->Enable(FALSE);

		output = wxT("");

		do {
			while ((cp = tqsl_getConverterGABBI(logConv)) != 0) {
				if (!this->IsQuiet())
					wxSafeYield(conv_dial);
				if (!conv_dial->running)
					break;
				// Only count QSO records
				if (strstr(cp, "tCONTACT")) {
					++numrecs;
					++processed;
				}
				if ((processed % 10) == 0) {
					wxString progress = wxString::Format(_("QSOs: %d"), processed);
					if (!allow_dupes && duplicates > 0)
						progress += wxT(" ") + wxString::Format(_("Already Uploaded: %d"), duplicates);
					if (errors > 0 || out_of_range > 0)
						progress += wxT(" ") + wxString::Format(_("Errors: %d"), errors + out_of_range);
					conv_dial->msg->SetLabel(progress);
				}
				output << (wxString::FromUTF8(cp) + wxT("\n"));
			}
			if ((processed % 10) == 0) {
				wxString progress = wxString::Format(_("QSOs: %d"), processed);
				if (!allow_dupes && duplicates > 0)
					progress += wxT(" ") + wxString::Format(_("Already uploaded: %d"), duplicates);
				if (errors > 0 || out_of_range > 0)
					progress += wxT(" ") + wxString::Format(_("Errors: %d"), errors + out_of_range);
				conv_dial->msg->SetLabel(progress);
			}
			// Handle large logs already uploaded.
			// More than 50,000 QSOs. 80% dupes or more (40,000 Qs).
			// Running with all QSOs allowed
			if (processed > 50000 && duplicates > 40000 && allow_dupes) {
				wxLogError(_("This log has too many previously uploaded QSOs. Please only upload new QSOs or break the log into smaller pieces (50,000 QSOs or less)."));
				aborted = true;
				break;
			}
			if (cp == 0) {
				if (!this->IsQuiet())
					wxSafeYield(conv_dial);
				if (!conv_dial->running)
					break;
			}
			if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
				tQSL_Cert cert;
				int rval;
				check_tqsl_error(tqsl_getConverterCert(logConv, &cert));
				do {
					if ((rval = tqsl_beginSigning(cert, const_cast<char *>(password), getCertPassword, cert)) == 0)
						break;
					if (tQSL_Error == TQSL_PASSWORD_ERROR) {
						if ((rval = tqsl_beginSigning(cert, const_cast<char *>(unipwd), NULL, cert)) == 0)
							break;
						wxLogMessage(_("Passphrase error"));
						if (password)
							free(reinterpret_cast<void *>(const_cast<char *>(password)));
						password = NULL;
					}
				} while (tQSL_Error == TQSL_PASSWORD_ERROR);
				if (tQSL_Error == TQSL_CUSTOM_ERROR && (tQSL_Errno == ENOENT || tQSL_Errno == EPERM)) {
					snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
						"Can't open the private key file for %s: %s", callsign, strerror(tQSL_Errno));
				}
				check_tqsl_error(rval);
				if (this->IsQuiet()) {
					this->Show(false);
					wxSafeYield(this);
				}
				continue;
			}
			if (tQSL_Error == TQSL_DATE_OUT_OF_RANGE) {
				processed++;
				out_of_range++;
				continue;
			}
			char dupeErrString[256];
			dupeErrString[0] = '\0';
			bool dupe_error = (tQSL_Error == TQSL_DUPLICATE_QSO);
			if (dupe_error) {
				strncpy(dupeErrString, tQSL_CustomError, sizeof dupeErrString);
				duplicates++;
				if (!show_dupes) {
					processed++;
					continue;
				}
			}
			bool has_error = (tQSL_Error != TQSL_NO_ERROR);
			if (has_error) {
				processed++;
				if (!dupe_error) {
					errors++;
				}
				try {
					check_tqsl_error(1);
				} catch(TQSLException& x) {
					tqsl_getConverterLine(logConv, &lineno);
					wxString msg = wxGetTranslation(wxString::FromUTF8(x.what()));
					if (lineno)
						msg += wxT(" ") + wxString::Format(_("on line %d"), lineno);
					const char *bad_text = tqsl_getConverterRecordText(logConv);
					if (bad_text)
						msg += wxString(wxT("\n")) + wxString::FromUTF8(bad_text);

					if (dupeErrString[0] != '\0') {
						wxStringTokenizer dupes(wxString::FromUTF8(dupeErrString), wxT("|"));
						wxString olddupe = dupes.GetNextToken();
						wxString newdupe = dupes.GetNextToken();
						if (olddupe != newdupe) {
							msg += wxT("\n") + wxString::Format(_("Your QTH Details changed for this QSO.\n\nOriginally these were: %s\nNow they are:%s\n\nPlease verify that you intended to change this QSO!\n"), olddupe.c_str(), newdupe.c_str());
						}
					}
					wxLogError(wxT("%s"), msg.c_str());
					if (frame->IsQuiet()) {
						switch (action) {
							case TQSL_ACTION_ABORT:
								aborted = true;
								ignore_err = true;
								goto abortSigning;
							case TQSL_ACTION_NEW:		// For ALL or NEW, let the signing proceed
							case TQSL_ACTION_ALL:
								ignore_err = true;
								break;
							case TQSL_ACTION_ASK:
							case TQSL_ACTION_UNSPEC:
								break;			// The error will show as a popup
						}
					}
					if (!ignore_err) {
						tqslTrace("MyFrame::ConvertLogToString", "Error: %s/asking for action", S(msg));
						wxString errmsg = msg + wxT("\n");
						errmsg += _("Click 'Ignore' to continue signing this log while ignoring errors.");
						errmsg += wxT("\n");
						errmsg += _("Click 'Cancel' to abandon signing this log.");
						ErrorsDialog dial(this, errmsg);
						int choice = dial.ShowModal();
						if (choice == TQSL_AE_CAN) {
							cancelled = true;
							goto abortSigning;
						}
						if (choice == TQSL_AE_OK) {
							ignore_err = true;
							if (this->IsQuiet()) {
								this->Show(false);
								wxSafeYield(this);
							}
						}
					}
				}
			}
			tqsl_getErrorString();	// Clear error
			if (has_error && ignore_err)
				continue;
			break;
		} while (1);
		cancelled = !conv_dial->running;

 abortSigning:
		this->Enable(TRUE);

		if (cancelled) {
			wxLogWarning(_("Signing cancelled"));
			numrecs = 0;
		} else if (aborted) {
			wxLogWarning(_("Signing aborted"));
			numrecs = 0;
		} else if (tQSL_Error != TQSL_NO_ERROR) {
			check_tqsl_error(1);
		}
		delete conv_dial;
	} catch(TQSLException& x) {
		this->Enable(TRUE);
		delete conv_dial;
		string msg = x.what();
		tqsl_getConverterLine(logConv, &lineno);
		tqsl_converterRollBack(logConv);
		tqsl_endConverter(&logConv);
		unlock_db();
		if (lineno) {
			msg += " ";
			msg += wxString::Format(_("on line %d"), lineno).ToUTF8();
		}

		wxLogError(_("Signing aborted due to errors"));
		throw TQSLException(msg.c_str());
	}
	if (!cancelled && out_of_range > 0)
		wxLogMessage(_("%s: %d QSO records were outside the selected date range"),
			infile.c_str(), out_of_range);
	if (duplicates > 0) {
		if (cancelled || aborted) {
			tqsl_converterRollBack(logConv);
			tqsl_endConverter(&logConv);
			unlock_db();
			return TQSL_EXIT_CANCEL;
		}
		if (action == TQSL_ACTION_ASK || action == TQSL_ACTION_UNSPEC) { // want to ask the user
			DupesDialog dial(this, processed - errors - out_of_range, duplicates, action);
			int choice = dial.ShowModal();
			if (choice == TQSL_DP_CAN) {
				wxLogMessage(_("Cancelled"));
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				unlock_db();
				return TQSL_EXIT_CANCEL;
			}
			if (choice == TQSL_DP_ALLOW) {
				allow_dupes = true;
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				restarting = true;
				if (this->IsQuiet()) {
					this->Show(false);
					wxSafeYield(this);
				}
				goto restart;
			}
		} else if (action == TQSL_ACTION_ABORT) {
				if (processed == duplicates) {
					wxLogMessage(_("All QSOs are already uploaded; aborted"));
					tqsl_converterRollBack(logConv);
					tqsl_endConverter(&logConv);
					unlock_db();
					numrecs = 0;
					return TQSL_EXIT_NO_QSOS;
				} else {
					wxLogMessage(_("%d of %d QSOs are already uploaded; aborted"), duplicates, processed);
					tqsl_converterRollBack(logConv);
					tqsl_endConverter(&logConv);
					unlock_db();
					numrecs = 0;
					return TQSL_EXIT_NO_QSOS;
				}
		} else if (action == TQSL_ACTION_ALL) {
			allow_dupes = true;
			tqsl_converterRollBack(logConv);
			tqsl_endConverter(&logConv);
			restarting = true;
			goto restart;
		}
		// Otherwise it must be TQSL_ACTION_NEW, so fall through
		// and output the new records.
		if (!quiet) {
			wxLogMessage(_("%s: %d QSO records were already uploaded"),
			infile.c_str(), duplicates);
		} else {
			if (duplicates > 1)
				tqslTrace(NULL, "%d duplicate QSO records", duplicates);
			else
				tqslTrace(NULL, "%s", "Duplicate QSO record");
		}
	}
	//if (!cancelled) tqsl_converterCommit(logConv);
	if (cancelled || processed == 0) {
		tqsl_converterRollBack(logConv);
		tqsl_endConverter(&logConv);
	}
	unlock_db();
	if (cancelled)
		return TQSL_EXIT_CANCEL;
	if (numrecs == 0)
		return TQSL_EXIT_NO_QSOS;
	if (aborted || duplicates > 0 || out_of_range > 0 || errors > 0)
		return TQSL_EXIT_QSOS_SUPPRESSED;
	return TQSL_EXIT_SUCCESS;
}

// Errors from tqsllib that indicate bad QSO data.
// Add here so they're picked up for translation.
#ifdef tqsltranslate
static const char* qsoerrs[] = {
	__("Invalid contact - QSO does not specify a Callsign"),
	__("Invalid contact - QSO does not specify a band or frequency"),
	__("Invalid contact - QSO does not specify a mode"),
	__("Invalid contact - QSO does not specify a date"),
	__("Invalid contact - QSO does not specify a time"),
	// Piggy-back this error as well
	__("This callsign certificate is already active and cannot be restored.")
}
#endif

int
MyFrame::ConvertLogFile(tQSL_Location loc, const wxString& infile, const wxString& outfile,
	bool compressed, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, int logverify, const char *password, const char *defcall) {
	tqslTrace("MyFrame::ConvertLogFile", "loc=%lx, infile=%s, outfile=%s, compressed=%d, suppressdate=%d, startdate=0x%lx enddate=0x%lx action=%d, logverify=%d", reinterpret_cast<void *>(loc), S(infile), S(outfile), compressed, suppressdate, reinterpret_cast<void *>(startdate), reinterpret_cast<void*>(enddate), action, logverify);
	gzFile gout = 0;
#ifdef _WIN32
	int fd = -1;
#endif
	ofstream out;

	if (compressed) {
#ifdef _WIN32
		wchar_t* lfn = utf8_to_wchar(outfile.ToUTF8());
		fd = _wopen(lfn, _O_WRONLY |_O_CREAT|_O_BINARY, _S_IREAD|_S_IWRITE);
		free_wchar(lfn);
		if (fd != -1)
			gout = gzdopen(fd, "wb9");
#else
		gout = gzopen(outfile.ToUTF8(), "wb9");
#endif
	} else {
#ifdef _WIN32
		wchar_t* lfn = utf8_to_wchar(outfile.ToUTF8());
		out.open(lfn, ios::out|ios::trunc|ios::binary);
		free_wchar(lfn);
#else
		out.open(outfile.ToUTF8(), ios::out|ios::trunc|ios::binary);
#endif
	}

	if ((compressed && !gout) || (!compressed && !out)) {
		wxLogError(_("Unable to open %s for output"), outfile.c_str());
		return TQSL_EXIT_ERR_OPEN_OUTPUT;
	}

	wxString output;
	int numrecs = 0;
	int status = this->ConvertLogToString(loc, infile, output, numrecs, suppressdate, startdate, enddate, action, logverify, password, defcall);

	if (numrecs == 0) {
		wxLogMessage(_("No records output"));
		if (compressed) {
			gzclose(gout);
		} else {
			out.close();
		}
#ifdef _WIN32
		wchar_t* lfn = utf8_to_wchar(outfile.ToUTF8());
		_wunlink(lfn);
		free_wchar(lfn);
#else
		unlink(outfile.ToUTF8());
#endif
		if (status == TQSL_EXIT_CANCEL || status == TQSL_EXIT_QSOS_SUPPRESSED)
			return status;
		else
			return TQSL_EXIT_NO_QSOS;
	} else {
		if (status == TQSL_EXIT_CANCEL)
			return status;
		if(compressed) {
			if (gzwrite(gout, output.ToUTF8(), output.size()) <= 0) {
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				gzclose(gout);
				return TQSL_EXIT_LIB_ERROR;
			}
			if (gzflush(gout, Z_FINISH) != Z_OK) {
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				gzclose(gout);
				return TQSL_EXIT_LIB_ERROR;
			}
			if (gzclose(gout) != Z_OK) {
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				return TQSL_EXIT_LIB_ERROR;
			}
		} else {
			out << output;
			if (out.fail()) {
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				out.close();
				return TQSL_EXIT_LIB_ERROR;
			}
			out.close();
			if (out.fail()) {
				tqsl_converterRollBack(logConv);
				tqsl_endConverter(&logConv);
				return TQSL_EXIT_LIB_ERROR;
			}
		}

		tqsl_converterCommit(logConv);
		tqsl_endConverter(&logConv);

		wxLogMessage(_("%s: wrote %d records to %s"), infile.c_str(), numrecs,
			outfile.c_str());
		wxLogMessage(_("%s is ready to be emailed or uploaded."), outfile.c_str());
		wxLogMessage(_("Note: TQSL assumes that this file will be uploaded to LoTW."));
		wxLogMessage(_("Resubmitting these QSOs will cause them to be reported as already uploaded."));
		wxLogMessage(wxT(""));
		wxLogMessage(_("To submit the signed log file to LoTW:\n"
			     "1. Move the signed log file to a computer with internet access\n"
   		             "2. Log in to your LoTW Web Account\n"
			     "3. Select the Upload File tab\n"
			     "4. Click the Choose File button, and select the signed log file you created (%s)\n"
			     "5. Click the Upload file button\n\n"
			     "Alternatively, you can attach the signed log file to an email message, and send the message to lotw-logs@arrl.org"), outfile.c_str());
	}

	return status;
}

long compressToBuf(string& buf, const char* input) {
	tqslTrace("compressToBuf", NULL);
	const size_t TBUFSIZ = 128*1024;
	uint8_t* tbuf = new uint8_t[TBUFSIZ];

	//vector<uint8_t> buf;
	z_stream stream;
	stream.zalloc = 0;
	stream.zfree = 0;
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char *>(input));
	stream.avail_in = strlen(input);
	stream.next_out = tbuf;
	stream.avail_out = TBUFSIZ;

	//deflateInit(&stream, Z_BEST_COMPRESSION);
	deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 16+15, 9, Z_DEFAULT_STRATEGY); //use gzip header

	while (stream.avail_in) { //while still some left
		int res = deflate(&stream, Z_NO_FLUSH);
		assert(res == Z_OK);
		if (!stream.avail_out) {
			buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ);
			stream.next_out = tbuf;
			stream.avail_out = TBUFSIZ;
		}
	}

	do {
		if (stream.avail_out == 0) {
			buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ);
			stream.next_out = tbuf;
			stream.avail_out = TBUFSIZ;
		}
	} while (deflate(&stream, Z_FINISH) == Z_OK);

	buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ-stream.avail_out);
	deflateEnd(&stream);

	delete[] tbuf;

	return buf.length();
}


class UploadThread: public wxThread {
 public:
  UploadThread(CURL* handle_, wxDialog* dial_): wxThread(wxTHREAD_JOINABLE),
						handle(handle_), dial(dial_) {
		wxThread::Create();
	}
 protected:
	CURL* handle;
	wxDialog* dial;
	virtual wxThread::ExitCode Entry() {
		int ret = curl_easy_perform(handle);
		wxCommandEvent evt(wxEVT_LOGUPLOAD_DONE, wxID_ANY);
		dial->GetEventHandler()->AddPendingEvent(evt);
		return (wxThread::ExitCode)((intptr_t)ret);
	}
};

int MyFrame::UploadLogFile(tQSL_Location loc, const wxString& infile, bool compressed, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, int logverify, const char* password, const char *defcall) {
	tqslTrace("MyFrame::UploadLogFile", "infile=%s, compressed=%d, suppressdate=%d, action=%d, logverify=%d", S(infile), compressed, suppressdate, action, logverify);
	int numrecs = 0;
	wxString signedOutput;

	tqslTrace("MyFrame::UploadLogFile", "About to convert log to string");
	int status = this->ConvertLogToString(loc, infile, signedOutput, numrecs, suppressdate, startdate, enddate, action, logverify, password, defcall);
	tqslTrace("MyFrame::UploadLogFile", "Log converted, status = %d, numrecs=%d", status, numrecs);

	if (numrecs == 0) {
		wxLogMessage(_("No records to upload"));
		if (status == TQSL_EXIT_CANCEL || status == TQSL_EXIT_QSOS_SUPPRESSED)
			return status;
		else
			return TQSL_EXIT_NO_QSOS;
	} else {
		if (status == TQSL_EXIT_CANCEL)
			return status;
		//compress the upload
		tqslTrace("MyFrame::UploadLogFile", "Compressing");
		string compressed;
		size_t compressedSize = compressToBuf(compressed, (const char*)signedOutput.ToUTF8());
		tqslTrace("MyFrame::UploadLogFile", "Compressed to %d bytes", compressedSize);
		//ofstream f; f.open("testzip.tq8", ios::binary); f<<compressed; f.close(); //test of compression routine
		if (compressedSize == 0) {
			wxLogMessage(_("Error compressing before upload"));
			return TQSL_EXIT_TQSL_ERROR;
		}

		tqslTrace("MyFrame::UploadLogFile", "Creating filename");
		wxDateTime now = wxDateTime::Now().ToUTC();

		wxString name, ext;
		wxFileName::SplitPath(infile, 0, &name, &ext);
		name += wxT(".tq8");
		tqslTrace("MyFrame::UploadLogFile", "file=%s", S(name));
		//unicode mess. can't just use mb_str directly because it's a temp ptr
		// and the curl form expects it to still be there during perform() so
		// we have to do all this copying around to please the unicode gods

		char filename[TQSL_MAX_PATH_LEN];
		strncpy(filename, wxString::Format(wxT("<TQSLUpl %s-%s> %s"),
			now.Format(wxT("%Y%m%d")).c_str(),
			now.Format(wxT("%H%M")).c_str(),
			name.c_str()).ToUTF8(), sizeof filename);
		filename[sizeof filename - 1] = '\0';
		tqslTrace("MyFrame::UploadLogFile", "Upload Name=%s", filename);

		wxString fileType(wxT("Log"));
		tqslTrace("MyFrame::UploadLogFile", "About to call UploadFile");
		int retval = UploadFile(infile, filename, numrecs, reinterpret_cast<void *>(const_cast<char *>(compressed.c_str())),
					compressedSize, fileType);

		tqslTrace("MyFrame::UploadLogFile", "UploadFile returns %d", retval);
		if (retval == 0) {
			tqsl_converterCommit(logConv);
			tqslTrace("MyFrame::UploadLogFile", "Committing");
		} else {
			tqslTrace("MyFrame::UploadLogFile", "Rollback");
			tqsl_converterRollBack(logConv);
		}

		tqsl_endConverter(&logConv);
		tqslTrace("MyFrame::UploadLogFile", "Upload done");
		return retval;
	}
}

static CURL*
tqsl_curl_init(const char *logTitle, const char *url, FILE **curlLogFile, bool newFile) {
	tqslTrace("tqsl_curl_init", "title=%s url=%s newFile=%d", logTitle, url, newFile);
	CURL* curlReq = curl_easy_init();
	if (!curlReq)
		return NULL;

	wxString uri = wxString::FromUTF8(url);

#if defined(_WIN32)
 retry:
#endif

	if (!verifyCA) {
		uri.Replace(wxT("https:"), wxT("http:"));
	}

	wxString filename = wxString::FromUTF8(tQSL_BaseDir);
#ifdef _WIN32
	filename = filename + wxT("\\curl.log");
#else
	filename = filename + wxT("/curl.log");
#endif
#ifdef _WIN32
	wchar_t*lfn = utf8_to_wchar(filename.ToUTF8());
	*curlLogFile = _wfopen(lfn, newFile ? L"wb" : L"ab");
	free_wchar(lfn);
#else
	*curlLogFile = fopen(filename.ToUTF8(), newFile ? "wb" : "ab");
#endif
	if (*curlLogFile) {
		curl_easy_setopt(curlReq, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curlReq, CURLOPT_STDERR, *curlLogFile);
		fprintf(*curlLogFile, "%s:\n", logTitle);
	}
	//set up options
	curl_easy_setopt(curlReq, CURLOPT_URL, (const char *)uri.ToUTF8());
	curl_easy_setopt(curlReq, CURLOPT_USERAGENT, "tqsl/" TQSL_VERSION);

#ifdef __WXMAC__
	DocPaths docpaths(wxT("tqsl.app"));
#else
	DocPaths docpaths(wxT("tqslapp"));
#endif
	docpaths.Add(wxString::FromUTF8(tQSL_BaseDir));
#ifdef CONFDIR
	docpaths.Add(wxT(CONFDIR));
#endif
#ifdef _WIN32
	wxString exePath;
	wxFileName::SplitPath(wxStandardPaths::Get().GetExecutablePath(), &exePath, 0, 0);
	docpaths.Add(exePath);
#endif

	wxString caBundlePath = docpaths.FindAbsoluteValidPath(wxT("ca-bundle.crt"));
	if (!caBundlePath.IsEmpty()) {
		char caBundle[TQSL_MAX_PATH_LEN];
		strncpy(caBundle, caBundlePath.ToUTF8(), sizeof caBundle);
		curl_easy_setopt(curlReq, CURLOPT_CAINFO, caBundle);
#if defined(_WIN32)
	} else {
		if (verifyCA) {
			verifyCA = false;               // Can't verify if no trusted roots
			wxLogMessage(_("Unable to open ca-bundle.crt. Your TQSL installation is incomplete"));
			tqslTrace("tqsl_curl_init", "Can't find ca-bundle.crt in the docpaths!");
			goto retry;
		}
#endif
	}


	// Get the proxy configuration and pass it to cURL
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/Proxy"));

	bool enabled = false;
	config->Read(wxT("ProxyEnabled"), &enabled, false);
	wxString pHost = config->Read(wxT("proxyHost"), wxT(""));
	wxString pPort = config->Read(wxT("proxyPort"), wxT(""));
	wxString pType = config->Read(wxT("proxyType"), wxT(""));
	config->SetPath(wxT("/"));

	if (!enabled) return curlReq;	// No proxy defined

	long port = strtol(pPort.ToUTF8(), NULL, 10);
	if (port == 0 || pHost.IsEmpty())
		return curlReq;		// Invalid proxy. Ignore it.

	curl_easy_setopt(curlReq, CURLOPT_PROXY, (const char *)pHost.ToUTF8());
	curl_easy_setopt(curlReq, CURLOPT_PROXYPORT, port);
	if (pType == wxT("HTTP")) {
		curl_easy_setopt(curlReq, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);	// Default is HTTP
	} else if (pType == wxT("Socks4")) {
		curl_easy_setopt(curlReq, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
	} else if (pType == wxT("Socks5")) {
		curl_easy_setopt(curlReq, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
	}
	return curlReq;
}

int MyFrame::UploadFile(const wxString& infile, const char* filename, int numrecs, void *content, size_t clen, const wxString& fileType) {
	tqslTrace("MyFrame::UploadFile", "infile=%s, filename=%s, numrecs=%d, content=0x%lx, clen=%d fileType=%s",  S(infile), filename, numrecs, reinterpret_cast<void *>(content), clen, S(fileType));

	//upload the file

	//get the url from the config, can be overridden by an installer
	//defaults are valid for LoTW as of 1/31/2013

	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->SetPath(wxT("/LogUpload"));
	wxString uploadURL = config->Read(wxT("UploadURL"), DEFAULT_UPL_URL);
	wxString uploadField = config->Read(wxT("PostField"), DEFAULT_UPL_FIELD);
	wxString uplStatus = config->Read(wxT("StatusRegex"), DEFAULT_UPL_STATUSRE);
	wxString uplStatusSuccess = config->Read(wxT("StatusSuccess"), DEFAULT_UPL_STATUSOK).Lower();
	wxString uplMessage = config->Read(wxT("MessageRegex"), DEFAULT_UPL_MESSAGERE);
	bool uplVerifyCA;
	config->Read(wxT("VerifyCA"), &uplVerifyCA, DEFAULT_UPL_VERIFYCA);
	config->SetPath(wxT("/"));

	// Copy the strings so they remain around
	char *urlstr = strdup(uploadURL.ToUTF8());
	char *cpUF = strdup(uploadField.ToUTF8());

 retry_upload:

	curlReq = tqsl_curl_init("Upload Log", urlstr, &curlLogFile, true);

	if (!curlReq) {
		wxLogMessage(_("Error: Could not upload file (CURL Init error)"));
		free(urlstr);
		free(cpUF);
		return TQSL_EXIT_TQSL_ERROR;
	}

	//the following allow us to write our log and read the result

	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);
	curl_easy_setopt(curlReq, CURLOPT_SSL_VERIFYPEER, uplVerifyCA);

	char errorbuf[CURL_ERROR_SIZE];
	errorbuf[0] = '\0';
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);

	struct curl_httppost* post = NULL, *lastitem = NULL;

	curl_formadd(&post, &lastitem,
		CURLFORM_PTRNAME, cpUF,
		CURLFORM_BUFFER, filename,
		CURLFORM_BUFFERPTR, content,
		CURLFORM_BUFFERLENGTH, clen,
		CURLFORM_END);

	curl_easy_setopt(curlReq, CURLOPT_HTTPPOST, post);

	intptr_t retval;

	UploadDialog* upload = NULL;

	if (numrecs > 0) {
		if (numrecs == 1) {
			wxLogMessage(_("Attempting to upload one QSO"));
		} else {
			wxLogMessage(_("Attempting to upload %d QSOs"), numrecs);
		}
	} else {
		wxLogMessage(_("Attempting to upload %s"), fileType.c_str());
	}

	if (frame && !quiet) {
		if (fileType == wxT("Log")) {
			upload = new UploadDialog(this);
		} else {
			upload = new UploadDialog(this, wxString(_("Uploading Callsign Certificate")), wxString(_("Uploading Callsign Certificate Request...")));
		}

		curl_easy_setopt(curlReq, CURLOPT_PROGRESSFUNCTION, &UploadDialog::UpdateProgress);
		curl_easy_setopt(curlReq, CURLOPT_PROGRESSDATA, upload);
		curl_easy_setopt(curlReq, CURLOPT_NOPROGRESS, 0);

		UploadThread thread(curlReq, upload);
		if (thread.Run() != wxTHREAD_NO_ERROR) {
			wxLogError(_("Could not spawn upload thread!"));
			upload->Destroy();
			free(urlstr);
			free(cpUF);
			if (curlLogFile) {
				fclose(curlLogFile);
				curlLogFile = NULL;
			}
			return TQSL_EXIT_TQSL_ERROR;
		}

		upload->ShowModal();
		retval = ((intptr_t)thread.Wait());
	} else { retval = curl_easy_perform(curlReq); }

	if (retval == 0) { //success
		//check the result

		wxString uplresult = wxString::FromAscii(handler.s.c_str());

		wxRegEx uplStatusRE(uplStatus);
		wxRegEx uplMessageRE(uplMessage);
		wxRegEx stripSpacesRE(wxT("\\n +"), wxRE_ADVANCED);
		wxRegEx certUploadRE(wxT("(Started processing.*For QSOs not after: [0-9\\-: <none>]*)\\n(.*Your certificate request contains error.*)"), wxRE_ADVANCED);

		if (uplStatusRE.Matches(uplresult)) { //we can make sense of the error
			//sometimes has leading/trailing spaces
			if (uplStatusRE.GetMatch(uplresult, 1).Lower().Trim(true).Trim(false) == uplStatusSuccess) { //success
				if (uplMessageRE.Matches(uplresult)) { //and a message
					wxString lotwmessage = uplMessageRE.GetMatch(uplresult, 1).Trim(true).Trim(false);
					stripSpacesRE.ReplaceAll(&lotwmessage, wxString(wxT("\n")));
					if (fileType == wxT("Log")) {
						wxLogMessage(_("%s: Log uploaded successfully with result:\n\n%s"),
							infile.c_str(), lotwmessage.c_str());
						wxLogMessage(_("After reading this message, you may close this program."));

						retval = TQSL_EXIT_SUCCESS;
					} else {
						// Split the log message when there is an error detected
						if (certUploadRE.Matches(lotwmessage)) {
							wxString log = certUploadRE.GetMatch(lotwmessage, 1).Trim(true).Trim(false);
							wxString err = certUploadRE.GetMatch(lotwmessage, 2).Trim(true).Trim(false);
							wxLogMessage(_("%s uploaded with result:\n\n%s"),
								fileType.c_str(), log.c_str());
							wxMessageBox(wxString::Format(_("%s Uploaded with result:\n\n%s"), fileType.c_str(), err.c_str()), _("Error"), wxOK | wxICON_EXCLAMATION);
							retval = TQSL_EXIT_REJECTED;
						} else {
							wxLogMessage(_("%s uploaded with result:\n\n%s"),
								fileType.c_str(), lotwmessage.c_str());
							retval = TQSL_EXIT_SUCCESS;
						}
					}
				} else { // no message we could find
					if (fileType == wxT("Log")) {
						wxLogMessage(_("%s: Log uploaded successfully"), infile.c_str());
						wxLogMessage(_("After reading this message, you may close this program."));
					} else {
						wxLogMessage(_("%s uploaded successfully"), fileType.c_str());
					}
					retval = TQSL_EXIT_SUCCESS;
				}

			} else { // failure, but site is working
				if (uplMessageRE.Matches(uplresult)) { //and a message
					wxLogMessage(_("%s: %s upload was rejected with result \"%s\""),
						infile.c_str(), fileType.c_str(), uplMessageRE.GetMatch(uplresult, 1).c_str());

				} else { // no message we could find
					wxLogMessage(_("%s: %s upload was rejected"), infile.c_str(), fileType.c_str());
				}

				retval = TQSL_EXIT_REJECTED;
			}
		} else { //site isn't working
			wxLogMessage(_("%s: Got an unexpected response on %s upload! Maybe the site is down?"), infile.c_str(), fileType.c_str());
			retval = TQSL_EXIT_UNEXP_RESP;
		}

	} else {
		tqslTrace("MyFrame::UploadFile", "cURL Error: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		if ((retval == CURLE_PEER_FAILED_VERIFICATION) && verifyCA) {
			tqslTrace("MyFrame::UploadFile", "cURL SSL Certificate error - disabling verify and retry");
			verifyCA = false;
			goto retry_upload;
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(_("%s: Unable to upload - either your Internet connection is down or LoTW is unreachable."), infile.c_str());
			wxLogMessage(_("Please try uploading the %s later."), fileType.c_str());
			retval = TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(_("%s: Unable to upload. The network is down or the LoTW site is too busy."), infile.c_str());
			wxLogMessage(_("Please try uploading the %s later."), fileType.c_str());
			retval = TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(_("%s: Unable to connect to the upload site."), infile.c_str());
			wxLogMessage(_("Please try uploading the %s later."), fileType.c_str());
			retval = TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval == CURLE_ABORTED_BY_CALLBACK) { //cancelled.
			wxLogMessage(_("%s: Upload cancelled"), infile.c_str());
			retval = TQSL_EXIT_CANCEL;
		} else { //error
			//don't know why the conversion from char* -> wxString -> char* is necessary but it
			// was turned into garbage otherwise
			wxLogMessage(_("%s: Couldn't upload the file: CURL returned \"%hs\" (%hs)"), infile.c_str(), curl_easy_strerror((CURLcode)retval), errorbuf);
			retval = TQSL_EXIT_TQSL_ERROR;
		}
	}
	if (frame && upload) upload->Destroy();

	curl_formfree(post);
	curl_easy_cleanup(curlReq);
	curlReq = NULL;

	// If there's a GUI and we didn't successfully upload and weren't cancelled,
	// ask the user if we should retry the upload.
	if ((frame && !quiet) && retval != TQSL_EXIT_CANCEL && retval != TQSL_EXIT_SUCCESS) {
		if (wxMessageBox(_("Your upload appears to have failed. Should TQSL try again?"), _("Retry?"), wxYES_NO | wxICON_QUESTION, this) == wxYES)
			goto retry_upload;
	}

	if (urlstr) free(urlstr);
	if (cpUF) free (cpUF);
	if (curlLogFile) {
		fclose(curlLogFile);
		curlLogFile = NULL;
	}
	return retval;
}

// Verify that a certificate exists for this station location
// before allowing the location to be edited
static bool verify_cert(tQSL_Location loc, bool editing) {
	tqslTrace("verify_cert", "loc=%lx, editing=%d", reinterpret_cast<void *>(loc), editing);
	char call[128];
	tQSL_Cert *certlist;
	int ncerts;
	// Get the callsign from the location
	check_tqsl_error(tqsl_getLocationCallSign(loc, call, sizeof(call)));
	// See if there is a certificate for that call
	int flags = 0;
	if (editing)
		flags = TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED;
	tqsl_selectCertificates(&certlist, &ncerts, call, 0, 0, 0, flags);
	if (ncerts == 0 && strcmp(call, "NONE") && strcmp(call, "[None]")) {
		if (editing) {
			wxMessageBox(wxString::Format(_("There are no callsign certificates for callsign %hs. This station location cannot be edited."), call), _("No Certificate"), wxOK | wxICON_EXCLAMATION);
		} else {
			wxMessageBox(wxString::Format(_("There are no current callsign certificates for callsign %hs. This station location cannot be used to sign a log file."), call), _("No Certificate"), wxOK | wxICON_EXCLAMATION);
		}
		return false;
	}
	if (editing) {
		tqsl_freeCertificateList(certlist, ncerts);
		return true;
	}
	if (!strcmp(call, "NONE") || !strcmp(call, "[None]")) {
		return true;
	}
// check if there's a good cert right now for this call
	bool goodCert = false;
	long serial = 0;
	wxString status;
	wxString errString;
	for (int i = 0; i < ncerts; i++) {
		tqsl_getCertificateSerial(certlist[i], &serial);
		frame->CheckCertStatus(serial, status);
		if (status == wxT("Bad serial")) {
			errString = wxString::Format(_("There are no current callsign certificates for callsign %hs. This station location cannot be used to sign a log file."), call);
		} else if (status == wxT("Superceded")) {
			errString = wxString::Format(_("There is a newer callsign certificate for callsign %hs. This station location cannot be used to sign a log file until the new certificate is installed."), call);
		} else if (status == wxT("Expired")) {
			errString = wxString::Format(_("The callsign certificate for callsign %hs has expired. This station location cannot be used to sign a log file until a valid callsign certificate is installed."), call);
		} else if (status == wxT("Unrevoked")) {
			goodCert = true;
			break;
		} else {			// Can't tell - no network?
			goodCert = true;
			break;
		}
	}
	if (!goodCert) {
		wxMessageBox(errString, _("No Certificate"), wxOK | wxICON_EXCLAMATION);
	}
	tqsl_freeCertificateList(certlist, ncerts);
	return goodCert;
}

tQSL_Location
MyFrame::SelectStationLocation(const wxString& title, const wxString& okLabel, bool editonly) {
	tqslTrace("MyFrame::SelectStationLocation", "title=%s, okLabel=%s, editonly=%d", S(title), S(okLabel), editonly);
	int rval;
	tQSL_Location loc;
	wxString selname;
	char errbuf[512];
	do {
		TQSLGetStationNameDialog station_dial(this, help, wxDefaultPosition, false, title, okLabel, editonly);
		if (!selname.IsEmpty())
			station_dial.SelectName(selname);
		rval = station_dial.ShowModal();
		switch (rval) {
			case wxID_CANCEL:	// User hit Close
				return 0;
			case wxID_APPLY:	// User hit New
				try {
					check_tqsl_error(tqsl_initStationLocationCapture(&loc));
					selname = run_station_wizard(this, loc, help, false, false);
					check_tqsl_error(tqsl_endStationLocationCapture(&loc));
					frame->loc_tree->Build();
					break;
				}
				catch(TQSLException& x) {
					wxLogError(wxT("%s"), x.what());
					return 0;
				}
			case wxID_MORE:		// User hit Edit
				try {
					check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().ToUTF8()));
					if (verify_cert(loc, true)) {	// Check if there is a certificate before editing
						check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
						if (strlen(errbuf) > 0) {
							wxString fmt = wxT("%hs\n");
							fmt += _("The invalid data was ignored.");
							wxMessageBox(wxString::Format(fmt, errbuf), _("Station Location data error"), wxOK | wxICON_EXCLAMATION, this);
						}
						char loccall[512];
						check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
						selname = run_station_wizard(this, loc, help, true, true, wxString::Format(_("Edit Station Location : %hs - %s"), loccall, station_dial.Selected().c_str()), station_dial.Selected());
						check_tqsl_error(tqsl_endStationLocationCapture(&loc));
					}
					break;
				}
				catch(TQSLException& x) {
					wxLogError(wxT("%hs"), x.what());
					return 0;
				}
			case wxID_OK:		// User hit OK
				try {
					check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().ToUTF8()));
					check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
					if (strlen(errbuf) > 0) {
						wxString fmt = wxT("%hs\n");
						fmt += _("This should be corrected before signing a log file.");
						wxMessageBox(wxString::Format(fmt, errbuf), _("Station Location data error"), wxOK | wxICON_EXCLAMATION, this);
					}
					break;
				}
				catch(TQSLException& x) {
					wxLogError(wxT("%hs"), x.what());
					return 0;
				}
		}
	} while (rval != wxID_OK);
	return loc;
}

void MyFrame::CheckForUpdates(wxCommandEvent&) {
	tqslTrace("MyFrame::CheckForUpdates", NULL);
	DoUpdateCheck(false, false);
}

wxString GetUpdatePlatformString() {
	tqslTrace("GetUpdatePlatformString", NULL);
	wxString ret;
#if defined(_WIN32)
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

#elif defined(__APPLE__) && defined(__MACH__) //osx has universal binaries
	ret = wxT("osx");

#elif defined(__gnu_linux__)
	#if defined(__amd64__)
		ret = wxT("linux64 linux32 source");
	#elif defined(__i386__)
		ret = wxT("linux32 source");
	#else
		ret = wxT("source"); //source distribution is kosher on linux
	#endif
#else
	ret = wxT(""); //give them a homepage
#endif
	return ret;
}

// Class for encapsulating version information
class revLevel {
 public:
	explicit revLevel(long _major = 0, long _minor = 0, long _patch = 0) {
		major = _major;
		minor = _minor;
		patch = _patch;
	}
	explicit revLevel(const wxString& _value) {
		wxString str = _value;
		str.Trim(true);
		str.Trim(false);
		wxStringTokenizer vers(str, wxT("."));
		wxString majorVer = vers.GetNextToken();
		wxString minorVer = vers.GetNextToken();
		wxString patchVer = vers.GetNextToken();
		if (majorVer.IsNumber()) {
			majorVer.ToLong(&major);
		} else {
			major = -1;
		}
		if (minorVer.IsNumber()) {
			minorVer.ToLong(&minor);
		} else {
			minor = -1;
		}
		if (!patchVer.IsEmpty() && patchVer.IsNumber()) {
			patchVer.ToLong(&patch);
		} else {
			patch = 0;
		}
	}
	wxString Value(void) {
		if (patch > 0)
			return wxString::Format(wxT("%ld.%ld.%ld"), major, minor, patch);
		else
			return wxString::Format(wxT("%ld.%ld"), major, minor);
	}
	long major;
	long minor;
	long patch;
	bool operator >(const revLevel& other) {
		if (major > other.major) return true;
		if (major == other.major) {
			if (minor > other.minor) return true;
			if (minor == other.minor) {
				if (patch > other.patch) return true;
			}
		}
		return false;
	}
	bool operator  >=(const revLevel& other) {
		if (major > other.major) return true;
		if (major == other.major) {
			if (minor > other.minor) return true;
			if (minor == other.minor) {
				if (patch >= other.patch) return true;
			}
		}
		return false;
	}
};

class revInfo {
 public:
	explicit revInfo(bool _noGUI = false, bool _silent = false) {
		noGUI = _noGUI;
		silent = _silent;
		error = false;
		message = false;
		programRev = newProgramRev = NULL;
		configRev = newConfigRev = NULL;
		mutex = new wxMutex;
		condition = new wxCondition(*mutex);
		mutex->Lock();
	}
	~revInfo() {
		if (programRev)
			delete programRev;
		if (newProgramRev)
			delete newProgramRev;
		if (configRev)
			delete configRev;
		if (newConfigRev)
			delete newConfigRev;
		if (condition)
			delete condition;
		if (mutex)
			delete mutex;
	}
	bool error;
	bool message;
	bool noGUI;
	bool silent;
	revLevel* programRev;
	revLevel* newProgramRev;
	revLevel* configRev;
	revLevel* newConfigRev;
	bool newProgram;
	bool newConfig;
	wxString errorText;
	wxString homepage;
	wxString url;
	wxMutex* mutex;
	wxCondition* condition;
};


class UpdateDialogMsgBox: public wxDialog {
 public:
	UpdateDialogMsgBox(wxWindow* parent, bool newProg, bool newConfig, revLevel* currentProgRev, revLevel* newProgRev,
			revLevel* currentConfigRev, revLevel* newConfigRev, wxString platformURL, wxString homepage)
			: wxDialog(parent, (wxWindowID)wxID_ANY, _("TQSL Update Available"), wxDefaultPosition, wxDefaultSize) {
		tqslTrace("UpdateDialogMsgBox::UpdateDialogMsgBox", "parent=%lx, newProg=%d, newConfig=%d, currentProgRev %s, newProgRev %s, currentConfigRev %s, newConfigRev=%s, platformURL=%s, homepage=%s", reinterpret_cast<void *>(parent), newProg, newConfig, S(currentProgRev->Value()), S(newProgRev->Value()), S(currentConfigRev->Value()), S(newConfigRev->Value()), S(platformURL), S(homepage));
		wxSizer* overall = new wxBoxSizer(wxVERTICAL);
		long flags = wxOK;
#ifndef _WIN32
		if (newConfig)
#endif
			flags |= wxCANCEL;

		wxSizer* buttons = CreateButtonSizer(flags);
		wxString notice;
		if (newProg)
			notice = wxString::Format(_("A new TQSL release (V%s) is available!"), newProgRev->Value().c_str());
		else if (newConfig)
			notice = wxString::Format(_("An updated TrustedQSL configuration file (V%s) is available!\nThe configuration file installs definitions for entities, modes, etc."), newConfigRev->Value().c_str());

		overall->Add(new wxStaticText(this, wxID_ANY, notice), 0, wxALIGN_CENTER_HORIZONTAL);

		if (newProg) {
#ifndef _WIN32
			if (!platformURL.IsEmpty()) {
				wxSizer* thisline = new wxBoxSizer(wxHORIZONTAL);
				thisline->Add(new wxStaticText(this, wxID_ANY, _("Download from:")));
				thisline->Add(new wxHyperlinkCtrl(this, wxID_ANY, platformURL, platformURL));

				overall->AddSpacer(10);
				overall->Add(thisline);
			}

			if (!homepage.IsEmpty()) {
				wxSizer* thisline = new wxBoxSizer(wxHORIZONTAL);
				thisline->Add(new wxStaticText(this, wxID_ANY, _("More details at:")));
				thisline->Add(new wxHyperlinkCtrl(this, wxID_ANY, homepage, homepage));

				overall->AddSpacer(10);
				overall->Add(thisline);
			}
#else
			overall->AddSpacer(10);
			overall->Add(new wxStaticText(this, wxID_ANY, _("Click 'OK' to install the new version of TQSL, or Cancel to ignore it.")));
#endif
		}
		if (newConfig) {
			overall->AddSpacer(10);
			overall->Add(new wxStaticText(this, wxID_ANY, _("Click 'OK' to install the new configuration file, or Cancel to ignore it.")));
		}
		if (buttons) { //should always be here but documentation says to check
			overall->AddSpacer(10);
			overall->Add(buttons, 0, wxALIGN_CENTER_HORIZONTAL);
		}

#if wxMAJOR_VERSION > 2 || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
		SetSizerAndFit(overall);
#else
		wxSizer* padding = new wxBoxSizer(wxVERTICAL);
		padding->Add(overall, 0, wxALL, 10);
		SetSizer(padding);
		Fit();
#endif
	}

 private:
};

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

void MyFrame::UpdateConfigFile() {
	tqslTrace("MyFrame::UpdateConfigFile", NULL);
	wxConfig* config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString newConfigURL = config->Read(wxT("NewConfigURL"), DEFAULT_CONFIG_FILE_URL);
 retry:
	curlReq = tqsl_curl_init("Config File Download Log", (const char *)newConfigURL.ToUTF8(), &curlLogFile, false);

	wxString filename = wxString::FromUTF8(tQSL_BaseDir);
#ifdef _WIN32
	filename = filename + wxT("\\config.tq6");
	wchar_t* lfn = utf8_to_wchar(filename.ToUTF8());
	FILE *configFile = _wfopen(lfn, L"wb");
	free_wchar(lfn);
#else
	filename = filename + wxT("/config.tq6");
	FILE *configFile = fopen(filename.ToUTF8(), "wb");
#endif
	if (!configFile) {
		tqslTrace("UpdateConfigFile", "Can't open new file %s: %s", static_cast<const char *>(filename.ToUTF8()), strerror(errno));
		wxMessageBox(wxString::Format(_("Can't open new configuration file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &file_recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, configFile);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(curlReq);
	if (retval == CURLE_OK) {
		if (fclose(configFile)) {
			tqslTrace("UpdateConfigFile", "Error writing new file %s: %s", static_cast<const char *>(filename.ToUTF8()), strerror(errno));
			wxMessageBox(wxString::Format(_("Error writing new configuration file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
			return;
		}
		notifyData nd;
		if (tqsl_importTQSLFile(filename.ToUTF8(), notifyImport, &nd)) {
			wxLogError(getLocalizedErrorString());
		} else {
			tqslTrace("UpdateConfigFile", "Config update success");
			wxMessageBox(_("Configuration file successfully updated"), _("Update Completed"), wxOK | wxICON_INFORMATION, this);
			// Reload the DXCC mapping
			DXCC dx;
			dx.reset();
		}
	} else {
		tqslTrace("MyFrame::UpdateConfigFile", "cURL Error during config file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		if ((retval == CURLE_PEER_FAILED_VERIFICATION) && verifyCA) {
			tqslTrace("MyFrame::UpdateConfigFile", "cURL SSL Certificate error - disabling verify and retry");
			verifyCA = false;
			goto retry;
		}
		if (curlLogFile) {
			fprintf(curlLogFile, "cURL Error during config file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(_("Unable to update - either your Internet connection is down or LoTW is unreachable."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(_("Unable to update. The network is down or the LoTW site is too busy."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(_("Unable to connect to the update site."));
			wxLogMessage(_("Please try again later."));
		} else { // some other error
			wxString fmt = _("Error downloading new configuration file:");
			fmt += wxT("\n%hs");
			wxMessageBox(wxString::Format(fmt, errorbuf), _("Update"), wxOK | wxICON_EXCLAMATION, this);
		}
	}
}

#if defined(_WIN32) || defined(__APPLE__)
void MyFrame::UpdateTQSL(wxString& url) {
	tqslTrace("MyFrame::UpdateTQSL", "url=%s", S(url));
 retry:
	bool needToCleanUp = false;
	if (curlReq) {
		curl_easy_setopt(curlReq, CURLOPT_URL, (const char *)url.ToUTF8());
	} else {
		curlReq = tqsl_curl_init("TQSL Update Download Log", (const char *)url.ToUTF8(), &curlLogFile, false);
		needToCleanUp = true;
	}

	wxString filename = wxString::FromUTF8(tQSL_BaseDir);
#ifdef _WIN32
	filename = filename + wxT("\\tqslupdate.msi");
	wchar_t* lfn = utf8_to_wchar(filename.ToUTF8());
	FILE *updateFile = _wfopen(lfn, L"wb");
	free_wchar(lfn);
#else
	filename = filename + wxT("/tqslupdate.pkg");
	FILE *updateFile = fopen(filename.ToUTF8(), "wb");
#endif
	if (!updateFile) {
		tqslTrace("UpdateTQSL", "Can't open new file %s: %s", static_cast<const char *>(filename.ToUTF8()), strerror(errno));
		wxMessageBox(wxString::Format(_("Can't open TQSL update file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
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
			tqslTrace("UpdateTQSL", "Error writing new file %s: %s", static_cast<const char *>(filename.ToUTF8()), strerror(errno));
			wxMessageBox(wxString::Format(_("Error writing new configuration file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
			return;
		}
#ifdef _WIN32
		tqslTrace("MyFrame::UpdateTQSL", "Executing msiexec \"%s\"", filename.ToUTF8());
		wxExecute(wxString::Format(wxT("msiexec /i \"%s\""), filename), wxEXEC_ASYNC);
#else
		tqslTrace("MyFrame::UpdateTQSL", "Executing installer");
		wxExecute(wxString::Format(wxT("open \"%s\""), filename.c_str()), wxEXEC_ASYNC);
#endif
		tqslTrace("MyFrame::UpdateTQSL", "GUI Destroy");
		wxExit();
		exit(0);
	} else {
		tqslTrace("MyFrame::UpdateTQSL", "cURL Error during file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		if ((retval == CURLE_PEER_FAILED_VERIFICATION) && verifyCA) {
			tqslTrace("MyFrame::UpdateTQSL", "cURL SSL Certificate error - disabling verify and retry");
			verifyCA = false;
			goto retry;
		}
		if (curlLogFile) {
			fprintf(curlLogFile, "cURL Error during file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(_("Unable to update - either your Internet connection is down or LoTW is unreachable."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(_("Unable to update. The network is down or the LoTW site is too busy."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(_("Unable to connect to the update site."));
			wxLogMessage(_("Please try again later."));
		} else { // some other error
			wxString fmt = _("Error downloading new file:");
			fmt += wxT("\n%hs");
			wxMessageBox(wxString::Format(fmt, errorbuf), _("Update"), wxOK | wxICON_EXCLAMATION, this);
		}
	}
	if (needToCleanUp) {
		curl_easy_cleanup(curlReq);
		curlReq = NULL;
	}
}
#endif /* _WIN32  || __APPLE__ */

// Check if a certificate is still valid and current at LoTW
bool MyFrame::CheckCertStatus(long serial, wxString& result) {
	tqslTrace("MyFrame::CheckCertStatus()", "Serial=%ld", serial);
	wxConfig* config = reinterpret_cast<wxConfig *>(wxConfig::Get());

	wxString certCheckURL = config->Read(wxT("CertCheckURL"), DEFAULT_CERT_CHECK_URL);
	wxString certCheckRE = config->Read(wxT("StatusRegex"), DEFAULT_CERT_CHECK_RE);
	certCheckURL = certCheckURL + wxString::Format(wxT("%ld"), serial);
	bool needToCleanUp = false;

	if (!verifyCA) {
		certCheckURL.Replace(wxT("https:"), wxT("http:"));
	}
	if (curlReq == NULL) {
		needToCleanUp = true;
		curlReq = tqsl_curl_init("checkCert", certCheckURL.ToUTF8(), &curlLogFile, false);
	} else {
		curl_easy_setopt(curlReq, CURLOPT_URL, (const char *)certCheckURL.ToUTF8());
	}

	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	errorbuf[0] = '\0';
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(curlReq);
	result = wxString(wxT("Unknown"));
	bool ret = false;
	if (retval == CURLE_OK) {
		wxString checkresult = wxString::FromAscii(handler.s.c_str());

		wxRegEx checkStatusRE(certCheckRE);

		if (checkStatusRE.Matches(checkresult)) { // valid response
			result = checkStatusRE.GetMatch(checkresult, 1).Trim(true).Trim(false);
			ret = true;
		}
	} else {
		tqslTrace("MyFrame::CheckCertStatus", "cURL Error during cert status check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		if (curlLogFile) {
			fprintf(curlLogFile, "cURL Error during cert status check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if ((retval == CURLE_PEER_FAILED_VERIFICATION) && verifyCA) {
			verifyCA = false;
		}
	}
	if (needToCleanUp) {
		curl_easy_cleanup(curlReq);
		curlReq = NULL;
	}
	return ret;
}

class expInfo {
 public:
	explicit expInfo(bool _noGUI = false) {
		noGUI = _noGUI;
		days = 0;
		callsign = NULL;
		error = false;
		mutex = new wxMutex;
		condition = new wxCondition(*mutex);
		mutex->Lock();
	}
	bool noGUI;
	int days;
	tQSL_Cert cert;
	char* callsign;
	bool error;
	wxString errorText;
	wxMutex* mutex;
	wxCondition* condition;
	~expInfo() {
		if (callsign) free(callsign);
		if (condition) delete condition;
		if (mutex) delete mutex;
	}
};

// Report an error back to the main thread
static void
report_error(expInfo **eip) {
	expInfo *ei = *eip;
	ei->error = true;
	ei->errorText = getLocalizedErrorString();
	// Send the result back to the main thread
	wxCommandEvent* event = new wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED, bg_expiring);
	event->SetClientData(ei);
	wxPostEvent(frame, *event);
	ei->condition->Wait();		// stalls here until the main thread resumes the thread
	ei->mutex->Unlock();
	delete ei;
	delete event;
	*eip = new expInfo;
}

// Check for certificates expiring in the next nn (default 60) days
void
MyFrame::DoCheckExpiringCerts(bool noGUI) {
	tQSL_Cert *clist;
	int nc;

	tqsl_selectCertificates(&clist, &nc, 0, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED);
	if (nc == 0) return;

	expInfo *ei = new expInfo;
	ei->noGUI = noGUI;

	bool needToCleanUp = false;

        if (curlReq == NULL) {
                needToCleanUp = true;
		curlReq = tqsl_curl_init("Certificate Check Log", "https://lotw.arrl.org", &curlLogFile, false);
        }

	long expireDays = DEFAULT_CERT_WARNING;
	wxConfig::Get()->Read(wxT("CertWarnDays"), &expireDays);
	// Get today's date
	time_t t = time(0);
	struct tm *tm = gmtime(&t);
	tQSL_Date d;
	d.year = tm->tm_year + 1900;
	d.month = tm->tm_mon + 1;
	d.day = tm->tm_mday;
	tQSL_Date exp;

	for (int i = 0; i < nc; i ++) {
		ei->cert = clist[i];
		char callsign[64];
		if (tqsl_getCertificateCallSign(clist[i], callsign, sizeof callsign)) {
			report_error(&ei);
			continue;
		}
		wxString county;
		wxString grid;

		// Get the user detail info for this callsign from the ARRL server
		int dxcc;
		if (tqsl_getCertificateDXCCEntity(clist[i], &dxcc)) {
			report_error(&ei);
			continue;
		}
		SaveAddressInfo(callsign, dxcc);

		int expired = 0;
		tqsl_isCertificateExpired(clist[i], &expired);
		if (expired) continue;			// Don't check expired already

		int keyonly, pending;
		keyonly = pending = 0;
		if (tqsl_getCertificateKeyOnly(clist[i], &keyonly)) {
			report_error(&ei);
			continue;
		}
		long serial = 0;
		wxString status = wxString(wxT("KeyOnly"));
		if (!keyonly) {
			if (tqsl_getCertificateSerial(clist[i], &serial)) {
				report_error(&ei);
				continue;
			}
			CheckCertStatus(serial, status);
			if (tqsl_setCertificateStatus(serial, (const char *)status.ToUTF8())) {
				report_error(&ei);
				continue;
			}
		}
		wxString reqPending = wxConfig::Get()->Read(wxT("RequestPending"));
		wxStringTokenizer tkz(reqPending, wxT(","));
		while (tkz.HasMoreTokens()) {
			wxString pend = tkz.GetNextToken();
			if (pend == wxString::FromUTF8(callsign)) {
				pending = true;
				break;
			}
		}

		if (keyonly || pending)
			continue;

		if (0 == tqsl_getCertificateNotAfterDate(clist[i], &exp)) {
			int days_left;
			tqsl_subtractDates(&d, &exp, &days_left);
			if (days_left < expireDays) {
				ei->days = days_left;
				ei->callsign = strdup(callsign);
				ei->cert = clist[i];
				// Send the result back to the main thread
				wxCommandEvent* event = new wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED, bg_expiring);
				event->SetClientData(ei);
				wxPostEvent(frame, *event);
				ei->condition->Wait();
				ei->mutex->Unlock();
				delete ei;
				delete event;
				ei = new expInfo;
				ei->noGUI = noGUI;
			}
		}
	}
	ei->mutex->Unlock();
	delete ei;
	if (clist) {
		tqsl_freeCertificateList(clist, nc);
	}
	if (needToCleanUp) {
		curl_easy_cleanup(curlReq);
		curlReq = NULL;
		if (curlLogFile) {
			fclose(curlLogFile);
			curlLogFile = NULL;
		}
	}
	return;
}

void
MyFrame::OnExpiredCertFound(wxCommandEvent& event) {
	expInfo *ei = reinterpret_cast<expInfo *>(event.GetClientData());
	if (ei->error) {
		if (ei->noGUI) {
			wxLogError(ei->errorText);
		} else {
			wxMessageBox(wxString(_("Error checking for expired callsign certificates:")) + ei->errorText,
				_("Check Error"), wxOK | wxICON_EXCLAMATION, this);
		}
	} else if (ei->noGUI) {
		wxLogMessage(_("The certificate for %hs expires in %d days."),
			ei->callsign, ei->days);
	} else {
		wxString fmt = _("The certificate for %hs expires in %d days");
			fmt += wxT("\n");
			fmt += _("Do you want to renew it now?");
		if (wxMessageBox(wxString::Format(fmt, ei->callsign, ei->days),
					_("Certificate Expiring"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
				// Select the certificate in the tree
				cert_tree->SelectCert(ei->cert);
				// Then start the renewal
				wxCommandEvent dummy;
				CRQWizardRenew(dummy);
		}
	}
	// Tell the background thread that it's OK to continue
	wxMutexLocker lock(*ei->mutex);
	ei->condition->Signal();
}

void
MyFrame::OnUpdateCheckDone(wxCommandEvent& event) {
	revInfo *ri = reinterpret_cast<revInfo *>(event.GetClientData());
	if (!ri) return;
	if (ri->error) {
		if (!ri->silent && !ri->noGUI)
			wxLogMessage(ri->errorText);
		wxMutexLocker lock(*ri->mutex);
		ri->condition->Signal();
		return;
	}
	if (ri->message) {
		if (!ri->silent && !ri->noGUI)
			wxMessageBox(ri->errorText, _("Update"), wxOK | wxICON_EXCLAMATION, this);
		wxMutexLocker lock(*ri->mutex);
		ri->condition->Signal();
		return;
	}
	// For win32, can't have config and program updates together.
	// randomize which gets flagged
#ifdef _WIN32
	if (ri->newProgram && ri->newConfig) {
		if (time(0) & 0x1) {
			ri->newProgram = false;
		} else {
			ri->newConfig = false;
		}
	}
#endif
	if (ri->newProgram) {
		if (ri->noGUI) {
			wxLogMessage(_("A new TQSL release (V%s) is available."), ri->newProgramRev->Value().c_str());
		} else {
			//will create ("homepage"->"") if none there, which is what we'd be checking for anyway
			UpdateDialogMsgBox msg(this, true, false, ri->programRev, ri->newProgramRev,
					ri->configRev, ri->newConfigRev, ri->url, ri->homepage);

#if defined(_WIN32) || defined(__APPLE__)
			if (msg.ShowModal() == wxID_OK) {
				UpdateTQSL(ri->url);
			}
#else
			msg.ShowModal();
#endif
		}
	}
	if (ri->newConfig) {
		if (ri->noGUI) {
			wxLogMessage(_("A new TrustedQSL configuration file (V%s) is available."), ri->newConfigRev->Value().c_str());
		} else {
			UpdateDialogMsgBox msg(this, false, true, ri->programRev, ri->newProgramRev,
					ri->configRev, ri->newConfigRev, wxT(""), wxT(""));

			if (msg.ShowModal() == wxID_OK) {
				UpdateConfigFile();
			}
		}
	}

	if (!ri->newProgram && !ri->newConfig) {
		if (!ri->silent && !ri->noGUI) {
			wxString fmt = _("Your system is up to date");
			fmt += wxT("\n");
			fmt += _("TQSL Version %hs and Configuration Data Version %s");
			fmt += wxT("\n");
			fmt += _("are the newest available");
			wxMessageBox(wxString::Format(fmt, TQSL_VERSION, ri->configRev->Value().c_str()), _("No Updates"), wxOK | wxICON_INFORMATION, this);
		}
	}
	// Tell the background thread that it's OK to continue
	wxMutexLocker lock(*ri->mutex);
	ri->condition->Signal();
}


void
MyFrame::DoCheckForUpdates(bool silent, bool noGUI) {
	tqslTrace("MyFrame::DoCheckForUpdates", "silent=%d noGUI=%d", silent, noGUI);
	wxConfig* config = reinterpret_cast<wxConfig *>(wxConfig::Get());

	wxString lastUpdateTime = config->Read(wxT("UpdateCheckTime"));
	int numdays = config->Read(wxT("UpdateCheckInterval"), 1); // in days

	bool check = true;
	bool networkError = false;
	if (!lastUpdateTime.IsEmpty()) {
		wxDateTime lastcheck; lastcheck.ParseFormat(lastUpdateTime, wxT("%Y-%m-%d"));
		lastcheck+=wxDateSpan::Days(numdays); // x days from when we checked
		if (lastcheck > wxDateTime::Today()) //if we checked less than x days ago
			check = false;  // don't check again
	} // else no stored value, means check

	if (!silent) check = true; //unless the user explicitly asked

	if (!check) return;	//if we really weren't supposed to check, get out of here

	revInfo* ri = new revInfo;
	ri->noGUI = noGUI;
	ri->silent = silent;
	ri->programRev = new revLevel(wxT(TQSL_VERSION));

	int currentConfigMajor, currentConfigMinor;
	tqsl_getConfigVersion(&currentConfigMajor, &currentConfigMinor);
	ri->configRev = new revLevel(currentConfigMajor, currentConfigMinor, 0);		// config files don't have patch levels

	wxString updateURL = config->Read(wxT("UpdateURL"), DEFAULT_UPD_URL);

	bool needToCleanUp = false;
 retry:
	if (!verifyCA) {
		updateURL.Replace(wxT("https:"), wxT("http:"));
	}

	if (curlReq) {
		curl_easy_setopt(curlReq, CURLOPT_URL, (const char *)updateURL.ToUTF8());
	} else {
		needToCleanUp = true;
		curlReq = tqsl_curl_init("Version Check Log", (const char*)updateURL.ToUTF8(), &curlLogFile, false);
	}

	//the following allow us to analyze our file

	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);

	if (silent) { // if there's a problem, we don't want the program to hang while we're starting it
		curl_easy_setopt(curlReq, CURLOPT_CONNECTTIMEOUT, 120);
	}

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);

	tqslTrace("MyFrame::DoCheckForUpdates", "calling curl_easy_perform");
	int retval = curl_easy_perform(curlReq);
	tqslTrace("MyFrame::DoCheckForUpdates", "Program rev check returns %d", retval);
	if (retval == CURLE_OK) {
		tqslTrace("MyFrame::DoCheckForUpdates", "Program rev returns %d chars, %s", handler.s.size(), handler.s.c_str());
		// Add the config.xml text to the result
		wxString configURL = config->Read(wxT("ConfigFileVerURL"), DEFAULT_UPD_CONFIG_URL);
		curl_easy_setopt(curlReq, CURLOPT_URL, (const char*)configURL.ToUTF8());
		curl_easy_setopt(curlReq, CURLOPT_USERAGENT, "tqsl/" TQSL_VERSION);

		retval = curl_easy_perform(curlReq);
		if (retval == CURLE_OK) {
			tqslTrace("MyFrame::DoCheckForUpdates", "Prog + Config rev returns %d chars, %s", handler.s.size(), handler.s.c_str());
			wxString result = wxString::FromAscii(handler.s.c_str());
			wxString url;

// The macro for declaring a hash map defines a couple of typedefs
// that it never uses. Current GCC warns about those. The pragma
// below suppresses those warnings for those.
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
			WX_DECLARE_STRING_HASH_MAP(wxString, URLHashMap);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic warning "-Wunused-local-typedefs"
#endif
			URLHashMap map;
			ri->newProgramRev = NULL;
			ri->newConfigRev = NULL;

			wxStringTokenizer urls(result, wxT("\n"));
			wxString onlinever;
			while(urls.HasMoreTokens()) {
				wxString header = urls.GetNextToken().Trim();
				if (header.StartsWith(wxT("TQSLVERSION;"), &onlinever)) {
					ri->newProgramRev = new revLevel(onlinever);
				} else if (header.IsEmpty()) {
					continue; //blank line
				} else if (header[0] == '#') {
					continue; //comments
				} else if (header.StartsWith(wxT("config.xml"), &onlinever)) {
					onlinever.Replace(wxT(":"), wxT(""));
					onlinever.Replace(wxT("Version"), wxT(""));
					onlinever.Trim(true);
					onlinever.Trim(false);
					ri->newConfigRev = new revLevel(onlinever);
				} else {
					int sep = header.Find(';'); //; is invalid in URLs
					if (sep == wxNOT_FOUND) continue; //malformed string
					wxString plat = header.Left(sep);
					wxString url = header.Right(header.size()-sep-1);
					map[plat] = url;
				}
			}
#ifdef TQSL_TEST_BUILD
			ri->newProgram = ri->newProgramRev ? (*ri->newProgramRev >= *ri->programRev) : false;
#else
			ri->newProgram = ri->newProgramRev ? (*ri->newProgramRev > *ri->programRev) : false;
#endif
			ri->newConfig = ri->newConfigRev ? (*ri->newConfigRev > *ri->configRev) : false;
			if (ri->newProgram) {
				wxString ourPlatURL; //empty by default (we check against this later)

				wxStringTokenizer plats(GetUpdatePlatformString(), wxT(" "));
				while(plats.HasMoreTokens()) {
					wxString tok = plats.GetNextToken();
					//see if this token is here
					if (map.count(tok)) { ourPlatURL=map[tok]; break; }
				}
				ri->homepage = map[wxT("homepage")];
				ri->url = ourPlatURL;
			}
		} else {
			tqslTrace("MyFrame::DoCheckForUpdates", "cURL Error during config file version check: %d : %s (%s)\n", retval, curl_easy_strerror((CURLcode)retval), errorbuf);
			if (curlLogFile) {
				fprintf(curlLogFile, "cURL Error during config file version check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
			}
			if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
				networkError = true;
				ri->error = true;
				ri->errorText = wxString(_("Unable to check for updates - either your Internet connection is down or LoTW is unreachable."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
			} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
				networkError = true;
				ri->error = true;
				ri->errorText = wxString(_("Unable to check for updates. The network is down or the LoTW site is too busy."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
			} else if (retval == CURLE_SSL_CONNECT_ERROR) {
				networkError = true;
				ri->error = true;
				ri->errorText = wxString(_("Unable to connect to the update site."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
			} else { // some other error
				ri->message = true;
				wxString fmt = _("Error downloading new version information:");
				fmt += wxT("\n%hs");
				ri->errorText = wxString::Format(fmt, errorbuf);
			}
		}
	} else {
		tqslTrace("MyFrame::DoCheckForUpdates", "cURL Error during program revision check: %d: %s (%s)\n", retval, curl_easy_strerror((CURLcode)retval), errorbuf);
		if ((retval == CURLE_PEER_FAILED_VERIFICATION) && verifyCA) {
			tqslTrace("MyFrame::DoCheckForUpdates", "cURL SSL Certificate error - disabling verify and retry");
			verifyCA = false;
			goto retry;
		}
		if (curlLogFile) {
			fprintf(curlLogFile, "cURL Error during program revision check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			networkError = true;
			ri->error = true;
			ri->errorText = wxString(_("Unable to check for updates - either your Internet connection is down or LoTW is unreachable."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			networkError = true;
			ri->error = true;
			ri->errorText = wxString(_("Unable to check for updates. The network is down or the LoTW site is too busy."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			networkError = true;
			ri->error = true;
			ri->errorText = wxString(_("Unable to connect to the update site."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else { // some other error
			ri->message = true;
			wxString fmt = _("Error downloading update version information:");
			fmt += wxT("\n%hs");
			ri->errorText = wxString::Format(fmt, errorbuf);
		}
	}

	// Send the result back to the main thread
	wxCommandEvent* event = new wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED, bg_updateCheck);
	event->SetClientData(ri);
	wxPostEvent(frame, *event);
	ri->condition->Wait();
	ri->mutex->Unlock();
	delete ri;
	delete event;
	if (needToCleanUp) {
		if (curlReq) curl_easy_cleanup(curlReq);
		if (curlLogFile) fclose(curlLogFile);
		curlReq = NULL;
		curlLogFile = NULL;
	}

	// we checked today, and whatever the result, no need to (automatically) check again until the next interval

	config->Write(wxT("UpdateCheckTime"), wxDateTime::Today().FormatISODate());

	// After update check, validate user certificates
	if (!networkError)
		DoCheckExpiringCerts(noGUI);
	return;
}

static void
wx_tokens(const wxString& str, vector<wxString> &toks) {
	size_t idx = 0;
	size_t newidx;
	wxString tok;
	do {
		newidx = str.find(wxT(" "), idx);
		if (newidx != wxString::npos) {
			toks.push_back(str.Mid(idx, newidx - idx));
			idx = newidx + 1;
		}
	} while (newidx != wxString::npos);
	if (!str.Mid(idx).IsEmpty())
		toks.push_back(str.Mid(idx));
}

int
SaveAddressInfo(const char *callsign, int dxcc) {
	if (callsign == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	bool needToCleanUp = false;
	char url[512];
	strncpy(url, (wxString::Format(wxT("https://lotw.arrl.org/tqsl-setup.php?callsign=%hs&dxcc=%d"), callsign, dxcc)).ToUTF8(), sizeof url);
	tqslTrace("SaveAddressInfo", "Call = %s, Entity=%d, url = %s", callsign, dxcc, url);
	if (curlReq) {
		curl_easy_setopt(curlReq, CURLOPT_URL, url);
	} else {
		curlReq = tqsl_curl_init("checkLoc", url, &curlLogFile, false);
		needToCleanUp = true;
	}
	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	errorbuf[0] = '\0';
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(curlReq);
	wxString checkresult = wxT("");

	if (needToCleanUp) {
		if (curlLogFile)
			fclose(curlLogFile);
		curl_easy_cleanup(curlReq);
		curlReq = NULL;
	}

	if (retval == CURLE_OK) {
		if (handler.s != "null") {
			tqslTrace("SaveAddressInfo", "callsign=%s, result = %s", callsign, handler.s.c_str());
			tqsl_saveCallsignLocationInfo(callsign, handler.s.c_str());
		}
	} else {
		tqslTrace("save_address_info", "cURL Error during cert status check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		return 1;
	}

	return 0;
}

int
get_address_field(const char *callsign, const char *field, string& result) {
	typedef map<string, string>LocMap;
	static LocMap locInfo;

	if (callsign == NULL || field == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	result = "";

	if (strcmp(callsign, "[None]") == 0) {
		return 1;
	}

	if (locInfo["call"] ==  callsign) { // Already got data for this call
		LocMap::iterator it;

		it = locInfo.find(field);
		if (it == locInfo.end()) {
			return 1;
		}
		string temp = it->second;
		if (temp.empty() || temp == "null") {
			return 1;
		}
		result = temp;
		return 0;
	}

	locInfo.clear();

	char *buf;
	if (tqsl_getCallsignLocationInfo(callsign, &buf)) {
		return 1;
	}
	wxString checkresult = wxString::FromUTF8(buf);

	wxJSONReader reader;
	wxJSONValue root;

	int errors = reader.Parse(checkresult, &root);
	if (errors > 0)
		return 1;

	locInfo["call"] = callsign;

	locInfo["status"] = root[wxT("Status")].AsString().ToUTF8();
	if (locInfo["status"] != "OK") {
		return 1;
	}
	locInfo["dxcc"] = root[wxT("DXCC_Entity")].AsString().ToUTF8();
	locInfo["entity"] = root[wxT("DXCC_Entity_Name")].AsString().ToUTF8();
	locInfo["country"] = root[wxT("Country")].AsString().ToUTF8();
	locInfo["state"] = root[wxT("STATE")].AsString().ToUTF8();
	wxStringTokenizer toker(root[wxT("CNTY")].AsString(), wxT(","));
	wxString state = toker.GetNextToken();
	wxString cty = toker.GetNextToken();
	locInfo["county"] = cty.ToUTF8();
	if (locInfo["state"] == "") {
		locInfo["state"] = state.ToUTF8();
	}
	locInfo["grid"] = root[wxT("GRID")].AsString().ToUTF8();
	locInfo["cqzone"] = root[wxT("CQ Zone")].AsString().ToUTF8();
	if (locInfo["cqzone"].substr(0, 1) == "0") locInfo["cqzone"].erase(0, 1);
	locInfo["ituzone"] = root[wxT("ITU Zone")].AsString().ToUTF8();
	if (locInfo["ituzone"].substr(0, 1) == "0") locInfo["ituzone"].erase(0, 1);
	locInfo["pas"] = root[wxT("Primary_Admnistrative_Subdivision")].AsString().ToUTF8();
	locInfo["sas"] = root[wxT("Secondary_Admnistrative_Subdivision")].AsString().ToUTF8();
	locInfo["address"] = root[wxT("Address_In")].AsString().ToUTF8();
	string grids;
	wxJSONValue gridlist = root[wxT("VUCC_Grids")];
	for (int x = 0; x < gridlist.Size(); x++) {
		if (!grids.empty()) {
			grids = grids + "|";
		}
		grids = grids + ((string)gridlist[x].AsString().ToUTF8());
	}
	locInfo["grids"] = grids;
	wxString first = root[wxT("Source_address_components")][wxT("first_name")].AsString();
	if (first == wxT("null")) first = wxT("");
	wxString middle = root[wxT("Source_address_components")][wxT("middle_name")].AsString();
	if (middle == wxT("null")) middle = wxT("");
	wxString last = root[wxT("Source_address_components")][wxT("last_name")].AsString();
	if (last == wxT("null")) last = wxT("");

	wxString name;
	if (middle.IsEmpty())
		name = first + wxT(" ") + last;
	else
		name = first + wxT(" ") + middle + wxT(" ") + last;
	if (!name.IsEmpty() || name == wxT(" "))
		locInfo["name"] = ((string)name.ToUTF8());

	locInfo["addr1"] = root[wxT("Source_address_components")][wxT("addr1")].AsString().ToUTF8();
	locInfo["addr2"] = root[wxT("Source_address_components")][wxT("addr2")].AsString().ToUTF8();
	locInfo["addr3"] = root[wxT("Source_address_components")][wxT("addr3")].AsString().ToUTF8();
	locInfo["city"] = root[wxT("Source_address_components")][wxT("city")].AsString().ToUTF8();
	locInfo["addrState"] = root[wxT("Source_address_components")][wxT("state")].AsString().ToUTF8();
	locInfo["mailCode"] = root[wxT("Source_address_components")][wxT("mail_code")].AsString().ToUTF8();
	locInfo["aCountry"] = root[wxT("Source_address_components")][wxT("country")].AsString().ToUTF8();

	LocMap::iterator it;

	it = locInfo.find(field);
	if (it == locInfo.end()) {
		return 1;
	}
	string temp = it->second;
	if (temp.empty() || temp == "null") {
		return 1;
	}
	result = temp;
	return 0;
}

/*
 * Lookup of ULS info for a call.
 * Returns: 0 - Success
 * 	    1 - Unable to check
 *	    2 - Not found
 */
int
GetULSInfo(const char *callsign, wxString &name, wxString &attn, wxString &street, wxString &city, wxString &state, wxString &zip, wxString &updateDate) {
	if (callsign == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	bool needToCleanUp = false;
	char url[512];
	strncpy(url, (wxString::Format(wxT("https://lotw.arrl.org/uls.php?callsign=%hs"), callsign)).ToUTF8(), sizeof url);
	tqslTrace("GetULSInfo", "Call = %s, url = %s", callsign, url);
	if (curlReq) {
		curl_easy_setopt(curlReq, CURLOPT_URL, url);
	} else {
		curlReq = tqsl_curl_init("checkULS", url, &curlLogFile, false);
		needToCleanUp = true;
	}
	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	errorbuf[0] = '\0';
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);

	int retval = curl_easy_perform(curlReq);

	tqslTrace("GetULSInfo", "upload result = %d", retval);

	if (needToCleanUp) {
		if (curlLogFile)
			fclose(curlLogFile);
		curl_easy_cleanup(curlReq);
		curlReq = NULL;
	}

	if (retval == CURLE_OK) {
		if (handler.s != "null") {
			tqslTrace("GetULSInfo", "callsign=%s, result = %s", callsign, handler.s.c_str());
			// fields: name, callsign, street, city, state, zip.
			wxString checkresult = wxString::FromUTF8(handler.s.c_str());

			wxJSONReader reader;
			wxJSONValue root;

			int errors = reader.Parse(checkresult, &root);
			if (errors > 0)
				return 1;

			if (strcmp(root[wxT("callsign")].AsString().ToUTF8(), callsign) != 0)
				return 1;

			name = root[wxT("name")].AsString();
			attn = root[wxT("attention")].AsString();
			street = root[wxT("street")].AsString();
			city = root[wxT("city")].AsString();
			state = root[wxT("state")].AsString();
			zip = root[wxT("zip")].AsString();
			updateDate = root[wxT("ULS Last Updated")].AsString();
			return 0;
		} else {
			return 2;	// Not valid
		}
	} else {
		tqslTrace("GetULSInfo", "cURL Error %d during ULS check: %s (%s)\n", retval, curl_easy_strerror((CURLcode)retval), errorbuf);
		return 1;
	}

	return 0;
}

// Common method for sign and (save, upload) a log
void
MyFrame::ProcessQSODataFile(bool upload, bool compressed) {
	tqslTrace("MyFrame::ProcessQSODataFile", "upload=%d, compressed=%d", upload, compressed);
	wxString infile;
	wxString outfile;

	// Does the user have any certificates?
	get_certlist("", 0, false, false, true);	// any call, any DXCC, not expired or superceded
	if (ncerts == 0) {
		wxString msg = _("You have no callsign certificates to use to sign a log file.");
			msg += wxT("\n");
			msg += _("Please install a callsign certificate then try again.");
		wxMessageBox(msg, _("No Callsign Certificates"),
			   wxOK | wxICON_EXCLAMATION, this);
		free_certlist();
		return;
	}
	free_certlist();
	try {
		wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
		// Get input file
		wxString path = config->Read(wxT("ImportPath"), wxString(wxT("")));
		wxString defext = config->Read(wxT("ImportExtension"), wxString(wxT("adi"))).Lower();
		bool defFound = false;

		// Construct filter string for file-open dialog
		wxString filter = wxT("All files (*.*)|*.*");
		vector<wxString> exts;
		wxString file_exts = config->Read(wxT("ADIFFiles"), wxString(DEFAULT_ADIF_FILES));
		wx_tokens(file_exts, exts);
		wxString extList;
		for (int i = 0; i < static_cast<int>(exts.size()); i++) {
			extList += wxT("*.") + exts[i] + wxT(";");
			if (exts[i] == defext)
				defFound = true;
		}
		extList.RemoveLast();		// Remove the trailing semicolon
		filter += _("|ADIF files (") + extList + wxT(")|") + extList;

		exts.clear();
		extList.Clear();
		file_exts = config->Read(wxT("CabrilloFiles"), wxString(DEFAULT_CABRILLO_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < static_cast<int>(exts.size()); i++) {
			extList += wxT("*.") + exts[i] + wxT(";");
			if (exts[i] == defext)
				defFound = true;
		}
		extList.RemoveLast();
		filter += _("|Cabrillo files (") + extList + wxT(")|") + extList;
		if (defext.IsEmpty() || !defFound)
			defext = wxString(wxT("adi"));
		infile = wxFileSelector(_("Select file to Sign"), path, wxT(""), defext, filter,
			wxFD_OPEN|wxFD_FILE_MUST_EXIST, this);
		if (infile.IsEmpty())
			return;
		wxString inPath;
		wxString inExt;
		wxFileName::SplitPath(infile.c_str(), &inPath, NULL, &inExt);
		inExt.Lower();
		config->Write(wxT("ImportPath"), inPath);
		config->Write(wxT("ImportExtension"), inExt);
		if (!upload) {
			// Get output file
			wxString basename;
			wxFileName::SplitPath(infile.c_str(), 0, &basename, 0);
			path = wxConfig::Get()->Read(wxT("ExportPath"), wxString(wxT("")));
			wxString deftype = compressed ? wxT("tq8") : wxT("tq7");
			filter = compressed ? _("TQSL compressed data files (*.tq8)|*.tq8")
				: _("TQSL data files (*.tq7)|*.tq7");
			basename += wxT(".") + deftype;
			outfile = wxFileSelector(_("Select file to write to"),
				path, basename, deftype, filter + _("|All files (*.*)|*.*"),
				wxFD_SAVE|wxFD_OVERWRITE_PROMPT, this);
			if (outfile.IsEmpty())
				return;
			config->Write(wxT("ExportPath"), wxPathOnly(outfile));
		}

		// Get Station Location
		int n;
		tQSL_Location loc;
		check_tqsl_error(tqsl_initStationLocationCapture(&loc));
		check_tqsl_error(tqsl_getNumStationLocations(loc, &n));
		if (n != 1) {
			check_tqsl_error(tqsl_endStationLocationCapture(&loc));
			frame->Show(true);
			loc = SelectStationLocation(_("Select Station Location for Signing"));
		} else {
			// There's only one station location. Use that and don't prompt.
			char deflocn[512];
			check_tqsl_error(tqsl_getStationLocationName(loc, 0, deflocn, sizeof deflocn));
			check_tqsl_error(tqsl_endStationLocationCapture(&loc));
			check_tqsl_error(tqsl_getStationLocation(&loc, deflocn));
		}
		if (loc == 0)
			return;

		if (!verify_cert(loc, false))
			return;
		char callsign[40];
		char loc_name[256];
		int dxccnum;
		check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxccnum));
		check_tqsl_error(tqsl_getStationLocationCaptureName(loc, loc_name, sizeof loc_name));
		DXCC dxcc;
		dxcc.getByEntity(dxccnum);
		tqslTrace("MyFrame::ProcessQSODataFile", "file=%s location %hs, call %hs dxcc %hs",
				S(infile), loc_name, callsign, dxcc.name());
		if (strcmp(callsign, "[None]")) {
			LocPropDial dial(wxString::FromUTF8(loc_name), false, infile.ToUTF8(), this);
			int locnOK = dial.ShowModal();
			if (locnOK == wxID_OK) {
				if (upload) {
					UploadLogFile(loc, infile);
				} else {
					ConvertLogFile(loc, infile, outfile, compressed);
				}
			} else {
				wxLogMessage(_("Signing abandoned"));
			}
		} else {
			if (upload) {
				UploadLogFile(loc, infile);
			} else {
				ConvertLogFile(loc, infile, outfile, compressed);
			}
		}
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
	}
	catch(TQSLException& x) {
		wxString s;
		wxString err = wxString::FromUTF8(x.what());
		if (err.Find(infile) == wxNOT_FOUND) {
			if (!infile.IsEmpty())
				s = infile + wxT(": ");
		}
		s += err;
		wxLogError(wxT("%s"), (const char *)s.c_str());
	}
	return;
}

void
MyFrame::ImportQSODataFile(wxCommandEvent& event) {
	tqslTrace("MyFrame::ImportQSODataFile", NULL);

	bool compressed = (event.GetId() == tm_f_import_compress || event.GetId() == tl_Save);
	ProcessQSODataFile(false, compressed);
	return;
}

void
MyFrame::UploadQSODataFile(wxCommandEvent& event) {
	tqslTrace("MyFrame::UploadQSODataFile", NULL);
	ProcessQSODataFile(true, true);
	return;
}

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnPreferences", NULL);
	Preferences* dial = new Preferences(this, help);
	dial->Show(true);
	file_menu->Enable(tm_f_preferences, false);
}

class TQSLConfig {
 public:
	TQSLConfig() {
		callSign = "";
		serial = 0;
		dxcc = 0;
		dupes = 0;
		elementBody = wxT("");
		locstring = wxT("");
		config = NULL;
		outstr = NULL;
		conv = NULL;
	}
	void SaveSettings(gzFile* out, wxString appname);
	void RestoreCert(void);
	void RestoreConfig(const gzFile& in);
	void ParseLocations(gzFile* out, const tQSL_StationDataEnc loc);
	wxConfig *config;
	long serial;
	int dxcc;
	int dupes;
	string callSign;
	wxString signedCert;
	wxString privateKey;
	wxString elementBody;
	wxString locstring;
	gzFile* outstr;
	tQSL_Converter conv;

 private:
	static void xml_restore_start(void *data, const XML_Char *name, const XML_Char **atts);
	static void xml_restore_end(void *data, const XML_Char *name);
	static void xml_text(void *data, const XML_Char *text, int len);
	static void xml_location_start(void *data, const XML_Char *name, const XML_Char **atts);
	static void xml_location_end(void *data, const XML_Char *name);
};

// Save the user's configuration settings - appname is the
// application name (tqslapp)

void TQSLConfig::SaveSettings(gzFile* out, wxString appname) {
	tqslTrace("TQSLConfig::SaveSettings", "appname=%s", S(appname));
	config = new wxConfig(appname);
	wxString name, gname;
	long	context;
	wxString svalue;
	long	lvalue;
	bool	bvalue;
	double	dvalue;
	wxArrayString groupNames;

	tqslTrace("TQSLConfig::SaveSettings", "... init groups");
	groupNames.Add(wxT("/"));
	bool more = config->GetFirstGroup(gname, context);
	while (more) {
		tqslTrace("TQSLConfig::SaveSettings", "... add group %s", S(name));
		groupNames.Add(wxT("/") + gname);
		more = config->GetNextGroup(gname, context);
	}
	tqslTrace("TQSLConfig::SaveSettings", "... groups done.");

	for (unsigned i = 0; i < groupNames.GetCount(); i++) {
		tqslTrace("TQSLConfig::SaveSettings", "Group %d setting path %s", i, S(groupNames[i]));
		int err;
		config->SetPath(groupNames[i]);
		more = config->GetFirstEntry(name, context);
		while (more) {
			tqslTrace("TQSLConfig::SaveSettings", "name=%s", S(name));
			if (name.IsEmpty()) {
				more = config->GetNextEntry(name, context);
				continue;
			}
			if (gzprintf(*out, "<Setting name=\"%s\" group=\"%s\" ",
					(const char *)name.ToUTF8(), (const char *)groupNames[i].ToUTF8()) < 0) {
				throw TQSLException(gzerror(*out, &err));
			}
			wxConfigBase::EntryType etype = config->GetEntryType(name);
			switch (etype) {
				case wxConfigBase::Type_Unknown:
				case wxConfigBase::Type_String:
					config->Read(name, &svalue);
					long testlong;
					if (svalue.ToLong(&testlong)) {
						if (gzprintf(*out, "Type=\"Int\" Value=\"%d\"/>\n", testlong) < 0) {
							throw TQSLException(gzerror(*out, &err));
						}
					} else {
						urlEncode(svalue);
						if (gzprintf(*out, "Type=\"String\" Value=\"%s\"/>\n",
								(const char *)svalue.ToUTF8()) < 0) {
							throw TQSLException(gzerror(*out, &err));
						}
					}
					break;
				case wxConfigBase::Type_Boolean:
					config->Read(name, &bvalue);
					if (bvalue) {
						if (gzprintf(*out, "Type=\"Bool\" Value=\"true\"/>\n") < 0) {
							throw TQSLException(gzerror(*out, &err));
						}
					} else {
						if (gzprintf(*out, "Type=\"Bool\" Value=\"false\"/>\n") < 0) {
							throw TQSLException(gzerror(*out, &err));
						}
					} break;
				case wxConfigBase::Type_Integer:
					config->Read(name, &lvalue);
					if (gzprintf(*out, "Type=\"Int\" Value=\"%d\"/>\n", lvalue) < 0)
						throw TQSLException(gzerror(*out, &err));
					break;
				case wxConfigBase::Type_Float:
					config->Read(name, &dvalue);
					if (gzprintf(*out, "Type=\"Float\" Value=\"%f\"/>\n", dvalue) < 0)
						throw TQSLException(gzerror(*out, &err));
					break;
			}
			more = config->GetNextEntry(name, context);
		}
	}
	tqslTrace("TQSLConfig::SaveSettings", "Done.");
	config->SetPath(wxT("/"));

	return;
}

void
MyFrame::BackupConfig(const wxString& filename, bool quiet) {
	tqslTrace("MyFrame::BackupConfig", "filename=%s, quiet=%d", S(filename), quiet);
	int i;
	gzFile out = 0;
	int err;
	wxBusyCursor wait;
#ifdef _WIN32
	int fd = -1;
#endif
	if (lock_db(false) < 0) {
		if (quiet)			// If there's an active signing thread,
			return;			// then exit without taking a backup.
		wxSafeYield();
		wxLogMessage(_("TQSL must wait for other running copies of TQSL to exit before backing up..."));
		wxSafeYield();
		lock_db(true);
	}
	try {
#ifdef _WIN32
		wchar_t* lfn = utf8_to_wchar(filename.ToUTF8());
		fd = _wopen(lfn, _O_WRONLY |_O_CREAT|_O_BINARY, _S_IREAD|_S_IWRITE);
		free_wchar(lfn);
		if (fd != -1)
			out = gzdopen(fd, "wb9");
#else
		out = gzopen(filename.ToUTF8(), "wb9");
#endif
		if (!out) {
			wxLogError(_("Error opening save file %s: %hs"), filename.c_str(), strerror(errno));
			unlock_db();
			return;
		}
		TQSLConfig* conf = new TQSLConfig();

		if (gzprintf(out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<TQSL_Configuration>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (gzprintf(out, "<!-- Warning! If you directly edit this file, you are responsible for its content.\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (gzprintf(out, "The ARRL's LoTW Help Desk will be unable to assist you. -->\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (gzprintf(out, "<Certificates>\n") < 0)
			throw TQSLException(gzerror(out, &err));

		if (!quiet) {
			wxLogMessage(_("Saving callsign certificates"));
			wxSafeYield(frame);
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving callsign certificates");
		}
		int ncerts;
		char buf[8192];
		// Save root certificates
		check_tqsl_error(tqsl_selectCACertificates(&certlist, &ncerts, "root"));
		for (i = 0; i < ncerts; i++) {
			if (gzprintf(out, "<RootCert>\n") < 0)
				throw TQSLException(gzerror(out, &err));
			check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
			if (gzwrite(out, buf, strlen(buf)) < 0)
				throw TQSLException(gzerror(out, &err));
			if (gzprintf(out, "</RootCert>\n") < 0)
				throw TQSLException(gzerror(out, &err));
		}
		tqsl_freeCertificateList(certlist, ncerts);
		// Save CA certificates
		check_tqsl_error(tqsl_selectCACertificates(&certlist, &ncerts, "authorities"));
		for (i = 0; i < ncerts; i++) {
			if (gzprintf(out, "<CACert>\n") < 0)
				throw TQSLException(gzerror(out, &err));
			check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
			if (gzwrite(out, buf, strlen(buf)) < 0)
				throw TQSLException(gzerror(out, &err));
			if (gzprintf(out, "</CACert>\n") < 0)
				throw TQSLException(gzerror(out, &err));
		}
		tqsl_freeCertificateList(certlist, ncerts);
		tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED | TQSL_SELECT_CERT_SUPERCEDED);
		for (i = 0; i < ncerts; i++) {
			char callsign[64];
			long serial = 0;
			int dxcc = 0;
			int keyonly;
			check_tqsl_error(tqsl_getCertificateKeyOnly(certlist[i], &keyonly));
			check_tqsl_error(tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign));
			if (!keyonly) {
				check_tqsl_error(tqsl_getCertificateSerial(certlist[i], &serial));
			}
			check_tqsl_error(tqsl_getCertificateDXCCEntity(certlist[i], &dxcc));
			if (!quiet) {
				wxLogMessage(wxString(wxT("\t")) + _("Saving callsign certificate for %hs"), callsign);
			}
			if (gzprintf(out, "<UserCert CallSign=\"%s\" dxcc=\"%d\" serial=\"%d\">\n", callsign, dxcc, serial) < 0)
				throw TQSLException(gzerror(out, &err));
			if (!keyonly) {
				if (gzprintf(out, "<SignedCert>\n") < 0)
					throw TQSLException(gzerror(out, &err));
				check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
				if (gzwrite(out, buf, strlen(buf)) < 0)
					throw TQSLException(gzerror(out, &err));
				if (gzprintf(out, "</SignedCert>\n") < 0)
					throw TQSLException(gzerror(out, &err));
			}
			// Handle case where there's no private key
			if (tqsl_getKeyEncoded(certlist[i], buf, sizeof buf) == 0) {
				if (gzprintf(out, "<PrivateKey>\n") < 0)
					throw TQSLException(gzerror(out, &err));
				if (gzwrite(out, buf, strlen(buf)) < 0)
					throw TQSLException(gzerror(out, &err));
				if (gzprintf(out, "</PrivateKey>\n</UserCert>\n") < 0)
					throw TQSLException(gzerror(out, &err));
			} else {
				// No private key.
				if (gzprintf(out, "</UserCert>\n") < 0)
					throw TQSLException(gzerror(out, &err));
			}
		}
		free_certlist();
		if (gzprintf(out, "</Certificates>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (gzprintf(out, "<Locations>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (!quiet) {
			wxLogMessage(_("Saving Station Locations"));
			wxSafeYield(frame);
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving Station Locations");
		}

		tQSL_StationDataEnc sdbuf = NULL;
		check_tqsl_error(tqsl_getStationDataEnc(&sdbuf));
		TQSLConfig* parser = new TQSLConfig();
		if (sdbuf)
			parser->ParseLocations(&out, sdbuf);
		check_tqsl_error(tqsl_freeStationDataEnc(sdbuf));
		if (gzprintf(out, "</Locations>\n") < 0)
			throw TQSLException(gzerror(out, &err));

		if (!quiet) {
			wxLogMessage(_("Saving TQSL Preferences"));
			wxSafeYield(frame);
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving TQSL Preferences - out=0x%lx", reinterpret_cast<void *>(out));
		}

		if (gzprintf(out, "<TQSLSettings>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		conf->SaveSettings(&out, wxT("tqslapp"));
		tqslTrace("MyFrame::BackupConfig", "Done with settings. out=0x%lx", reinterpret_cast<void *>(out));
		if (gzprintf(out, "</TQSLSettings>\n") < 0)
			throw TQSLException(gzerror(out, &err));

		if (!quiet) {
			wxLogMessage(_("Saving QSOs"));
			wxSafeYield(frame);
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving QSOs");
		}

		tQSL_Converter conv = NULL;
		check_tqsl_error(tqsl_beginConverter(&conv));
		tqslTrace("MyFrame::BackupConfig", "beginConverter call success");
		if (gzprintf(out, "<DupeDb>\n") < 0)
			throw TQSLException(gzerror(out, &err));

		char dupekey[256];
		char dupedata[256];
		int count = 0;
		while (true) {
			int status = tqsl_getDuplicateRecordsV2(conv, dupekey, dupedata, sizeof(dupekey));
			if (status == -1)		// End of file
				break;
			check_tqsl_error(status);
			wxString dk = wxString::FromUTF8(dupekey);
			wxString dd = wxString::FromUTF8(dupedata);
			dk = urlEncode(dk);
			dd = urlEncode(dd);
			strncpy(dupekey, dk.ToUTF8(), sizeof dupekey);
			strncpy(dupedata, dd.ToUTF8(), sizeof dupedata);
			if (gzprintf(out, "<Dupe key=\"%s\" data=\"%s\" />\n", dupekey, dupedata) < 0)
				throw TQSLException(gzerror(out, &err));
			if (!quiet && (count++ % 100000) == 0) {
				wxSafeYield(frame);
			}
		}
		if (gzprintf(out, "</DupeDb>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		tqsl_converterCommit(conv);
		tqsl_endConverter(&conv);
		unlock_db();
		tqslTrace("MyFrame::BackupConfig", "Dupes db saved OK");

		if (gzprintf(out, "</TQSL_Configuration>\n") < 0)
			throw TQSLException(gzerror(out, &err));
		if (gzclose(out) != Z_OK)
			throw TQSLException(gzerror(out, &err));
		if (!quiet) {
			wxLogMessage(_("Save operation complete."));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Save operation complete.");
		}
	}
	catch(TQSLException& x) {
		unlock_db();
		if (out) gzclose(out);
		if (quiet) {
			wxString errmsg = wxString::Format(_("Error performing automatic backup: %hs"), x.what());
			wxMessageBox(errmsg, _("Backup Error"), wxOK | wxICON_EXCLAMATION, this);
		} else {
			wxLogError(_("Backup operation failed: %hs"), x.what());
		}
	}
}

void
MyFrame::OnSaveConfig(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnSaveConfig", NULL);
	try {
		wxString file_default = wxT("tqslconfig.tbk");
		wxString filename = wxFileSelector(_("Enter file to save to"), wxT(""),
			file_default, wxT(".tbk"), _("Configuration files (*.tbk)|*.tbk|All files (*.*)|*.*"),
			wxFD_SAVE|wxFD_OVERWRITE_PROMPT, this);
		if (filename.IsEmpty())
			return;

		BackupConfig(filename, false);
	}
	catch(TQSLException& x) {
		wxLogError(_("Backup operation failed: %hs"), x.what());
	}
}


void
restore_user_cert(TQSLConfig* loader) {
	tqslTrace("restore_user_cert", "Restoring certificate for callsign %s", loader->callSign.c_str());
	get_certlist(loader->callSign.c_str(), loader->dxcc, true, true, true);
	for (int i = 0; i < ncerts; i++) {
		long serial;
		int dxcc;
		int keyonly;
		check_tqsl_error(tqsl_getCertificateKeyOnly(certlist[i], &keyonly));
		check_tqsl_error(tqsl_getCertificateDXCCEntity(certlist[i], &dxcc));
		if (!keyonly) {
			check_tqsl_error(tqsl_getCertificateSerial(certlist[i], &serial));
			if (serial == loader->serial && dxcc == loader->dxcc) {
				// Is there a good private key as well?
				int pktype;
				pktype = tqsl_getCertificatePrivateKeyType(certlist[i]);
				if (pktype == TQSL_PK_TYPE_ERR || pktype == TQSL_PK_TYPE_NONE) {
					break;
				}
				return;			// This certificate is already installed.
			}
		} else {
			// See if the keyonly cert is the one we're trying to load
			char buf[8192];
			check_tqsl_error(tqsl_getKeyEncoded(certlist[i], buf, sizeof buf));
			if (!strcmp(buf, loader->privateKey.ToUTF8())) {
				return;			// Already installed
			}
		}
	}

	// There is no certificate matching this callsign/entity/serial.
	wxLogMessage(wxString(wxT("\t")) + _("Restoring callsign certificate for %hs"), loader->callSign.c_str());
	wxSafeYield(frame);
	check_tqsl_error(tqsl_importKeyPairEncoded(loader->callSign.c_str(), "user", loader->privateKey.ToUTF8(), loader->signedCert.ToUTF8()));
}

void
restore_root_cert(TQSLConfig* loader) {
	int rval = tqsl_importKeyPairEncoded(NULL, "root", NULL, loader->signedCert.ToUTF8());
	if (rval && tQSL_Error != TQSL_CERT_ERROR)
		check_tqsl_error(rval);
}

void
restore_ca_cert(TQSLConfig* loader) {
	int rval = tqsl_importKeyPairEncoded(NULL, "authorities", NULL, loader->signedCert.ToUTF8());
	if (rval && tQSL_Error != TQSL_CERT_ERROR)
		check_tqsl_error(rval);
}

void
TQSLConfig::xml_restore_start(void *data, const XML_Char *name, const XML_Char **atts) {
	TQSLConfig* loader = reinterpret_cast<TQSLConfig *> (data);
	int i;

	loader->elementBody = wxT("");
	if (strcmp(name, "UserCert") == 0) {
		for (int i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "CallSign") == 0) {
				loader->callSign = atts[i + 1];
			} else if (strcmp(atts[i], "serial") == 0) {
				if (strlen(atts[i+1]) == 0) {
					loader->serial = 0;
				} else {
					loader->serial =  strtol(atts[i+1], NULL, 10);
				}
			} else if (strcmp(atts[i], "dxcc") == 0) {
				if (strlen(atts[i+1]) == 0) {
					loader->dxcc = 0;
				} else {
					loader->dxcc =  strtol(atts[i+1], NULL, 10);
				}
			}
		}
		loader->privateKey = wxT("");
		loader->signedCert = wxT("");
	} else if (strcmp(name, "TQSLSettings") == 0) {
		wxLogMessage(_("Restoring Preferences"));
		wxSafeYield(frame);
		loader->config = new wxConfig(wxT("tqslapp"));
	} else if (strcmp(name, "Setting") == 0) {
		wxString sname;
		wxString sgroup;
		wxString stype;
		wxString svalue;
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "name") == 0) {
				sname = wxString::FromUTF8(atts[i+1]);
			} else if (strcmp(atts[i], "group") == 0) {
				sgroup = wxString::FromUTF8(atts[i+1]);
			} else if (strcmp(atts[i], "Type") == 0) {
				stype = wxString::FromUTF8(atts[i+1]);
			} else if (strcmp(atts[i], "Value") == 0) {
				svalue = wxString::FromUTF8(atts[i+1]);
			}
		}
		// Don't restore wxHtmlWindow as these settings are OS-specific.
		if (sgroup != wxT("wxHtmlWindow")) {
			loader->config->SetPath(sgroup);
			if (stype == wxT("String")) {
				svalue.Replace(wxT("&lt;"), wxT("<"), true);
				svalue.Replace(wxT("&gt;"), wxT(">"), true);
				svalue.Replace(wxT("&amp;"), wxT("&"), true);
				if (sname == wxT("BackupFolder") && !svalue.IsEmpty()) {
					// If it's the backup directory, don't restore it if the
					// referenced directory doesn't exist.
#ifdef _WIN32
					struct _stat32 s;
					wchar_t* lfn = utf8_to_wchar(svalue.ToUTF8());
					int ret = _wstat32(lfn, &s);
					free_wchar(lfn);
					if (ret == 0) {
#else
					struct stat s;
					if (lstat(svalue.ToUTF8(), &s) == 0) {		// Does it exist?
#endif
						if (S_ISDIR(s.st_mode)) {		// And is it a directory?
							loader->config->Write(sname, svalue); // OK to use it.
						}
					}
				} else {
					loader->config->Write(sname, svalue);
				}
			} else if (stype == wxT("Bool")) {
				bool bsw = (svalue == wxT("true"));
				loader->config->Write(sname, bsw);
			} else if (stype == wxT("Int")) {
				long lval = strtol(svalue.ToUTF8(), NULL, 10);
				loader->config->Write(sname, lval);
			} else if (stype == wxT("Float")) {
				double dval = strtod(svalue.ToUTF8(), NULL);
				loader->config->Write(sname, dval);
			}
			loader->config->SetPath(wxT("/"));
		}
	} else if (strcmp(name, "Locations") == 0) {
		wxLogMessage(_("Restoring Station Locations"));
		wxSafeYield(frame);
		loader->locstring = wxT("<StationDataFile>\n");
	} else if (strcmp(name, "Location") == 0) {
		for (i = 0; atts[i]; i+=2) {
			wxString attval = wxString::FromUTF8(atts[i+1]);
			if (strcmp(atts[i], "name") == 0) {
				tqslTrace("TQSLConfig::xml_restore_start", "Restoring location %s", atts[i+1]);
				loader->locstring += wxT("<StationData name=\"") + urlEncode(attval) + wxT("\">\n");
				break;
			}
		}
		for (i = 0; atts[i]; i+=2) {
			wxString attname = wxString::FromUTF8(atts[i]);
			wxString attval = wxString::FromUTF8(atts[i+1]);
			if (strcmp(atts[i], "name") != 0) {
				loader->locstring += wxT("<") + attname + wxT(">") +
					urlEncode(attval) + wxT("</") + attname + wxT(">\n");
			}
		}
	} else if (strcmp(name, "DupeDb") == 0) {
		wxLogMessage(_("Restoring QSO records"));
		wxSafeYield(frame);
		check_tqsl_error(tqsl_beginConverter(&loader->conv));
	} else if (strcmp(name, "Dupe") == 0) {
		const char *dupekey = NULL;
		const char *dupedata = NULL;
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "key") == 0) {
				dupekey = atts[i+1];
			}
			if (strcmp(atts[i], "data") == 0) {
				dupedata = atts[i+1];
			}
		}
		if (dupedata == NULL) {
			dupedata = "D"; // Old school dupe record
		}
		int status = tqsl_putDuplicateRecord(loader->conv,  dupekey, dupedata, dupekey ? strlen(dupekey) : 0);
		if (status > 0) {		// Error writing that record
			check_tqsl_error(status);
		}
		if ((loader->dupes++ % 100000) == 0) {
			wxSafeYield(frame);
		}
	}
}

void
TQSLConfig::xml_restore_end(void *data, const XML_Char *name) {
	TQSLConfig* loader = reinterpret_cast<TQSLConfig *> (data);
	if (strcmp(name, "SignedCert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
	} else if (strcmp(name, "PrivateKey") == 0) {
		loader->privateKey = loader->elementBody.Trim(false);
	} else if (strcmp(name, "RootCert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
		restore_root_cert(loader);
	} else if (strcmp(name, "CACert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
		restore_ca_cert(loader);
	} else if (strcmp(name, "UserCert") == 0) {
		restore_user_cert(loader);
	} else if (strcmp(name, "Location") == 0) {
		loader->locstring += wxT("</StationData>\n");
	} else if (strcmp(name, "Locations") == 0) {
		loader->locstring += wxT("</StationDataFile>\n");
		tqslTrace("TQSLConfig::xml_restore_end", "Merging station locations");
		if (tqsl_mergeStationLocations(loader->locstring.ToUTF8()) != 0) {
			char buf[500];
			strncpy(buf, getLocalizedErrorString().ToUTF8(), sizeof buf);
			wxLogError(wxString::Format(wxString(wxT("\t")) + _("Error importing station locations: %hs"), buf));
		}
		tqslTrace("TQSLConfig::xml_restore_end", "Completed merging station locations");
	} else if (strcmp(name, "TQSLSettings") == 0) {
		loader->config->Flush(false);
	} else if (strcmp(name, "DupeDb") == 0) {
		check_tqsl_error(tqsl_converterCommit(loader->conv));
		check_tqsl_error(tqsl_endConverter(&loader->conv));
	}
	loader->elementBody = wxT("");
}

void
TQSLConfig::xml_location_start(void *data, const XML_Char *name, const XML_Char **atts) {
	TQSLConfig* parser = reinterpret_cast<TQSLConfig *> (data);
	int err;

	if (strcmp(name, "StationDataFile") == 0)
		return;
	if (strcmp(name, "StationData") == 0) {
		wxString locname = wxString::FromUTF8(atts[1]);
		urlEncode(locname);
		if (gzprintf(*parser->outstr, "<Location name=\"%s\"", (const char *)locname.ToUTF8()) < 0)
			throw TQSLException(gzerror(*parser->outstr, &err));
	}
}
void
TQSLConfig::xml_location_end(void *data, const XML_Char *name) {
	TQSLConfig* parser = reinterpret_cast<TQSLConfig *> (data);
	int err;
	if (strcmp(name, "StationDataFile") == 0)
		return;
	if (strcmp(name, "StationData") == 0) {
		if (gzprintf(*parser->outstr , " />\n") < 0)
			throw TQSLException(gzerror(*parser->outstr, &err));
		return;
	}
	// Anything else is a station attribute. Add it to the definition.
	parser->elementBody.Trim(false);
	parser->elementBody.Trim(true);
	urlEncode(parser->elementBody);
	if (gzprintf(*parser->outstr,  " %s=\"%s\"", name, (const char *)parser->elementBody.ToUTF8()) < 0)
		throw TQSLException(gzerror(*parser->outstr, &err));
	parser->elementBody = wxT("");
}

void
TQSLConfig::xml_text(void *data, const XML_Char *text, int len) {
	TQSLConfig* loader = reinterpret_cast<TQSLConfig *>(data);
	char buf[512];
	memcpy(buf, text, len);
	buf[len] = '\0';
	loader->elementBody += wxString::FromUTF8(buf);
}

void
TQSLConfig::RestoreConfig(const gzFile& in) {
	tqslTrace("TQSLConfig::RestoreConfig", NULL);
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, reinterpret_cast<void *>(this));
	XML_SetStartElementHandler(xp, &TQSLConfig::xml_restore_start);
	XML_SetEndElementHandler(xp, &TQSLConfig::xml_restore_end);
	XML_SetCharacterDataHandler(xp, &TQSLConfig::xml_text);

	char buf[4096];
	wxBusyCursor wait;
	wxLogMessage(_("Restoring Callsign Certificates"));
	wxSafeYield(frame);
	int rcount = 0;
	do {
		rcount = gzread(in, buf, sizeof(buf));
		if (rcount > 0) {
			if (XML_Parse(xp, buf, rcount, 0) == 0) {
				wxLogError(_("Error parsing saved configuration file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
				XML_ParserFree(xp);
				return;
			}
		}
	} while (rcount > 0);
	if (!gzeof(in)) {
		int gerr;
		wxLogError(_("Error parsing saved configuration file: %hs"), gzerror(in, &gerr));
		XML_ParserFree(xp);
		return;
	}
	if (XML_Parse(xp, "", 0, 1) == 0) {
		wxLogError(_("Error parsing saved configuration file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
		XML_ParserFree(xp);
		return;
	}
	wxLogMessage(_("Restore Complete."));
}

void
TQSLConfig::ParseLocations(gzFile* out, const tQSL_StationDataEnc loc) {
	tqslTrace("TQSL::ParseLocations", "loc=%s", loc);
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, reinterpret_cast<void *>(this));
	XML_SetStartElementHandler(xp, &TQSLConfig::xml_location_start);
	XML_SetEndElementHandler(xp, &TQSLConfig::xml_location_end);
	XML_SetCharacterDataHandler(xp, &TQSLConfig::xml_text);
	outstr = out;

	if (XML_Parse(xp, loc, strlen(loc), 1) == 0) {
		wxLogError(_("Error parsing station location file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
		XML_ParserFree(xp);
		return;
	}
}

void
MyFrame::OnLoadConfig(wxCommandEvent& WXUNUSED(event)) {
#ifdef _WIN32
	int fd = -1;
#endif
	tqslTrace("MyFrame::OnLoadConfig", NULL);
	wxString filename = wxFileSelector(_("Select saved configuration file"), wxT(""),
					   wxT("tqslconfig.tbk"), wxT("tbk"), _("Saved configuration files (*.tbk)|*.tbk"),
					   wxFD_OPEN|wxFD_FILE_MUST_EXIST);
	if (filename.IsEmpty())
		return;

	gzFile in = 0;
	try {
#ifdef _WIN32
		wchar_t* lfn = utf8_to_wchar(filename.ToUTF8());
		fd = _wopen(lfn, _O_RDONLY|_O_BINARY);
		free_wchar(lfn);
		if (fd != -1)
			in = gzdopen(fd, "rb");
#else
		in = gzopen(filename.ToUTF8(), "rb");
#endif
		if (!in) {
			wxLogError(_("Error opening save file %s: %hs"), filename.c_str(), strerror(errno));
			return;
		}

		TQSLConfig loader;
		loader.RestoreConfig(in);
		cert_tree->Build(CERTLIST_FLAGS);
		loc_tree->Build();
		LocTreeReset();
		CertTreeReset();
		gzclose(in);
	}
	catch(TQSLException& x) {
		wxLogError(_("Restore operation failed: %hs"), x.what());
		gzclose(in);
	}
}

QSLApp::QSLApp() : wxApp() {
	lang = wxLANGUAGE_UNKNOWN;
	locale = NULL;
#ifdef __WXMAC__	// Tell wx to put these items on the proper menu
	wxApp::s_macAboutMenuItemId = long(tm_h_about);
	wxApp::s_macPreferencesMenuItemId = long(tm_f_preferences);
	wxApp::s_macExitMenuItemId = long(tm_f_exit);
#endif

	wxConfigBase::Set(new wxConfig(wxT("tqslapp")));
}

/*
wxLog *
QSLApp::CreateLogTarget() {
cerr << "called" << endl;
	MyFrame *mf = (MyFrame *)GetTopWindow();
	if (mf) {
		LogList *log = new LogList(this);
#if wxMAJOR_VERSION > 2
		log->SetFormatter(new SimpleLogFormatter);
#endif
		return log;
	}
	return 0;
}
*/

MyFrame *
QSLApp::GUIinit(bool checkUpdates, bool quiet) {
	tqslTrace("QSLApp::GUIinit", "checkUpdates=%d", checkUpdates);
	int x, y, w, h;
	bool maximized;
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Read(wxT("MainWindowX"), &x, 50);
	config->Read(wxT("MainWindowY"), &y, 50);
	config->Read(wxT("MainWindowWidth"), &w, 800);
	config->Read(wxT("MainWindowHeight"), &h, 600);
	config->Read(wxT("MainWindowMaximized"), &maximized, false);
	if (w < MAIN_WINDOW_MIN_WIDTH) w = MAIN_WINDOW_MIN_WIDTH;
	if (h < MAIN_WINDOW_MIN_HEIGHT) w = MAIN_WINDOW_MIN_HEIGHT;

	frame = new MyFrame(wxT("TQSL"), x, y, w, h, checkUpdates, quiet, locale);
	frame->SetMinSize(wxSize(MAIN_WINDOW_MIN_WIDTH, MAIN_WINDOW_MIN_HEIGHT));
	if (maximized)
		frame->Maximize();
	if (checkUpdates)
		frame->FirstTime();
	frame->Show(!quiet);
	if (checkUpdates && !quiet)
		frame->SetFocus();
	SetTopWindow(frame);

	return frame;
}

// Override OnRun so we can have a last-chance exception handler
// in case something doesn't handle an error.
int
QSLApp::OnRun() {
	tqslTrace("QSLApp::OnRun", NULL);
	try {
		if (m_exitOnFrameDelete == Later)
			m_exitOnFrameDelete = Yes;
		return MainLoop();
	}
	catch(TQSLException& x) {
		string msg = x.what();
		tqslTrace("QSLApp::OnRun", "Last chance handler, string=%s", (const char *)msg.c_str());
		cerr << "An exception has occurred! " << msg << endl;
		wxLogError(wxT("%hs"), x.what());
		exitNow(TQSL_EXIT_TQSL_ERROR, false);
	}
	return 0;
}

static vector<wxLanguage> langIds;
static wxArrayString langNames;

static void
initLang() {
	if (langIds.size() == 0) {
		char langfile[TQSL_MAX_PATH_LEN];
		FILE *lfp;
#ifdef _WIN32
		snprintf(langfile, sizeof langfile, "%s\\languages.dat", tQSL_RsrcDir);
		wchar_t *wfilename = utf8_to_wchar(langfile);
		if ((lfp = _wfopen(wfilename, L"rb, ccs=UTF-8")) == NULL) {
			free_wchar(wfilename);
#else
		snprintf(langfile, sizeof langfile, "%s/languages.dat", tQSL_RsrcDir);
		if ((lfp = fopen(langfile, "rb")) == NULL) {
#endif
			goto nosyslang;
		}
#ifdef _WIN32
		free_wchar(wfilename);
#endif

		char lBuf[128];
		while (fgets(lBuf, sizeof lBuf, lfp)) {
			wxStringTokenizer langData(wxString::FromUTF8(lBuf), wxT(","));
			langNames.Add(langData.GetNextToken());
			langIds.push_back(static_cast<wxLanguage>(strtol(langData.GetNextToken().ToUTF8(), NULL, 10)));
		}

		fclose(lfp);

 nosyslang:
		// Now merge in the user directory copy
#ifdef _WIN32
		snprintf(langfile, sizeof langfile, "%s\\languages.dat", tQSL_BaseDir);
		wfilename = utf8_to_wchar(langfile);
		if ((lfp = _wfopen(wfilename, L"rb, ccs=UTF-8")) == NULL) {
			free_wchar(wfilename);
#else
		snprintf(langfile, sizeof langfile, "%s/languages.dat", tQSL_BaseDir);
		if ((lfp = fopen(langfile, "rb")) == NULL) {
#endif
			return;
		}
#ifdef _WIN32
		free_wchar(wfilename);
#endif

		while (fgets(lBuf, sizeof lBuf, lfp)) {
			wxStringTokenizer langData(wxString::FromUTF8(lBuf), wxT(","));
			wxString ln = langData.GetNextToken();
			bool found = false;
			for (size_t i = 0; i < langIds.size(); i++) {
				if (langNames[i] == ln) {
					found = true;
					break;
				}
			}
			if (!found) {
				langNames.Add(ln);
				langIds.push_back(static_cast<wxLanguage>(strtol(langData.GetNextToken().ToUTF8(), NULL, 10)));
			}
		}
		fclose(lfp);
	}
	return;
}

bool
QSLApp::OnInit() {
	frame = 0;
	long lng = -1;

        wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
#ifdef _WIN32
	bool disa;
	config->Read(wxT("DisableAdminCheck"), &disa, false);
	if (!disa && IsElevated(NULL) == S_OK) {
		TOKEN_ELEVATION_TYPE tet = TokenElevationTypeDefault;
		GetElevationType(&tet);
		if (tet == TokenElevationTypeFull) {
			wxArrayString ch;
			ch.Add(_("Exit TQSL so I can re-run as a normal user"));
			ch.Add(_("Allow TQSL to continue this time."));
			ch.Add(_("Always allow running as Administrator."));
			wxSingleChoiceDialog dial(frame, _("TQSL must not be run 'As Administrator'"), _("Administrator Error"), ch);
			int res = dial.ShowModal();
			switch (res) {
			    case wxID_CANCEL:
				exitNow(TQSL_EXIT_TQSL_ERROR, quiet);
				break;
			    case wxID_OK:
				int sel = dial.GetSelection();
				switch (sel) {
					case 0:
						exitNow(TQSL_EXIT_TQSL_ERROR, quiet);
					case 1:
						break;
					case 2:
						config->Write(wxT("DisableAdminCheck"), true);
						break;
				}
				break;
			}
		}
	}
#endif	 // Administrator check
	int major, minor;
	if (tqsl_getConfigVersion(&major, &minor)) {
		wxMessageBox(getLocalizedErrorString(), _("Error"), wxOK | wxICON_ERROR, frame);
		exitNow(TQSL_EXIT_TQSL_ERROR, quiet);
	}

	initLang();
	config->Read(wxT("Language"), &lng, wxLANGUAGE_UNKNOWN);
	lang = (wxLanguage) lng;

	if (lang == wxLANGUAGE_UNKNOWN) {
		lang = wxLANGUAGE_DEFAULT;
	}

	// If this is (wx2) Chinese, map to Simplified
	if (lang == 43) {			// wxLANGUAGE_CHINESE
		lang = (wxLanguage) 44;		// wxLANGUAGE_CHINESE_SIMPLIFIED
	}

	for (lng = 0; (unsigned) lng < langIds.size(); lng++) {
		if (lang == langIds[lng])
			break;
	}

#ifdef __WXGTK__
	// Add locale search path for where we install language files
	locale->AddCatalogLookupPathPrefix(wxT("/usr/local/share/locale"));
#endif
	lang = langWX2toWX3(lang);		// Translate to wxWidgets 3 language ID.
	if (wxLocale::IsAvailable(lang)) {
		locale = new wxLocale(lang);
		if (!locale)
			locale = new wxLocale(wxLANGUAGE_DEFAULT);
	} else {
#ifdef DEBUG
		wxLogError(wxT("This language is not supported by the system."));
#endif
		locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}

	// Add a subdirectory for language files
	locale->AddCatalogLookupPathPrefix(wxT("lang"));
#ifdef _WIN32
	locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(tQSL_BaseDir) + wxT("\\lang"));
#else
	locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(tQSL_BaseDir) + wxT("/lang"));
#endif

	// Initialize the catalogs we'll be using
#if defined(TQSL_TESTING)
	const wxLanguageInfo* pinfo = wxLocale::GetLanguageInfo(lang);
	// Enabling this causes the error to pop up for any locale where we don't have a translation.
	// This should not be used in production.
	if (!locale->AddCatalog(wxT("tqslapp"))) {
		const char* cname = pinfo->CanonicalName.ToUTF8();
		wxLogError(wxT("Can't find the tqslapp catalog for locale '%s'."), cname);
	}
#else
	locale->AddCatalog(wxT("tqslapp"));
#endif
	locale->AddCatalog(wxT("wxstd"));

	// this catalog is installed in standard location on Linux systems and
	// shows that you may make use of the standard message catalogs as well
	//
	// If it's not installed on your system, it is just silently ignored
#ifdef __LINUX__
        {
		wxLogNull nolog;
		locale->AddCatalog(wxT("fileutils"));
	}
#endif

	wxFileSystem::AddHandler(new tqslInternetFSHandler());
	// Allow JAWS for windows to speak the context-sensitive help.
	wxHelpProvider::Set(new wxSimpleHelpProvider());

	//short circuit if no arguments

	if (argc <= 1) {
		GUIinit(true, quiet);
		return true;
	}

	tQSL_Location loc = 0;
	wxString locname;
	bool suppressdate = false;
	int action = TQSL_ACTION_UNSPEC;
	int logverify;
	bool upload = false;
	char *password = NULL;
	char *defcall = NULL;
	wxString infile(wxT(""));
	wxString outfile(wxT(""));
	wxString importfile(wxT(""));
	wxString diagfile(wxT(""));

	config->Read(wxT("LogVerify"), &logverify, TQSL_LOC_REPORT);
	wxCmdLineParser parser;

#if wxMAJOR_VERSION > 2 || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
#define arg(x) (x)
#define i18narg(x) (x)
#else
#define arg(x) wxT(x)
#define i18narg(x) _(x)
#endif
	// arg letters used abcdef.hi..lmnopq.stuvwx..
	static const wxCmdLineEntryDesc cmdLineDesc[] = {
		{ wxCMD_LINE_OPTION, arg("a"), arg("action"),	i18narg("Specify dialog action - abort, all, compliant or ask") },
		{ wxCMD_LINE_OPTION, arg("b"), arg("begindate"), i18narg("Specify start date for QSOs to sign") },
		{ wxCMD_LINE_OPTION, arg("c"), arg("callsign"),	i18narg("Specify default callsign for log signing") },
		{ wxCMD_LINE_SWITCH, arg("d"), arg("nodate"),	i18narg("Suppress date range dialog") },
		{ wxCMD_LINE_OPTION, arg("e"), arg("enddate"),	i18narg("Specify end date for QSOs to sign") },
		{ wxCMD_LINE_OPTION, arg("f"), arg("verify"),	i18narg("Specify QSO verification action - ignore, report or update") },
		// not used - "g"
		{ wxCMD_LINE_SWITCH, arg("h"), arg("help"),	i18narg("Display command line help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
		{ wxCMD_LINE_OPTION, arg("i"), arg("import"),	i18narg("Import a certificate file (.p12 or .tq6)") },
		// not used - "j", "k"
		{ wxCMD_LINE_OPTION, arg("l"), arg("location"),	i18narg("Selects Station Location") },
		// not used - "m"
		{ wxCMD_LINE_SWITCH, arg("n"), arg("updates"),	i18narg("Check for updates to tqsl and the configuration file") },
		{ wxCMD_LINE_OPTION, arg("o"), arg("output"),	i18narg("Output file name (defaults to input name minus extension plus .tq8") },
		{ wxCMD_LINE_OPTION, arg("p"), arg("password"),	i18narg("Passphrase for the signing key") },
		{ wxCMD_LINE_SWITCH, arg("q"), arg("quiet"),	i18narg("Quiet Mode - same behavior as -x") },
		// not used - "r"

		{ wxCMD_LINE_SWITCH, arg("s"), arg("editlocation"), i18narg("Edit (if used with -l) or create Station Location") },
		{ wxCMD_LINE_OPTION, arg("t"), arg("diagnose"),	i18narg("File name for diagnostic tracking log") },
		{ wxCMD_LINE_SWITCH, arg("u"), arg("upload"),	i18narg("Upload after signing instead of saving") },
		{ wxCMD_LINE_SWITCH, arg("v"), arg("version"),  i18narg("Display the version information and exit") },
		// not used - "w"
		{ wxCMD_LINE_SWITCH, arg("x"), arg("batch"),	i18narg("Exit after processing log (otherwise start normally)") },
		// not used - "y", "z"

		{ wxCMD_LINE_PARAM,  NULL, NULL,		i18narg("Input ADIF or Cabrillo log file to sign"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
		{ wxCMD_LINE_NONE }
	};

	// Lowercase command options
	origCommandLine = argv[0];
	char** myArgv = new char*[argc];
	int myArgc;
#if wxMAJOR_VERSION == 2
	wxString av = argv[0];
	myArgv[0] = strdup(av.ToUTF8());
#else
	myArgv[0] = strdup(argv[0].ToUTF8());
#endif
	myArgc = 1;
	for (int i = 1; i < argc; i++) {
		if ((i + 1) < argc) {
			wxString av1 = argv[i+1];
#if wxMAJOR_VERSION == 2
			av = argv[i];
			if (av == wxT("-p")  && av1.IsEmpty()) {		// -p with blank password
#else
			if (argv[i] == "-p" && av1.IsEmpty()) {			// -p with blank password
#endif
				i++;						// skip -p and password
				continue;
			}
		}
		origCommandLine += wxT(" ");
#if wxMAJOR_VERSION == 2
		myArgv[myArgc] = strdup(wxString(argv[i]).ToUTF8());
#else
		myArgv[myArgc] = strdup(argv[i].ToUTF8());
#endif
#ifdef _WIN32
		if (myArgv[myArgc][0] == '-' || myArgv[myArgc][0] == '/')
			if (wxIsalpha(myArgv[myArgc][1]) && wxIsupper(myArgv[myArgc][1]))
				myArgv[myArgc][1] = wxTolower(myArgv[myArgc][1]);
#endif
#if wxMAJOR_VERSION == 2
		origCommandLine += wxString::FromUTF8(myArgv[myArgc]);
#else
		origCommandLine += myArgv[myArgc];
#endif
		myArgc++;
	}

	parser.SetCmdLine(myArgc, myArgv);
	parser.SetDesc(cmdLineDesc);
	// only allow "-" for options, otherwise "/path/something.adif"
	// is parsed as "-path"
	//parser.SetSwitchChars(wxT("-")); //by default, this is '-' on Unix, or '-' or '/' on Windows. We should respect the Win32 conventions, but allow the cross-platform Unix one for cross-plat loggers
	int parseStatus = parser.Parse(true);
	if (parseStatus == -1) {	// said "-h"
		return false;
	}
	// Always display TQSL version
	if ((!parser.Found(wxT("n"))) || parser.Found(wxT("v"))) {
		cerr << "TQSL Version " TQSL_VERSION " " TQSL_BUILD "\n";
	}
	if (parseStatus != 0)  {
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}

	// version already displayed - just exit
	if (parser.Found(wxT("v"))) {
		return false;
	}

	if (parser.Found(wxT("x")) || parser.Found(wxT("q"))) {
		quiet = true;
		LogStderr *logger = new LogStderr();
#if wxMAJOR_VERSION > 2
		logger->SetFormatter(new SimpleLogFormatter);
#endif
		wxLog::SetActiveTarget(logger);
	}

	if (parser.Found(wxT("t"), &diagfile)) {
		if (tqsl_openDiagFile(diagfile.ToUTF8())) {
			cerr << "Error opening diagnostic log " << diagfile.ToUTF8() << ": " << strerror(errno) << endl;
		} else {
			wxString about = getAbout();
			tqslTrace(NULL, "TQSL Diagnostics\r\n%s\n\n", (const char *)about.ToUTF8());
			tqslTrace(NULL, "Command Line: %s\r\n", (const char *)origCommandLine.ToUTF8());
			if (tqsl_init()) {
				wxLogError(getLocalizedErrorString());
			}
			tqslTrace(NULL, "Working Directory: %s\r\n", tQSL_BaseDir);
		}
	}

	if (tqsl_init()) { // Init tqsllib
		wxLogError(getLocalizedErrorString());
	}
	// check for logical command switches
	if (parser.Found(wxT("o")) && parser.Found(wxT("u"))) {
		cerr << "Option -o cannot be combined with -u" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}
	if ((parser.Found(wxT("o")) || parser.Found(wxT("u"))) && parser.Found(wxT("s"))) {
		cerr << "Option -s cannot be combined with -u or -o" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}
	if (parser.Found(wxT("s")) && parser.GetParamCount() > 0) {
		cerr << "Option -s cannot be combined with an input file" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}

	// Request to check for new versions of tqsl/config/certs
	if (parser.Found(wxT("n"))) {
		if (parser.Found(wxT("i")) || parser.Found(wxT("o")) ||
		    parser.Found(wxT("s")) || parser.Found(wxT("u"))) {
			cerr << "Option -n cannot be combined with any other options" << endl;
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		}
		frame = GUIinit(false, true);
		frame->Show(false);
		// Check for updates then bail out.
		LogStderr *logger = new LogStderr();
#if wxMAJOR_VERSION > 2
		logger->SetFormatter(new SimpleLogFormatter);
#endif
		wxLog::SetActiveTarget(logger);
		frame->DoUpdateCheck(false, true);
		return(false);
	}

	frame = GUIinit(!quiet, quiet);
	if (quiet) {
		frame->Show(false);
	}

	if (parser.Found(wxT("l"), &locname)) {
		locname.Trim(true);			// clean up whitespace
		locname.Trim(false);
		tqsl_endStationLocationCapture(&loc);
		if (tqsl_getStationLocation(&loc, locname.ToUTF8())) {
			if (quiet) {
				wxLogError(getLocalizedErrorString());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			} else {
				wxMessageBox(getLocalizedErrorString(), ErrorTitle, wxOK | wxICON_ERROR | wxCENTRE, frame);
				return false;
			}
		}
	}

	wxString call;
	if (parser.Found(wxT("c"), &call)) {
		call.Trim(true);
		call.Trim(false);
		defcall = strdup(call.MakeUpper().ToUTF8());
	}

	wxString pwd;
	if (parser.Found(wxT("p"), &pwd)) {
		password = strdup(pwd.ToUTF8());
		utf8_to_ucs2(password, unipwd, sizeof unipwd);
	}

	if (parser.Found(wxT("d"))) {
		suppressdate = true;
	}
	wxString start = wxT("");
	wxString end = wxT("");
	tQSL_Date* startdate = NULL;
	tQSL_Date* enddate = NULL;
	tQSL_Date s, e;
	if (parser.Found(wxT("b"), &start)) {
		if (start.Trim().IsEmpty()) {
			startdate = NULL;
		} else if (tqsl_initDate(&s, start.ToUTF8()) || !tqsl_isDateValid(&s)) {
			if (quiet) {
				wxLogError(_("Start date of %s is invalid"), start.c_str());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			} else {
				wxMessageBox(wxString::Format(_("Start date of %s is invalid"), start.c_str()), ErrorTitle, wxOK | wxICON_ERROR | wxCENTRE, frame);
				return false;
			}
		}
		startdate = &s;
	}
	if (parser.Found(wxT("e"), &end)) {
		if (end.Trim().IsEmpty()) {
			enddate = NULL;
		} else if (tqsl_initDate(&e, end.ToUTF8()) || !tqsl_isDateValid(&e)) {
			if (quiet) {
				wxLogError(_("End date of %s is invalid"), end.c_str());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			} else {
				wxMessageBox(wxString::Format(_("End date of %s is invalid"), end.c_str()), ErrorTitle, wxOK | wxICON_ERROR | wxCENTRE, frame);
				return false;
			}
		}
		enddate = &e;
	}

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
			if (quiet)
				wxLogMessage(_("The -a parameter %hs is not recognized"), tmp);
			else
				cerr << "The action parameter " << tmp << " is not recognized" << endl;
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		}
	}

	wxString verify;
	if (parser.Found(wxT("f"), &verify)) {			// action for verifying QSOs
		if (!verify.CmpNoCase(wxT("ignore"))) {
			logverify = TQSL_LOC_IGNORE;
		} else if (!verify.CmpNoCase(wxT("report"))) {
			logverify = TQSL_LOC_REPORT;
		} else if (!verify.CmpNoCase(wxT("update"))) {
			logverify = TQSL_LOC_UPDATE;
		} else {
			char tmp[100];
			strncpy(tmp, (const char *)verify.ToUTF8(), sizeof tmp);
			tmp[sizeof tmp -1] = '\0';
			if (quiet)
				// TRANSLATORS: -f is the command line switch for log QTH handling
				wxLogMessage(_("The -f parameter %hs is not recognized"), tmp);
			else
				cerr << "The verify parameter " << tmp << " is not recognized" << endl;
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		}
	}

	if (parser.Found(wxT("u"))) {
		upload = true;
	}
	if (parser.Found(wxT("s"))) {
		// Add/Edit station location
		if (loc == 0) {
			if (tqsl_initStationLocationCapture(&loc)) {
				wxLogError(getLocalizedErrorString());
			}
			AddEditStationLocation(loc, true);
		} else {
			AddEditStationLocation(loc, false, _("Edit Station Location"));
		}
		tqsl_endStationLocationCapture(&loc);
		return false;
	}
	if (parser.GetParamCount() > 0) {
		infile = parser.GetParam(0);
	}

	wxString path, name, ext;
	wxFileName::SplitPath(infile, &path, &name, &ext);

	// Handle "-i" (import cert), or bare cert file on command line

	bool tq6File = false;
	bool p12File = false;
	if (!wxIsEmpty(infile)) {
		if (ext.CmpNoCase(wxT("tq6")) == 0) {
			tq6File = true;
		}
		if (ext.CmpNoCase(wxT("p12")) == 0) {
			p12File = true;
		}
	}
	if (parser.Found(wxT("i"), &infile) && (!wxIsEmpty(infile))) {
		wxFileName::SplitPath(infile, &path, &name, &ext);
		if (ext.CmpNoCase(wxT("tq6")) == 0) {
			tq6File = true;
		}
		if (ext.CmpNoCase(wxT("p12")) == 0) {
			p12File = true;
		}
	}

	if (tq6File || p12File) {
		infile.Trim(true).Trim(false);
		notifyData nd;
		if (tq6File) {
			if (tqsl_importTQSLFile(infile.ToUTF8(), notifyImport, &nd)) {
				if (tQSL_Error != TQSL_CERT_ERROR) {
					if (tQSL_Error != 0) wxLogError(getLocalizedErrorString());
				}
			} else {
				wxString call = wxString::FromUTF8(tQSL_ImportCall);
				wxString pending = config->Read(wxT("RequestPending"));
				pending.Replace(call, wxT(""), true);
				wxString rest;
				while (pending.StartsWith(wxT(","), &rest))
					pending = rest;
				while (pending.EndsWith(wxT(","), &rest))
					pending = rest;
				config->Write(wxT("RequestPending"), pending);
				cert_cleanup();
				frame->cert_tree->Build(CERTLIST_FLAGS);
			}
			wxLogMessage(nd.Message());
			return(true);
		} else if (p12File) {
			// First try with no password
			if (!tqsl_importPKCS12File(infile.ToUTF8(), "", 0, NULL, notifyImport, &nd) || tQSL_Error == TQSL_CERT_ERROR) {
				if (tQSL_Error != 0) wxLogError(getLocalizedErrorString());
			} else if (tQSL_Error == TQSL_PASSWORD_ERROR) {
				if ((password == NULL) || !tqsl_importPKCS12File(infile.ToUTF8(), password, 0, NULL, notifyImport, &nd))
					wxLogError(_("To import this passphrase protected P12 file, you must pass the passphrase on the command line"));
			} else if (tQSL_Error == TQSL_OPENSSL_ERROR) {
				wxLogError(_("This file is not a valid P12 file"));
			}
			cert_cleanup();
			frame->cert_tree->Build(CERTLIST_FLAGS);
			frame->CertTreeReset();
			wxLogMessage(nd.Message());
			return(true);
		}
	}

	// We need a logfile, else there's nothing to do.
	if (wxIsEmpty(infile)) {	// Nothing to sign
		if (quiet) {
			wxLogError(_("No logfile to sign!"));
			tqslTrace(NULL, "%s", "No logfile to sign!");
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			return false;
		}
		return true;
	}

	bool editAdif = DEFAULT_ADIF_EDIT;
	config->Read(wxT("AdifEdit"), &editAdif, DEFAULT_ADIF_EDIT);

	// If it's an ADIF file, invoke the editor if that's the only argument
	// unless we're running in batch mode
	if (editAdif && !quiet && !wxIsEmpty(infile) && (ext.CmpNoCase(wxT("adi")) || ext.CmpNoCase(wxT("adif")))) {
		QSORecordList recs;
		loadQSOfile(infile, recs);
		wxMessageBox(_("Warning: The TQSL ADIF editor only processes a limited number of ADIF fields.\n\nUsing the editor on an ADIF file can cause QSO details to be lost!"), _("Warning"), wxOK | wxICON_EXCLAMATION, frame);
		try {
			QSODataDialog dial(frame, infile, frame->help, &recs);
			dial.ShowModal();
		} catch(TQSLException& x) {
			wxLogError(wxT("%hs"), x.what());
		}
		exitNow(TQSL_EXIT_SUCCESS, quiet);
	}

	// Assume that it's a log to sign

	if (logverify == TQSL_LOC_UPDATE && loc == NULL) {			// Update mode, use fake empty location.
		if (tqsl_getStationLocation(&loc, ".empty")) {			// Already exists?
			tqsl_initStationLocationCapture(&loc);			// No, make a new one.
			tqsl_setStationLocationCaptureName(loc, ".empty");
			tqsl_saveStationLocationCapture(loc, 1);
			tqsl_getStationLocation(&loc, ".empty");
		}
	}

	if (loc == 0) {
		try {
			int n;
			check_tqsl_error(tqsl_initStationLocationCapture(&loc));
			check_tqsl_error(tqsl_getNumStationLocations(loc, &n));
			if (n != 1) {
				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
				frame->Show(true);
				loc = frame->SelectStationLocation(_("Select Station Location for Signing"));
			} else {
				// There's only one station location. Use that and don't prompt.
				char deflocn[512];
				check_tqsl_error(tqsl_getStationLocationName(loc, 0, deflocn, sizeof deflocn));
				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
				check_tqsl_error(tqsl_getStationLocation(&loc, deflocn));
			}
		}
		catch(TQSLException& x) {
			wxLogError(wxT("%hs"), x.what());
			if (quiet)
				exitNow(TQSL_EXIT_CANCEL, quiet);
		}
	}
	// If no location specified and not chosen, can't sign. Exit.
	if (loc == 0) {
		if (quiet)
			exitNow(TQSL_EXIT_CANCEL, quiet);
		return false;
	}
	if (!wxIsEmpty(outfile)) {
		path = outfile;
	} else {
		if (!wxIsEmpty(path))
			path += wxT("/");
		path += name + wxT(".tq8");
	}
	if (upload) {
		try {
			int val = frame->UploadLogFile(loc, infile, true, suppressdate, startdate, enddate, action, logverify, password, defcall);
			if (quiet)
				exitNow(val, quiet);
			else
				return true;	// Run the GUI
		} catch(TQSLException& x) {
			wxString s;
			wxString err = wxString::FromUTF8(x.what());
			if (err.Find(infile) == wxNOT_FOUND) {
				if (!infile.empty())
					s = infile + wxT(": ");
			}
			s += err;
			wxLogError(wxT("%s"), (const char *)s.c_str());
			if (quiet)
				exitNow(TQSL_EXIT_LIB_ERROR, quiet);
			else
				return true;
		}
	} else {
		try {
			int val = frame->ConvertLogFile(loc, infile, path, true, suppressdate, startdate, enddate, action, logverify, password, defcall);
			if (quiet)
				exitNow(val, quiet);
			else
				return true;
		} catch(TQSLException& x) {
			wxString s;
			wxString err = wxString::FromUTF8(x.what());
			if (err.Find(infile) == wxNOT_FOUND) {
				if (!infile.IsEmpty())
					s = infile + wxT(": ");
			}
			s += err;
			wxLogError(wxT("%s"), (const char *)s.c_str());
			if (quiet)
				exitNow(TQSL_EXIT_LIB_ERROR, quiet);
			else
				return true;
		}
	}
	check_tqsl_error(tqsl_endStationLocationCapture(&loc));
	if (quiet)
		exitNow(TQSL_EXIT_SUCCESS, quiet);
	return true;
} // NOLINT(readability/fn_size)


void MyFrame::FirstTime(void) {
	tqslTrace("MyFrame::FirstTime", NULL);
	if (wxConfig::Get()->Read(wxT("HasRun")).IsEmpty()) {
		wxConfig::Get()->Write(wxT("HasRun"), wxT("yes"));
		DisplayHelp();
		wxMessageBox(_("Please review the introductory documentation before using this program."),
			_("Notice"), wxOK | wxICON_INFORMATION, this);
	}
	int ncerts = cert_tree->Build(CERTLIST_FLAGS);
	CertTreeReset();
	if (ncerts == 0) {
		wxString msg = _("You have no callsign certificate installed on this computer with which to sign log submissions.");
		msg += wxT("\n");
		msg += _("Would you like to request a callsign certificate now?");
		if (wxMessageBox(msg, _("Alert"), wxYES_NO | wxICON_QUESTION, this) == wxYES) {
			wxCommandEvent e;
			CRQWizard(e);
		}
	}
	wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
	wxStringTokenizer tkz(pending, wxT(","));
	while (tkz.HasMoreTokens()) {
		wxString pend = tkz.GetNextToken();
		bool found = false;
		tQSL_Cert *certs;
		int ncerts = 0;
		if (!tqsl_selectCertificates(&certs, &ncerts, pend.ToUTF8(), 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS)) {
			for (int i = 0; i < ncerts; i++) {
				int keyonly;
				if (!tqsl_getCertificateKeyOnly(certs[i], &keyonly)) {
					if (!found && keyonly)
						found = true;
				}
			}
			tqsl_freeCertificateList(certs, ncerts);
		}

		if (!found) {
			// Remove this call from the list of pending certificate requests
			wxString p = wxConfig::Get()->Read(wxT("RequestPending"));
			p.Replace(pend, wxT(""), true);
			wxString rest;
			while (p.StartsWith(wxT(","), &rest))
				p = rest;
			while (p.EndsWith(wxT(","), &rest))
				p = rest;
			wxConfig::Get()->Write(wxT("RequestPending"), p);
		}
	}

// This check has been obsolete for decades. Remove it.
#ifdef CHECK_FOR_BETA_CERTS
	if (ncerts > 0) {
		TQ_WXCOOKIE cookie;
		wxTreeItemId it = cert_tree->GetFirstChild(cert_tree->GetRootItem(), cookie);
		while (it.IsOk()) {
			if (cert_tree->GetItemText(it) == wxT("Test Certificate Authority")) {
				wxMessageBox(wxT("You must delete your beta-test certificates (the ones\n")
					wxT("listed under \"Test Certificate Authority\") to ensure proprer\n")
					wxT("operation of the TrustedQSL software."), wxT("Warning"), wxOK, this);
				break;
			}
			it = cert_tree->GetNextChild(cert_tree->GetRootItem(), cookie);
		}
	}
#endif

// Copy tqslcert preferences to tqsl unless already done.
	if (wxConfig::Get()->Read(wxT("PrefsMigrated")).IsEmpty()) {
		wxConfig::Get()->Write(wxT("PrefsMigrated"), wxT("yes"));
		tqslTrace("MyFrame::FirstTime", "Migrating preferences from tqslcert");
		wxConfig* certconfig = new wxConfig(wxT("tqslcert"));

		wxString name, gname;
		long	context;
		wxString svalue;
		long	lvalue;
		bool	bvalue;
		double	dvalue;
		wxArrayString groupNames;

		groupNames.Add(wxT("/"));
		bool more = certconfig->GetFirstGroup(gname, context);
		while (more) {
			groupNames.Add(wxT("/") + gname);
			more = certconfig->GetNextGroup(gname, context);
		}

		for (unsigned i = 0; i < groupNames.GetCount(); i++) {
			certconfig->SetPath(groupNames[i]);
			wxConfig::Get()->SetPath(groupNames[i]);
			more = certconfig->GetFirstEntry(name, context);
			while (more) {
				wxConfigBase::EntryType etype = certconfig->GetEntryType(name);
				switch (etype) {
					case wxConfigBase::Type_Unknown:
					case wxConfigBase::Type_String:
						certconfig->Read(name, &svalue);
						wxConfig::Get()->Write(name, svalue);
						break;
					case wxConfigBase::Type_Boolean:
						certconfig->Read(name, &bvalue);
						wxConfig::Get()->Write(name, bvalue);
						break;
					case wxConfigBase::Type_Integer:
						certconfig->Read(name, &lvalue);
						wxConfig::Get()->Write(name, lvalue);
						break;
					case wxConfigBase::Type_Float:
						certconfig->Read(name, &dvalue);
						wxConfig::Get()->Write(name, dvalue);
						break;
				}
				more = certconfig->GetNextEntry(name, context);
			}
		}
		delete certconfig;
		wxConfig::Get()->SetPath(wxT("/"));
		wxConfig::Get()->Flush();
	}
	// Find and report conflicting mode maps
	init_modes();
	// Run automatic check for updates - except for wx 2.9, which breaks threading.
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	if (config->Read(wxT("AutoUpdateCheck"), true)) {
		DoUpdateCheck(true, false);
	}
	return;
}

wxMenu *
makeCertificateMenu(bool enable, bool keyonly, const char *callsign) {
	tqslTrace("makeCertificateMenu", "enable=%d, keyonly=%d", enable, keyonly);
	wxMenu *c_menu = new wxMenu;
	c_menu->Append(tc_c_Properties, _("Display Callsign Certificate &Properties"));
	c_menu->AppendSeparator();
	c_menu->Append(tc_c_Load, _("&Load Callsign Certificate from File"));
	c_menu->Append(tc_c_Export, _("&Save Callsign Certificate to File..."));
	c_menu->Enable(tc_c_Export, enable);
	if (!keyonly) {
		c_menu->AppendSeparator();
		c_menu->Append(tc_c_New, _("Request &New Callsign Certificate..."));
		c_menu->Append(tc_c_Renew, _("&Renew Callsign Certificate"));
		c_menu->Enable(tc_c_Renew, enable);
	} else {
		c_menu->AppendSeparator();
	}
	c_menu->Append(tc_c_Delete, _("&Delete Callsign Certificate"));
	c_menu->Enable(tc_c_Delete, (callsign != NULL));
	c_menu->AppendSeparator();

	int ncalls = 0;
	tqsl_getDeletedCallsignCertificates(NULL, &ncalls, callsign);
	c_menu->Append(tc_c_Undelete, _("Restore Deleted Callsign Certificate"));
	c_menu->Enable(tc_c_Undelete, ncalls > 0);

	return c_menu;
}

wxMenu *
makeLocationMenu(bool enable) {
	tqslTrace("makeLocationMenu", "enable=%d", enable);
	wxMenu *loc_menu = new wxMenu;
	loc_menu->Append(tl_c_Properties, _("&Properties"));
	loc_menu->Enable(tl_c_Properties, enable);
	stn_menu->Enable(tm_s_Properties, enable);
	loc_menu->Append(tl_c_Edit, _("&Edit"));
	loc_menu->Enable(tl_c_Edit, enable);
	loc_menu->Append(tl_c_Delete, _("&Delete"));
	loc_menu->Enable(tl_c_Delete, enable);
	return loc_menu;
}

// Handle clean-up after a certificate is imported
static void
cert_cleanup() {
	if (tQSL_ImportCall[0] != '\0') {				// If a user cert was imported
		if (tQSL_ImportSerial != 0) {
			wxString status;
			frame->CheckCertStatus(tQSL_ImportSerial, status);	// Update from LoTW's "CRL"
			tqsl_setCertificateStatus(tQSL_ImportSerial, (const char *)status.ToUTF8());
		}
		int certstat = tqsl_getCertificateStatus(tQSL_ImportSerial);

		if (tQSL_ImportCall[0] != '\0' && tQSL_ImportSerial != 0 && (certstat == TQSL_CERT_STATUS_OK || certstat == TQSL_CERT_STATUS_UNK)) {
			get_certlist(tQSL_ImportCall, 0, true, true, true);	// Get any superceded ones for this call
			for (int i = 0; i < ncerts; i++) {
				long serial = 0;
				int keyonly = false;
				tqsl_getCertificateKeyOnly(certlist[i], &keyonly);
				if (keyonly) {
					tqsl_deleteCertificate(certlist[i]);
				}
				if (tqsl_getCertificateSerial(certlist[i], &serial)) {
					continue;
				}
				if (serial == tQSL_ImportSerial) {	// Don't delete the one we just imported
					continue;
				}
				// This is not the one we just imported
				int sup, exp;
				if (tqsl_isCertificateSuperceded(certlist[i], &sup) == 0 && sup) {
					tqsl_deleteCertificate(certlist[i]);
				} else if (tqsl_isCertificateExpired(certlist[i], &exp) == 0 && exp) {
					tqsl_deleteCertificate(certlist[i]);
				}
			}
		}
	}
}

/////////// Frame /////////////

void MyFrame::OnLoadCertificateFile(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLoadCertificateFile", NULL);
	LoadCertWiz lcw(this, help, _("Load Certificate File"));
	lcw.RunWizard();
	cert_cleanup();
	cert_tree->Build(CERTLIST_FLAGS);
	CertTreeReset();
}

void MyFrame::CRQWizardRenew(wxCommandEvent& event) {
	tqslTrace("MyFrame::CRQWizardRenew", NULL);
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()));
	req = 0;
	tQSL_Cert cert;
	wxString callSign, name, address1, address2, city, state, postalCode,
		country, emailAddress;
	if (data != NULL && (cert = data->getCert()) != 0) {	// Should be true
		char buf[256];
		req = new TQSL_CERT_REQ;
		if (!tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf))
			strncpy(req->providerName, buf, sizeof req->providerName);
		if (!tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf))
			strncpy(req->providerUnit, buf, sizeof req->providerUnit);
		if (!tqsl_getCertificateCallSign(cert, buf, sizeof buf)) {
			callSign = wxString::FromUTF8(buf);
			strncpy(req->callSign, callSign.ToUTF8(), sizeof req->callSign);
		}
		if (!tqsl_getCertificateAROName(cert, buf, sizeof buf)) {
			name = wxString::FromUTF8(buf);
			strncpy(req->name, name.ToUTF8(), sizeof req->name);
		}
		tqsl_getCertificateDXCCEntity(cert, &(req->dxccEntity));
		tqsl_getCertificateQSONotBeforeDate(cert, &(req->qsoNotBefore));
		tqsl_getCertificateQSONotAfterDate(cert, &(req->qsoNotAfter));
		if (!tqsl_getCertificateEmailAddress(cert, buf, sizeof buf)) {
			emailAddress = wxString::FromUTF8(buf);
			strncpy(req->emailAddress, emailAddress.ToUTF8(), sizeof req->emailAddress);
		}
		if (!tqsl_getCertificateRequestAddress1(cert, buf, sizeof buf)) {
			address1 = wxString::FromUTF8(buf);
			strncpy(req->address1, address1.ToUTF8(), sizeof req->address1);
		}
		if (!tqsl_getCertificateRequestAddress2(cert, buf, sizeof buf)) {
			address2 = wxString::FromUTF8(buf);
			strncpy(req->address2, address2.ToUTF8(), sizeof req->address2);
		}
		if (!tqsl_getCertificateRequestCity(cert, buf, sizeof buf)) {
			city = wxString::FromUTF8(buf);
			strncpy(req->city, city.ToUTF8(), sizeof req->city);
		}
		if (!tqsl_getCertificateRequestState(cert, buf, sizeof buf)) {
			state = wxString::FromUTF8(buf);
			strncpy(req->state, state.ToUTF8(), sizeof req->state);
		}
		if (!tqsl_getCertificateRequestPostalCode(cert, buf, sizeof buf)) {
			postalCode = wxString::FromUTF8(buf);
			strncpy(req->postalCode, postalCode.ToUTF8(), sizeof req->postalCode);
		}
		if (!tqsl_getCertificateRequestCountry(cert, buf, sizeof buf)) {
			country = wxString::FromUTF8(buf);
			strncpy(req->country, country.ToUTF8(), sizeof req->country);
		}
	}
	CRQWizard(event);
	if (req)
		delete req;
	req = 0;
}

// Delete an abandoned/failed cert request
static void deleteRequest(const char *callsign, int dxccEntity) {
	int savedError = tQSL_Error;
	free_certlist();
	tqsl_selectCertificates(&certlist, &ncerts, callsign, dxccEntity, 0, 0, TQSL_SELECT_CERT_WITHKEYS);
	int ko;
	for (int i = 0; i < ncerts; i ++) {
		if (!tqsl_getCertificateKeyOnly(certlist[i], &ko) && ko) {
			if (tqsl_deleteCertificate(certlist[i])) {
				wxLogError(getLocalizedErrorString());
			}
			certlist = NULL;		// Invalidated in deleteCertificate flow
			ncerts = 0;
			tQSL_Error = savedError;
			return;
		}
	}
	tQSL_Error = savedError;
	return;
}

void MyFrame::CRQWizard(wxCommandEvent& event) {
	tqslTrace("MyFrame::CRQWizard", NULL);
	char renew = (req != 0) ? 1 : 0;
	tQSL_Cert cert = (renew ? (reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()))->getCert()) : 0);
	CRQWiz wiz(req, cert, this, help, renew ? _("Renew a Callsign Certificate") : _("Request a new Callsign Certificate"));
	int retval = 0;
/*
	CRQ_ProviderPage *prov = new CRQ_ProviderPage(wiz, req);
	CRQ_IntroPage *intro = new CRQ_IntroPage(wiz, req);
	CRQ_NamePage *name = new CRQ_NamePage(wiz, req);
	CRQ_EmailPage *email = new CRQ_EmailPage(wiz, req);
	wxSize size = prov->GetSize();
	if (intro->GetSize().GetWidth() > size.GetWidth())
		size = intro->GetSize();
	if (name->GetSize().GetWidth() > size.GetWidth())
		size = name->GetSize();
	if (email->GetSize().GetWidth() > size.GetWidth())
		size = email->GetSize();
	CRQ_PasswordPage *pw = new CRQ_PasswordPage(wiz);
	CRQ_SignPage *sign = new CRQ_SignPage(wiz, size, &(prov->provider));
	wxWizardPageSimple::Chain(prov, intro);
	wxWizardPageSimple::Chain(intro, name);
	wxWizardPageSimple::Chain(name, email);
	wxWizardPageSimple::Chain(email, pw);
	if (renew)
		sign->cert = ;
	else
		wxWizardPageSimple::Chain(pw, sign);

	wiz.SetPageSize(size);

*/

	if (wiz.RunWizard()) {
		// Save or upload?
		wxString file = flattenCallSign(wiz.callsign) + wxT(".") + wxT(TQSL_CRQ_FILE_EXT);
		bool upload = false;
		wxString msg = _("Do you want to upload this certificate request to LoTW now?");
		if (!renew) {
			msg += wxT("\n");
			msg += _("You do not need an account on LoTW to do this.");
		}
		if (wxMessageBox(msg, _("Upload"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
			upload = true;
			// Save it in the working directory
#ifdef _WIN32
			file = wxString::FromUTF8(tQSL_BaseDir) + wxT("\\") + file;
#else
			file = wxString::FromUTF8(tQSL_BaseDir) + wxT("/") + file;
#endif
		} else {
			// Where to put it?
			wxString wildcard = _("tQSL Cert Request files (*.");
			wildcard += wxString::FromUTF8(TQSL_CRQ_FILE_EXT ")|*." TQSL_CRQ_FILE_EXT);
			wildcard += _("|All files (") + wxString::FromUTF8(ALLFILESWILD ")|" ALLFILESWILD);
			file = wxFileSelector(_("Save request"), wxT(""), file, wxT(TQSL_CRQ_FILE_EXT), wildcard,
				wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
			if (file.IsEmpty()) {
				wxLogMessage(_("Request cancelled"));
				deleteRequest(wiz.callsign.ToUTF8(), wiz.dxcc);
				return;
			}
		}
		TQSL_CERT_REQ req;
		strncpy(req.providerName, wiz.provider.organizationName, sizeof req.providerName);
		strncpy(req.providerUnit, wiz.provider.organizationalUnitName, sizeof req.providerUnit);
		strncpy(req.callSign, wiz.callsign.ToUTF8(), sizeof req.callSign);
		strncpy(req.name, wiz.name.ToUTF8(), sizeof req.name);
		strncpy(req.address1, wiz.addr1.ToUTF8(), sizeof req.address1);
		strncpy(req.address2, wiz.addr2.ToUTF8(), sizeof req.address2);
		strncpy(req.city, wiz.city.ToUTF8(), sizeof req.city);
		strncpy(req.state, wiz.state.ToUTF8(), sizeof req.state);
		strncpy(req.postalCode, wiz.zip.ToUTF8(), sizeof req.postalCode);
		if (wiz.country.IsEmpty())
			strncpy(req.country, "USA", sizeof req.country);
		else
			strncpy(req.country, wiz.country.ToUTF8(), sizeof req.country);
		strncpy(req.emailAddress, wiz.email.ToUTF8(), sizeof req.emailAddress);
		strncpy(req.password, wiz.password.ToUTF8(), sizeof req.password);
		req.dxccEntity = wiz.dxcc;
		req.qsoNotBefore = wiz.qsonotbefore;
		req.qsoNotAfter = wiz.qsonotafter;
		req.signer = wiz.cert;
		if (req.signer) {
			char buf[40];
			void *call = 0;
			if (!tqsl_getCertificateCallSign(req.signer, buf, sizeof(buf)))
				call = &buf;
			while (tqsl_beginSigning(req.signer, 0, getPassword, call)) {
				if (tQSL_Error != TQSL_PASSWORD_ERROR) {
					if (tQSL_Error == TQSL_CUSTOM_ERROR && (tQSL_Errno == ENOENT || tQSL_Errno == EPERM)) {
						snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
							"Can't open the private key file for %s: %s", static_cast<char *>(call), strerror(tQSL_Errno));
					}
					wxLogError(getLocalizedErrorString());
					deleteRequest(req.callSign, req.dxccEntity);
					return;
				}
				// Try signing with the unicode version of the password
				if (tqsl_beginSigning(req.signer, unipwd, NULL, call) == 0) {
					// If OK, signing is ready to go.
					break;
				}
				if (tQSL_Error != TQSL_PASSWORD_ERROR) {
					deleteRequest(req.callSign, req.dxccEntity);
					return;
				} else {
					wxLogError(getLocalizedErrorString());
				}
			}
		}
		req.renew = renew ? 1 : 0;
		if (tqsl_createCertRequest(file.ToUTF8(), &req, 0, 0)) {
			wxString msg = getLocalizedErrorString();
			if (req.signer)
				tqsl_endSigning(req.signer);
			wxLogError(msg);
			char m[500];
			strncpy(m, msg.ToUTF8(), sizeof m);
			wxMessageBox(wxString::Format(_("Error creating callsign certificate request: %hs"), m), _("Error creating Callsign Certificate Request"), wxOK | wxICON_EXCLAMATION, this);
			deleteRequest(req.callSign, req.dxccEntity);
			return;
		}
		if (upload) {
#ifdef _WIN32
			wchar_t* wfile = utf8_to_wchar(file.ToUTF8());
			ifstream in(wfile, ios::in | ios::binary);
			free_wchar(wfile);
#else
			ifstream in(file.ToUTF8(), ios::in | ios::binary);
#endif
			if (!in) {
				wxLogError(_("Error opening certificate request file %s: %hs"), file.c_str(), strerror(errno));
				deleteRequest(req.callSign, req.dxccEntity);
			} else {
				string contents;
				in.seekg(0, ios::end);
				contents.resize(in.tellg());
				in.seekg(0, ios::beg);
				in.read(&contents[0], contents.size());
				in.close();

				wxString fileType(_("Certificate Request"));
				retval = UploadFile(file, file.ToUTF8(), 0, reinterpret_cast<void *>(const_cast<char *>(contents.c_str())),
							contents.size(), fileType);
				if (retval != 0) {
					wxLogError(_("Your certificate request did not upload properly"));
					wxLogError(_("Please try again."));
					deleteRequest(req.callSign, req.dxccEntity);
				}
			}
		} else {
			wxString msg = _("You may now send your new certificate request (");
			msg += file;
			msg += wxT(")");
			if (wiz.provider.emailAddress[0] != 0) {
				msg += wxT("\n");
				msg += _("to:");
				msg += wxT("\n   ");
				msg += wxString::FromUTF8(wiz.provider.emailAddress);
			}
			if (wiz.provider.url[0] != 0) {
				msg += wxT("\n");
				if (wiz.provider.emailAddress[0] != 0)
					msg += _("or ");
				msg += wxString(_("see:"));
				msg += wxT("\n    ");
				msg += wxString::FromUTF8(wiz.provider.url);
			}
			wxMessageBox(msg, wxT("TQSL"), wxOK | wxICON_ERROR, this);
		}
		if (retval == 0) {
			wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
			if (pending.IsEmpty())
				pending = wiz.callsign;
			else
				pending += wxT(",") + wiz.callsign;
			wxConfig::Get()->Write(wxT("RequestPending"), pending);
			// Record another certificate
                	wxString requestRecord = wxConfig::Get()->Read(wxT("RequestRecord"));
                	time_t now = time(NULL);
                	if (!requestRecord.IsEmpty()) {
                        	requestRecord = requestRecord + wxT(",");
                	}
                	requestRecord = requestRecord + wxString::Format(wxT("%s:%lu"), wiz.callsign.c_str(), now);
                	wxConfig::Get()->Write(wxT("RequestRecord"), requestRecord);
                	wxConfig::Get()->Flush();
		}
		if (req.signer)
			tqsl_endSigning(req.signer);
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
	}
}

void
MyFrame::CertTreeReset() {
	if (!cert_save_label) return;
	wxString nl = wxT("\n");
	cert_save_label->SetLabel(nl + _("Save a Callsign Certificate"));
	cert_save_button->SetLabel(nl + _("Save a Callsign Certificate"));
	cert_renew_label->SetLabel(nl + _("Renew a Callsign Certificate"));
	cert_renew_button->SetLabel(nl + _("Renew a Callsign Certificate"));
	cert_prop_label->SetLabel(nl + _("Display a Callsign Certificate"));
	cert_prop_button->SetLabel(nl + _("Display a Callsign Certificate"));
	cert_menu->Enable(tc_c_Renew, false);
	cert_renew_button->Enable(false);
	cert_select_label->SetLabel(nl + _("Select a Callsign Certificate to process"));
	cert_save_button->Enable(false);
	cert_prop_button->Enable(false);
	int ncalls = 0;
	tqsl_getDeletedCallsignCertificates(NULL, &ncalls, NULL);
	cert_menu->Enable(tc_c_Undelete, ncalls > 0);
}

void MyFrame::OnCertTreeSel(wxTreeEvent& event) {
	tqslTrace("MyFrame::OnCertTreeSel", NULL);
	wxTreeItemId id = event.GetItem();
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(id));
	if (data) {
		int keyonly = 0;
		int expired = 0;
		int superseded = 0;
		int deleted = 0;
		char call[40];
		tqsl_getCertificateCallSign(data->getCert(), call, sizeof call);
		wxString callSign = wxString::FromUTF8(call);
		tqsl_getCertificateKeyOnly(data->getCert(), &keyonly);
		tqsl_isCertificateExpired(data->getCert(), &expired);
		tqsl_isCertificateSuperceded(data->getCert(), &superseded);
		tqsl_getDeletedCallsignCertificates(NULL, &deleted, call);
		tqslTrace("MyFrame::OnCertTreeSel", "call=%s", call);

		cert_select_label->SetLabel(wxT(""));
		cert_menu->Enable(tc_c_Properties, true);
		cert_menu->Enable(tc_c_Export, true);
		cert_menu->Enable(tc_c_Delete, true);
		cert_menu->Enable(tc_c_Renew, true);
		cert_menu->Enable(tc_c_Undelete, deleted != 0);
		cert_save_button->Enable(true);
		cert_load_button->Enable(true);
		cert_prop_button->Enable(true);

		int w, h;
		loc_add_label->GetSize(&w, &h);
		wxString nl = wxT("\n");
		cert_save_label->SetLabel(nl + _("Save the Callsign Certificate for") + wxT(" ") + callSign);
		cert_save_label->Wrap(w - 10);
		cert_save_button->SetLabel(nl + _("Save the Callsign Certificate for") + wxT(" ") + callSign);
		cert_prop_label->SetLabel(nl + _("Display the Callsign Certificate properties for") + wxT(" ") + callSign);
		cert_prop_label->Wrap(w - 10);
		cert_prop_button->SetLabel(nl + _("Display the Callsign Certificate properties for") + wxT(" ") + callSign);
		if (!(keyonly || expired || superseded)) {
			cert_renew_label->SetLabel(nl + _("Renew the Callsign Certificate for") +wxT(" ") + callSign);
			cert_renew_label->Wrap(w - 10);
			cert_renew_button->SetLabel(nl + _("Renew the Callsign Certificate for") +wxT(" ") + callSign);
		} else {
			cert_renew_label->SetLabel(nl + _("Renew a Callsign Certificate"));
			cert_renew_button->SetLabel(nl + _("Renew a Callsign Certificate"));
		}
		cert_menu->Enable(tc_c_Renew, !(keyonly || expired || superseded));
		cert_menu->Enable(tc_c_Undelete, deleted != 0);
		cert_renew_button->Enable(!(keyonly || expired || superseded));
	} else {
		CertTreeReset();
	}
}

void MyFrame::OnCertProperties(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertProperties", NULL);
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()));
	if (data != NULL)
		displayCertProperties(data, this);
}

void MyFrame::OnCertExport(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertExport", NULL);
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()));
	if (data == NULL)	// "Never happens"
		return;

	char call[40];
	if (tqsl_getCertificateCallSign(data->getCert(), call, sizeof call)) {
		wxLogError(getLocalizedErrorString());
		return;
	}
	wxString filename;
	if (!data->path.IsEmpty() && !data->basename.IsEmpty()) {
		filename = data->path + data->basename + wxT(".p12");

	} else {
		tqslTrace("MyFrame::OnCertExport", "call=%s", call);
		wxString file_default = flattenCallSign(wxString::FromUTF8(call));
		int ko = 0;
		tqsl_getCertificateKeyOnly(data->getCert(), &ko);
		if (ko)
			file_default += wxT("-key-only");
		file_default += wxT(".p12");
		wxString path = wxConfig::Get()->Read(wxT("CertFilePath"), wxT(""));
		filename = wxFileSelector(_("Enter the name for the new Certificate Container file"), path,
			file_default, wxT(".p12"), _("Certificate Container files (*.p12)|*.p12|All files (*.*)|*.*"),
			wxFD_SAVE|wxFD_OVERWRITE_PROMPT, this);
		if (filename.IsEmpty())
			return;
	}

	wxConfig::Get()->Write(wxT("CertFilePath"), wxPathOnly(filename));
	wxString msg = _("Enter the passphrase for the certificate container file.");
		msg += wxT("\n\n");
		msg += _("If you are using a computer system that is shared with others, you should specify a passphrase to protect this certificate. However, if you are using a computer in a private residence, no passphrase need be specified.");
		msg += wxT("\n\n");
		msg += _("You will have to enter the passphrase any time you load the file into TrustedQSL.");
		msg += wxT("\n\n");
		msg += _("Leave the passphrase blank and click 'OK' unless you want to use a passphrase.");
		msg += wxT("\n\n");
	GetNewPasswordDialog dial(this, _("Certificate Container Passphrase"), msg, true, help, wxT("save.htm"));
	if (dial.ShowModal() != wxID_OK)
		return;	// Cancelled
	int terr;
	do {
		terr = tqsl_beginSigning(data->getCert(), 0, getPassword, reinterpret_cast<void *>(&call));
		if (terr) {
			if (tQSL_Error == TQSL_PASSWORD_ERROR) {
				terr = tqsl_beginSigning(data->getCert(), unipwd, NULL, reinterpret_cast<void *>(&call));
				if (terr) {
					if (tQSL_Error == TQSL_PASSWORD_ERROR)
						continue;
					wxLogError(getLocalizedErrorString());
				}
				continue;
			}
			if (tQSL_Error == TQSL_OPERATOR_ABORT)
				return;
			// Unable to open the private key
			if (tQSL_Error == TQSL_CUSTOM_ERROR && (tQSL_Errno == ENOENT || tQSL_Errno == EPERM)) {
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
					"Can't open the private key file for %s: %s", call, strerror(tQSL_Errno));
			}
			wxLogError(getLocalizedErrorString());
			return;
		}
	} while (terr);
	// When setting the password, always use UTF8.
	if (tqsl_exportPKCS12File(data->getCert(), filename.ToUTF8(), dial.Password().ToUTF8())) {
		char buf[500];
		strncpy(buf, getLocalizedErrorString().ToUTF8(), sizeof buf);
		wxLogError(wxString::Format(_("Export to %s failed: %hs"), filename.c_str(), buf));
	} else {
		wxLogMessage(_("Certificate saved in file %s"), filename.c_str());
	}
	tqsl_endSigning(data->getCert());
}

void MyFrame::OnCertDelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertDelete", NULL);
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()));
	if (data == NULL)	// "Never happens"
		return;

	wxString warn = _("This will remove the selected callsign certificate from your system.");
	warn += wxT("\n");
	warn += _("You will NOT be able to recover it by loading a .TQ6 file.");
	warn += wxT("\n");
	warn += _("You WILL be able to recover it from a container (.p12) file,");
	warn += wxT("\n");
	warn += _("if you have created one via the Callsign Certificate menu's");
	warn += wxT("\n");
	warn += _("'Save Callsign Certificate' command.");
	warn += wxT("\n\n");
	warn += _("Are you sure you want to delete the certificate?");
	if (wxMessageBox(warn, _("Warning"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		char buf[128];
		if (!tqsl_getCertificateCallSign(data->getCert(), buf, sizeof buf)) {
			wxString call = wxString::FromUTF8(buf);
			wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
			pending.Replace(call, wxT(""), true);
			wxString rest;
			while (pending.StartsWith(wxT(","), &rest))
				pending = rest;
			while (pending.EndsWith(wxT(","), &rest))
				pending = rest;
			wxConfig::Get()->Write(wxT("RequestPending"), pending);
		}
		int keyonly, sup, exp;
		long serial;
		tqsl_getCertificateKeyOnly(data->getCert(), &keyonly);
		tqsl_getCertificateSerial(data->getCert(), &serial);
		tqsl_isCertificateExpired(data->getCert(), &exp);
		tqsl_isCertificateSuperceded(data->getCert(), &sup);
		tqslTrace("MyFrame::OnCertDelete", "About to delete cert for callsign %s, serial %ld, keyonly %d, superceded %d, expired %d", buf, serial, keyonly, sup, exp);
		if (tqsl_deleteCertificate(data->getCert()))
			wxLogError(getLocalizedErrorString());
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
	}
}

void MyFrame::OnCertUndelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertUndelete", NULL);

	int ncalls;
	char **calls = NULL;

	try {
		check_tqsl_error(tqsl_getDeletedCallsignCertificates(&calls, &ncalls, NULL));

		if (ncalls <= 0) {
			wxMessageBox(_("There are no deleted Callsign Certificates to restore"), _("Undelete Error"), wxOK | wxICON_EXCLAMATION, this);
			return;
		}

		wxArrayString choices;
		choices.clear();
		for (int i = 0; i < ncalls; i++) {
			choices.Add(wxString::FromUTF8(calls[i]));
		}
		choices.Sort();

		wxString selected = wxGetSingleChoice(_("Choose a Callsign Certificate to restore"),
					 	_("Callsign Certificates"),
						choices);
		if (selected.IsEmpty())
			return;			// Cancelled

		check_tqsl_error(tqsl_restoreCallsignCertificate(selected.ToUTF8()));
		tqsl_freeDeletedCertificateList(calls, ncalls);
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
	} catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::LocTreeReset() {
	if (!loc_edit_button) return;
	loc_edit_button->Disable();
	loc_delete_button->Disable();
	loc_prop_button->Disable();
	stn_menu->Enable(tm_s_Properties, false);
	wxString nl = wxT("\n");
	loc_edit_label->SetLabel(nl + _("Edit a Station Location"));
	loc_edit_button->SetLabel(nl + _("Edit a Station Location"));
	loc_delete_label->SetLabel(nl + _("Delete a Station Location"));
	loc_delete_button->SetLabel(nl + _("Delete a Station Location"));
	loc_prop_label->SetLabel(nl + _("Display Station Location Properties"));
	loc_prop_button->SetLabel(nl + _("Display Station Location Properties"));
	loc_select_label->SetLabel(nl + _("Select a Station Location to process"));
}

void MyFrame::OnLocTreeSel(wxTreeEvent& event) {
	tqslTrace("MyFrame::OnLocTreeSel", NULL);
	wxTreeItemId id = event.GetItem();
	LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(id));
	if (data) {
		int w, h;
		wxString lname = data->getLocname();
		wxString call = data->getCallSign();
		tqslTrace("MyFrame::OnLocTreeSel", "lname=%s, call=%s", S(lname), S(call));

		loc_add_label->GetSize(&w, &h);

		loc_edit_button->Enable();
		loc_delete_button->Enable();
		loc_prop_button->Enable();
		stn_menu->Enable(tm_s_Properties, true);
		wxString nl = wxT("\n");
		loc_edit_label->SetLabel(nl + _("Edit Station Location ") + call + wxT(": ") + lname);
		loc_edit_label->Wrap(w - 10);
		loc_edit_button->SetLabel(nl + _("Edit Station Location ") + call + wxT(": ") + lname);
		loc_delete_label->SetLabel(nl + _("Delete Station Location ") + call + wxT(": ") + lname);
		loc_delete_label->Wrap(w - 10);
		loc_delete_button->SetLabel(nl +  _("Delete Station Location ") + call + wxT(": ") + lname);
		loc_prop_label->SetLabel(nl + _("Display Station Location Properties for ") + call + wxT(": ") + lname);
		loc_prop_label->Wrap(w - 10);
		loc_prop_button->SetLabel(nl + _("Display Station Location Properties for ") + call + wxT(": ") + lname);
		loc_select_label->SetLabel(wxT(""));
	} else {
		LocTreeReset();
	}
}

void MyFrame::OnLocProperties(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocProperties", NULL);
	LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(loc_tree->GetSelection()));
	if (data != NULL)
		displayLocProperties(data, this);
}

void MyFrame::OnLocDelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocDelete", NULL);
	LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(loc_tree->GetSelection()));
	if (data == NULL)	// "Never happens"
		return;

	wxString warn = _("This will remove this station location from your system.");
	warn += wxT("\n");
	warn += _("Are you sure you want to delete this station location?");
	if (wxMessageBox(warn, _("Warning"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		if (tqsl_deleteStationLocation(data->getLocname().ToUTF8()))
			wxLogError(getLocalizedErrorString());
		loc_tree->Build();
		LocTreeReset();
		AutoBackup();
	}
}

void MyFrame::OnLocUndelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocUndelete", NULL);

	int nloc;
	char **locp = NULL;

	try {
		check_tqsl_error(tqsl_getDeletedStationLocations(&locp, &nloc));

		if (nloc <= 0) {
			wxMessageBox(_("There are no deleted Station Locations to restore"), _("Undelete Error"), wxOK | wxICON_EXCLAMATION, this);
			return;
		}

		wxArrayString choices;
		choices.clear();
		for (int i = 0; i < nloc; i++) {
			choices.Add(wxString::FromUTF8(locp[i]));
		}
		choices.Sort();

		wxString selected = wxGetSingleChoice(_("Choose a Station Location to restore"),
					 	_("Station Locations"),
						choices);
		if (selected.IsEmpty()) {
			tqsl_freeDeletedLocationList(locp, nloc);
			return;			// Cancelled
		}

		check_tqsl_error(tqsl_restoreStationLocation(selected.ToUTF8()));
		tqsl_freeDeletedLocationList(locp, nloc);
	} catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
	loc_tree->Build();
	LocTreeReset();
	AutoBackup();
}

void MyFrame::OnLocEdit(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocEdit", NULL);
	LocTreeItemData *data = reinterpret_cast<LocTreeItemData *>(loc_tree->GetItemData(loc_tree->GetSelection()));
	if (data == NULL)	// "Never happens"
		return;

	tQSL_Location loc;
	wxString selname;
	char errbuf[512];

	try {
		check_tqsl_error(tqsl_getStationLocation(&loc, data->getLocname().ToUTF8()));
		if (verify_cert(loc, true)) {	// Check if there is a certificate before editing
			check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
			if (strlen(errbuf) > 0) {
				wxString fmt = wxT("%hs\n");
				fmt += _("The invalid data was ignored.");
				wxMessageBox(wxString::Format(fmt, errbuf), _("Station Location data error"), wxOK | wxICON_EXCLAMATION, this);
			}
			char loccall[512];
			check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
			selname = run_station_wizard(this, loc, help, true, true, wxString::Format(_("Edit Station Location : %hs - %s"), loccall, data->getLocname().c_str()));
			check_tqsl_error(tqsl_endStationLocationCapture(&loc));
		}
	}
	catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
	loc_tree->Build();
	LocTreeReset();
	AutoBackup();
}

void MyFrame::OnLoginToLogbook(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLoginToLogbook", NULL);
	wxString url = wxConfig::Get()->Read(wxT("LogbookURL"), DEFAULT_LOTW_LOGIN_URL);
	if (!url.IsEmpty())
		wxLaunchDefaultBrowser(url);
	return;
}

void MyFrame::OnChooseLanguage(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnChooseLanguage", "Language choice dialog");

	wxLanguage lang = wxGetApp().GetLang();

	int sel = 0;
	for (size_t i = 0; i < langIds.size(); i++) {
		if (langWX2toWX3(langIds[i]) == lang) {
			sel = i;
			break;
		}
	}
#if wxMAJOR_VERSION > 2
	long lng = wxGetSingleChoiceIndex(_("Please choose language:"),
					 _("Language"), langNames, sel, this);
#else
	wxSingleChoiceDialog dialog(this, _("Please choose language:"), _("Language"), langNames);

	dialog.SetSelection(sel);
	long lng = dialog.ShowModal() == wxID_OK ? dialog.GetSelection() : -1;
#endif
	tqslTrace("MyFrame::OnChooseLanguage", "Language chosen: %d", lng);
	if (lng == -1 || langWX2toWX3(langIds[lng]) == lang)		// Cancel or No change
		return;

	wxConfig::Get()->Write(wxT("Language"), static_cast<int>(langIds[lng]));
	wxConfig::Get()->Flush();

	wxLanguage chosen = langIds[lng];
	chosen = langWX2toWX3(chosen);
	if (wxLocale::IsAvailable(chosen)) {
		locale = new wxLocale(chosen);
		if (!locale) locale = new wxLocale(wxLANGUAGE_DEFAULT);
	} else {
#if defined(TQSL_TESTING)
		wxLogError(wxT("This language is not supported by the system."));
#endif
		locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
	// Add a subdirectory for language files
	locale->AddCatalogLookupPathPrefix(wxT("lang"));
#ifdef _WIN32
	locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(tQSL_BaseDir) + wxT("\\lang"));
#else
	locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(tQSL_BaseDir) + wxT("/lang"));
#endif

	// Initialize the catalogs we'll be using
	locale->AddCatalog(wxT("tqslapp"));
	locale->AddCatalog(wxT("wxstd"));

	// this catalog is installed in standard location on Linux systems and
	// shows that you may make use of the standard message catalogs as well
	//
	// If it's not installed on your system, it is just silently ignored
#ifdef __LINUX__
        {
		wxLogNull nolog;
		locale->AddCatalog(wxT("fileutils"));
	}
#endif
	SaveWindowLayout();
	tqslTrace("MyFrame::OnChooseLanguage", "Destroying GUI");
	Destroy();
	tqslTrace("MyFrame::OnChooseLanguage", "Recreating GUI");
	(reinterpret_cast<QSLApp*>(wxTheApp))->OnInit();
}

class CertPropDial : public wxDialog {
 public:
	explicit CertPropDial(tQSL_Cert cert, wxWindow *parent = 0);
	void closeMe(wxCommandEvent&) { EndModal(wxID_OK); }
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(CertPropDial, wxDialog)
	EVT_BUTTON(wxID_OK, CertPropDial::closeMe)
END_EVENT_TABLE()

CertPropDial::CertPropDial(tQSL_Cert cert, wxWindow *parent)
		: wxDialog(parent, -1, _("Certificate Properties"), wxDefaultPosition, wxSize(400, 15 * LABEL_HEIGHT)) {
	tqslTrace("CertPropDial::CertPropDial", "cert=%lx", static_cast<void *>(cert));
	const char *labels[] = {
		__("Begins: "),
		__("Expires: "),
		__("Organization: "),
		"",
		__("Serial: "),
		__("Operator: "),
		__("Call sign: "),
		__("DXCC Entity: "),
		__("QSO Start Date: "),
		__("QSO End Date: "),
		__("Passphrase: ")
	};

	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	int label_width = 0;
	int label_height = 0;

	wxStaticText* mst = new wxStaticText(this, -1, wxT("I"));
	int char_width = mst->GetSize().GetWidth();

	// Measure the widest label - starting with the one that isn't there by default
	wxString lab = wxGetTranslation(_("Certificate Request"));
	lab = lab + wxT(":");
	mst->SetLabel(lab);
	int em_w = mst->GetSize().GetWidth();

	// Only start with that label if it's going to appear
	int keyonly;
	tqsl_getCertificateKeyOnly(cert, &keyonly);
	if (!keyonly) em_w = 0;

	// Find the widest label
	for (int i = 0; i < static_cast<int>(sizeof labels / sizeof labels[0]); i++) {
		wxString lab = wxGetTranslation(wxString::FromUTF8(labels[i]));
		mst->SetLabel(lab);
		em_w = mst->GetSize().GetWidth();
		if (em_w > label_width) label_width = em_w;
	}

	char callsign[40];

	wxString blob = wxT("");
	wxString prefix = wxT("");
	for (int i = 0; i < static_cast<int>(sizeof labels / sizeof labels[0]); i++) {
		wxString lbl = wxGetTranslation(wxString::FromUTF8(labels[i]));
		while (1) {
			mst->SetLabel(lbl);
			int cur_size = mst->GetSize().GetWidth();
			int delta = label_width - cur_size;
			if (delta < char_width) break;
			lbl += wxT(" ");
		}
		// Store the "empty" label for later
		if (strlen(labels[i]) == 0)
			prefix = lbl;
		char buf[128];
		tQSL_Date certExpDate;
		tQSL_Date date;
		DXCC DXCC;
		int dxcc;
		long serial;
		buf[0] = '\0';
		switch (i) {
			case 0:
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else if (!tqsl_getCertificateNotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 1:
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else if (!tqsl_getCertificateNotAfterDate(cert, &certExpDate))
					tqsl_convertDateToText(&certExpDate, buf, sizeof buf);
				break;
			case 2:
				tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf);
				break;
			case 3:
				tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf);
				break;
			case 4:
				if (keyonly) {
					strncpy(buf, "N/A", sizeof buf);
				} else {
					tqsl_getCertificateSerial(cert, &serial);
					snprintf(buf, sizeof buf, "%ld", serial);
				}
				break;
			case 5:
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else
					tqsl_getCertificateAROName(cert, buf, sizeof buf);
				break;
			case 6:
				tqsl_getCertificateCallSign(cert, buf, sizeof buf);
				strncpy(callsign, buf, sizeof callsign);
				break;
			case 7:
				tqsl_getCertificateDXCCEntity(cert, &dxcc);
				DXCC.getByEntity(dxcc);
				strncpy(buf, DXCC.name(), sizeof buf);
				break;
			case 8:
				if (!tqsl_getCertificateQSONotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 9:
				if (!tqsl_getCertificateQSONotAfterDate(cert, &date)) {
				// If no end-date given, the QSO End Date is one less than
				// the expiration date
					int delta;
					if (keyonly) {
						strncpy(buf, "N/A", sizeof buf);
					} else if (!tqsl_subtractDates(&date, &certExpDate, &delta)) {
						if (delta == 1) {
							strncpy(buf, "N/A", sizeof buf);
						} else {
							tqsl_convertDateToText(&date, buf, sizeof buf);
						}
					} else {
						tqsl_convertDateToText(&date, buf, sizeof buf);
					}
				}
				break;
			case 10:
				switch (tqsl_getCertificatePrivateKeyType(cert)) {
					case TQSL_PK_TYPE_ERR:
						if (tQSL_Error == TQSL_CERT_NOT_FOUND) {
							strncpy(buf, __("Missing from this computer"), sizeof buf);
							break;
						}
						if (tQSL_Error == TQSL_CUSTOM_ERROR && tQSL_Errno == ENOENT) {
							strncpy(buf, __("Private Key not found"), sizeof buf);
							break;
						}
						if (tQSL_Error == TQSL_CUSTOM_ERROR && tQSL_Errno == EPERM) {
							strncpy(buf, __("Unable to read - no permission"), sizeof buf);
							break;
						}
						if (tQSL_Error == TQSL_CUSTOM_ERROR) {
							snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
								"Can't open the private key file for %s: %s", callsign, strerror(tQSL_Errno));
						}
						wxMessageBox(getLocalizedErrorString(), _("Error"), wxOK | wxICON_WARNING, this);
						strncpy(buf, __("<ERROR>"), sizeof buf);
						break;
					case TQSL_PK_TYPE_NONE:
						strncpy(buf, __("No Private Key"), sizeof buf);
						break;
					case TQSL_PK_TYPE_UNENC:
						strncpy(buf, __("None"), sizeof buf);
						break;
					case TQSL_PK_TYPE_ENC:
						strncpy(buf, __("Passphrase protected"), sizeof buf);
						break;
				}
				break;
		}
		if (keyonly && i == 0) {
			blob += _("Certificate Request");
			blob += wxT(":\t");
			blob += _("Awaiting response from ARRL");
			blob += wxT("\n");
		}
		if (!keyonly || i > 1) {
			blob += lbl;
			blob += wxT("\t");
			blob += wxGetTranslation(wxString::FromUTF8(buf));
			blob += wxT("\n");
		}
	}

	int sup, exp;
	if (tqsl_isCertificateSuperceded(cert, &sup) == 0 && sup) {
		blob += prefix + wxT("\t");
		blob += _("Replaced");
		blob += wxT("\n");
	}
	if (tqsl_isCertificateExpired(cert, &exp) == 0 && exp) {
		blob += prefix + wxT("\t");
		blob += _("Expired");
		blob += wxT("\n");
	}

	mst->SetLabel(blob);
	label_width = mst->GetSize().GetWidth();
	label_height = mst->GetSize().GetHeight();
	delete mst;

	topsizer->Add(new wxStaticText(this, -1, blob, wxDefaultPosition, wxSize(label_width + 20, label_height + 5)));
	topsizer->Add(
		new wxButton(this, wxID_OK, _("Close")),
		0, wxALIGN_CENTER | wxALL, 10
	);
	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CenterOnParent();
}

void
displayCertProperties(CertTreeItemData *item, wxWindow *parent) {
	tqslTrace("displayCertProperties", "item=%lx", static_cast<void *>(item));
	if (item != NULL) {
		CertPropDial dial(item->getCert(), parent);
		dial.ShowModal();
	}
}

BEGIN_EVENT_TABLE(LocPropDial, wxDialog)
	EVT_BUTTON(wxID_OK, LocPropDial::closeMe)
END_EVENT_TABLE()

LocPropDial::LocPropDial(wxString locname, bool display, const char *filename, wxWindow *parent)
		: wxDialog(parent, -1, _("Station Location Properties"), wxDefaultPosition, wxSize(1000, 15 * LABEL_HEIGHT)) {
	tqslTrace("LocPropDial", "locname=%s, filename=%s", S(locname), filename);

	const char *fields[] = { "CALL", __("Call sign: "),
				 "DXCC", __("DXCC Entity: "),
				 "GRIDSQUARE", __("Grid Square: "),
				 "ITUZ", __("ITU Zone: "),
				 "CQZ", __("CQ Zone: "),
				 "IOTA", __("IOTA Locator: "),
				 "US_STATE", __("State: "),
				 "US_COUNTY", __("County: "),
				 "US_PARK", __("Park: "),
				 "CA_PROVINCE", __("Province: "),
				 "CA_US_PARK", __("Park: "),
				 "RU_OBLAST", __("Oblast: "),
				 "CN_PROVINCE", __("Province: "),
				 "AU_STATE", __("State: "),
				 "DX_US_PARK", __("Park: ") };

	if (!display) {
		SetTitle(_("Verify QTH details: ") + locname);
	}

	tQSL_Location loc;
	try {
		check_tqsl_error(tqsl_getStationLocation(&loc, locname.ToUTF8()));
	}
	catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
		return;
	}

	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	int label_width = 0;

	wxStaticText* mst = new wxStaticText(this, -1, wxT("I"));
	// Measure the widest label
	for (int i = 0; i < static_cast<int>(sizeof fields / sizeof fields[0]); i += 2) {
		int em_w;
		mst->SetLabel(wxString::FromUTF8(fields[i+1]));
		em_w = mst->GetSize().GetWidth();
		if (em_w > label_width) label_width = em_w;
	}


	wxFont callSignFont(32, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	wxFont dxccFont(24, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	char fieldbuf[512];
	for (int i = 0; i < static_cast<int>(sizeof fields / sizeof fields[0]); i += 2) {
		if (tqsl_getStationLocationField(loc, fields[i], fieldbuf, sizeof fieldbuf) == 0) {
			if (strlen(fieldbuf) > 0 && strcmp(fieldbuf, "[None]") != 0) {
				if (!strcmp(fields[i], "CALL")) {
				        wxStaticText* callLabel = new wxStaticText(this, -1, wxString::FromUTF8(fieldbuf), wxDefaultPosition, wxSize(label_width*8, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE);
        				callLabel->SetFont(callSignFont);
					topsizer->Add(callLabel);
				}  else if (!strcmp(fields[i], "DXCC")) {
					int dxcc = strtol(fieldbuf, NULL, 10);
					const char *dxccname = NULL;
					if (tqsl_getDXCCEntityName(dxcc, &dxccname))
						strncpy(fieldbuf, "Unknown", sizeof fieldbuf);
					else
						strncpy(fieldbuf, dxccname, sizeof fieldbuf);
				        wxStaticText* dxccLabel = new wxStaticText(this, -1, wxString::FromUTF8(fieldbuf), wxDefaultPosition, wxSize(label_width*8, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE);
        				dxccLabel->SetFont(dxccFont);
					topsizer->Add(dxccLabel);
				} else {
					wxString lbl = wxGetTranslation(wxString::FromUTF8(fields[i+1]));
					wxString det = lbl + wxT(" ") + wxString::FromUTF8(fieldbuf);
				        wxStaticText* detailsLabel = new wxStaticText(this, -1, det, wxDefaultPosition, wxSize(label_width*8, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE);
					topsizer->Add(detailsLabel);
				}
			}
		}
	}
	delete mst;
	if (display) {
		topsizer->Add(new wxButton(this, wxID_OK, _("Close")),
				0, wxALIGN_CENTER | wxALL, 10);
	} else {
		wxString det = _("Signing File: ") + wxString::FromUTF8(filename);
		wxStaticText* detailsLabel = new wxStaticText(this, -1, det, wxDefaultPosition, wxSize(label_width*8, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE);
		topsizer->Add(detailsLabel);
		int fontSize = wxNORMAL_FONT->GetPointSize();
		wxFont fnt(fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

		wxStaticText* m1 = new wxStaticText(this, -1, _("Please verify that this is the correct Station Location for the QSOs being signed."));
		m1->SetFont(fnt);
		topsizer->Add(m1, 0, wxALL | wxALIGN_CENTER, 10);
		wxStaticText* m2 = new wxStaticText(this, -1, _("Click 'Cancel' if this is not the correct Station Location."));
		m2->SetFont(fnt);
		topsizer->Add(m2, 0, wxALL | wxALIGN_CENTER, 10);
		wxBoxSizer *butSizer = new wxBoxSizer(wxHORIZONTAL);
		butSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxALL | wxALIGN_CENTER, 10);
		topsizer->Add(butSizer, 0, wxALIGN_CENTER |wxALL, 10);
	}

	topsizer->AddSpacer(50);
	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CenterOnParent();
}

void
displayLocProperties(LocTreeItemData *item, wxWindow *parent) {
	tqslTrace("displayLocProperties", "item=%lx", item);
	if (item != NULL) {
		LocPropDial dial(item->getLocname(), true, NULL, parent);
		dial.ShowModal();
	}
}

int
getPassword(char *buf, int bufsiz, void *callsign) {
	tqslTrace("getPassword", "buf=%lx, bufsiz=%d, callsign=%s", buf, bufsiz, callsign ? callsign : "NULL");
	wxString prompt(_("Enter the Passphrase to unlock the callsign certificate"));

	if (callsign)
		prompt = wxString::Format(_T("Enter the Passphrase for your active %hs Callsign Certificate"),  reinterpret_cast<char *>(callsign));

	tqslTrace("getPassword", "Probing for top window");
	wxWindow* top = wxGetApp().GetTopWindow();
	tqslTrace("getPassword", "Top window = 0x%lx", reinterpret_cast<void *>(top));
	top->SetFocus();
	tqslTrace("getPassword", "Focus grabbed. About to pop up password dialog");
	GetPasswordDialog dial(top, _("Enter passphrase"), prompt);
	if (dial.ShowModal() != wxID_OK) {
		tqslTrace("getPassword", "Password entry cancelled");
		return 1;
	}
	tqslTrace("getPassword", "Password entered OK");
	strncpy(buf, dial.Password().ToUTF8(), bufsiz);
	utf8_to_ucs2(buf, unipwd, sizeof unipwd);
	buf[bufsiz-1] = 0;
	return 0;
}

void
displayTQSLError(const char *pre) {
	tqslTrace("displayTQSLError", "pre=%s", pre);
	wxString s = wxGetTranslation(wxString::FromUTF8(pre));
	s += wxT(":\n");
	s += getLocalizedErrorString();
	wxMessageBox(s, _("Error"), wxOK | wxICON_WARNING, frame);
}


static wxString
flattenCallSign(const wxString& call) {
	tqslTrace("flattenCallSign", "call=%s", S(call));
	wxString flat = call;
	size_t idx;
	while ((idx = flat.find_first_not_of(wxT("ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_"))) != wxString::npos)
		flat[idx] = '_';
	return flat;
}

static int lockfileFD = -1;

#ifndef _WIN32
static int
lock_db(bool wait) {
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	tqslTrace("lock_db", "wait = %d", wait);

	wxString lfname = wxString::FromUTF8(tQSL_BaseDir) + wxT("/dblock");

	if (lockfileFD < 0) {
		lockfileFD = open(lfname.ToUTF8(), O_RDWR| O_CREAT, 0644);
		if (lockfileFD < 0) {
			tqslTrace("lock_db", "can't set lock: %m");
			return 1;
		}
	}
	if (wait) {
		tqslTrace("lock_db", "waiting for lock");
		fcntl(lockfileFD, F_SETLKW, &fl);
		tqslTrace("lock_db", "got lock");
		return 0;
	}
	int ret = fcntl(lockfileFD, F_SETLK, &fl);
	tqslTrace("lock_db", "trying to set a lock");
	if (ret < 0 && (errno == EACCES || errno == EAGAIN)) {
		tqslTrace("lock_db", "Lock not taken : %m");
		return -1;
	}
	return 0;
}

static void
unlock_db(void) {
	tqslTrace("unlock_db", "entered");
	if (lockfileFD < 0) {
		tqslTrace("unlock_db", "lock wasn't taken");
		return;
	}
	close(lockfileFD);
	lockfileFD = -1;
	tqslTrace("unlock_db", "unlocked");
	return;
}
#else /* _WIN32 */

static OVERLAPPED ov;
static HANDLE hFile = 0;

static int
lock_db(bool wait) {
	BOOL ret = FALSE;
	DWORD locktype = LOCKFILE_EXCLUSIVE_LOCK;

	wxString lfname = wxString::FromUTF8(tQSL_BaseDir) + wxT("\\dblock");

	tqslTrace("lock_db", "wait = %d", wait);
	if (lockfileFD < 0) {
		wchar_t* wlfname = utf8_to_wchar(lfname.ToUTF8());
		lockfileFD = _wopen(wlfname, O_RDWR| O_CREAT, 0644);
		free_wchar(wlfname);
		if (lockfileFD < 0) {
			tqslTrace("lock_db", "can't open file: %m");
			return 1;
		}
		ZeroMemory(&ov, sizeof(ov));
		ov.hEvent = NULL;
		ov.Offset = 0;
		ov.OffsetHigh = 0x80000000;
	}

	hFile = (HANDLE) _get_osfhandle(lockfileFD);

	if (!wait) {
		locktype |= LOCKFILE_FAIL_IMMEDIATELY;
	}
	tqslTrace("lock_db", "trying to lock");
	ret = LockFileEx(hFile, locktype, 0, 0, 0x80000000, &ov);
	if (!ret) {
		switch (GetLastError()) {
			case ERROR_SHARING_VIOLATION:
			case ERROR_LOCK_VIOLATION:
			case ERROR_IO_PENDING:
				tqslTrace("lock_db", "unable to lock");
				return -1;
			default:
				tqslTrace("lock_db", "locked");
				return 0;
		}
	}
	return 0;
}

static void
unlock_db(void) {
	tqslTrace("unlock_db", "hFile=%lx", hFile);
	if (hFile) {
		UnlockFileEx(hFile, 0, 0, 0x80000000, &ov);
		tqslTrace("unlock_db", "unlocked");
	}
	if (lockfileFD != -1) {
		tqslTrace("unlock_db", "closing lock");
		_close(lockfileFD);
	}
	lockfileFD = -1;
	hFile = 0;
}

///////////////////////////////
/* Derived from VistaTools.cxx - version 2.1 http://www.softblog.com/files/VistaTools.cxx

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (C) 2008.  WinAbility Software Corporation. All rights reserved.

Author: Andrei Belogortseff [ http://www.softblog.com ]

TERMS OF USE: You are free to use this file in any way you like, 
for both the commercial and non-commercial purposes, royalty-free,
AS LONG AS you agree with the warranty disclaimer above, 
EXCEPT that you may not remove or modify this or any of the 
preceeding paragraphs. If you make any changes, please document 
them in the MODIFICATIONS section below. If the changes are of general 
interest, please let us know and we will consider incorporating them in 
this file, as well.

If you use this file in your own project, an acknowledgment will be appreciated, 
although it's not required.

SUMMARY:

This file contains several Vista-specific functions helpful when dealing with the 
"elevation" features of Windows Vista. See the descriptions of the functions below
for information on what each function does and how to use it.

This file contains the Win32 stuff only, it can be used with or without other frameworks, 
such as MFC, ATL, etc.

*/

#if ( NTDDI_VERSION < NTDDI_LONGHORN )
#	error NTDDI_VERSION must be defined as NTDDI_LONGHORN or later
#endif

static BOOL
IsVista();

/*
Use IsVista() to determine whether the current process is running under Windows Vista or 
(or a later version of Windows, whatever it will be)

Return Values:
	If the function succeeds, and the current version of Windows is Vista or later, 
		the return value is TRUE. 
	If the function fails, or if the current version of Windows is older than Vista 
		(that is, if it is Windows XP, Windows 2000, Windows Server 2003, Windows 98, etc.)
		the return value is FALSE.
*/

static HRESULT
GetElevationType(__out TOKEN_ELEVATION_TYPE * ptet);

/*
Use GetElevationType() to determine the elevation type of the current process.

Parameters:

ptet
	[out] Pointer to a variable that receives the elevation type of the current process.

	The possible values are:

	TokenElevationTypeDefault - User is not using a "split" token. 
		This value indicates that either UAC is disabled, or the process is started
		by a standard user (not a member of the Administrators group).

	The following two values can be returned only if both the UAC is enabled and
	the user is a member of the Administrator's group (that is, the user has a "split" token):

	TokenElevationTypeFull - the process is running elevated. 

	TokenElevationTypeLimited - the process is not running elevated.

Return Values:
	If the function succeeds, the return value is S_OK. 
	If the function fails, the return value is E_FAIL. To get extended error information, 
	call GetLastError().
*/

HRESULT
IsElevated(BOOL * pbElevated);

/*
Use IsElevated() to determine whether the current process is elevated or not.

Parameters:

pbElevated
	[out] [optional] Pointer to a BOOL variable that, if non-NULL, receives the result.

	The possible values are:

	TRUE - the current process is elevated.
		This value indicates that either UAC is enabled, and the process was elevated by 
		the administrator, or that UAC is disabled and the process was started by a user 
		who is a member of the Administrators group.

	FALSE - the current process is not elevated (limited).
		This value indicates that either UAC is enabled, and the process was started normally, 
		without the elevation, or that UAC is disabled and the process was started by a standard user. 

Return Values
	If the function succeeds, and the current process is elevated, the return value is S_OK. 
	If the function succeeds, and the current process is not elevated, the return value is S_FALSE. 
	If the function fails, the return value is E_FAIL. To get extended error information, 
	call GetLastError().
*/

static
BOOL IsVista() {
	OSVERSIONINFO osver;

	osver.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	if (	::GetVersionEx( &osver ) &&
			osver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
			(osver.dwMajorVersion >= 6) )
		return TRUE;

	return FALSE;
}

static
HRESULT
GetElevationType(__out TOKEN_ELEVATION_TYPE * ptet) {
	if (!IsVista() || ptet == NULL )
		return E_FAIL;

	HRESULT hResult = E_FAIL; // assume an error occurred
	HANDLE hToken	= NULL;

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		return hResult;
	}

	DWORD dwReturnLength = 0;

	if (::GetTokenInformation(hToken, TokenElevationType, ptet, sizeof(*ptet), &dwReturnLength)) {
		hResult = S_OK;
	}

	::CloseHandle(hToken);

	return hResult;
}

HRESULT
IsElevated(BOOL * pbElevated) {
	if (!IsVista())
	    return E_FAIL;

	HRESULT hResult = E_FAIL; // assume an error occurred
	HANDLE hToken	= NULL;

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		return hResult;
	}

	TOKEN_ELEVATION te = { 0 };
	DWORD dwReturnLength = 0;

	if (::GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &dwReturnLength)) {
		hResult = te.TokenIsElevated ? S_OK : S_FALSE;

		if ( pbElevated)
			*pbElevated = (te.TokenIsElevated != 0);
	}

	::CloseHandle(hToken);

	return hResult;
}

#endif /* _WIN32 */
