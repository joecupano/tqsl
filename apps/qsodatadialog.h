/***************************************************************************
                          qsodatadialog.h  -  description
                             -------------------
    begin                : Sat Dec 7 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef QSODATADIALOG_H
#define QSODATADIALOG_H

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "tqsllib.h"
#include <wx/bmpbuttn.h>
#include <wx/textctrl.h>
#include <wx/wxhtml.h>

#include <vector>
#include <map>
#include <string>

using std::vector;
using std::map;
using std::string;

/** Display and edit QSO data
  *
  *@author Jon Bloom
  */

class QSORecord {
 public:
	QSORecord() {
		_call = _freq = _rxfreq = _rxband = _propmode = _satellite = wxT("");
		_mode = wxT("CW");
		_band = wxT("160M");
		tqsl_initDate(&_date, NULL);
		tqsl_initTime(&_time, NULL);
		_extraFields.clear();
	}

	wxString _call, _freq, _rxfreq;
	wxString _mode, _band, _rxband;
	wxString _propmode, _satellite;
	tQSL_Date _date;
	tQSL_Time _time;
	map <string, string> _extraFields;
	bool operator == (const QSORecord other) {
		if (_call == other._call && _freq == other._freq &&
		    _rxfreq == other._rxfreq && _mode == other._mode &&
		    _band == other._band && _rxband == other._rxband &&
		    _propmode == other._propmode &&
		    _date.year == other._date.year &&
		    _date.month == other._date.month &&
		    _date.day == other._date.day &&
		    _time.hour == other._time.hour &&
		    _time.minute == other._time.minute &&
		    _time.second == other._time.second) {
			return true;
		}
		return false;
	}
};

typedef vector<QSORecord> QSORecordList;

class QSODataDialog : public wxDialog  {
 public:
	QSODataDialog(wxWindow *parent, wxString &filename, wxHtmlHelpController *help, QSORecordList *reclist = 0,
		wxWindowID id = -1, const wxString& title = _("QSO Data"));
	~QSODataDialog();
	wxString GetMode() const;
	bool SetMode(const wxString&);
	bool WriteQSOFile(QSORecordList& recs, const char *fname);
	QSORecord rec;

	virtual bool TransferDataToWindow();
	virtual bool TransferDataFromWindow();

 private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnHelp(wxCommandEvent&);
	void OnRecUp(wxCommandEvent&);
	void OnRecDown(wxCommandEvent&);
	void OnRecBottom(wxCommandEvent&);
	void OnRecTop(wxCommandEvent&);
	void OnRecNew(wxCommandEvent&);
	void OnRecDelete(wxCommandEvent&);
	void OnFieldChanged(wxCommandEvent&);
	void OnClose(wxCloseEvent&);
	void SetRecno(int recno);
	void UpdateControls();

	wxTextCtrl *_recno_ctrl;
	wxTextCtrl *_call_ctrl;
	wxTextCtrl *_date_ctrl;
	wxTextCtrl *_time_ctrl;
	wxChoice *_band_ctrl;
	wxChoice *_rxband_ctrl;
	wxStaticText *_recno_label_ctrl;
	wxBitmapButton *_recdown_ctrl, *_recup_ctrl, *_recbottom_ctrl, *_rectop_ctrl;
	wxButton *_recadd_ctrl;
	QSORecordList *_reclist;
	int _mode, _band, _rxband, _propmode, _satellite;
	int _recno;
	DECLARE_EVENT_TABLE()
	bool _isend;
	wxString _filename;
	wxHtmlHelpController *_help;
	int _newrec;
	bool _something_changed;
};

#endif
