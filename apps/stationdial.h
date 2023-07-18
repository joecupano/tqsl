/***************************************************************************
                          stationdial.h  -  description
                             -------------------
    begin                : Mon Nov 11 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __stationdial_h
#define __stationdial_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "wx/wxhtml.h"

#include <vector>

using std::vector;

class item {
 public:
	wxString name;
	wxString call;
	wxString label;
};

class TQSLStationListBox;

class TQSLGetStationNameDialog : public wxDialog {
 public:
	TQSLGetStationNameDialog(wxWindow *parent, wxHtmlHelpController *help = 0, const wxPoint& pos = wxDefaultPosition,
		bool i_issave = false, const wxString& title = wxT(""), const wxString& okLabel = _("OK"), bool i_editonly = false);
	void OnOk(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnDelete(wxCommandEvent& event);
	void OnHelp(wxCommandEvent& event);
	void OnNew(wxCommandEvent& event);
	void OnModify(wxCommandEvent& event);
	void OnNamelist(wxCommandEvent& event);
	void OnNameChange(wxCommandEvent& event);
	void OnDblClick(wxCommandEvent& event);
	wxString Selected() { return name_entry->GetValue().Trim(); }
	void DisplayProperties(wxCommandEvent& event);
	void SelectName(const wxString& name);
	virtual int ShowModal();

 protected:
	void UpdateButtons();
	void UpdateControls();
	void RefreshList();
	void OnSetFocus(wxFocusEvent& event);
	TQSLStationListBox *namelist;
	wxString _station_data_name;
	vector<item> item_data;
	wxString _selected;
	bool issave, editonly;
	wxTextCtrl *name_entry;
	wxButton *okbut, *delbut, *newbut, *modbut;
	bool hack;
	int want_selected;
	bool updating;
	wxArrayInt sels;
	bool firstFocused;
	wxHtmlHelpController *_help;

	DECLARE_EVENT_TABLE()
};

#endif	// __stationdial_h
