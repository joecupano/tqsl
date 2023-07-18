/***************************************************************************
                          qsodatadialog.cpp  -  description
                             -------------------
    begin                : Sat Dec 7 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: qsodatadialog.cpp,v 1.7 2013/03/01 12:59:37 k1mu Exp $
 ***************************************************************************/

#include "qsodatadialog.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslvalidator.h"
#include "wx/valgen.h"
#include "wx/spinctrl.h"
#include "wx/statline.h"
#include "tqsllib.h"
#include "tqslexcept.h"
#include "tqsltrace.h"
#include "wxutil.h"

using std::vector;
using std::ofstream;
using std::ios;
using std::cerr;
using std::endl;

#define TQSL_ID_LOW 6000

#ifdef __WIN32__
	#define TEXT_HEIGHT 24
	#define LABEL_HEIGHT 18
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 10
	#define VSEP 3
	#define GEOM1 4
#elif defined(__APPLE__)
	#define TEXT_HEIGHT 24
	#define LABEL_HEIGHT 18
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 10
	#define VSEP 3
	#define GEOM1 4
#else
	#define TEXT_HEIGHT 18
	#define LABEL_HEIGHT TEXT_HEIGHT
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 12
	#define VSEP 4
	#define GEOM1 6
#endif

#undef TEXT_HEIGHT
#define TEXT_HEIGHT -1

#define SKIP_HEIGHT (TEXT_HEIGHT+VSEP)

#define QD_CALL	TQSL_ID_LOW
#define QD_DATE	TQSL_ID_LOW+1
#define QD_TIME	TQSL_ID_LOW+2
#define QD_MODE	TQSL_ID_LOW+3
#define QD_BAND	TQSL_ID_LOW+4
#define QD_FREQ	TQSL_ID_LOW+5
#define QD_OK TQSL_ID_LOW+6
#define QD_CANCEL TQSL_ID_LOW+7
#define QD_RECNO TQSL_ID_LOW+8
#define QD_RECDOWN TQSL_ID_LOW+9
#define QD_RECUP TQSL_ID_LOW+10
#define QD_RECBOTTOM TQSL_ID_LOW+11
#define QD_RECTOP TQSL_ID_LOW+12
#define QD_RECNEW TQSL_ID_LOW+13
#define QD_RECDELETE TQSL_ID_LOW+14
#define QD_RECNOLABEL TQSL_ID_LOW+15
#define QD_HELP TQSL_ID_LOW+16
#define QD_PROPMODE TQSL_ID_LOW+17
#define QD_SATELLITE TQSL_ID_LOW+18
#define QD_RXBAND TQSL_ID_LOW+19
#define QD_RXFREQ TQSL_ID_LOW+20


static void set_font(wxWindow *w, wxFont& font) {
#ifndef __WIN32__
	w->SetFont(font);
#endif
}

// Images for buttons.

#include "left.xpm"
#include "right.xpm"
#include "bottom.xpm"
#include "top.xpm"

class choice {
 public:
	explicit choice(const wxString& _value, const wxString& _display = wxT(""), int _low = 0, int _high = 0) {
		value = _value;
		display = (_display.IsEmpty()) ? value : _display;
		low = _low;
		high = _high;
	}
	wxString value, display;
	int low, high;
	bool operator ==(const choice& other) { return other.value == value; }
	bool operator ==(const wxString& other) { return other == value; }
};

class valid_list : public vector<choice> {
 public:
	valid_list() {}
	valid_list(const char **values, int nvalues);
	wxString *GetChoices() const;
};

valid_list::valid_list(const char **values, int nvalues) {
	while(nvalues--)
		push_back(choice(wxString::FromUTF8(*(values++))));
}

wxString *
valid_list::GetChoices() const {
	wxString *ary = new wxString[size()];
	wxString *sit = ary;
	const_iterator it;
	for (it = begin(); it != end(); it++)
		*sit++ = (*it).display;
	return ary;
}
static bool
sat_cmp(const choice& p1, const choice& p2) {
        return p1.value < p2.value;
}

static valid_list valid_modes;
static valid_list valid_bands;
static valid_list valid_rxbands;
static valid_list valid_propmodes;
static valid_list valid_satellites;

