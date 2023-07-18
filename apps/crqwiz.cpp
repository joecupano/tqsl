/***************************************************************************
                          crqwiz.cpp  -  description
                             -------------------
    begin                : Sat Jun 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "crqwiz.h"
#include <ctype.h>
#include <stdlib.h>
#include <wx/validate.h>
#include <wx/datetime.h>
#include <wx/config.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <iostream>
#include <string>
#include "dxcc.h"
#include "util.h"
#include "tqslctrls.h"
#include "tqsltrace.h"
#include "tqsl_prefs.h"

#include "winstrdefs.h"

extern int SaveAddressInfo(const char *callsign, int dxcc);

extern int GetULSInfo(const char *callsign, wxString &name, wxString &attn, wxString &street, wxString &city, wxString &state, wxString &zip, wxString &updateDate);

using std::string;

extern int get_address_field(const char *callsign, const char *field, string& result);

CRQWiz::CRQWiz(TQSL_CERT_REQ *crq, tQSL_Cert xcert, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title)
	: ExtWizard(parent, help, title), cert(xcert), _crq(crq)  {
	tqslTrace("CRQWiz::CRQWiz", "crq=%lx, xcert=%lx, title=%s", reinterpret_cast<void *>(cert), reinterpret_cast<void *>(xcert), S(title));

	dxcc = -1;
	validcerts = false;		// No signing certs to use
	onebyone = false;
	renewal = (_crq != NULL);	// It's a renewal if there's a CRQ provided
	usa = validusa = false;		// Not usa
	// Get count of valid certificates
	int ncerts = 0;
	tqsl_selectCertificates(NULL, &ncerts, NULL, 0, NULL, NULL, 0);
	validcerts = (ncerts > 0);
	nprov = 1;
	tqsl_getNumProviders(&nprov);
	providerPage = new CRQ_ProviderPage(this, _crq);
	signPage = new CRQ_SignPage(this, _crq);
	callsignPage = new CRQ_CallsignPage(this, _crq);
	namePage = new CRQ_NamePage(this, _crq);
	emailPage = new CRQ_EmailPage(this, _crq);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Read(wxT("CertPwd"), &CertPwd, DEFAULT_CERTPWD);
	pwPage = new CRQ_PasswordPage(this);
	if (nprov != 1)
		wxWizardPageSimple::Chain(providerPage, callsignPage);
	wxWizardPageSimple::Chain(callsignPage, namePage);
	wxWizardPageSimple::Chain(namePage, emailPage);
	wxWizardPageSimple::Chain(emailPage, pwPage);
	if (!cert)
		wxWizardPageSimple::Chain(pwPage, signPage);
	if (nprov == 1)
		_first = callsignPage;
	else
		_first = providerPage;
	AdjustSize();
	CenterOnParent();
}


// Page constructors

BEGIN_EVENT_TABLE(CRQ_ProviderPage, CRQ_Page)
	EVT_COMBOBOX(ID_CRQ_PROVIDER, CRQ_ProviderPage::UpdateInfo)
END_EVENT_TABLE()

static bool
prov_cmp(const TQSL_PROVIDER& p1, const TQSL_PROVIDER& p2) {
	return strcasecmp(p1.organizationName, p2.organizationName) < 0;
}

CRQ_ProviderPage::CRQ_ProviderPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	tqslTrace("CRQ_ProviderPage::CRQ_ProviderPage", "parent=%lx, crq=%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(crq));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxWindowDC dc(this);
	dc.SetFont(this->GetFont());
	Parent()->maxWidth = 0;

	wxCoord em_w, em_h;
	dc.GetTextExtent(wxString(wxT("M")), &em_w, &em_h);

	if (Parent()->maxWidth < em_w * 40)
		Parent()->maxWidth = em_w * 40;

	wxString lbl = _("This will create a new Callsign Certificate request file.");
		lbl += wxT("\n\n");
		lbl += _("Once you supply the requested information and the request file has been created, you must send the request file to the certificate issuer.");
	wxStaticText *st = new wxStaticText(this, -1, lbl);
	st->SetSize(Parent()->maxWidth + em_w * 2, em_h * 5);
	st->Wrap(Parent()->maxWidth + em_w * 3);

	sizer->Add(st, 0, wxALL, 10);

	sizer->Add(new wxStaticText(this, -1, _("Certificate Issuer:")), 0, wxLEFT|wxRIGHT, 10);
	tc_provider = new wxOwnerDrawnComboBox(this, ID_CRQ_PROVIDER, wxT(""), wxDefaultPosition,
		wxDefaultSize, 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	sizer->Add(tc_provider, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_provider_info = new wxStaticText(this, ID_CRQ_PROVIDER_INFO, wxT(""), wxDefaultPosition,
		wxSize(0, em_h*5));
	sizer->Add(tc_provider_info, 0, wxALL|wxEXPAND, 10);
	int nprov = 0;
	if (tqsl_getNumProviders(&nprov))
		wxMessageBox(getLocalizedErrorString(), _("Error"), wxOK | wxICON_ERROR, this);
	for (int i = 0; i < nprov; i++) {
		TQSL_PROVIDER prov;
		if (!tqsl_getProvider(i, &prov))
			providers.push_back(prov);
	}
	sort(providers.begin(), providers.end(), prov_cmp);
	int selected = -1;
	for (int i = 0; i < static_cast<int>(providers.size()); i++) {
		tc_provider->Append(wxString::FromUTF8(providers[i].organizationName), reinterpret_cast<void *>(i));
		if (crq && !strcmp(providers[i].organizationName, crq->providerName)
			&& !strcmp(providers[i].organizationalUnitName, crq->providerUnit)) {
			selected = i;
		}
	}
	tc_provider->SetSelection((selected < 0) ? 0 : selected);
	if (providers.size() < 2 || selected >= 0)
		tc_provider->Enable(false);
	DoUpdateInfo();
	AdjustPage(sizer, wxT("crq.htm"));
}

void
CRQ_ProviderPage::DoUpdateInfo() {
	tqslTrace("CRQ_ProviderPage::DoUpdateInfo", NULL);
	int sel = tc_provider->GetSelection();
	if (sel >= 0) {
		long idx = (long)(tc_provider->GetClientData(sel));
		if (idx >=0 && idx < static_cast<int>(providers.size())) {
			Parent()->provider = providers[idx];
			wxString info;
			info = wxString::FromUTF8(Parent()->provider.organizationName);
			if (Parent()->provider.organizationalUnitName[0] != 0)
				info += wxString(wxT("\n  ")) + wxString::FromUTF8(Parent()->provider.organizationalUnitName);
			if (Parent()->provider.emailAddress[0] != 0)
				info += wxString(wxT("\n")) += _("Email: ") + wxString::FromUTF8(Parent()->provider.emailAddress);
			if (Parent()->provider.url[0] != 0)
				info += wxString(wxT("\n")) + _("URL: ") + wxString::FromUTF8(Parent()->provider.url);
			tc_provider_info->SetLabel(info);
		}
	}
}

void
CRQ_ProviderPage::UpdateInfo(wxCommandEvent&) {
	tqslTrace("CRQ_ProviderPage::UpdateInfo", NULL);
	DoUpdateInfo();
}


static wxDateTime::Month mons[] = {
	wxDateTime::Inv_Month, wxDateTime::Jan, wxDateTime::Feb, wxDateTime::Mar,
	wxDateTime::Apr, wxDateTime::May, wxDateTime::Jun, wxDateTime::Jul,
	wxDateTime::Aug, wxDateTime::Sep, wxDateTime::Oct, wxDateTime::Nov,
	wxDateTime::Dec };

BEGIN_EVENT_TABLE(CRQ_CallsignPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_CALL, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_DXCC, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBYEAR, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBMONTH, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBDAY, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEYEAR, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEMONTH, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEDAY, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_CallsignPage::CRQ_CallsignPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	tqslTrace("CRQ_CallsignPage::CRQ_CallsignPage", "parent=%lx, crq=%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(crq));
	initialized = false;
	_parent = parent;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *dst = new wxStaticText(this, -1, _("DXCC entity:"));
	wxSize sz = getTextSize(this);
	int em_h = sz.GetHeight();
	em_w = sz.GetWidth();
	wxStaticText *st = new wxStaticText(this, -1, _("Call sign:"), wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	st->SetSize(dst->GetSize());

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	wxString cs;
	if (crq && crq->callSign[0])
		cs = wxString::FromUTF8(crq->callSign);
	tc_call = new wxTextCtrl(this, ID_CRQ_CALL, cs, wxDefaultPosition, wxSize(em_w*15, -1));
	tc_call->SetMaxLength(TQSL_CALLSIGN_MAX);
	hsizer->Add(tc_call, 0, wxEXPAND, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	if (crq && crq->callSign[0])
		tc_call->Enable(false);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(dst, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_dxcc = new wxOwnerDrawnComboBox(this, ID_CRQ_DXCC, wxT(""), wxDefaultPosition,
		wxSize(em_w*25, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	hsizer->Add(tc_dxcc, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);

	DXCC dx;
	bool ok = dx.getFirst();
	while (ok) {
		tc_dxcc->Append(wxString::FromUTF8(dx.name()), reinterpret_cast<void *>(dx.number()));
		ok = dx.getNext();
	}
	const char *ent = "NONE";
	if (crq) {
		if (dx.getByEntity(crq->dxccEntity)) {
			ent = dx.name();
			tc_dxcc->Enable(false);
		}
	}
	int i = tc_dxcc->FindString(wxString::FromUTF8(ent));
	if (i >= 0)
		tc_dxcc->SetSelection(i);
	struct {
		wxOwnerDrawnComboBox **cb;
		int id;
	} boxes[][3] = {
	    { {&tc_qsobeginy, ID_CRQ_QBYEAR}, {&tc_qsobeginm, ID_CRQ_QBMONTH}, {&tc_qsobegind, ID_CRQ_QBDAY} },
	    { {&tc_qsoendy, ID_CRQ_QEYEAR}, {&tc_qsoendm, ID_CRQ_QEMONTH}, {&tc_qsoendd, ID_CRQ_QEDAY} }
	};
	int year = wxDateTime::GetCurrentYear() + 1;

	int sels[2][3];
	int dates[2][3];
	if (crq) {
		dates[0][0] = crq->qsoNotBefore.year;
		dates[0][1] = crq->qsoNotBefore.month;
		dates[0][2] = crq->qsoNotBefore.day;
		dates[1][0] = crq->qsoNotAfter.year;
		dates[1][1] = crq->qsoNotAfter.month;
		dates[1][2] = crq->qsoNotAfter.day;
	}
	wxString label = _("Date of the first QSO you made or will make using this callsign:");
	for (int i = 0; i < 2; i++) {
		sels[i][0] = sels[i][1] = sels[i][2] = 0;
		sizer->Add(new wxStaticText(this, -1, label), 0, wxBOTTOM, 5);
		hsizer = new wxBoxSizer(wxHORIZONTAL);
		hsizer->Add(new wxStaticText(this, -1, wxT("Y")), 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 20);
		*(boxes[i][0].cb) = new wxOwnerDrawnComboBox(this, boxes[i][0].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*8, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][0].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, wxT("M")), 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 10);
		*(boxes[i][1].cb) = new wxOwnerDrawnComboBox(this, boxes[i][1].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*6, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][1].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, wxT("D")), 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 10);
		*(boxes[i][2].cb) = new wxOwnerDrawnComboBox(this, boxes[i][2].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*6, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][2].cb), 0, wxLEFT, 5);
		int iofst = 0;
		if (i > 0) {
			iofst++;
			for (int j = 0; j < 3; j++)
				(*(boxes[i][j].cb))->Append(wxT(""));
		}
		for (int j = 1945; j <= year; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][0] == j)
				sels[i][0] = j - 1945 + iofst;
			(*(boxes[i][0].cb))->Append(s);
		}
		year++;
		for (int j = 1; j <= 12; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][1] == j)
				sels[i][1] = j - 1 + iofst;
			(*(boxes[i][1].cb))->Append(s);
		}
		for (int j = 1; j <= 31; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][2] == j)
				sels[i][2] = j - 1 + iofst;
			(*(boxes[i][2].cb))->Append(s);
		}
		sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
		if (i == 0)
			sizer->Add(0, 40);
		label = _("Date of the last QSO you made or will make using this callsign:\n(Leave this date blank if this is still your valid callsign.)");
	}
	if (crq) {
		tc_qsobeginy->SetSelection(sels[0][0]);
		tc_qsobeginm->SetSelection(sels[0][1]);
		tc_qsobegind->SetSelection(sels[0][2]);
		wxDateTime now = wxDateTime::Now();
		wxDateTime qsoEnd(crq->qsoNotAfter.day, mons[crq->qsoNotAfter.month],
			crq->qsoNotAfter.year, 23, 59, 59);
		if (qsoEnd < now) {
			// Looks like this is a cert for an expired call sign,
			// so keep the QSO end date as-is. Otherwise, leave it
			// blank so CA can fill it in.
			tc_qsoendy->SetSelection(sels[1][0]);
			tc_qsoendm->SetSelection(sels[1][1]);
			tc_qsoendd->SetSelection(sels[1][2]);
		}
	}
	tc_status = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxSize(_parent->maxWidth, em_h*4));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq0.htm"));
	initialized = true;
}

CRQ_Page *
CRQ_CallsignPage::GetNext() const {
	tqslTrace("CRQ_CallsignPage::GetNext", NULL);
	if (_parent->cert) {			// Renewal
		_parent->signIt = false;
		reinterpret_cast<CRQ_NamePage*>(_parent->namePage)->Preset(reinterpret_cast<CRQ_CallsignPage*>(_parent->callsignPage));
		return _parent->namePage;
	}
	if (_parent->dxcc == 0) {		// NONE always requires signature
		_parent->signIt = true;
		return _parent->namePage;
	}
	if (_parent->onebyone) {		// 1x1 always requires signature
		_parent->signIt = true;
		_parent->signPrompt = _("Please select a Callsign Certificate to validate your request.");	// 1x1 callsign
		return _parent->namePage;
	}
	if (!_parent->validcerts) {		// No certs, can't sign.
		_parent->signIt = false;
		reinterpret_cast<CRQ_NamePage*>(_parent->namePage)->Preset(reinterpret_cast<CRQ_CallsignPage*>(_parent->callsignPage));
		return _parent->namePage;
	}
	reinterpret_cast<CRQ_NamePage*>(_parent->namePage)->Preset(reinterpret_cast<CRQ_CallsignPage*>(_parent->callsignPage));
	return _parent->namePage;
}

CRQ_Page *
CRQ_CallsignPage::GetPrev() const {
	tqslTrace("CRQ_CallsignPage::GetPrev", NULL);
	if (_parent->nprov > 1)
		return _parent->providerPage;
	return _parent->callsignPage;
}

BEGIN_EVENT_TABLE(CRQ_NamePage, CRQ_Page)
	EVT_TEXT(ID_CRQ_NAME, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_ADDR1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_CITY, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_NamePage::CRQ_NamePage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	tqslTrace("CRQ_NamePage::CRQ_NamePage", "parent=%lx, crq=%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(crq));
	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	_parent = parent;

	wxStaticText *zst = new wxStaticText(this, -1, _("Zip/Postal"));

	wxSize sz = getTextSize(this);
	int em_w = sz.GetWidth();
	int def_w = em_w * 20;
	wxStaticText *st = new wxStaticText(this, -1, _("Name"), wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	st->SetSize(zst->GetSize());

	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString val;
	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	wxString s;
	if (crq && crq->name[0])
		s = wxString::FromUTF8(crq->name);
	else if (config->Read(wxT("Name"), &val))
		s = val;
	tc_name = new wxTextCtrl(this, ID_CRQ_NAME, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_name, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);
	tc_name->SetMaxLength(TQSL_CRQ_NAME_MAX);

	s = wxT("");
	if (crq && crq->address1[0])
		s = wxString::FromUTF8(crq->address1);
	else if (config->Read(wxT("Addr1"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("Address"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_addr1 = new wxTextCtrl(this, ID_CRQ_ADDR1, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr1, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_addr1->SetMaxLength(TQSL_CRQ_ADDR_MAX);

	s = wxT("");
	if (crq && crq->address2[0])
		s = wxString::FromUTF8(crq->address2);
	else if (config->Read(wxT("Addr2"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT(""), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_addr2 = new wxTextCtrl(this, ID_CRQ_ADDR2, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr2, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_addr2->SetMaxLength(TQSL_CRQ_ADDR_MAX);

	s = wxT("");
	if (crq && crq->city[0])
		s = wxString::FromUTF8(crq->city);
	else if (config->Read(wxT("City"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("City"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_city = new wxTextCtrl(this, ID_CRQ_CITY, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_city, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_city->SetMaxLength(TQSL_CRQ_CITY_MAX);

	s = wxT("");
	if (crq && crq->state[0])
		s = wxString::FromUTF8(crq->state);
	else if (config->Read(wxT("State"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("State"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_state = new wxTextCtrl(this, ID_CRQ_STATE, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_state, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_state->SetMaxLength(TQSL_CRQ_STATE_MAX);

	s = wxT("");
	if (crq && crq->postalCode[0])
		s = wxString::FromUTF8(crq->postalCode);
	else if (config->Read(wxT("ZIP"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(zst, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_zip = new wxTextCtrl(this, ID_CRQ_ZIP, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_zip, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_zip->SetMaxLength(TQSL_CRQ_POSTAL_MAX);

	s = wxT("");
	if (crq && crq->country[0])
		s = wxString::FromUTF8(crq->country);
	else if (config->Read(_("Country"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, _("Country"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
	tc_country = new wxTextCtrl(this, ID_CRQ_COUNTRY, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_country, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_country->SetMaxLength(TQSL_CRQ_COUNTRY_MAX);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq1.htm"));
	initialized = true;
}

void
CRQ_NamePage::Preset(CRQ_CallsignPage *ip) {
	wxString s;
	string t;
	SaveAddressInfo(_parent->callsign.ToUTF8(), _parent->dxcc);
	if (!_parent->name.IsEmpty()) {
		tc_name->SetValue(_parent->name);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "name", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_name->SetValue(s);
	}

	if (!_parent->addr1.IsEmpty()) {
		tc_addr1->SetValue(_parent->addr1);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "addr1", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_addr1->SetValue(s);
	}
	if (!_parent->addr2.IsEmpty()) {
		if (_parent->addr2 == wxT("."))
			_parent->addr2 = wxT("");
		tc_addr2->SetValue(_parent->addr2);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "addr2", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_addr2->SetValue(s);
	}
	if (!_parent->city.IsEmpty()) {
		tc_city->SetValue(_parent->city);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "city", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_city->SetValue(s);
	}
	if (!_parent->state.IsEmpty()) {
		tc_state->SetValue(_parent->state);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "addrState", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_state->SetValue(s);
	}
	if (!_parent->zip.IsEmpty()) {
		tc_zip->SetValue(_parent->zip);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "mailCode", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_zip->SetValue(s);
	}
	if (!_parent->country.IsEmpty()) {
		tc_country->SetValue(_parent->country);
	} else if (get_address_field(_parent->callsign.ToUTF8(), "aCountry", t) == 0) {
		s = wxString::FromUTF8(t.c_str());
		tc_country->SetValue(s);
	}
}

CRQ_Page *
CRQ_NamePage::GetNext() const {
	tqslTrace("CRQ_NamePage::GetNext", NULL);
	return _parent->emailPage;
}

CRQ_Page *
CRQ_NamePage::GetPrev() const {
	tqslTrace("CRQ_NamePage::GetPrev", NULL);
	return _parent->callsignPage;
}


BEGIN_EVENT_TABLE(CRQ_EmailPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_EMAIL, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_EmailPage::CRQ_EmailPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	tqslTrace("CRQ_EmailPage::CRQ_EmailPage", "parent=%lx, crq=%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(crq));
	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	_parent = parent;
	wxSize sz = getTextSize(this);
	int em_w = sz.GetWidth();
	wxStaticText *st = new wxStaticText(this, -1, _("Your e-mail address"));

	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	wxString val;
	wxString s;
	if (crq && crq->emailAddress[0])
		s = wxString::FromUTF8(crq->emailAddress);
	else if (config->Read(wxT("Email"), &val))
		s = val;
	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_email = new wxTextCtrl(this, ID_CRQ_EMAIL, s, wxDefaultPosition, wxSize(em_w*30, -1));
	sizer->Add(tc_email, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_email->SetMaxLength(TQSL_CRQ_EMAIL_MAX);
	wxStaticText *tc_warn = new wxStaticText(this, -1, _("Note: The e-mail address you provide here is the address to which the issued Certificate will be sent. Make sure it's the correct address!"));
	sizer->Add(tc_warn, 0, wxALL, 10);
	tc_warn->Wrap(_parent->maxWidth);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq2.htm"));
	initialized = true;
}

CRQ_Page *
CRQ_EmailPage::GetNext() const {
	tqslTrace("CRQ_EmailPage::GetNext", NULL);
	if (_parent->CertPwd)
		return _parent->pwPage;
	else
		if (_parent->signIt)
			return _parent->signPage;
		else
			return NULL;
}

CRQ_Page *
CRQ_EmailPage::GetPrev() const {
	tqslTrace("CRQ_EmailPage::GetPrev", NULL);

	return _parent->namePage;
}

BEGIN_EVENT_TABLE(CRQ_PasswordPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_PW1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_PW2, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_PasswordPage::CRQ_PasswordPage(CRQWiz *parent) :  CRQ_Page(parent) {
	tqslTrace("CRQ_PasswordPage::CRQ_PasswordPage", "parent=%lx", reinterpret_cast<void *>(parent));
	initialized = false;
	_parent = parent;

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxSize sz = getTextSize(this);
	em_w = sz.GetWidth();
	em_h = sz.GetHeight();
	wxString lbl = _("You may protect this Callsign Certificate using a passphrase. If you are using a computer system that is shared with others, you should specify a passphrase to protect this Callsign Certificate. However, if you are using a computer in a private residence, no passphrase need be specified.");
	wxStaticText *st = new wxStaticText(this, -1, lbl);
	st->SetSize(_parent->maxWidth, em_h * 5);
	st->Wrap(_parent->maxWidth);
	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	fwdPrompt = new wxStaticText(this, -1, _("Leave the passphrase blank and click 'Next' unless you want to use a passphrase."));
	fwdPrompt->SetSize(_parent->maxWidth, em_h * 5);
	fwdPrompt->Wrap(_parent->maxWidth);
	sizer->Add(fwdPrompt, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	sizer->Add(new wxStaticText(this, -1, _("Passphrase:")),
		0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw1 = new wxTextCtrl(this, ID_CRQ_PW1, wxT(""), wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw1, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, _("Enter the passphrase again for verification:")),
		0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw2 = new wxTextCtrl(this, ID_CRQ_PW2, wxT(""), wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw2, 0, wxLEFT|wxRIGHT, 10);
	wxStaticText *tc_pwwarn = new wxStaticText(this, -1, _("DO NOT lose the passphrase you choose! You will be unable to use the Certificate without this passphrase!"));
	tc_pwwarn->Wrap(em_w * 40);
	sizer->Add(tc_pwwarn, 0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq3.htm"));
	initialized = true;
}

CRQ_Page *
CRQ_PasswordPage::GetNext() const {
	tqslTrace("CRQ_PasswordPage::GetNext", NULL);
	if (_parent->signIt) {
		fwdPrompt->SetLabel(_("Leave the passphrase blank and click 'Next' unless you want to use a passphrase."));
		fwdPrompt->SetSize(_parent->maxWidth, em_h * 5);
		fwdPrompt->Wrap(_parent->maxWidth);
		return _parent->signPage;
	} else {
		fwdPrompt->SetLabel(_("Leave the passphrase blank and click 'Finish' unless you want to use a passphrase."));
		fwdPrompt->SetSize(_parent->maxWidth, em_h * 5);
		fwdPrompt->Wrap(_parent->maxWidth);
		return NULL;
	}
}

CRQ_Page *
CRQ_PasswordPage::GetPrev() const {
	tqslTrace("CRQ_PasswordPage::GetPrev", NULL);
	return _parent->emailPage;
}

BEGIN_EVENT_TABLE(CRQ_SignPage, CRQ_Page)
	EVT_TREE_SEL_CHANGED(ID_CRQ_CERT, CRQ_SignPage::CertSelChanged)
	EVT_RADIOBOX(ID_CRQ_SIGN, CRQ_Page::check_valid)
	EVT_WIZARD_PAGE_CHANGING(wxID_ANY, CRQ_SignPage::OnPageChanging)
END_EVENT_TABLE()


void CRQ_SignPage::CertSelChanged(wxTreeEvent& event) {
	tqslTrace("CRQ_SignPage::CertSelChanged", NULL);
	if (cert_tree->GetItemData(event.GetItem()))
		_parent->signIt = true;
	wxCommandEvent dummy;
	check_valid(dummy);
}

CRQ_SignPage::CRQ_SignPage(CRQWiz *parent, TQSL_CERT_REQ *crq)
	:  CRQ_Page(parent) {
	tqslTrace("CRQ_SignPage::CRQ_SignPage", "parent=%lx", reinterpret_cast<void *>(parent));

	initialized = false;
	_parent = parent;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxString itext;
	itext = wxString(_("Is this new certificate for a callsign where you already have a "
		"LoTW account, and you want the QSOs for this call to be added to an existing LoTW account? "));
        itext += wxT("\n\n");
	itext += _("If so, choose a callsign below for the primary LoTW account. If not, choose 'No', and a new LoTW account will be set up for these QSOs.");

	itext += wxT("\n\n");
	itext += _("CAUTION: Mixing QSOs for unrelated callsigns into one LoTW account can cause issues with handling awards.");
	introText = new wxStaticText(this, -1, itext);
	sizer->Add(introText);

	wxSize sz = getTextSize(this);
	int em_h = sz.GetHeight();
	em_w = sz.GetWidth();
	tc_status = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxSize(_parent->maxWidth, em_h*3));

	wxString choices[] = { _("This is a Club call, I'm the QSL manager for this call, or this is a DXpedition call"), _("No, Create a new LoTW account for this call"), _("Yes, Save these QSOs into an existing LoTW account") };

	choice = new wxRadioBox(this, ID_CRQ_SIGN, _("Add QSOs for the new callsign to an existing LoTW account?"), wxDefaultPosition,
		wxSize(em_w*30, -1), 3, choices, 1, wxRA_SPECIFY_COLS);
	sizer->Add(choice, 0, wxALL|wxEXPAND, 10);

	cert_tree = new CertTree(this, ID_CRQ_CERT, wxDefaultPosition,
		wxSize(em_w*30, em_h*8), wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
	sizer->Add(cert_tree, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND);
	cert_tree->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	// Default to 'signed' unless there's no valid certificates to use for signing.
	if (cert_tree->Build(0, &(_parent->provider)) > 0) {
		choice->SetSelection(2);
		_parent->signIt = true;
	} else {
		choice->SetSelection(1);
		_parent->signIt = false;
		cert_tree->Show(false);
		introText->SetLabel(_("Since you have no callsign certificates, you must "
					"submit an 'Unsigned' certificate request. This will allow you to "
					"create your initial callsign certificate for LoTW use. "
					"Click 'Finish' to complete this callsign certificate request."));
	}
	introText->Wrap(em_w * 50);
	AdjustPage(sizer, wxT("crq4.htm"));
	initialized = true;
}

void
CRQ_SignPage::refresh() {
	tqslTrace("CRQ_SignPage::refresh", NULL);
	if (cert_tree->Build(0, &(_parent->provider)) > 0) {
		choice->SetSelection(2);
		_parent->signIt = true;
	} else {
		choice->SetSelection(1);
		_parent->signIt = false;
	}
}

CRQ_Page *
CRQ_SignPage::GetPrev() const {
	tqslTrace("CRQ_SignPage::GetPrev", NULL);

	if (_parent->CertPwd)
		return _parent->pwPage;
	else
		return _parent->emailPage;
}

// Page validation

bool
CRQ_ProviderPage::TransferDataFromWindow() {
	// Nothing to validate
	return true;
}

const char *
CRQ_CallsignPage::validate() {
	tqslTrace("CRQ_CallsignPage::validate", NULL);
	tQSL_Cert *certlist = 0;
	int ncert = 0;
	// List of DXCC entities in the US.
	int USEntities[] = { 6, 9, 20, 43, 103, 105, 110, 123, 138, 166, 174, 182, 197, 202, 285, 291, 297, 515, -1 };
	if (!initialized)
		return 0;
	const char *dxccname = NULL;
	bool ok = true;
	valMsg = wxT("");
	if (tc_call->GetValue().Len() > TQSL_CALLSIGN_MAX) {
		valMsg = wxString::Format(_("The callsign is too long. Only %d characters are allowed."), TQSL_CALLSIGN_MAX);
		goto notok;
	}
	_parent->callsign = tc_call->GetValue().MakeUpper();
	int sel;

	_parent->onebyone = false;
	if (_parent->callsign.Len() < 3)
		ok = false;
	if (ok) {
		string call = string(_parent->callsign.ToUTF8());
		// Check for invalid characters
		if (call.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/") != string::npos)
			ok = false;
		// Need at least one letter
		if (call.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == string::npos)
			ok = false;
		// Need at least one number
		if (call.find_first_of("0123456789") == string::npos)
			ok = false;
		// Invalid callsign patterns
		// Starting with 0, Q, (no longer: C7, or 4Y)
		// 1x other than 1A, 1M, 1S
		string first = call.substr(0, 1);
		string second = call.substr(1, 1);
		string third = call.substr(2, 1);
		if (first == "0" || first == "Q" ||
		    (first == "1" && second != "A" && second != "M" && second != "S"))
			ok = false;
		if (call.size() == 3 &&
		    (first == "W" || first == "K" || first == "N") &&
		    (second >= "0" && second <= "9") &&
		    (third != "X")) {
			_parent->onebyone = true;
		}
	}

	if (!ok) {
		valMsg = _("You must enter a valid call sign.");
		goto notok;
	}

	long old_dxcc;
	old_dxcc = _parent->dxcc;

	tQSL_Date oldStartDate;
	tQSL_Date oldEndDate;
	tQSL_Date startDate;
	tQSL_Date endDate;
	tqsl_getDXCCStartDate(old_dxcc, &oldStartDate);
	tqsl_getDXCCEndDate(old_dxcc, &oldEndDate);
	sel = tc_dxcc->GetSelection();
	if (sel >= 0)
		_parent->dxcc = (long)(tc_dxcc->GetClientData(sel));

	tqsl_getDXCCStartDate(_parent->dxcc, &startDate);
	tqsl_getDXCCEndDate(_parent->dxcc, &endDate);

	if (sel < 0 || _parent->dxcc < 0) {
		valMsg = _("You must select a DXCC entity.");
		goto notok;
	}

	if (_parent->dxcc != old_dxcc) {
		if (tqsl_isDateValid(&startDate) && !tqsl_isDateNull(&startDate) &&
		    tqsl_compareDates(&_parent->qsonotbefore, &oldStartDate) == 0) {
			tc_qsobeginy->SetSelection(startDate.year - 1945);
			tc_qsobeginm->SetSelection(startDate.month - 1);
			tc_qsobegind->SetSelection(startDate.day - 1);
		}
		if ((tqsl_isDateValid(&endDate) || tqsl_isDateNull(&endDate)) &&
		     tqsl_compareDates(&_parent->qsonotafter, &oldEndDate) == 0) {
			if (tqsl_isDateNull(&endDate)) {
				tc_qsoendy->SetSelection(0);
				tc_qsoendm->SetSelection(0);
				tc_qsoendd->SetSelection(0);
			} else {
				tc_qsoendy->SetSelection(endDate.year - 1944);
				tc_qsoendm->SetSelection(endDate.month);
				tc_qsoendd->SetSelection(endDate.day);
			}
		}
	}
	_parent->qsonotbefore.year = strtol(tc_qsobeginy->GetValue().ToUTF8(), NULL, 10);
	_parent->qsonotbefore.month = strtol(tc_qsobeginm->GetValue().ToUTF8(), NULL, 10);
	_parent->qsonotbefore.day = strtol(tc_qsobegind->GetValue().ToUTF8(), NULL, 10);
	_parent->qsonotafter.year = strtol(tc_qsoendy->GetValue().ToUTF8(), NULL, 10);
	_parent->qsonotafter.month = strtol(tc_qsoendm->GetValue().ToUTF8(), NULL, 10);
	_parent->qsonotafter.day = strtol(tc_qsoendd->GetValue().ToUTF8(), NULL, 10);
	if (!tqsl_isDateValid(&_parent->qsonotbefore)) {
		valMsg = _("QSO begin date: You must choose proper values for Year, Month and Day.");
		goto notok;
	}
	if (!tqsl_isDateNull(&_parent->qsonotafter) && !tqsl_isDateValid(&_parent->qsonotafter)) {
		valMsg = _("QSO end date: You must either choose proper values for Year, Month and Day or leave all three blank.");
		goto notok;
	}
	if (tqsl_isDateValid(&_parent->qsonotbefore) && tqsl_isDateValid(&_parent->qsonotafter)
		&& tqsl_compareDates(&_parent->qsonotbefore, &_parent->qsonotafter) > 0) {
		valMsg = _("QSO end date cannot be before QSO begin date.");
		goto notok;
	}
	char startStr[50], endStr[50];
	tqsl_convertDateToText(&endDate, endStr, sizeof endStr);
	if (tqsl_getDXCCEntityName(_parent->dxcc, &dxccname))
		dxccname = "UNKNOWN";
	if (!tqsl_isDateValid(&startDate)) {
		startDate.year = 1945; startDate.month = 11; startDate.day = 1;
	}
	tqsl_convertDateToText(&startDate, startStr, sizeof startStr);

	if (tqsl_isDateValid(&endDate) && tqsl_isDateNull(&_parent->qsonotafter)) {
		_parent->qsonotafter = endDate;
		if (tqsl_isDateNull(&endDate)) {
			tc_qsoendy->SetSelection(0);
			tc_qsoendm->SetSelection(0);
			tc_qsoendd->SetSelection(0);
		} else {
			tc_qsoendy->SetSelection(endDate.year - 1944);
			tc_qsoendm->SetSelection(endDate.month);
			tc_qsoendd->SetSelection(endDate.day);
		}
	}

	if (tqsl_isDateValid(&endDate)) {
		tqsl_convertDateToText(&endDate, endStr, sizeof endStr);
	} else {
		endStr[0] = '\0';
	}

	if (tqsl_isDateValid(&startDate) && tqsl_compareDates(&_parent->qsonotbefore, &startDate) < 0) {
		valMsg = wxString::Format(_("The date of your first QSO is before the first valid date (%hs) of the selected DXCC Entity %hs"), startStr, dxccname);
		goto notok;
	}
	if (tqsl_isDateValid(&endDate) && tqsl_compareDates(&_parent->qsonotbefore, &endDate) > 0) {
		valMsg = wxString::Format(_("The date of your first QSO is after the last valid date (%hs) of the selected DXCC Entity %hs"), endStr, dxccname);
		goto notok;
	}
	if (tqsl_isDateValid(&startDate) && !tqsl_isDateNull(&_parent->qsonotafter) && tqsl_compareDates(&_parent->qsonotafter, &startDate) < 0) {
		valMsg = wxString::Format(_("The date of your last QSO is before the first valid date (%hs) of the selected DXCC Entity %hs"), startStr, dxccname);
		goto notok;
	}
	if (tqsl_isDateValid(&endDate) && tqsl_compareDates(&_parent->qsonotafter, &endDate) > 0) {
		valMsg = wxString::Format(_("The date of your last QSO is after the last valid date (%hs) of the selected DXCC Entity %hs"), endStr, dxccname);
		goto notok;
	}

	// Check for US 1x1 callsigns
	_parent->usa = false;
	for (int i = 0; USEntities[i] > 0; i++) {
		if (_parent->dxcc == USEntities[i]) {
			_parent->usa = true;
			break;
		}
	}
	if (!_parent->usa || _parent->callsign.Len() != 3)
		_parent->onebyone = false;

	// Data looks okay, now let's make sure this isn't a duplicate request
	// (unless it's a renewal).

	_parent->callsign.MakeUpper();
	tqsl_selectCertificates(&certlist, &ncert, _parent->callsign.ToUTF8(), _parent->dxcc, 0,
				&(_parent->provider), TQSL_SELECT_CERT_WITHKEYS);
	if (!_parent->_crq && ncert > 0) {
		char cert_before_buf[40], cert_after_buf[40];
		for (int i = 0; i < ncert; i++) {
			// See if this cert overlaps the user-specified date range
			tQSL_Date cert_not_before, cert_not_after;
			int cert_dxcc = 0;
			tqsl_getCertificateQSONotBeforeDate(certlist[i], &cert_not_before);
			tqsl_getCertificateQSONotAfterDate(certlist[i], &cert_not_after);
			tqsl_getCertificateDXCCEntity(certlist[i], &cert_dxcc);
			if (cert_dxcc == _parent->dxcc
					&& ((tqsl_isDateValid(&_parent->qsonotafter)
					&& !(tqsl_compareDates(&_parent->qsonotbefore, &cert_not_after) == 1
					|| tqsl_compareDates(&_parent->qsonotafter, &cert_not_before) == -1))
					|| (!tqsl_isDateValid(&_parent->qsonotafter)
					&& !(tqsl_compareDates(&_parent->qsonotbefore, &cert_not_after) == 1)))) {
				ok = false;	// Overlap!
				tqsl_convertDateToText(&cert_not_before, cert_before_buf, sizeof cert_before_buf);
				tqsl_convertDateToText(&cert_not_after, cert_after_buf, sizeof cert_after_buf);
			}
		}
		tqsl_freeCertificateList(certlist, ncert);
		if (ok == false) {
			DXCC dxcc;
			dxcc.getByEntity(_parent->dxcc);
			// TRANSLATORS: first argument is callsign (%s), second is the related DXCC entity name (%hs)
			valMsg = wxString::Format(_("You have an overlapping Certificate for %s (DXCC=%hs) having QSO dates: "), _parent->callsign.c_str(), dxcc.name());
			// TRANSLATORS: here "to" separates two dates in a date range
			valMsg += wxString::FromUTF8(cert_before_buf) + _(" to ") + wxString::FromUTF8(cert_after_buf);
		}
	}
	{
		wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
		wxStringTokenizer tkz(pending, wxT(","));
		while (tkz.HasMoreTokens()) {
			wxString pend = tkz.GetNextToken();
			if (pend == _parent->callsign) {
				wxString fmt = _("You have already requested a Callsign Certificate for %s and can not request another until that request has been processed by LoTW Staff.");
					fmt += wxT("\n\n");
					fmt += _("Please wait until you receive an e-mail bearing your requested Callsign Certificate.");
					fmt += wxT("\n\n");
					fmt += _("If you are sure that the earlier request is now invalid you should delete the pending Callsign Certificate for %s.");
				valMsg = wxString::Format(fmt, _parent->callsign.c_str(), _parent->callsign.c_str());
				goto notok;
			}
		}
	}
        {
		wxString requestRecord = wxConfig::Get()->Read(wxT("RequestRecord"));
		wxString requestList;
		wxStringTokenizer rectkz(requestRecord, wxT(","));
		time_t now = time(NULL);
		time_t yesterday = now - 24 * 60 * 60; // 24 hours ago
		int numRequests = 0;
		while (rectkz.HasMoreTokens()) {
			wxString rec = rectkz.GetNextToken();
			char csign[512];
			time_t rectime;
			strncpy(csign, rec.ToUTF8(), sizeof csign);
			char *s = csign;
			while (*s != ':' && *s != '\0')
				s++;
			*s = '\0';
			rectime = strtol(++s, NULL, 10);
			if (rectime < yesterday) continue;		// More than 24 hours old
			if (strcmp(csign, _parent->callsign.ToUTF8()) == 0) { // Same call
				numRequests++;
			}
			if (!requestList.IsEmpty()) {
				requestList = requestList + wxT(",");
			}
			requestList = requestList + wxString::Format(wxT("%hs:%Lu"), csign, rectime);
		}
		wxConfig::Get()->Write(wxT("RequestRecord"), requestList);
		wxConfig::Get()->Flush();

		if (numRequests > 3) {
			wxString fmt = _("You have already requested more than three Callsign Certificates for %s in the past 24 hours. You should submit a request only once, then wait for that request to processed by LoTW Staff. This may take several business days.");
					fmt += wxT("\n\n");
					fmt += _("Please wait until you receive an e-mail bearing your requested Callsign Certificate.");
					fmt += wxT("\n\n");
			valMsg = wxString::Format(fmt, _parent->callsign.c_str(), _parent->callsign.c_str());
		}
	}
 notok:
	tc_status->SetLabel(valMsg);
	tc_status->Wrap(_parent->maxWidth);
	return 0;
}

bool
CRQ_CallsignPage::TransferDataFromWindow() {
	tqslTrace("CRQ_CallsignPage::TransferDataFromWindow", NULL);
	bool ok;

	validate();

	bool hasEndDate = (!tqsl_isDateNull(&_parent->qsonotafter) && tqsl_isDateValid(&_parent->qsonotafter));
	bool notInULS = false;

	// First check if there's a slash. If so, it's a portable. Use the base callsign
	wxString callsign = _parent->callsign;
	int slashpos = callsign.Find('/', true);
	wxString prefix = callsign;
	wxString suffix = wxT("");
	if (slashpos != wxNOT_FOUND) {
		prefix = callsign.Left(slashpos);
		suffix = callsign.Right(slashpos+1);
		callsign = prefix;
	}

	// Is this in the ULS?
	if (valMsg.Len() == 0 && _parent->usa && !_parent->onebyone) {
		wxString name, attn, addr1, city, state, zip, update;
		int stat = GetULSInfo(callsign.ToUTF8(), name, attn, addr1, city, state, zip, update);
		// handle portable/home and home/portable
		if (stat == 2 && !wxIsEmpty(suffix)) {
			stat = GetULSInfo(suffix.ToUTF8(), name, attn, addr1, city, state, zip, update);
		}
		switch (stat) {
			case 0:
				_parent->validusa = true;		// Good data returned
				if (name == wxT("null"))
					name = wxT("");
				_parent->name = name;
				_parent->namePage->setName(name);

				if (addr1 == wxT("null"))
					addr1 = wxT("");
				if (attn == wxT("null")) {
					attn = wxT("");
					_parent->addr1 = addr1;
					_parent->addr2 = wxT(".");
					_parent->namePage->setAddr1(addr1);
					_parent->namePage->setAddr2(attn);
				} else {
					_parent->addr1 = attn;
					_parent->addr2 = addr1;
					_parent->namePage->setAddr1(attn);
					_parent->namePage->setAddr2(addr1);
				}

				if (city == wxT("null"))
					city = wxT("");
				_parent->city = city;
				_parent->namePage->setCity(city);

				if (state == wxT("null"))
					state = wxT("");
				_parent->state = state;
				_parent->namePage->setState(state);

				if (zip == wxT("null"))
					zip = wxT("");
				_parent->zip = zip;
				_parent->namePage->setZip(zip);

				_parent->country = wxT("USA");
				_parent->namePage->setCountry(_parent->country);
				break;
			case 1:
				break;						// Error reading ULS info
			case 2:

				int stat2 = GetULSInfo("W1AW", name, attn, addr1, city, state, zip, update);
				if (stat2 == 2)					// Also nothing for a good call
					break;
				if (hasEndDate) {				// Allow former calls
					notInULS = true;
					break;
				}
				// If this call has a slash, then it may be a portable call from
				// outside the US. We really can't tell at this point so just
				// let it go.
				if (slashpos != wxNOT_FOUND) {
					break;
				}
				valMsg = wxString::Format(_("The callsign %s is not currently registered in the FCC ULS database as of %s.\nIf this is a newly registered call, you must wait at least one business day for it to be valid. Please enter a currently valid callsign."), callsign.c_str(), update.c_str());
				break;
		}
	}

	if (valMsg.Len() == 0) {
		ok = true;
	} else {
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
		ok = false;
	}
	if (ok && _parent->dxcc == 0) {
		if (!_parent->validcerts) {
			wxString msg = _("You cannot select DXCC Entity NONE as you must sign any request for entity NONE and you have no valid Callsign Certificates that you can use to sign this request.");
			wxMessageBox(msg, _("TQSL Error"), wxOK | wxICON_ERROR, this);
			return false;
		}

		wxString msg = _("You have selected DXCC Entity NONE");
			msg += wxT("\n\n");
			msg += _("QSO records signed using the Certificate will not be valid for DXCC award credit (but will be valid for other applicable awards). If the Certificate is to be used for signing QSOs from maritime/marine mobile, shipboard, or air mobile operations, that is the correct selection. Otherwise, you probably should use the \"Back\" button to return to the DXCC page after clicking \"OK\"");
		wxMessageBox(msg, _("TQSL Warning"), wxOK | wxICON_WARNING, this);
	}
	if (ok && _parent->onebyone) {
		if (!_parent->validcerts) {
			wxString msg = _("You cannot request a certificate for a 1x1 callsign as you must sign those requests, but you have no valid Callsign Certificates that you can use to sign this request.");
			wxMessageBox(msg, _("TQSL Error"), wxOK | wxICON_ERROR, this);
			return false;
		}
	}

	if (ok && hasEndDate && !notInULS) {		// If it has an end date and it's a current call
		wxString msg = _("You have chosen a QSO end date for this Callsign Certificate. The 'QSO end date' should ONLY be set if that date is the date when that callsign's license expired or the license was replaced by a new callsign.");
			msg += wxT("\n\n");
			msg += _("If you set an end date, you will not be able to sign QSOs past that date, even if the Callsign Certificate itself is still valid.");
			msg += wxT("\n\n");
			msg += _("If you still hold this callsign (or if you plan to renew the license for the callsign), you should not set a 'QSO end date'.");
			msg += wxT("\n");
			msg += _("Do you really want to keep this 'QSO end date'?");
		if (wxMessageBox(msg, _("Warning"), wxYES_NO|wxICON_EXCLAMATION, this) == wxNO) {
				tc_qsoendy->SetSelection(0);
				tc_qsoendm->SetSelection(0);
				tc_qsoendd->SetSelection(0);
				return false;
		}
	}
	_parent->callsign = tc_call->GetValue();
	_parent->callsign.MakeUpper();
	tc_call->SetValue(_parent->callsign);
	return ok;
}

static bool
cleanString(wxString &str) {
	str.Trim();
	str.Trim(FALSE);
	int idx;
	while ((idx = str.Find(wxT("  "))) > 0) {
		str.Remove(idx, 1);
	}
	return str.IsEmpty();
}

const char *
CRQ_NamePage::validate() {
	tqslTrace("CRQ_NamePage::validate", NULL);
	if (!initialized)
		return 0;
	valMsg = wxT("");
	_parent->name = tc_name->GetValue();
	_parent->addr1 = tc_addr1->GetValue();
	_parent->city = tc_city->GetValue();

	if (cleanString(_parent->name))
		valMsg = _("You must enter your name");
	if (valMsg.Len() == 0 && cleanString(_parent->addr1))
		valMsg = _("You must enter your address");
	if (valMsg.Len() == 0 && cleanString(_parent->city))
		valMsg = _("You must enter your city");
	tc_status->SetLabel(valMsg);
	if (!valMsg.IsEmpty())
		return 0;
	//
	// If this is not a renewal, and it's in the USA, and there's no certs to sign it with,
	// then this is an initial certificate and must match the FCC database. Say so.
	//
	if (!_parent->renewal && _parent->validusa && !_parent->validcerts) {
		tc_status->SetLabel(_("This address must match the FCC ULS database.\nIf this address information is incorrect, please correct your FCC record."));
		tc_name->Enable(false);
		tc_addr1->Enable(false);
		tc_addr2->Enable(false);
		tc_city->Enable(false);
		tc_state->Enable(false);
		tc_zip->Enable(false);
		tc_country->Enable(false);
	}
	return 0;
}

bool
CRQ_NamePage::TransferDataFromWindow() {
	tqslTrace("CRQ_NamePage::TransferDataFromWindow", NULL);
	_parent->name = tc_name->GetValue();
	_parent->addr1 = tc_addr1->GetValue();
	_parent->addr2 = tc_addr2->GetValue();
	_parent->city = tc_city->GetValue();
	_parent->state = tc_state->GetValue();
	_parent->zip = tc_zip->GetValue();
	_parent->country = tc_country->GetValue();

	bool ok;
	validate();
	if (valMsg.Len() == 0) {
		ok = true;
	} else {
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
		ok = false;
	}

	cleanString(_parent->name);
	cleanString(_parent->addr1);
	cleanString(_parent->addr2);
	cleanString(_parent->city);
	cleanString(_parent->state);
	cleanString(_parent->zip);
	cleanString(_parent->country);
	tc_name->SetValue(_parent->name);
	tc_addr1->SetValue(_parent->addr1);
	tc_addr2->SetValue(_parent->addr2);
	tc_city->SetValue(_parent->city);
	tc_state->SetValue(_parent->state);
	tc_zip->SetValue(_parent->zip);
	tc_country->SetValue(_parent->country);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Write(wxT("Name"), _parent->name);
	config->Write(wxT("Addr1"), _parent->addr1);
	config->Write(wxT("Addr2"), _parent->addr2);
	config->Write(wxT("City"), _parent->city);
	config->Write(wxT("State"), _parent->state);
	config->Write(wxT("ZIP"), _parent->zip);
	config->Write(wxT("Country"), _parent->country);
	return ok;
}

const char *
CRQ_EmailPage::validate() {
	tqslTrace("CRQ_EmailPage::validate()", NULL);

	if (!initialized)
		return 0;
	valMsg = wxT("");
	_parent->email = tc_email->GetValue();
	cleanString(_parent->email);
	int i = _parent->email.First('@');
	int j = _parent->email.Last('.');
	if (i < 1 || j < i+2 || j == static_cast<int>(_parent->email.length())-1)
		valMsg = _("You must enter a valid email address");
	tc_status->SetLabel(valMsg);
	return 0;
}

bool
CRQ_EmailPage::TransferDataFromWindow() {
	tqslTrace("CRQ_EmailPage::TransferDataFromWindow", NULL);
	bool ok;
	validate();
	if (valMsg.Len() == 0) {
		ok = true;
	} else {
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
		ok = false;
	}

	_parent->email = tc_email->GetValue();
	cleanString(_parent->email);
	wxConfig *config = reinterpret_cast<wxConfig *>(wxConfig::Get());
	config->Write(wxT("Email"), _parent->email);
	return ok;
}

const char *
CRQ_PasswordPage::validate() {
	tqslTrace("CRQ_PasswordPage::validate", NULL);

	if (!initialized)
		return 0;
	valMsg = wxT("");
	wxString pw1 = tc_pw1->GetValue();
	wxString pw2 = tc_pw2->GetValue();

	if (pw1 != pw2)
		valMsg = _("The two copies of the passphrase do not match.");
	tc_status->SetLabel(valMsg);
	return 0;
}

bool
CRQ_PasswordPage::TransferDataFromWindow() {
	tqslTrace("CRQ_PasswordPage::TransferDataFromWindow", NULL);
	bool ok;
	validate();
	if (valMsg.Len() == 0) {
		ok = true;
	} else {
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
		ok = false;
	}
	_parent->password = tc_pw1->GetValue();
	return ok;
}

void
CRQ_SignPage::OnPageChanging(wxWizardEvent& ev) {
	tqslTrace("CRQ_SignPage::OnPageChanging", "Direction=", ev.GetDirection());

	validate();
	if (valMsg.Len() > 0 && ev.GetDirection()) {
		ev.Veto();
		wxMessageBox(valMsg, _("TQSL Error"), wxOK | wxICON_ERROR, this);
	}
}


const char *
CRQ_SignPage::validate() {
	tqslTrace("CRQ_SignPage::validate", NULL);
	bool error = false;

	if (!initialized)
		return 0;

	valMsg = wxT("");
	wxString nextprompt = _("Click 'Finish' to complete this Callsign Certificate request.");

	bool doSigned = (choice->GetSelection() == 2);

	cert_tree->Show(doSigned);

	if (doSigned) {
		if (!cert_tree->GetSelection().IsOk() || cert_tree->GetItemData(cert_tree->GetSelection()) == NULL) {
			error = true;
			valMsg = _("Please select a callsign certificate for the account where you would like the QSOs to be stored");
		} else {
			char callsign[512];
			tQSL_Cert cert = cert_tree->GetItemData(cert_tree->GetSelection())->getCert();
			if (0 == tqsl_getCertificateCallSign(cert, callsign, sizeof callsign)) {
				wxString fmt = wxT("\n\n");
					fmt += _("QSOs for %hs will be stored in the LoTW account for %s.");
				nextprompt+=wxString::Format(fmt, _parent->callsign.c_str(), callsign);
			}
		}
	}

	tc_status->SetLabel(error ? valMsg : nextprompt);
	tc_status->Wrap(_parent->maxWidth);
	return 0;
}

bool
CRQ_SignPage::TransferDataFromWindow() {
	tqslTrace("CRQ_SignPage::TransferDataFromWindow", NULL);
	validate();

	_parent->cert = 0;
	CertTreeItemData *data = reinterpret_cast<CertTreeItemData *>(cert_tree->GetItemData(cert_tree->GetSelection()));
	if (data)
		_parent->cert = data->getCert();
	return true;
}

