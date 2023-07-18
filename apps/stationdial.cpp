/***************************************************************************
                          stationdial.cpp  -  description
                             -------------------
    begin                : Mon Nov 11 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: stationdial.cpp,v 1.6 2013/03/01 13:00:59 k1mu Exp $
 ***************************************************************************/

#define TQSL_ID_LOW 6000

#include "stationdial.h"
#include <wx/listctrl.h>
#include <algorithm>
#include <iostream>
#include <map>
#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif
#include "tqslwiz.h"
#include "tqslexcept.h"
#include "tqsltrace.h"
#include "tqsllib.h"
#include "wxutil.h"

using std::map;

#define GS_NAMELIST TQSL_ID_LOW
#define GS_OKBUT TQSL_ID_LOW+1
#define GS_CANCELBUT TQSL_ID_LOW+2
#define GS_NAMEENTRY TQSL_ID_LOW+3
#define GS_DELETEBUT TQSL_ID_LOW+4
#define GS_NEWBUT TQSL_ID_LOW+5
#define GS_MODIFYBUT TQSL_ID_LOW+6
#define GS_CMD_PROPERTIES TQSL_ID_LOW+7
#define GS_HELPBUT TQSL_ID_LOW+8

class TQSLStationListBox : public wxListBox {
 public:
	TQSLStationListBox(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL,
		long style = 0) : wxListBox(parent, id, pos, size, n, choices, style) {}
	void OnRightDown(wxMouseEvent& event);

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TQSLStationListBox, wxListBox)
	EVT_RIGHT_DOWN(TQSLStationListBox::OnRightDown)
END_EVENT_TABLE()

void
TQSLStationListBox::OnRightDown(wxMouseEvent& event) {
	wxMenu menu;
	menu.Append(GS_CMD_PROPERTIES, _("&Properties"));
	PopupMenu(&menu, event.GetPosition());
}

class PropList : public wxDialog {
 public:
	explicit PropList(wxWindow *parent);
	wxListCtrl *list;
};

PropList::PropList(wxWindow *parent) : wxDialog(parent, -1, _("Properties"), wxDefaultPosition,
	wxSize(400, 300)) {
	tqslTrace("PropList::PropList", "parent=0x%lx", reinterpret_cast<void *>(parent));
	list = new wxListCtrl(this, -1, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_SINGLE_SEL);
	list->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 100);
	list->InsertColumn(1, _("Value"), wxLIST_FORMAT_LEFT, 300);
	wxLayoutConstraints *c = new wxLayoutConstraints;
	c->top.SameAs(this, wxTop);
	c->left.SameAs(this, wxLeft);
	c->right.SameAs(this, wxRight);
	c->height.PercentOf(this, wxHeight, 66);
	list->SetConstraints(c);
	CenterOnParent();
}

static void
check_tqsl_error(int rval) {
	if (rval == 0)
		return;
	wxString serr = getLocalizedErrorString();
	wxLogError(serr);
}

static bool
itemLess(const item& s1, const item& s2) {
	int i = s1.call.CmpNoCase(s2.call);
	if (i == 0)
		i = s1.name.CmpNoCase(s2.name);
	return (i < 0);
}

BEGIN_EVENT_TABLE(TQSLGetStationNameDialog, wxDialog)
	EVT_BUTTON(GS_OKBUT, TQSLGetStationNameDialog::OnOk)
	EVT_BUTTON(GS_CANCELBUT, TQSLGetStationNameDialog::OnCancel)
	EVT_BUTTON(GS_DELETEBUT, TQSLGetStationNameDialog::OnDelete)
	EVT_BUTTON(GS_HELPBUT, TQSLGetStationNameDialog::OnHelp)
	EVT_BUTTON(GS_NEWBUT, TQSLGetStationNameDialog::OnNew)
	EVT_BUTTON(GS_MODIFYBUT, TQSLGetStationNameDialog::OnModify)
	EVT_LISTBOX(GS_NAMELIST, TQSLGetStationNameDialog::OnNamelist)
	EVT_LISTBOX_DCLICK(GS_NAMELIST, TQSLGetStationNameDialog::OnDblClick)
	EVT_TEXT(GS_NAMEENTRY, TQSLGetStationNameDialog::OnNameChange)
	EVT_MENU(GS_CMD_PROPERTIES, TQSLGetStationNameDialog::DisplayProperties)
	EVT_SET_FOCUS(TQSLGetStationNameDialog::OnSetFocus)