static int
init_valid_lists() {
	tqslTrace("init_valid_lists", NULL);
	if (valid_bands.size() > 0)
		return 0;
	if (tqsl_init())
		return 1;
	int count;
	if (tqsl_getNumADIFMode(&count))
		return 1;
	const char *cp, *cp1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getADIFModeEntry(i, &cp))
			return 1;
		valid_modes.push_back(choice(wxString::FromUTF8(cp)));
	}
	valid_rxbands.push_back(choice(wxT(""), _("NONE")));

	if (tqsl_getNumBand(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		int low, high, scale;
		if (tqsl_getBand(i, &cp, &cp1, &low, &high))
			return 1;
		wxString low_s = wxString::Format(wxT("%d"), low);
		wxString high_s = wxString::Format(wxT("%d"), high);
		const char *hz;
		if (!strcmp(cp1, "HF")) {
			hz = "kHz";
			scale = 1;		// Config file freqs are in KHz
		} else {
			hz = "mHz";
			scale = 1000;		// Freqs are in MHz for VHF/UHF.
		}
		if (low >= 1000) {
			low_s = wxString::Format(wxT("%g"), low / 1000.0);
			high_s = wxString::Format(wxT("%g"), high / 1000.0);
			if (!strcmp(cp1, "HF")) {
				hz = "MHz";
			} else {
				hz = "GHz";
			}
			if (high == 0)
				high_s = _("UP");
		}
		wxString display = wxString::Format(wxT("%hs (%s-%s %hs)"), cp,
			low_s.c_str(), high_s.c_str(), hz);
		valid_bands.push_back(choice(wxString::FromUTF8(cp), display, low*scale, high*scale));
		valid_rxbands.push_back(choice(wxString::FromUTF8(cp), display, low*scale, high*scale));
	}
	valid_propmodes.push_back(choice(wxT(""), _("NONE")));
	if (tqsl_getNumPropagationMode(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getPropagationMode(i, &cp, &cp1))
			return 1;
		valid_propmodes.push_back(choice(wxString::FromUTF8(cp), wxString::FromUTF8(cp1)));
	}
	valid_satellites.push_back(choice(wxT(""), _("NONE")));
	if (tqsl_getNumSatellite(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getSatellite(i, &cp, &cp1, 0, 0))
			return 1;
		valid_satellites.push_back(choice(wxString::FromUTF8(cp), wxString::Format(wxT("[%hs] %hs"), cp, cp1)));
	}
	sort(valid_satellites.begin(), valid_satellites.end(), sat_cmp);
	return 0;
}

#define LABEL_WIDTH (22*TEXT_WIDTH)

BEGIN_EVENT_TABLE(QSODataDialog, wxDialog)
	EVT_COMBOBOX(-1, QSODataDialog::OnFieldChanged)
	EVT_TEXT(-1, QSODataDialog::OnFieldChanged)
	EVT_BUTTON(QD_OK, QSODataDialog::OnOk)
	EVT_BUTTON(QD_CANCEL, QSODataDialog::OnCancel)
	EVT_BUTTON(QD_HELP, QSODataDialog::OnHelp)
	EVT_BUTTON(QD_RECDOWN, QSODataDialog::OnRecDown)
	EVT_BUTTON(QD_RECUP, QSODataDialog::OnRecUp)
	EVT_BUTTON(QD_RECBOTTOM, QSODataDialog::OnRecBottom)
	EVT_BUTTON(QD_RECTOP, QSODataDialog::OnRecTop)
	EVT_BUTTON(QD_RECNEW, QSODataDialog::OnRecNew)
	EVT_BUTTON(QD_RECDELETE, QSODataDialog::OnRecDelete)
	EVT_CLOSE(QSODataDialog::OnClose)
END_EVENT_TABLE()

