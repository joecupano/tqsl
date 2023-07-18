/***************************************************************************
                          loadcertwiz.cpp  -  description
                             -------------------
    begin                : Wed Aug 6 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include <curl/curl.h>
#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "loadcertwiz.h"
#include "getpassword.h"
#include "tqslctrls.h"
#include "tqsllib.h"
#include "tqslerrno.h"
#include "tqslapp.h"
#include "tqsltrace.h"
#include "tqsl_prefs.h"

wxString
notifyData::Message() const {
	tqslTrace(NULL, "%s\n"
			"Root Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"CA Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Callsign Certificates:\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Private Keys:\t\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Configuration Data:\tLoaded: %d  Duplicate: %d  Error: %d\n",
			S(status),
			root.loaded, root.duplicate, root.error,
			ca.loaded, ca.duplicate, ca.error,
			user.loaded, user.duplicate, user.error,
			pkey.loaded, pkey.duplicate, pkey.error,
			config.loaded, config.duplicate, config.error);
	if (status.IsEmpty()) {
		wxString msg = wxT("\n");
		msg += _("Import completed successfully");
		return msg;
	}
	return status;
}

int
notifyImport(int type, const char *message, void *data) {
	tqslTrace("notifyImport", "type=%d, message=%s, data=0x%lx", type, message, reinterpret_cast<void *>(data));
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_PROMPT) {
		const char *nametype = 0;
		const char *configkey = 0;
		bool default_prompt = false;
		switch (TQSL_CERT_CB_CERT_TYPE(type)) {
			case TQSL_CERT_CB_ROOT:
				nametype = "Trusted root";
				configkey = "NotifyNewRoot";
				break;
			case TQSL_CERT_CB_CA:
				nametype = "Certificate Authority";
				configkey = "NotifyCA";
				break;
			case TQSL_CERT_CB_USER:
				nametype = "Callsign";
				configkey = "NotifyUser";
				break;
		}
		if (!nametype)
			return 0;
		wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
		bool b;
		config->Read(wxString::FromUTF8(configkey), &b, default_prompt);
		if (!b)
			return 0;
		wxString s(_("OK to install "));
		s = s + wxString::FromUTF8(nametype) + wxT(" ") + _("certificate?") + wxT("\n\n") + wxString::FromUTF8(message);
		if (wxMessageBox(s, _("Install Certificate"), wxYES_NO | wxICON_QUESTION) == wxYES)
			return 0;
		return 1;
	} // end TQSL_CERT_CB_PROMPT
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT && data) {
		// Keep count
		notifyData *nd = reinterpret_cast<notifyData *>(data);
		notifyData::counts *counts = 0;
		switch (TQSL_CERT_CB_CERT_TYPE(type)) {
			case TQSL_CERT_CB_ROOT:
				counts = &(nd->root);
				break;
			case TQSL_CERT_CB_CA:
				counts = &(nd->ca);
				break;
			case TQSL_CERT_CB_USER:
				counts = &(nd->user);
				break;
			case TQSL_CERT_CB_PKEY:
				counts = &(nd->pkey);
				break;
			case TQSL_CERT_CB_CONFIG:
				counts = &(nd->config);
				break;
		}
		if (counts) {
			switch (TQSL_CERT_CB_RESULT_TYPE(type)) {
				case TQSL_CERT_CB_DUPLICATE:
					if (TQSL_CERT_CB_CERT_TYPE(type) == TQSL_CERT_CB_USER) {
						if (message) {
							nd->status = nd->status + wxString::FromUTF8(message) + wxT("\n");
						} else {
							nd->status = nd->status + _("This callsign certificate is already installed");
						}
					}
					counts->duplicate++;
					break;
				case TQSL_CERT_CB_ERROR:
					if (message) {
						switch (TQSL_CERT_CB_CERT_TYPE(type)) {
							case TQSL_CERT_CB_ROOT:
								nd->status = nd->status + _("Trusted root certificate");
								break;
							case TQSL_CERT_CB_CA:
								nd->status = nd->status + _("Certificate Authority certificate");
								break;
							case TQSL_CERT_CB_USER:
								nd->status = nd->status + _("Callsign Certificate");
								break;
						}
						nd->status = nd->status + wxT(": ") + wxString::FromUTF8(message) + wxT("\n");
						counts->error++;
						if (TQSL_CERT_CB_CERT_TYPE(type) == TQSL_CERT_CB_USER) {
							wxMessageBox(wxString::FromUTF8(message), _("Error"));
						}
					}
					break;
				case TQSL_CERT_CB_LOADED:
					if (TQSL_CERT_CB_CERT_TYPE(type) == TQSL_CERT_CB_USER)
						nd->status = nd->status + wxString::FromUTF8("Callsign Certificate ") +
							wxString::FromUTF8(message) + wxT("\n");
					counts->loaded++;
					break;
			}
		}
	}
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_ERROR)
		return 1;	// Errors get posted later
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT
		|| TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_DUPLICATE) {
//		wxMessageBox(message, "Certificate Notice");
		return 0;
	}
	return 1;
}

static wxHtmlHelpController *pw_help = 0;
static wxString pw_helpfile;

static int
GetNewPassword(char *buf, int bufsiz, void *) {
	tqslTrace("GetNewPassword", NULL);
	wxString msg = _("Enter a passphrase for this callsign certificate.");
		msg += wxT("\n\n");
		msg += _("If you are using a computer system that is shared with others, you should specify a passphrase to protect this certificate. However, if you are using a computer in a private residence no passphrase need be specified.");
		msg += wxT("\n\n");
		msg += _("This passphrase will have to be entered each time you use this callsign certificate for signing or when saving the key.");
		msg += wxT("\n\n");
		msg += _("Leave the passphrase blank and click 'OK' unless you want to use a passphrase.");
		msg += wxT("\n\n");
	GetNewPasswordDialog dial(0, _("New Passphrase"),
		msg, true, pw_help, pw_helpfile);
	if (dial.ShowModal() == wxID_OK) {
		strncpy(buf, dial.Password().ToUTF8(), bufsiz);
		buf[bufsiz-1] = 0;
		return 0;
	}
	return 1;
}

static void
export_new_cert(ExtWizard *_parent, const char *filename, wxString path, wxString basename) {
	tqslTrace("export_new_cert", "_parent=0x%lx, filename=%s", _parent, filename);
	long newserial;
	if (!tqsl_getSerialFromTQSLFile(filename, &newserial)) {
		MyFrame *frame = reinterpret_cast<MyFrame *>(((reinterpret_cast<LoadCertWiz *>(_parent))->Parent()));
		TQ_WXCOOKIE cookie;
		int nproviders = frame->cert_tree->GetNumIssuers();		// Number of certificate issuers - currently 1
		wxTreeItemId root = frame->cert_tree->GetRootItem();
		wxTreeItemId item, prov;
		if (nproviders > 1) {
			prov = frame->cert_tree->GetFirstChild(root, cookie); // First child is the providers
			item = frame->cert_tree->GetFirstChild(prov, cookie);// Then it's certs
		} else {
			item = frame->cert_tree->GetFirstChild(root, cookie); // First child is the certs
		}

		while (item.IsOk()) {
			tQSL_Cert cert;
			CertTreeItemData *id = frame->cert_tree->GetItemData(item);
			if (id && (cert = id->getCert())) {
				long serial;
				if (!tqsl_getCertificateSerial(cert, &serial)) {
					if (serial == newserial) {
						wxCommandEvent e;
						id->path = path;
						id->basename = basename;
						frame->OnCertExport(e);
						id->path = id->basename = wxEmptyString;
						wxString msg = _("You will not be able to use this tq6 file to recover your callsign certificate if it gets lost. For security purposes, you should back up your certificate on removable media for safe-keeping.");
							msg += wxT("\n\n");
							msg += _("Would you like to back up your callsign certificate now?");
						if (wxMessageBox(msg, _("Warning"), wxYES_NO | wxICON_QUESTION, _parent) == wxNO) {
							return;
						}
						frame->cert_tree->SelectItem(item);
						frame->OnCertExport(e);
						break;
					}
				}
			}
			if (nproviders > 1) {
				item = frame->cert_tree->GetNextChild(prov, cookie);
			} else {
				item = frame->cert_tree->GetNextChild(root, cookie);
			}
		}
	}
}

LoadCertWiz::LoadCertWiz(wxWindow *parent, wxHtmlHelpController *help, const wxString& title, const wxString& ext)
	: ExtWizard(parent, help, title), _nd(0) {
	tqslTrace("LoadCertWiz::LoadCertWiz", "parent=0x%lx, title=%s, ext=%s", reinterpret_cast<void *>(parent), S(title), S(ext));

	LCW_FinalPage *final = new LCW_FinalPage(this);
	LCW_P12PasswordPage *p12pw = new LCW_P12PasswordPage(this);
	wxWizardPageSimple::Chain(p12pw, final);
	_first = p12pw;
	_parent = parent;
	_final = final;
	_p12pw = p12pw;
	bool setPassword = false;

	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Read(wxT("CertPwd"), &setPassword, DEFAULT_CERTPWD);

#if !defined(__APPLE__) && !defined(_WIN32)
	wxString wild(_("Callsign Certificate container files (*.p12,*.P12;*.tq6;*.TQ6)|*.p12;*.P12;*.tq6;*.TQ6"));
#else
	wxString wild(_("Callsign Certificate container files (*.p12,*.tq6))|*.p12;*.tq6"));
#endif
	wild += _("|All files (*.*)|*.*");

	tQSL_ImportCall[0] = '\0';
	wxString path = config->Read(wxT("CertFilePath"), wxT(""));
	wxString filename = wxFileSelector(_("Select Certificate File"), path,
		wxT(""), ext, wild, wxFD_OPEN|wxFD_FILE_MUST_EXIST);
	if (filename.IsEmpty()) {
		// Cancelled!
		_first = 0;
	} else {
		ResetNotifyData();
		wxString path, basename, ext;
		wxFileName::SplitPath(filename, &path, &basename, &ext);

		config->Write(wxT("CertFilePath"), path);
		if (ext.MakeLower() == wxT("tq6")) {
			_first = _final;
			_final->SetPrev(0);
			if (tqsl_importTQSLFile(filename.ToUTF8(), notifyImport, GetNotifyData())) {
				if (tQSL_Error != TQSL_CERT_ERROR) {  // if not already reported by the callback
					//wxMessageBox(getLocalizedErrorString(), _("Error"), wxOK | wxICON_ERROR, _parent);
					_nd->status = getLocalizedErrorString();
				}
			} else {
				if (tQSL_ImportCall[0] != '\0') {
					wxString call = wxString::FromUTF8(tQSL_ImportCall);
					wxString pending = config->Read(wxT("RequestPending"));
					pending.Replace(call, wxT(""), true);
					wxString rest;
					while (pending.StartsWith(wxT(","), &rest))
						pending = rest;
					while (pending.EndsWith(wxT(","), &rest))
						pending = rest;
					config->Write(wxT("RequestPending"), pending);
				}
				export_new_cert(this, filename.ToUTF8(), path, basename);
			}
		} else {
			// First try with no password
			if (!tqsl_importPKCS12File(filename.ToUTF8(), "", 0, setPassword ? GetNewPassword : NULL, notifyImport, GetNotifyData()) || tQSL_Error == TQSL_CERT_ERROR) {
				_first = _final;
				_final->SetPrev(0);
			} else {
				if (tQSL_Error == TQSL_PASSWORD_ERROR) {
					_first = _p12pw;
					_p12pw->SetPrev(0);
					p12pw->SetFilename(filename);
				} else if (tQSL_Error == TQSL_OPENSSL_ERROR) {
					wxMessageBox(_("This file is not a valid P12 file"), _("Error"), wxOK | wxICON_ERROR, _parent);
					_first = 0;	// Cancel
				} else {
					_first = 0;	// Cancel
				}
			}
		}
	}
	AdjustSize();
	CenterOnParent();
}

LCW_FinalPage::LCW_FinalPage(LoadCertWiz *parent) : LCW_Page(parent) {
	tqslTrace("LCW_FinalPage::LCW_FinalPage", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, _("Loading complete"));
	sizer->Add(st, 0, wxALL, 10);
	wxSize tsize = getTextSize(this);
	tc_status = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, tsize.Scale(40, 16), wxTE_MULTILINE|wxTE_READONLY);
	sizer->Add(tc_status, 1, wxALL|wxEXPAND, 10);
	AdjustPage(sizer);
}

void
LCW_FinalPage::refresh() {
	tqslTrace("LCW_FinalPage::refresh", NULL);
	const notifyData *nd = (reinterpret_cast<LoadCertWiz *>(_parent))->GetNotifyData();
	if (nd)
		tc_status->SetValue(nd->Message());
	else
		tc_status->SetValue(_("No status information available"));
}

LCW_P12PasswordPage::LCW_P12PasswordPage(LoadCertWiz *parent) : LCW_Page(parent) {
	tqslTrace("LCW_P12PasswordPage::LCW_P12PasswordPage", "parent=0x%lx", reinterpret_cast<void *>(parent));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, _("Enter the passphrase to unlock the .p12 file:"));
	sizer->Add(st, 0, wxALL, 10);

	_pwin = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
	sizer->Add(_pwin, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL, 10);

	AdjustPage(sizer, wxT("lcf1.htm"));
}

bool
LCW_P12PasswordPage::TransferDataFromWindow() {
	tqslTrace("LCW_P12PasswordPage::TransferDataFromWindow", NULL);
	wxString _pw = _pwin->GetValue();
	pw_help = Parent()->GetHelp();
	pw_helpfile = wxT("lcf2.htm");
	bool setPassword = false;
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Read(wxT("CertPwd"), &setPassword, DEFAULT_CERTPWD);

	if (tqsl_importPKCS12File(_filename.ToUTF8(), _pw.ToUTF8(), 0, setPassword ? GetNewPassword : NULL, notifyImport,
		(reinterpret_cast<LoadCertWiz *>(_parent))->GetNotifyData())) {
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			// UTF-8 password didn't work - try converting to UCS-2.
			char unipwd[64];
			utf8_to_ucs2(_pw.ToUTF8(), unipwd, sizeof unipwd);
			if (!tqsl_importPKCS12File(_filename.ToUTF8(), unipwd, 0, setPassword ? GetNewPassword : NULL, notifyImport,
					(reinterpret_cast<LoadCertWiz *>(_parent))->GetNotifyData())) {
				tc_status->SetLabel(wxT(""));
				return true;
			}
		}
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			tc_status->SetLabel(_("Passphrase error"));
			return false;
		} else if (tQSL_Error == TQSL_OPENSSL_ERROR) {
			wxMessageBox(_("This file is not a valid P12 file"), _("Error"), wxOK | wxICON_ERROR, _parent);
		} else {
			wxMessageBox(getLocalizedErrorString(), _("Error"), wxOK | wxICON_ERROR, _parent);
		}
	}
	tc_status->SetLabel(wxT(""));
	return true;
}