END_EVENT_TABLE()

void
TQSLGetStationNameDialog::OnSetFocus(wxFocusEvent& event) {
	if (!firstFocused) {
		firstFocused = true;
		if (issave) {
			if (name_entry)
				name_entry->SetFocus();
		} else if (namelist) {
			namelist->SetFocus();
		}
	}
}

void
TQSLGetStationNameDialog::OnOk(wxCommandEvent&) {
	wxString s = name_entry->GetValue().Trim().Trim(false);
	tqslTrace("TQSLGetStationNameDialog::OnOk", "selected = %s", S(s));
	if (editonly) {
		EndModal(wxID_CANCEL);
	} else if (!s.IsEmpty()) {
		_selected = s;
		EndModal(wxID_OK);
	}
}

void
TQSLGetStationNameDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnCancel", NULL);
	EndModal(wxID_CANCEL);
}

// Reset the list of Station Locations in the listbox
void
TQSLGetStationNameDialog::RefreshList() {
	tqslTrace("TQSLGetStationNameDialog::RefreshList", NULL);
	namelist->Clear();
	item_data.clear();
	int n;
	tQSL_Location loc;
	check_tqsl_error(tqsl_initStationLocationCapture(&loc));
	check_tqsl_error(tqsl_getNumStationLocations(loc, &n));
	for (int i = 0; i < n && i < 2000; i++) {
		item it;
		char buf[256];
		check_tqsl_error(tqsl_getStationLocationName(loc, i, buf, sizeof buf));
		it.name = wxString::FromUTF8(buf);
		char cbuf[256];
		check_tqsl_error(tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf));
		it.call = wxString::FromUTF8(cbuf);
		it.label = it.call + wxT(" - ") + it.name;
		item_data.push_back(it);
	}
	sort(item_data.begin(), item_data.end(), itemLess);
	for (int i = 0; i < static_cast<int>(item_data.size()); i++)
		namelist->Append(item_data[i].label, &(item_data[i].name));
	check_tqsl_error(tqsl_endStationLocationCapture(&loc));
}