QSODataDialog::QSODataDialog(wxWindow *parent, wxString& filename, wxHtmlHelpController *help, QSORecordList *reclist, wxWindowID id, const wxString& title)
	: wxDialog(parent, id, title), _reclist(reclist), _isend(false), _filename(filename), _help(help) {
	tqslTrace("QSODataDialog::QSODataDialog", "parent=0x%lx, reclist=0x%lx, id=0x%lx, %s", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(reclist), reinterpret_cast<void *>(id), S(title));
	_something_changed = false;
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxFont font = GetFont();
//	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);

#define QD_MARGIN 3

	if (init_valid_lists()) {
		char err[256];
		strncpy(err, getLocalizedErrorString().ToUTF8(), sizeof err);
		throw TQSLException(err);
	}
	// Call sign
	wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, _("Call Sign:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_call_ctrl = new wxTextCtrl(this, QD_CALL, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._call));
	sizer->Add(_call_ctrl, 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Date
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, _("UTC Date (YYYY-MM-DD):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_date_ctrl = new wxTextCtrl(this, QD_DATE, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, TQSLDateValidator(&rec._date));
	sizer->Add(_date_ctrl, 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Time
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, _("UTC Time (HHMM):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_time_ctrl = new wxTextCtrl(this, QD_TIME, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, TQSLTimeValidator(&rec._time));
	sizer->Add(_time_ctrl, 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Mode
	sizer = new wxBoxSizer(wxHORIZONTAL);
	wxString *choices = valid_modes.GetChoices();
	sizer->Add(new wxStaticText(this, -1, _("Mode:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_MODE, wxDefaultPosition, wxDefaultSize,
		valid_modes.size(), choices, 0, wxGenericValidator(&_mode)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Band
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_bands.GetChoices();
	sizer->Add(new wxStaticText(this, -1, _("Band:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_band_ctrl = new wxChoice(this, QD_BAND, wxDefaultPosition, wxDefaultSize,
		valid_bands.size(), choices, 0, wxGenericValidator(&_band));
	sizer->Add(_band_ctrl, 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// RX Band
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_rxbands.GetChoices();
	sizer->Add(new wxStaticText(this, -1, _("RX Band:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_rxband_ctrl = new wxChoice(this, QD_RXBAND, wxDefaultPosition, wxDefaultSize,
		valid_rxbands.size(), choices, 0, wxGenericValidator(&_rxband));
	sizer->Add(_rxband_ctrl, 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Frequency
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, _("Frequency (MHz):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_FREQ, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._freq)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// RX Frequency
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, _("RX Frequency (MHz):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_RXFREQ, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._rxfreq)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Propagation Mode
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_propmodes.GetChoices();
	sizer->Add(new wxStaticText(this, -1, _("Propagation Mode:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_PROPMODE, wxDefaultPosition, wxDefaultSize,
		valid_propmodes.size(), choices, 0, wxGenericValidator(&_propmode)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Satellite
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_satellites.GetChoices();
	sizer->Add(new wxStaticText(this, -1, _("Satellite:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_SATELLITE, wxDefaultPosition, wxDefaultSize,
		valid_satellites.size(), choices, 0, wxGenericValidator(&_satellite)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);

	if (_reclist != 0) {
		_newrec = -1;
		if (_reclist->empty()) {
			_reclist->push_back(QSORecord());
			_newrec = 1;
		}
		topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
		_recno_label_ctrl = new wxStaticText(this, QD_RECNOLABEL, wxT(""), wxDefaultPosition,
			wxSize(20*TEXT_WIDTH, TEXT_HEIGHT), wxST_NO_AUTORESIZE|wxALIGN_CENTER);
		topsizer->Add(_recno_label_ctrl, 0, wxALIGN_CENTER|wxALL, 5);
		_recno = 1;
		sizer = new wxBoxSizer(wxHORIZONTAL);
		// Use a really tiny label font on the buttons, as the labels are there
		// for accessibility only.
		wxFont f(1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
		_recbottom_ctrl = new wxBitmapButton(this, QD_RECBOTTOM, wxBitmap(bottom_xpm));
		_recbottom_ctrl->SetLabel(_("Go to the first QSO in this log"));
		_recbottom_ctrl->SetFont(f);
		sizer->Add(_recbottom_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recdown_ctrl = new wxBitmapButton(this, QD_RECDOWN, wxBitmap(left_xpm));
		_recdown_ctrl->SetLabel(_("Go to the previous QSO in this log"));
		_recdown_ctrl->SetFont(f);
		sizer->Add(_recdown_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recno_ctrl = new wxTextCtrl(this, QD_RECNO, wxT(""), wxDefaultPosition,
			wxSize(4*TEXT_WIDTH, TEXT_HEIGHT));
		_recno_ctrl->Enable(FALSE);
		sizer->Add(_recno_ctrl, 0, wxALL, 5);
		_recup_ctrl = new wxBitmapButton(this, QD_RECUP, wxBitmap(right_xpm));
		_recup_ctrl->SetLabel(_("Go to the next QSO in this log"));
		_recup_ctrl->SetFont(f);
		sizer->Add(_recup_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_rectop_ctrl = new wxBitmapButton(this, QD_RECTOP, wxBitmap(top_xpm));
		_rectop_ctrl->SetLabel(_("Go to the last QSO in this log"));
		_rectop_ctrl->SetFont(f);
		sizer->Add(_rectop_ctrl, 0, wxTOP|wxBOTTOM, 5);
		if (_reclist->size() > 0)
			rec = *(_reclist->begin());
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
		sizer = new wxBoxSizer(wxHORIZONTAL);
		_recadd_ctrl = new wxButton(this, QD_RECNEW, _("Add QSO"));
		sizer->Add(_recadd_ctrl, 0, wxALL, 5);
		sizer->Add(new wxButton(this, QD_RECDELETE, _("Delete")), 0, wxALL, 5);
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
	}

	topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxButton(this, QD_HELP, _("Help")), 0, wxALL, 10);
	sizer->Add(new wxButton(this, QD_CANCEL, _("Cancel")), 0, wxALL, 10);
	sizer->Add(new wxButton(this, QD_OK, _("OK")), 0, wxALL, 10);
	topsizer->Add(sizer, 0, wxALIGN_CENTER);

	UpdateControls();

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);

	SetFocus();
	CentreOnParent();
	if (_reclist) rec = (*_reclist)[0];
	_recno = 1;
	TransferDataToWindow();
}

QSODataDialog::~QSODataDialog() {
}

void
QSODataDialog::OnFieldChanged(wxCommandEvent& event) {
	// If there's no record currently being added, enable
	// the "add" button

	if (_newrec < 0) {
		_recadd_ctrl->Enable(true);
		return;
	}
	_recadd_ctrl->Enable(false);
	// If we're not on the record pending add, get out
	if (_newrec != _recno)
		return;
	// If there's an error in the data, can't Add.
	if (!wxDialog::TransferDataFromWindow())
		return;
	// Update the band selections to match the frequencies
	double freq, rxfreq;
	bool freqOK = false, rxfreqOK = false;

	char *oldloc = setlocale(LC_ALL, "C");
	if (!rec._freq.IsEmpty()) {
		freqOK = rec._freq.Trim(TRUE).Trim(FALSE).ToDouble(&freq);
	}
	if (!rec._rxfreq.IsEmpty()) {
		rxfreqOK = rec._rxfreq.Trim(TRUE).Trim(FALSE).ToDouble(&rxfreq);
	}
	setlocale(LC_ALL, oldloc);
	if (freqOK) {
		freq = freq * 1000.0;		// Freq is is MHz but the limits are in KHz
		for (size_t i = 0; i < valid_bands.size(); i++) {
			if (freq >= valid_bands[i].low && freq <= valid_bands[i].high) {
				_band = i;
				_band_ctrl->SetSelection(_band);
				break;
			}
		}
	}
	if (rxfreqOK) {
		rxfreq = rxfreq * 1000.0;		// Freq is is MHz but the limits are in KHz
		for (size_t i = 0; i < valid_bands.size(); i++) {
			if (rxfreq >= valid_bands[i].low && rxfreq <= valid_bands[i].high) {
				_rxband = i+1;	// Add one for 'NONE'.
				_rxband_ctrl->SetSelection(_rxband);
				break;
			}
		}
	}

	if (_call_ctrl->GetValue().IsEmpty() || _call_ctrl->GetValue() == wxT("NONE"))	// No callsign
		return;
	if (_date_ctrl->GetValue().IsEmpty())	// No date
		return;
	if (_time_ctrl->GetValue().IsEmpty())	// No time
		return;
	// All is OK, allow save.
	_recadd_ctrl->Enable(true);
}

bool
QSODataDialog::TransferDataFromWindow() {
	tqslTrace("QSODataDialog::TransferDataFromWindow", NULL);
	rec._call.Trim(FALSE).Trim(TRUE);
	QSORecord old = rec;
	if (!wxDialog::TransferDataFromWindow())
		return false;
	if (!(rec == old))
		_something_changed = true;
	if (_mode < 0 || _mode >= static_cast<int>(valid_modes.size()))
		return false;
	rec._mode = valid_modes[_mode].value;
	if (_band < 0 || _band >= static_cast<int>(valid_bands.size()))
		return false;
	rec._band = valid_bands[_band].value;
	if (_rxband < 0) {
		rec._rxband = wxT("");
	} else {
		rec._rxband = valid_rxbands[_rxband].value;
	}
	rec._freq.Trim(FALSE).Trim(TRUE);
	rec._rxfreq.Trim(FALSE).Trim(TRUE);
	if (_propmode < 0) {
		rec._propmode = wxT("");
	} else {
		rec._propmode = valid_propmodes[_propmode].value;
	}
	if (_satellite < 0) {
		rec._satellite = wxT("");
	} else {
		rec._satellite = valid_satellites[_satellite].value;
	}

	double freq;
	// Set locale to "C" so the . is forced as a decimal point
	char *oldloc = setlocale(LC_ALL, "C");

		if (!rec._freq.IsEmpty()) {
			if (!rec._freq.ToDouble(&freq)) {
				wxMessageBox(_("QSO Frequency is invalid"), _("QSO Data Error"),
					wxOK | wxICON_EXCLAMATION, this);
				setlocale(LC_ALL, oldloc);
				return false;
			}
			freq = freq * 1000.0;		// Freq is is MHz but the limits are in KHz
			if (freq < valid_bands[_band].low || (valid_bands[_band].high > 0 && freq > valid_bands[_band].high)) {
				wxMessageBox(_("QSO Frequency is out of range for the selected band"), _("QSO Data Error"),
					wxOK | wxICON_EXCLAMATION, this);
				setlocale(LC_ALL, oldloc);
				return false;
			}
			_band_ctrl->SetSelection(_band);
		}
		if (!rec._rxfreq.IsEmpty()) {
			if (!rec._rxfreq.ToDouble(&freq)) {
				wxMessageBox(_("QSO RX Frequency is invalid"), _("QSO Data Error"),
					wxOK | wxICON_EXCLAMATION, this);
				setlocale(LC_ALL, oldloc);
				return false;
			}
			freq = freq * 1000.0;		// Freq is is MHz but the limits are in KHz
			if (freq < valid_rxbands[_rxband].low || (valid_rxbands[_rxband].high > 0 && freq > valid_rxbands[_rxband].high)) {
				wxMessageBox(_("QSO RX Frequency is out of range for the selected band"), _("QSO Data Error"),
					wxOK | wxICON_EXCLAMATION, this);
				setlocale(LC_ALL, oldloc);
				return false;
			}
			_rxband_ctrl->SetSelection(_rxband);
		}
		// No other numeric conversions, revert to original locale
		setlocale(LC_ALL, oldloc);
		if (rec._freq.IsEmpty() && rec._band.IsEmpty()) {
			wxMessageBox(_("You must select a band or enter a frequency"), _("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
		if (!_isend && rec._call.IsEmpty()) {
			wxMessageBox(_("Call Sign cannot be empty"), _("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
		if (rec._propmode == wxT("SAT") && rec._satellite.IsEmpty()) {
			wxMessageBox(_("'Satellite' propagation mode selected, so a Satellite must be chosen"), _("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
		if (rec._propmode != wxT("SAT") && !rec._satellite.IsEmpty()) {
			wxMessageBox(_("Satellite choice requires that Propagation Mode be 'Satellite'"), _("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
	if (_reclist != 0)
		(*_reclist)[_recno-1] = rec;
	return true;
}

bool
QSODataDialog::TransferDataToWindow() {
	tqslTrace("QSODataDialog::TransferDataToWindow", NULL);
	valid_list::iterator it;
	wxString mode = rec._mode.Upper();
	if ((it = find(valid_modes.begin(), valid_modes.end(), mode))  != valid_modes.end()) {
		_mode = distance(valid_modes.begin(), it);
	} else {
		wxLogWarning(_("QSO Data: Invalid Mode ignored - %s"), mode.c_str());
		_mode = 0;
	}
	if ((it = find(valid_bands.begin(), valid_bands.end(), rec._band.Upper())) != valid_bands.end()) {
		_band = distance(valid_bands.begin(), it);
		_band_ctrl->SetSelection(_band);
	}
	if ((it = find(valid_rxbands.begin(), valid_rxbands.end(), rec._rxband.Upper())) != valid_rxbands.end()) {
		_rxband = distance(valid_rxbands.begin(), it);
		_rxband_ctrl->SetSelection(_rxband);
	}
	if ((it = find(valid_propmodes.begin(), valid_propmodes.end(), rec._propmode.Upper())) != valid_propmodes.end())
		_propmode = distance(valid_propmodes.begin(), it);
	if ((it = find(valid_satellites.begin(), valid_satellites.end(), rec._satellite.Upper())) != valid_satellites.end())
		_satellite = distance(valid_satellites.begin(), it);
	return wxDialog::TransferDataToWindow();
}

bool
QSODataDialog::WriteQSOFile(QSORecordList& recs, const char *fname) {
	tqslTrace("QSODataDialog::writeQSOFile", "fname=%s", fname);
	if (recs.empty()) {
		wxLogWarning(_("No QSO records"));
		return true;
	}
	wxString s_fname;
	if (fname)
		s_fname = wxString::FromUTF8(fname);
	wxString path, basename, type;
	wxFileName::SplitPath(s_fname, &path, &basename, &type);
	if (type.IsEmpty())
		basename += wxT(".adi");
	else
		basename += wxT(".") + type;
	if (path.IsEmpty())
		path = wxConfig::Get()->Read(wxT("QSODataPath"), wxT(""));
	s_fname = wxFileSelector(_("Save File"), path, basename, wxT("adi"),
#if !defined(__APPLE__) && !defined(_WIN32)
		_("ADIF files (*.adi;*.adif;*.ADI;*.ADIF)|*.adi;*.adif;*.ADI;*.ADIF|All files (*.*)|*.*"),
#else
		_("ADIF files (*.adi;*.adif)|*.adi;*.adif|All files (*.*)|*.*"),
#endif
		wxFD_SAVE|wxFD_OVERWRITE_PROMPT, this);
	if (s_fname.IsEmpty()) { // Cancel
		return false;
	}
	wxConfig::Get()->Write(wxT("QSODataPath"), wxPathOnly(s_fname));

#ifdef _WIN32
	wchar_t* lfn = utf8_to_wchar(s_fname.ToUTF8());
	ofstream out(lfn, ios::out|ios::trunc|ios::binary);
	free_wchar(lfn);
#else
	ofstream out(s_fname.ToUTF8(), ios::out|ios::trunc|ios::binary);
#endif
	if (!out.is_open())
		return false;
	unsigned char buf[2048];
	int rec_cnt = 0;
	QSORecordList::iterator it;
	for (it = recs.begin(); it != recs.end(); it++) {
		wxString dtstr;
		if (it->_call == wxT("NONE"))		// Skipped back on added record
			continue;
		rec_cnt++;
		tqsl_adifMakeField("CALL", 0, (const unsigned char*)(const char *)it->_call.ToUTF8(), -1, buf, sizeof buf);
		out << buf << endl;
		tqsl_adifMakeField("BAND", 0, (const unsigned char*)(const char *)it->_band.ToUTF8(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		char mode[128], submode[128];
		if (tqsl_getADIFSubMode(it->_mode.ToUTF8(), mode, submode, sizeof mode) == 0) {
			tqsl_adifMakeField("MODE", 0, (const unsigned char*)mode, -1, buf, sizeof buf);
			out << "   " << buf;
			tqsl_adifMakeField("SUBMODE", 0, (const unsigned char*)submode, -1, buf, sizeof buf);
			out << buf << endl;
		} else {
			tqsl_adifMakeField("MODE", 0, (const unsigned char*)(const char *)it->_mode.ToUTF8(), -1, buf, sizeof buf);
		}
		out << "   " << buf << endl;
		dtstr.Printf(wxT("%04d%02d%02d"), it->_date.year, it->_date.month, it->_date.day);
		tqsl_adifMakeField("QSO_DATE", 0, (const unsigned char*)(const char *)dtstr.ToUTF8(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		dtstr.Printf(wxT("%02d%02d%02d"), it->_time.hour, it->_time.minute, it->_time.second);
		tqsl_adifMakeField("TIME_ON", 0, (const unsigned char*)(const char *)dtstr.ToUTF8(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		if (!it->_freq.IsEmpty()) {
			tqsl_adifMakeField("FREQ", 0, (const unsigned char*)(const char *)it->_freq.ToUTF8(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (!it->_rxband.IsEmpty()) {
			tqsl_adifMakeField("BAND_RX", 0, (const unsigned char*)(const char *)it->_rxband.ToUTF8(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (!it->_rxfreq.IsEmpty()) {
			tqsl_adifMakeField("FREQ_RX", 0, (const unsigned char*)(const char *)it->_rxfreq.ToUTF8(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (!it->_propmode.IsEmpty()) {
			tqsl_adifMakeField("PROP_MODE", 0, (const unsigned char*)(const char *)it->_propmode.ToUTF8(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (!it->_satellite.IsEmpty()) {
			tqsl_adifMakeField("SAT_NAME", 0, (const unsigned char*)(const char *)it->_satellite.ToUTF8(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		map<string, string>::iterator xit;
		for (xit = it->_extraFields.begin(); xit != it->_extraFields.end(); ++xit) {
			const char *xtraName = xit->first.c_str();
			const unsigned char *xtraVal = reinterpret_cast<const unsigned char*> (xit->second.c_str());
			if (!tqsl_adifMakeField(xtraName, 0, xtraVal, -1, buf, sizeof buf)) {
				out << "   " << buf << endl;
			}
		}
		out << "<EOR>" << endl;
	}
	out.close();
	wxLogMessage(_("Wrote %d QSO records to %s"), rec_cnt, s_fname.c_str());
	return true;
}

void
QSODataDialog::OnOk(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnOk", NULL);
	if (!Validate())
		return;
	_isend = true;
	TransferDataFromWindow();
	_isend = false;
	if (rec._call.IsEmpty() && _recno == static_cast<int>(_reclist->size())) {
		_reclist->erase(_reclist->begin() + _recno - 1);
		if (WriteQSOFile(*_reclist, _filename.ToUTF8()))
			EndModal(wxID_OK);
	} else if (Validate() && TransferDataFromWindow()) {
		if (WriteQSOFile(*_reclist, _filename.ToUTF8()))
			EndModal(wxID_OK);
	}
}

void
QSODataDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnCancel", NULL);
	EndModal(wxID_CANCEL);
}

void
QSODataDialog::OnHelp(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnHelp", NULL);
	if (_help)
		_help->Display(wxT("qsodata.htm"));
}

void
QSODataDialog::OnClose(wxCloseEvent& event) {
	tqslTrace("QSODataDialog::OnClose", NULL);
	_isend = true;
	TransferDataFromWindow();
	_isend = false;
	if (!_something_changed) {
		EndModal(wxID_CANCEL);
		return;				// Nothing to save
	}

	if (rec._call.IsEmpty() && _recno == static_cast<int>(_reclist->size())) {
		_reclist->erase(_reclist->begin() + _recno - 1);
	}
	if (_reclist->empty()) {
		EndModal(wxID_CANCEL);
		return;				// Nothing to save
	}
	if (wxMessageBox(_("The file has not been saved. Should the QSOs be saved?"), _("Confirm Close"), wxICON_QUESTION | wxYES_NO) == wxNO) {
		EndModal(wxID_CANCEL);
		return;				// Nothing to save
	}
	WriteQSOFile(*_reclist, _filename.ToUTF8());
	EndModal(wxID_OK);
	return;
}

void
QSODataDialog::SetRecno(int new_recno) {
	tqslTrace("QSODataDialog::SetRecno", "new_recno=%d", new_recno);
	if (_reclist == NULL || new_recno < 1)
		return;
	if (Validate() && TransferDataFromWindow()) {
//   		(*_reclist)[_recno-1] = rec;
		if (_reclist && new_recno > static_cast<int>(_reclist->size())) {
			_newrec = _reclist->size() + 1;
			QSORecord newrec;
			// Copy QSO fields from current record
			if (_recno > 0) {
				newrec = (*_reclist)[_recno-1];
				newrec._call = wxT("");
			}
			_reclist->push_back(newrec);
		}
		_recno = new_recno;
		if (_reclist) rec = (*_reclist)[_recno-1];
		TransferDataToWindow();
		UpdateControls();
		_call_ctrl->SetFocus();
	}
}

void
QSODataDialog::OnRecDown(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecDown", NULL);
	if (_reclist == 0)
		return;
	if (_recno == _newrec) { 		// Backing up from a record being added
		if (rec._call.IsEmpty()) { 	// And the call is empty
			rec._call = wxT("NONE");
			_call_ctrl->SetValue(wxT("NONE"));
		}
	}
	if (_recno > 1)
		SetRecno(_recno - 1);
}

void
QSODataDialog::OnRecUp(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecUp", NULL);
	SetRecno(_recno + 1);
}

void
QSODataDialog::OnRecBottom(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecBottom", NULL);
	if (_reclist == 0)
		return;
	if (_recno == _newrec) { 		// Backing up from a record being added
		if (rec._call.IsEmpty()) { 	// And the call is empty
			rec._call = wxT("NONE");
			_call_ctrl->SetValue(wxT("NONE"));
			_reclist->erase(_reclist->begin() + _recno - 1);
		}
	}
	SetRecno(1);
}

void
QSODataDialog::OnRecTop(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecTop", NULL);
	if (_reclist == 0)
		return;
	if (_recno == _newrec) { 		// Backing up from a record being added
		if (rec._call.IsEmpty()) { 	// And the call is empty
			_reclist->erase(_reclist->begin() + _recno - 1);
			if (_reclist->empty())
				_reclist->push_back(QSORecord());
			if (_recno > static_cast<int>(_reclist->size()))
				_recno = _reclist->size();
			rec = (*_reclist)[_recno-1];
			TransferDataToWindow();
		}
		_recadd_ctrl->Enable(true);
	}
	SetRecno(_reclist->size());
}

void
QSODataDialog::OnRecNew(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecNew", NULL);
	if (_reclist == 0)
		return;
	SetRecno(_reclist->size()+1);
}

void
QSODataDialog::OnRecDelete(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecDelete", NULL);
	if (_reclist == 0)
		return;
	_reclist->erase(_reclist->begin() + _recno - 1);
	if (_reclist->empty())
		_reclist->push_back(QSORecord());
	if (_recno > static_cast<int>(_reclist->size()))
		_recno = _reclist->size();
	rec = (*_reclist)[_recno-1];
	TransferDataToWindow();
	UpdateControls();
}

void
QSODataDialog::UpdateControls() {
	tqslTrace("QSODataDialog::UpdateControls", NULL);
	if (_reclist == 0)
		return;
	_recdown_ctrl->Enable(_recno > 1);
	_recbottom_ctrl->Enable(_recno > 1);
	_recup_ctrl->Enable(_recno < static_cast<int>(_reclist->size()));
	_rectop_ctrl->Enable(_recno < static_cast<int>(_reclist->size()));
	_recno_ctrl->SetValue(wxString::Format(wxT("%d"), _recno));
	if (_reclist->size() == 1) {
		_recno_label_ctrl->SetLabel(_("One QSO Record"));
	} else {
		_recno_label_ctrl->SetLabel(wxString::Format(_("%d QSO Records"), static_cast<int>(_reclist->size())));
	}
	_recadd_ctrl->Enable(_newrec < 0);
}