TQSLGetStationNameDialog::TQSLGetStationNameDialog(wxWindow *parent, wxHtmlHelpController *help, const wxPoint& pos,
	bool i_issave, const wxString& title, const wxString& okLabel, bool i_editonly)
	: wxDialog(parent, -1, _("Select Station Data"), pos), issave(i_issave), editonly(i_editonly),
	newbut(0), modbut(0), updating(false), firstFocused(false), _help(help) {
	tqslTrace("TQSLGetStationNameDialog::TQSLGetStationNameDialog", "parent=0x%lx, i_issave=%d, title=%s, okLabel=%s, i_editonly=%d", parent, i_issave, S(title), S(okLabel), i_editonly);
	wxBoxSizer *topsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*30;

	if (!title.IsEmpty())
		SetTitle(title);
	else if (issave)
		SetTitle(_("Save Station Data"));
	// List
	namelist = new TQSLStationListBox(this, GS_NAMELIST, wxDefaultPosition,
		wxSize(control_width, text_size.GetHeight()*10),
		0, 0, wxLB_MULTIPLE|wxLB_HSCROLL|wxLB_ALWAYS_SB);
	sizer->Add(namelist, 1, wxALL|wxEXPAND, 10);
	RefreshList();
	sizer->Add(new wxStaticText(this, -1,
		issave ? _("Enter a name for this Station Location") : _("Selected Station Location")),
		0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	name_entry = new wxTextCtrl(this, GS_NAMEENTRY, wxT(""), wxDefaultPosition,
		wxSize(control_width, -1));
	if (!issave)
		name_entry->Enable(false);
	sizer->Add(name_entry, 0, wxALL|wxEXPAND, 10);
	topsizer->Add(sizer, 1, 0, 0);
	wxBoxSizer *button_sizer = new wxBoxSizer(wxVERTICAL);
	if (!issave) {
		newbut = new wxButton(this, GS_NEWBUT, _("New..."));
		newbut->Enable(TRUE);
		button_sizer->Add(newbut, 0, wxALL|wxALIGN_TOP, 3);
		modbut = new wxButton(this, GS_MODIFYBUT, _("Edit..."));
		modbut->Enable(FALSE);
		button_sizer->Add(modbut, 0, wxALL|wxALIGN_TOP, 3);
	}
	delbut = new wxButton(this, GS_DELETEBUT, _("Delete"));
	delbut->Enable(FALSE);
	button_sizer->Add(delbut, 0, wxALL|wxALIGN_TOP, 3);
	button_sizer->Add(new wxStaticText(this, -1, wxT("")), 1, wxEXPAND);
	if (_help)
		button_sizer->Add(new wxButton(this, GS_HELPBUT, _("Help") ), 0, wxALL, 3);
	if (!editonly)
		button_sizer->Add(new wxButton(this, GS_CANCELBUT, _("Cancel") ), 0, wxALL, 3);
	okbut = new wxButton(this, GS_OKBUT, okLabel);
	button_sizer->Add(okbut, 0, wxALL, 3);
	topsizer->Add(button_sizer, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 7);
	hack = (namelist->GetCount() > 0) ? true : false;

	UpdateControls();
	SetAutoLayout(TRUE);
	SetSizer(topsizer);

	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CentreOnParent();
}

void
TQSLGetStationNameDialog::UpdateButtons() {
	tqslTrace("TQSLGetStationNameDialog::UpdateButtons", NULL);
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	delbut->Enable(newsels.GetCount() > 0);
	if (modbut)
		modbut->Enable(newsels.GetCount() > 0);
	wxArrayInt sels;
	namelist->GetSelections(sels);
	if (!editonly)
		okbut->Enable(issave ? (!name_entry->GetValue().Trim().IsEmpty()) : (sels.GetCount() > 0));
}

void
TQSLGetStationNameDialog::UpdateControls() {
	tqslTrace("TQSLGetStationNameDialog::UpdateControls", NULL);
	if (updating)	// Sentinel to prevent recursion
		return;
	updating = true;
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int newsel = -1;
	for (int i = 0; i < static_cast<int>(newsels.GetCount()); i++) {
		if (sels.Index(newsels[i]) == wxNOT_FOUND) {
			newsel = newsels[i];
			break;
		}
	}
//cout << "newsel: " << newsel << endl;
	if (newsel > -1) {
		for (int i = 0; i < static_cast<int>(newsels.GetCount()); i++) {
			if (newsels[i] != newsel)
				namelist->Deselect(newsels[i]);
		}
	}
	namelist->GetSelections(sels);
	int idx = (sels.GetCount() > 0) ? sels[0] : -1;
//cout << "UpdateControls selection: " << idx << endl;
	if (idx >= 0)
		name_entry->SetValue((idx < 0) ? wxT("") : *reinterpret_cast<wxString *>(namelist->GetClientData(idx)));
	UpdateButtons();
//cout << "UpdateControls selection(1): " << idx << endl;
	updating = false;
}

void
TQSLGetStationNameDialog::OnDelete(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnDelete", NULL);
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int idx = (newsels.GetCount() > 0) ? newsels[0] : -1;
	if (idx < 0)
		return;
	wxString name = *reinterpret_cast<wxString *>(namelist->GetClientData(idx));
	if (name == wxT(""))
		return;
	if (wxMessageBox(wxString(_("Delete \"")) + name + wxT("\"?"), _("TQSL Confirm"), wxYES_NO | wxICON_QUESTION | wxCENTRE, this) == wxYES) {
		check_tqsl_error(tqsl_deleteStationLocation(name.ToUTF8()));
		if (!issave)
			name_entry->Clear();
		RefreshList();
		UpdateControls();
	}
}

void
TQSLGetStationNameDialog::OnNamelist(wxCommandEvent& event) {
	tqslTrace("TQSLGetStationNameDialog::OnNamelist", NULL);
/*	if (hack) {		// We seem to get an extraneous start-up event (GTK only?)
cout << "OnNamelist hack" << endl;
		hack = false;
		if (want_selected < 0)
			namelist->Deselect(0);
		else
			namelist->SetSelection(want_selected);
	}
*/
//cout << "OnNamelist" << endl;
	UpdateControls();
}

void
TQSLGetStationNameDialog::OnDblClick(wxCommandEvent& event) {
	tqslTrace("TQSLGetStationNameDialog::OnDblClick", NULL);
	if(editonly) {
		OnNamelist(event);
		EndModal(wxID_MORE);
	} else {
		UpdateControls();
		OnOk(event);
	} //updatecontrols sets up the text field that ok checks
}

void
TQSLGetStationNameDialog::OnNew(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnNew", NULL);
	EndModal(wxID_APPLY);
}

void
TQSLGetStationNameDialog::OnModify(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnModify", NULL);
	EndModal(wxID_MORE);
}

void
TQSLGetStationNameDialog::OnHelp(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnHelp", NULL);
	if (_help)
		_help->Display(wxT("stnloc.htm"));
}

void
TQSLGetStationNameDialog::OnNameChange(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::OnNameChange", NULL);
	UpdateButtons();
}

// Location fields, here for translation purposes
#ifdef tqsltranslate
static const char* labels[] = {
	__("State"),
	__("Call Sign"),
	__("Province"),
	__("Continent"),
	__("CQ Zone"),
	__("DXCC Entity"),
	__("Grid Square"),
	__("IOTA ID"),
	__("ITU Zone"),
	__("Oblast"),
	__("County"),
	__("State"),
	__("WPX Prefix")
}
#endif

void
TQSLGetStationNameDialog::DisplayProperties(wxCommandEvent&) {
	tqslTrace("TQSLGetStationNameDialog::DisplayProperties", NULL);
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int idx = (newsels.GetCount() > 0) ? newsels[0] : -1;
	if (idx < 0)
		return;
	wxString name = *reinterpret_cast<wxString *>(namelist->GetClientData(idx));
	if (name == wxT(""))
		return;
	tQSL_Location loc;
	try {
		map<wxString, wxString> props;
		check_tqsl_error(tqsl_getStationLocation(&loc, name.ToUTF8()));
		do {
			int nfield;
			check_tqsl_error(tqsl_getNumLocationField(loc, &nfield));
			for (int i = 0; i < nfield; i++) {
				char buf[256];
				check_tqsl_error(tqsl_getLocationFieldDataLabel(loc, i, buf, sizeof buf));
				wxString key = wxGetTranslation(wxString::FromUTF8(buf));
				int type;
				check_tqsl_error(tqsl_getLocationFieldDataType(loc, i, &type));
				if (type == TQSL_LOCATION_FIELD_DDLIST || type == TQSL_LOCATION_FIELD_LIST) {
					int sel;
					check_tqsl_error(tqsl_getLocationFieldIndex(loc, i, &sel));
					check_tqsl_error(tqsl_getLocationFieldListItem(loc, i, sel, buf, sizeof buf));
				} else {
					check_tqsl_error(tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf));
				}
				props[key] = wxString::FromUTF8(buf);
			}
			int rval;
			if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
				break;
			check_tqsl_error(tqsl_nextStationLocationCapture(loc));
		} while (1);
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
		PropList plist(this);
		int i = 0;
		for (map<wxString, wxString>::iterator it = props.begin(); it != props.end(); it++) {
			plist.list->InsertItem(i, it->first);
			plist.list->SetItem(i, 1, it->second);
			i++;
//cout << idx << ", " << it->first << " => " << it->second << endl;
		}
		plist.ShowModal();
	}
	catch(TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
TQSLGetStationNameDialog::SelectName(const wxString& name) {
	tqslTrace("TQSLGetStationNameDialog::SelectName", "name=%s", S(name));
	wxArrayInt sels;
	namelist->GetSelections(sels);
	for (int i = 0; i < static_cast<int>(sels.GetCount()); i++)
		namelist->Deselect(sels[i]);
	for (int i = 0; i < static_cast<int>(namelist->GetCount()); i++) {
		if (name == *reinterpret_cast<wxString *>(namelist->GetClientData(i))) {
			namelist->SetSelection(i, TRUE);
			break;
		}
	}
	namelist->GetSelections(sels);
	UpdateControls();
}

int
TQSLGetStationNameDialog::ShowModal() {
	tqslTrace("TQSLGetStationNameDialog::ShowModal", NULL);
	if (namelist->GetCount() == 0 && !issave) {
		wxString msg = _("You have no Station Locations defined.");
			msg += wxT("\n\n");
			msg += _("You must define at least one Station Location to use for signing.");
			msg += wxT("\n");
			msg += _("Use the \"New\" Button of the dialog you're about to see to define a Station Location.");
		wxMessageBox(msg, _("TQSL Warning"), wxOK | wxICON_WARNING, this);
	}
	return wxDialog::ShowModal();
}
