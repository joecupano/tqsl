/***************************************************************************
                          tqslwiz.cpp  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqslwiz.cpp,v 1.6 2013/03/01 13:12:57 k1mu Exp $
 ***************************************************************************/

#include "tqslwiz.h"
#include <wx/tokenzr.h>
#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <string>
using std::string;

#include "wxutil.h"
#include "tqsltrace.h"
#include "winstrdefs.h"

extern int get_address_field(const char *callsign, const char *field, string& result);

#define TQSL_LOCATION_FIELD_UPPER	1
#define TQSL_LOCATION_FIELD_MUSTSEL	2
#define TQSL_LOCATION_FIELD_SELNXT	4

BEGIN_EVENT_TABLE(TQSLWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, TQSLWizard::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TQSLWizLocPage, TQSLWizPage)
	EVT_COMBOBOX(-1, TQSLWizLocPage::OnComboBoxEvent)
	EVT_CHECKBOX(-1, TQSLWizLocPage::OnCheckBoxEvent)
	EVT_WIZARD_PAGE_CHANGING(wxID_ANY, TQSLWizLocPage::OnPageChanging)
	EVT_TEXT(wxID_ANY, TQSLWizLocPage::OnTextEvent)
#if wxMAJOR_VERSION < 3 && (wxMAJOR_VERSION != 2 && wxMINOR_VERSION != 9)
	EVT_SIZE(TQSLWizLocPage::OnSize)
#endif
END_EVENT_TABLE()

static char callsign[TQSL_CALLSIGN_MAX+1];

// Returns true if the gabbi name is a Primary Administrative Subdivision
static bool
isPAS(const char *gabbi) {
	if (!strcmp(gabbi, "US_STATE") ||
	    !strcmp(gabbi, "CA_PROVINCE") ||
	    !strcmp(gabbi, "RU_OBLAST") ||
	    !strcmp(gabbi, "CN_PROVINCE") ||
	    !strcmp(gabbi, "AU_STATE") ||
	    !strcmp(gabbi, "JA_PREFECTURE") ||
	    !strcmp(gabbi, "FI_KUNTA"))
		return true;
	return false;
}

// Return true if it's a secondary (county)
static bool
isSAS(const char *gabbi) {
	if (!strcmp(gabbi, "US_COUNTY") ||
	    !strcmp(gabbi, "JA_CITY_GUN_KU"))
		return true;
	return false;
}

// Return true if it's a park
static bool
isPark(const char *gabbi) {
	if (!strcmp(gabbi, "US_PARK") ||
	    !strcmp(gabbi, "DX_US_PARK") ||
	    !strcmp(gabbi, "CA_US_PARK"))
		return true;
	return false;
}

void
TQSLWizard::OnPageChanged(wxWizardEvent& ev) {
	tqslTrace("TQSLWizard::OnPageChanged", NULL);
	(reinterpret_cast<TQSLWizPage *>(GetCurrentPage()))->SetFocus();
	ExtWizard::OnPageChanged(ev);
}

TQSLWizard::TQSLWizard(tQSL_Location locp, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title, bool expired, bool _editing)
	: ExtWizard(parent, help, title), loc(locp), _curpage(-1) {
	tqslTrace("TQSLWizard::TQSLWizard", "locp=0x%lx, parent=0x%lx, title=%s, expired=%d, editing=%d", reinterpret_cast<void *>(locp), reinterpret_cast<void *>(parent), S(title), expired, _editing);

	callsign[0] = '\0';
	char buf[256];
	editing = _editing;
	if (!tqsl_getStationLocationCaptureName(locp, buf, sizeof buf)) {
		wxString s = wxString::FromUTF8(buf);
		SetLocationName(s);
	}
	tqsl_setStationLocationCertFlags(locp, expired ? TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED : TQSL_SELECT_CERT_WITHKEYS);
	tqsl_setStationLocationCapturePage(locp, 1);
}

TQSLWizPage *
TQSLWizard::GetPage(bool final) {
	tqslTrace("TQSLWizard::GetPage", "final=%d", final);
	int page_num;
	if (final)
		page_num = 0;
	else if (tqsl_getStationLocationCapturePage(loc, &page_num))
		return 0;
	tqslTrace("TQSLWizard::GetPage", "page_num = %d", page_num);
	if (_pages[page_num]) {
		if (page_num == 0)
			(reinterpret_cast<TQSLWizFinalPage *>(_pages[0]))->prev = GetCurrentTQSLPage();
		return _pages[page_num];
	}
	if (page_num == 0)
		_pages[page_num] = new TQSLWizFinalPage(this, loc, GetCurrentTQSLPage());
	else
		_pages[page_num] = new TQSLWizLocPage(this, loc);
	_curpage = page_num;
	if (page_num == 0)
		(reinterpret_cast<TQSLWizFinalPage *>(_pages[0]))->prev = GetCurrentTQSLPage();
	return _pages[page_num];
}

void TQSLWizLocPage::OnSize(wxSizeEvent& ev) {
#if wxMAJOR_VERSION < 3 && (wxMAJOR_VERSION != 2 && wxMINOR_VERSION != 9)
	TQSLWizPage::OnSize(ev);
#endif
	UpdateFields();
}

TQSLWizPage *
TQSLWizLocPage::GetPrev() const {
	tqslTrace("TQSLWizLocPage::GetPrev", NULL);
	int rval;

	tqsl_setStationLocationCapturePage(loc, loc_page);
	if (tqsl_hasPrevStationLocationCapture(loc, &rval) || !rval) {
		return 0;
	}
	tqsl_prevStationLocationCapture(loc);
	return GetParent()->GetPage();
}

TQSLWizPage *
TQSLWizLocPage::GetNext() const {
	tqslTrace("TQSLWizLocPage::GetNext", NULL);
	TQSLWizPage *newp;
	bool final = true;

	tqsl_setStationLocationCapturePage(loc, loc_page);

	newp = GetParent()->GetPage(final);
	return newp;
}

void
TQSLWizLocPage::UpdateFields(int noupdate_field) {
	tqslTrace("TQSLWizLocPage::UpdateFields", "noupdate_field=%d", noupdate_field);
	wxSize text_size = getTextSize(this);

	if (noupdate_field >= 0) {
		tqsl_updateStationLocationCapture(loc);
		errlbl->SetLabel(wxString::FromUTF8(tQSL_CustomError));
	}

	int cur_page;
	if (tqsl_getCurrentStationLocationCapturePage(loc, &cur_page)) cur_page = 1;

        tqsl_setStationLocationCapturePage(loc, 1);		// Always start with page 1
	tqslTrace("TQSLWizLocPage::UpdateFields", "Current page = %d, next_page=%d", cur_page, second_page);

	validate();

	tqslTrace("TQSLWizLocPage::UpdateFields", "Validation done");
	for (int i = noupdate_field+1; i < static_cast<int>(p1_controls.size()-1); i++) {
		int changed;
		int in_type;
		char gabbi_name[40];

		wxOwnerDrawnComboBox* cb = reinterpret_cast<wxOwnerDrawnComboBox *>(p1_controls[i]);
		wxTextCtrl* tx = reinterpret_cast<wxTextCtrl *>(p1_controls[i]);
		wxStaticText* st = reinterpret_cast<wxStaticText *>(p1_controls[i]);
		tqsl_getLocationFieldChanged(loc, i, &changed);
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);

		/*
		 * Code below is used to revert fields that have had defaults set based on callsign
		 */
		ForcedMap::iterator it;
		it = forced.find(gabbi_name);
		if (it != forced.end()) {		// Something set
			if (it->second == "") {
				forced.erase(it);
			} else if (it->second != callsign) {	// For a different call
				if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
					tqsl_setLocationFieldIndex(loc, i, 0);
					cb->SetSelection(wxNOT_FOUND);
				} else {
					tqsl_setLocationFieldCharData(loc, i, "");
					tx->ChangeValue(wxT(""));
				}
				forced.erase(it);
			}
		}

		char buf[256];
		string s;
		tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);

		// Has this been set?
		bool wasUserSet = (userSet[gabbi_name] == gabbi_name);
		if (!wasUserSet && !parent->editing && strlen(buf) == 0) { // Empty, so set to default
			if (strcmp(gabbi_name, "GRIDSQUARE") == 0) {
				if (get_address_field(callsign, "grid", s) == 0) {	// Got something
					tqsl_setLocationFieldCharData(loc, i, s.c_str());
					 tx->ChangeValue(wxString::FromUTF8(s.c_str()));
					forced[gabbi_name] = callsign;
				}
			}
			if (strcmp(gabbi_name, "ITUZ") == 0) {
				if (get_address_field(callsign, "ituzone", s) == 0) {
					tqsl_setLocationFieldCharData(loc, i, s.c_str());
					int new_sel;
					tqsl_getLocationFieldIndex(loc, i, &new_sel);
					if (new_sel >= 0 && new_sel < static_cast<int>(cb->GetCount()))
						cb->SetSelection(new_sel);
					if (strlen(callsign) != 0) {
						forced[gabbi_name] = callsign;
					}
					UpdateFields(i);
				}
			}
			if (strcmp(gabbi_name, "CQZ") == 0) {
				if (get_address_field(callsign, "cqzone", s) == 0) {
					tqsl_setLocationFieldCharData(loc, i, s.c_str());
					int new_sel;
					tqsl_getLocationFieldIndex(loc, i, &new_sel);
					if (new_sel >= 0 && new_sel < static_cast<int>(cb->GetCount()))
						cb->SetSelection(new_sel);
					if (strlen(callsign) != 0) {
						forced[gabbi_name] = callsign;
					}
					UpdateFields(i);
				}
			}
		}

		if (noupdate_field >= 0 && !changed && in_type != TQSL_LOCATION_FIELD_BADZONE)
			continue;
		if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
			// Update this list
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
			int selected;
			bool defaulted = false;
			tqsl_getLocationFieldIndex(loc, i, &selected);
			int new_sel = 0;
			wxString old_sel = cb->GetValue();
			wxString old_text = old_sel;
			if (old_sel.IsEmpty() && strcmp(gabbi_name, "CALL") == 0) {
				old_sel = (reinterpret_cast<TQSLWizard*>(GetParent()))->GetDefaultCallsign();
				if (!old_sel.IsEmpty())
					defaulted = true;		// Set from default
			}
			if (strlen(callsign) >0 && forced[gabbi_name] == callsign) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				old_text = wxString::FromUTF8(buf);
				defaulted = true;
			}
			cb->Clear();
			int nitems;
			tqsl_getNumLocationFieldListItems(loc, i, &nitems);
			bool noneSeen = false;
			for (int j = 0; j < nitems && j < 2000; j++) {
				char item[200];
				tqsl_getLocationFieldListItem(loc, i, j, item, sizeof(item));
				// Translate the first [None] entry if it exists
#ifdef tqsltranslate
				__("[None]");
#endif
				wxString item_text = wxString::FromUTF8(item);
				if (j == 0 && item_text == wxT("[None]"))
					noneSeen = true;
				if (item_text == old_sel || item_text == old_text)
					new_sel = j;
				if (j == 0)
					item_text = wxGetTranslation(item_text);
				if ((strcmp(gabbi_name, "CALL") == 0) && new_sel == j) {
					callLabel->SetLabel(item_text);
				}
				cb->Append(item_text);
			}
			if (noupdate_field < 0 && !defaulted)
				new_sel = selected;
			if (noneSeen && nitems == 2) { // Really only one
				new_sel = 1;
			}
			tqsl_setLocationFieldIndex(loc, i, new_sel);
			if (new_sel >= 0 && nitems > new_sel && static_cast<int>(cb->GetCount()) > new_sel)
				cb->SetSelection(new_sel);
			if (noneSeen) {				// If 2 with "none"
				cb->Enable(nitems > 2);		// Then it's locked.
			} else {
				cb->Enable(nitems > 1);
			}
		} else if (in_type == TQSL_LOCATION_FIELD_TEXT) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			tx->GetSize(&w, &h);
			tx->SetSize((len+1)*text_size.GetWidth(), h);
			if (noupdate_field < 0) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				tx->ChangeValue(wxString::FromUTF8(buf));
				if (strcmp(gabbi_name, "GRIDSQUARE") == 0) {
					gridFromDB = true;
				}
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			st = errlbl;
			st->GetSize(&w, &h);
			st->SetSize((len+1)*text_size.GetWidth(), h);
			char buf[256];
			tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
			if (strlen(buf) > 0)
				valMsg = wxGetTranslation(wxString::FromUTF8(buf));
			else
				valMsg = wxT("");
			st->SetLabel(valMsg);
		}
	}

	tqslTrace("TQSLWizLocPage::UpdateFields", "Updating station location capture");
	tqsl_updateStationLocationCapture(loc);
	errlbl->SetLabel(wxString::FromUTF8(tQSL_CustomError));

	if (tqsl_getNextStationLocationCapturePage(loc, &second_page)) second_page = 0;
	if (second_page > 0) {
		tqslTrace("TQSLWizLocPage::UpdateFields", "Flipping to page %d", second_page);
		errlbl->SetLabel(wxString::FromUTF8(tQSL_CustomError));
		tqsl_setStationLocationCapturePage(loc, second_page);
	}

	// Do page 2
	// Assumption: second page is all dropdown lists
	PASexists = SASexists = Parkexists = false;
	// Start page 2 - if we're updating past the end of the first page
	// then start at the offset
	int p2start = 0;
	if (noupdate_field > static_cast<int>(p1_controls.size())) {
		p2start = noupdate_field - static_cast<int>(p1_controls.size());
	} else {
		tqsl_updateStationLocationCapture(loc);
	}
	bool relayout = false;
	for (int i = p2start; second_page > 0 && i < static_cast<int>(p2_controls.size()); i++) {
		int changed;
		int in_type;
		char gabbi_name[40];
		char label[128];

		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		wxOwnerDrawnComboBox* cb;
		if (isPAS(gabbi_name)) {
			if (!boxITUZ->IsShown(boxPAS))
				relayout = true;
			boxPAS->Show(true);
			cb = ctlPAS;
			lblPAS->SetLabel(wxString::FromUTF8(label));
			PASexists = true;
		}
		if (isSAS(gabbi_name)) {
			if (!boxCQZ->IsShown(boxSAS))
				relayout = true;
			boxSAS->Show(true);
			cb = ctlSAS;
			lblSAS->SetLabel(wxString::FromUTF8(label));
			SASexists = true;
		}
		if (isPark(gabbi_name)) {
			if (!boxIOTA->IsShown(boxPark))
				relayout = true;
			boxPark->Show(true);
			cb = ctlPark;
			lblPark->SetLabel(wxString::FromUTF8(label));
			Parkexists = true;
		}

		tqsl_getLocationFieldInputType(loc, i, &in_type);
		tqsl_getLocationFieldChanged(loc, i, &changed);

		/*
		 * Code below is used to revert fields that have had defaults set based on callsign
		 */
		ForcedMap::iterator it;
		it = forced.find(gabbi_name);
		if (it != forced.end()) {		// Something set
			if (it->second == "") {
				forced.erase(it);
			} else if (it->second != callsign) {	// For a different call
				if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
					tqsl_setLocationFieldIndex(loc, i, 0);
					cb->SetSelection(wxNOT_FOUND);
				}
				forced.erase(it);
			}
		}

		char buf[256];
		string s;
		tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);

		// Has this been set?
		bool wasUserSet = (userSet[gabbi_name] == gabbi_name);
		if (!wasUserSet && !parent->editing && strlen(buf) == 0) { // Empty, so set to default
			if (isPAS(gabbi_name)) {
				if (get_address_field(callsign, "state", s) == 0 || get_address_field(callsign, "pas", s) == 0) {
					tqsl_setLocationFieldCharData(loc, i, s.c_str());
					int new_sel;
					tqsl_getLocationFieldIndex(loc, i, &new_sel);
					if (new_sel >= 0 && new_sel < static_cast<int>(cb->GetCount()))
						cb->SetSelection(new_sel);
					if (strlen(callsign) != 0) {
						forced[gabbi_name] = callsign;
					}
				}
			}

			if (isSAS(gabbi_name)) {
				if (get_address_field(callsign, "county", s) == 0 || get_address_field(callsign, "sas", s) == 0) {
					tqsl_setLocationFieldCharData(loc, i, s.c_str());
					int new_sel;
					tqsl_getLocationFieldIndex(loc, i, &new_sel);
					if (new_sel >= 0 && new_sel < static_cast<int>(cb->GetCount()))
						cb->SetSelection(new_sel);
					if (strlen(callsign) != 0) {
						forced[gabbi_name] = callsign;
					}
				}
			}
		}

		if (noupdate_field >= 0 && !changed && in_type != TQSL_LOCATION_FIELD_BADZONE)
			continue;
		if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
			// Update this list
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
			int selected;
			bool defaulted = false;
			tqsl_getLocationFieldIndex(loc, i, &selected);
			int new_sel = 0;
			wxString old_sel = cb->GetValue();
			wxString old_text = old_sel;
			if (strlen(callsign) >0 && forced[gabbi_name] == callsign) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				old_text = wxString::FromUTF8(buf);
				defaulted = true;
			}
			cb->Clear();
			int nitems;
			tqsl_getNumLocationFieldListItems(loc, i, &nitems);
			bool noneSeen = false;
			for (int j = 0; j < nitems && j < 2000; j++) {
				char item[200];
				char itemkey[200];
				tqsl_getLocationFieldListItem(loc, i, j, item, sizeof(item));
				wxString item_text = wxString::FromUTF8(item);
				tqsl_getLocationFieldListItem(loc, i, j | 0x10000, itemkey, sizeof(itemkey));
				wxString item_label = wxString::FromUTF8(itemkey);
				// Translate the first [None] entry if it exists
#ifdef tqsltranslate
				__("[None]");
#endif
				if (j == 0 && item_text == wxT("[None]"))
					noneSeen = true;
				if (item_text == old_sel || item_text == old_text || item_label == old_text)
					new_sel = j;
				if (j == 0)
					item_text = wxGetTranslation(item_text);
				cb->Append(item_text);
			}
			if (noupdate_field < 0 && !defaulted)
				new_sel = selected;
			if (noneSeen && (nitems == 2) && !isPark(gabbi_name)) { // Really only one
				new_sel = 1;
			}
			tqsl_setLocationFieldIndex(loc, i, new_sel);
			if (new_sel >= 0 && nitems > new_sel && static_cast<int>(cb->GetCount()) > new_sel)
				cb->SetSelection(new_sel);
			if (noneSeen)
				cb->Enable(nitems > 2 || isPark(gabbi_name));
			else
				cb->Enable(nitems > 1);
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
			// Ignore BADZONE on first page
		}
	}
	if (PASexists != boxITUZ->IsShown(boxPAS) ||
	    SASexists != boxCQZ->IsShown(boxSAS) ||
	    Parkexists != boxIOTA->IsShown(boxPark)) {
		relayout = true;
	}
	boxPAS->Show(PASexists);
	boxSAS->Show(SASexists);
	boxPark->Show(Parkexists);

	if (relayout) {
		Layout();
	}

	// Back to initial page
        tqsl_setStationLocationCapturePage(loc, cur_page);
	if (noupdate_field >= 0) {
		tqsl_updateStationLocationCapture(loc);
		errlbl->SetLabel(wxString::FromUTF8(tQSL_CustomError));
	}
	tqslTrace("TQSLWizLocPage::update_fields", "done");
}

void
TQSLWizLocPage::OnComboBoxEvent(wxCommandEvent& event) {
	tqslTrace("TQSLWizLocPage::OnComboBoxEvent", NULL);
	int control_idx = event.GetId() - TQSL_ID_LOW;
	if (control_idx < 0 || control_idx >= static_cast<int>(p1_controls.size()) + static_cast<int>(p2_controls.size()))
		return;
	int cur_page;
	tqsl_getStationLocationCapturePage(loc, &cur_page);

	int cidx = control_idx;
	tqslTrace("TQSLWizLocPage::OnComboBoxEvent", "control index = %d", cidx);
	if (cidx >= page_2_offset) {
		cidx = control_idx - page_2_offset;
		tqslTrace("TQSLWizLocPage::OnComboBoxEvent", "page 2 control index = %d", cidx);
		if (cur_page != second_page) {
        		tqsl_setStationLocationCapturePage(loc, second_page);
		}
	} else {
		if (cur_page != first_page) {
        		tqsl_setStationLocationCapturePage(loc, first_page);
		}
	}

	int in_type;
	tqsl_getLocationFieldInputType(loc, cidx, &in_type);
	switch (in_type) {
		case TQSL_LOCATION_FIELD_DDLIST:
		case TQSL_LOCATION_FIELD_LIST:
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, cidx, gabbi_name, sizeof gabbi_name);
			tqsl_setLocationFieldIndex(loc, cidx, event.GetInt());
			if (strcmp(gabbi_name, "CALL") == 0) {
				tqsl_getLocationFieldCharData(loc, cidx, callsign, sizeof callsign);
			}
			if (strcmp(gabbi_name, "DXCC") == 0) {
				ForcedMap::iterator it;
				it = forced.find("ITUZ");
				if (it != forced.end()) {
					if (it->second == callsign) {
						forced.erase(it);
					}
				}
				it = forced.find("CQZ");
				if (it != forced.end()) {
					if (it->second == callsign) {
						forced.erase(it);
					}
				}
			}
			userSet[gabbi_name] = gabbi_name;
			UpdateFields(control_idx);
			UpdateFields();
			break;
	}
        tqsl_setStationLocationCapturePage(loc, cur_page);
}

void
TQSLWizLocPage::OnTextEvent(wxCommandEvent& event) {
	tqslTrace("TQSLWizLocPage::OnTextEvent", NULL);
	int control_idx = event.GetId() - TQSL_ID_LOW;
	if (control_idx < 0 || control_idx >= static_cast<int>(p1_controls.size()) + static_cast<int>(p2_controls.size()))
		return;
	int cur_page;
	tqsl_getStationLocationCapturePage(loc, &cur_page);

	int cidx = control_idx;
	if (cidx >= page_2_offset) {
		cidx = control_idx - page_2_offset;
		if (cur_page != second_page) {
        		tqsl_setStationLocationCapturePage(loc, second_page);
		}
	} else {
		if (cur_page != first_page) {
        		tqsl_setStationLocationCapturePage(loc, first_page);
		}
	}

	int in_type;
	tqsl_getLocationFieldInputType(loc, cidx, &in_type);
	if (in_type == TQSL_LOCATION_FIELD_TEXT) {
		char gabbi_name[40];
		tqsl_getLocationFieldDataGABBI(loc, cidx, gabbi_name, sizeof gabbi_name);
		if (strcmp(gabbi_name, "GRIDSQUARE") == 0) {
			gridFromDB = false;  // User set the grid
		}
	}
        tqsl_setStationLocationCapturePage(loc, cur_page);
}


void
TQSLWizLocPage::OnCheckBoxEvent(wxCommandEvent& event) {
	UpdateFields(-1);
}

TQSLWizLocPage::TQSLWizLocPage(TQSLWizard *_parent, tQSL_Location locp)
	: TQSLWizPage(_parent, locp) {
	tqslTrace("TQSLWizLocPage::TQSLWizLocPage", "parent=0x%lx, locp=0x%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(locp));
	initialized = false;
	errlbl = NULL;
	sizer = new wxBoxSizer(wxVERTICAL);
	int control_width = getTextSize(this).GetWidth() * 40;

	parent = _parent;
	valMsg = wxT("");
	invalidGrid = false;
	allowBadGrid = false;
	gridFromDB = false;
	tqsl_getStationLocationCapturePage(loc, &loc_page);
	wxScreenDC sdc;
	int label_w = 0;
	total_fields = 0;
	page_2_offset = 0;
	int numf;

	// Walk all of the pages to find the width of the largest label

	int numPages;
	tqsl_getNumStationLocationCapturePages(loc, &numPages);

	for (int p = 1; p <= numPages; p++) {
		tqsl_setStationLocationCapturePage(loc, p);
		tqsl_getNumLocationField(loc, &numf);
		for (int i = 0; i < numf; i++) {
			wxCoord w, h;
			char label[256];
			tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
			wxString lbl = wxGetTranslation(wxString::FromUTF8(label));
			sdc.GetTextExtent(lbl, &w, &h);
			if (w > label_w) {
				label_w = w;
			}
		}
	}
	label_w += 10;

	/*
	 * Assumptions here, which are more strict than the way the page layouts
	 * are defined:
	 * 1. The first page is always page 1.
	 * 2. The order on that page is call,dxcc,grid,itu,cq,iota
	 * 3. There's either only one page (no administrative division, etc.) or
	 *    there's at most two pages (no dependencies of dependencies)
	 */

	first_page = 1;
	tqsl_setStationLocationCapturePage(loc, first_page);
	if (tqsl_getNextStationLocationCapturePage(loc, &second_page)) second_page = 0;

        wxFont callSignFont(48, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxFont dxccFont(24, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxFont labelFont(6, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxBoxSizer *hsizer;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("Confirming QSOs with Station"), wxDefaultPosition, wxSize(label_w*9, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE), 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5);
	sizer->Add(hsizer, 1);
	sizer->AddStretchSpacer(1);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	callLabel = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
			wxSize(label_w*9, -1), wxALIGN_CENTRE_HORIZONTAL|wxST_NO_AUTORESIZE);
	callLabel->SetFont(callSignFont);
	hsizer->Add(callLabel, 0, wxALIGN_CENTRE_VERTICAL, 5);
	sizer->Add(hsizer, 0, wxALIGN_CENTRE_VERTICAL, 5);
	// Process the first page
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		char label[256];
		int in_type, flags;
		wxBoxSizer *hsizer;
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		wxString lbl = wxGetTranslation(wxString::FromUTF8(label));
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
			continue;
		}
		hsizer = new wxBoxSizer(wxHORIZONTAL);
		wxStaticText* fieldLabel = new wxStaticText(this, -1, lbl, wxDefaultPosition,
			wxSize(label_w, -1), wxALIGN_RIGHT/*|wxST_NO_AUTORESIZE*/);
		fieldLabel->SetFont(labelFont);
		hsizer->Add(fieldLabel, 0, wxTOP|wxALIGN_CENTER_VERTICAL, 5);

		wxWindow *control_p = NULL;
		char gabbi_name[256];
		int data_length;
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		tqsl_getLocationFieldFlags(loc, i, &flags);
		tqsl_getLocationFieldDataLength(loc, i, &data_length);
		if (!strcmp(gabbi_name, "DXCC")) {
			data_length *= 6;			// Wider font, DXCC set to 10 chs when 45 is right.
		} else if (!strcmp(gabbi_name, "GRIDSQUARE")) {
			data_length = 17;			// Logbook allows 27?
		} else if (strcmp(gabbi_name, "CQZ") == 0 || strcmp(gabbi_name, "ITUZ") == 0) {
			data_length = 8;			// Logbook 10, need "[NONE]".
		}
		control_width = getTextSize(this).GetWidth() * data_length;
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				control_p = new wxOwnerDrawnComboBox(this, TQSL_ID_LOW+total_fields, wxT(""), wxDefaultPosition, wxSize(control_width, -1),
				0, 0, wxCB_DROPDOWN|wxCB_READONLY);
				hsizer->Add(control_p, 0, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
				if(!strcmp(gabbi_name, "CALL")) {
					ctlCallSign = static_cast<wxOwnerDrawnComboBox*>(control_p);
					sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 5);
				}
				if(!strcmp(gabbi_name, "DXCC")) {
					ctlEntity = static_cast<wxOwnerDrawnComboBox*>(control_p);
					ctlEntity->SetFont(dxccFont);
					sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 5);
				}
				if(!strcmp(gabbi_name, "ITUZ")) {
					ctlITUZ = static_cast<wxOwnerDrawnComboBox*>(control_p);
					boxITUZ = hsizer;
				}
				if(!strcmp(gabbi_name, "CQZ")) {
					ctlCQZ = static_cast<wxOwnerDrawnComboBox*>(control_p);
					boxCQZ = hsizer;
				}
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				control_p = new wxTextCtrl(this, TQSL_ID_LOW+total_fields, wxT(""), wxDefaultPosition, wxSize(control_width, -1));
				hsizer->Add(control_p, 0, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
				if(!strcmp(gabbi_name, "GRIDSQUARE")) {
					ctlGrid = static_cast<wxTextCtrl*>(control_p);
					sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 5);
				}
				if(!strcmp(gabbi_name, "IOTA")) {
					ctlIOTA = static_cast<wxTextCtrl*>(control_p);
					boxIOTA = hsizer;
				}
				break;
			case TQSL_LOCATION_FIELD_BADZONE:
				break;
		}
		p1_controls.push_back(control_p);
		total_fields++;
	}

	// Process the second page
	// Always use page #2 (USA) as it has the maximum number of fields

	page_2_offset = total_fields;
	tqsl_setStationLocationCapturePage(loc, 2);	// Always page 2 so the fields all created
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		char label[256];
		int in_type, flags, data_length;
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		wxString lbl = wxGetTranslation(wxString::FromUTF8(label));
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		tqsl_getLocationFieldDataLength(loc, i, &data_length);
		char gabbi_name[256];
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		if (in_type != TQSL_LOCATION_FIELD_BADZONE) {
			switch (i) {
			    case 0:		// US_STATE
				control_width = getTextSize(this).GetWidth() * 30;
				boxPAS = new wxBoxSizer(wxHORIZONTAL);
				lblPAS = new wxStaticText(this, -1, lbl, wxDefaultPosition,
						wxSize(label_w, -1), wxALIGN_RIGHT/*|wxST_NO_AUTORESIZE*/);
				lblPAS->SetFont(labelFont);
				boxPAS->Add(lblPAS, 0, wxTOP|wxALIGN_CENTER_VERTICAL, 5);
				break;
			    case 1:		// US_COUNTY
				control_width = getTextSize(this).GetWidth() * 20;
				boxSAS = new wxBoxSizer(wxHORIZONTAL);
				lblSAS = new wxStaticText(this, -1, lbl, wxDefaultPosition,
						wxSize(label_w, -1), wxALIGN_RIGHT/*|wxST_NO_AUTORESIZE*/);
				lblSAS->SetFont(labelFont);
				boxSAS->Add(lblSAS, 0, wxTOP|wxALIGN_CENTER_VERTICAL, 5);
				break;
			    case 2:		// US_PARK
				control_width = getTextSize(this).GetWidth() * data_length;
				boxPark = new wxBoxSizer(wxHORIZONTAL);
				lblPark = new wxStaticText(this, -1, lbl, wxDefaultPosition,
						wxSize(label_w, -1), wxALIGN_RIGHT/*|wxST_NO_AUTORESIZE*/);
				lblPark->SetFont(labelFont);
				boxPark->Add(lblPark, 0, wxTOP|wxALIGN_CENTER_VERTICAL, 5);
				break;
			}
		}
		wxWindow *control_p = NULL;
		tqsl_getLocationFieldFlags(loc, i, &flags);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				control_p = new wxOwnerDrawnComboBox(this, TQSL_ID_LOW+total_fields, wxT(""), wxDefaultPosition, wxSize(control_width, -1),
				0, 0, wxCB_DROPDOWN|wxCB_READONLY);
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				control_p = new wxTextCtrl(this, TQSL_ID_LOW+total_fields, wxT(""), wxDefaultPosition, wxSize(control_width, -1));
				break;
			case TQSL_LOCATION_FIELD_BADZONE:
				continue;
		}
		p2_controls.push_back(control_p);
		switch (i) {
		    case 0:	// US_STATE
			ctlPAS = static_cast<wxOwnerDrawnComboBox*>(control_p);
			boxPAS->Add(control_p, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			boxITUZ->Add(boxPAS, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			sizer->Add(boxITUZ, 0, wxLEFT|wxRIGHT, 5);
			break;
		    case 1:	// US_COUNTY
			ctlSAS = static_cast<wxOwnerDrawnComboBox*>(control_p);
			boxSAS->Add(control_p, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			boxCQZ->Add(boxSAS, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			sizer->Add(boxCQZ, 0, wxLEFT|wxRIGHT, 5);
			break;
		    case 2:	// US_PARK
			ctlPark = static_cast<wxOwnerDrawnComboBox*>(control_p);
			boxPark->Add(control_p, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			boxIOTA->Add(boxPark, 1, wxALIGN_CENTER | wxLEFT | wxTOP, 5);
			sizer->Add(boxIOTA, 0, wxLEFT|wxRIGHT, 5);
			break;
		}
		total_fields++;
	}

	// Add the error label
	wxCoord w, h;
	int tsize = 80;
	sdc.GetTextExtent(wxString::FromUTF8("X"), &w, &h);
	errlbl = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
			wxSize(w*tsize, h), wxALIGN_LEFT|wxST_NO_AUTORESIZE);
	sizer->AddStretchSpacer(1);
	sizer->Add(errlbl, 0);
	sizer->AddStretchSpacer(1);

	//  Back to initial page
	tqsl_setStationLocationCapturePage(loc, loc_page);
	initialized = true;

	UpdateFields();
	UpdateFields();		// Twice thru to make sure state propagates
	AdjustPage(sizer, wxT("stnloc1.htm"));
}

TQSLWizLocPage::~TQSLWizLocPage() {
}

const char *
TQSLWizLocPage::validate() {
	tqslTrace("TQSLWizLocPage::validate", NULL);

	if (!initialized) return 0;
	valMsg = wxT("");
	int initial_page;

	tqsl_getStationLocationCapturePage(loc, &initial_page);
	tqsl_setStationLocationCapturePage(loc, first_page);
	for (int i = 0; i < static_cast<int>(p1_controls.size()); i++) {
		char gabbi_name[40];
		int in_type;
		int flags;
		int field = i;
		if (i >= page_2_offset) {
        		tqsl_setStationLocationCapturePage(loc, 2);
			field = i - page_2_offset;
		}
		tqsl_getLocationFieldDataGABBI(loc, field, gabbi_name, sizeof gabbi_name);
		tqsl_getLocationFieldInputType(loc, field, &in_type);
		tqsl_getLocationFieldFlags(loc, field, &flags);
		if (flags == TQSL_LOCATION_FIELD_SELNXT) {
			int index;
			tqsl_getLocationFieldIndex(loc, field, &index);
			// Less than zero, no match
			// equal zero, "None" - allow None for call.
			if (index < 0 || (index == 0 && strcmp(gabbi_name, "CALL"))) {
				char label[256];
				tqsl_getLocationFieldDataLabel(loc, field, label, sizeof label);
				valMsg = wxString::Format(_("You must select a %hs"), label);
			}
		} else if (strcmp(gabbi_name, "GRIDSQUARE") == 0) {
			tqsl_getLocationFieldCharData(loc, 0, callsign, sizeof callsign);
			wxString gridVal = (reinterpret_cast<wxTextCtrl *>(p1_controls[i]))->GetValue();
			if (gridVal.size() == 0) {
				continue;
			}
			string gridlist;
			get_address_field(callsign, "grids", gridlist);
			wxString editedGrids = wxT("");
			wxStringTokenizer grids(gridVal, wxT(","));	// Comma-separated list of squares
			while (grids.HasMoreTokens()) {
				wxString grid = grids.GetNextToken().Trim().Trim(false);
				// Truncate to six character field
				grid = grid.Left(6);
				if (grid[0] <= 'z' && grid[0] >= 'a')
					grid[0] = grid[0] - 'a' + 'A';	// Upper case first two
				if (grid.size() > 1 && (grid[1] <= 'z' && grid[1] >= 'a'))
					grid[1] = grid[1] - 'a' + 'A';
				if (grid[0] < 'A' || grid[0] > 'R') {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square Field"), grid.c_str());
				}
				if (grid.size() > 1 && (grid[1] < 'A' || grid[1] > 'R')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square Field"), grid.c_str());
				}
				if (grid.size() > 2 && (grid[2] < '0' || grid[2] > '9')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (grid.size() < 4) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (grid[3] < '0' || grid[3] > '9') {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}

				if (grid.size() > 4 && (grid[4] <= 'Z' && grid[4] >= 'A'))
					grid[4] = grid[4] - 'A' + 'a';	// Lower case subsquare
				if (grid.size() > 5 && (grid[5] <= 'Z' && grid[5] >= 'A'))
					grid[5] = grid[5] - 'A' + 'a';

				if (grid.size() > 4 && (grid[4] < 'a' || grid[4] > 'x')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Subsquare"), grid.c_str());
				}
				if (grid.size() > 5 && (grid[5] < 'a' || grid[5] > 'x')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Subsquare"), grid.c_str());
				}
				if (grid.size() != 6 && grid.size() != 4) {
					// Not long enough yet or too long.
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (valMsg.IsEmpty() && !gridlist.empty() && !gridFromDB) {
					string probe = string(grid.Left(4).mb_str());
					if (gridlist.find(probe) == string::npos) {
						valMsg = wxString::Format(_("Grid %s is not correct for your QTH. Click 'Next' again to use it anyway."), grid.c_str());
						invalidGrid = true;
					} else {
						invalidGrid = false;
					}
				}
				if (!editedGrids.IsEmpty())
					editedGrids += wxT(",");
				editedGrids += grid;
				(reinterpret_cast<wxTextCtrl *>(p1_controls[i]))->ChangeValue(editedGrids);
				tqsl_setLocationFieldCharData(loc, field, (reinterpret_cast<wxTextCtrl *>(p1_controls[i]))->GetValue().ToUTF8());
			}
		} else if (strcmp(gabbi_name, "IOTA") == 0) {
			wxString iotaVal = (reinterpret_cast<wxTextCtrl *>(p1_controls[i]))->GetValue();
			if (iotaVal.size() == 0) {
				continue;
			}
			// Format of an IOTA entry
			// AF,AN,AS,EU,NA,OC,SA-000
			iotaVal = iotaVal.Upper();
			if (iotaVal.size() != 6) {
				if (valMsg.IsEmpty()) {
					valMsg = wxString::Format(_("IOTA value %s is not valid."), iotaVal.c_str());
					continue;
				}
			}
			wxString cont = iotaVal.Left(3);
			if (cont != wxT("AF-") && cont != wxT("AN-") && cont != wxT("AS-") && cont != wxT("EU-") &&
			    cont != wxT("NA-") && cont != wxT("OC-") && cont != wxT("SA-")) {
				if (valMsg.IsEmpty()) {
					valMsg = wxString::Format(_("IOTA reference %s is not correct. Must start with AF-, AN-, AS-, EU-, NA-, OC- or SA-"), iotaVal.c_str());
				}
			}
			wxString num = iotaVal.Right(3);
			long iotanum;
			if (!iotaVal.Right(3).ToLong(&iotanum)) {
				if (valMsg.IsEmpty()) {
					valMsg = wxString::Format(_("IOTA reference %s is not correct. Must have a number after the '-'"), iotaVal.c_str());
				}
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE && p1_controls[i]) {
// Possible errors, here for harvesting
#ifdef tqsltranslate
	static const char* verrs[] = {
		__("Invalid zone selections for state"),
		__("Invalid zone selections for province"),
		__("Invalid zone selections for oblast"),
		__("Invalid zone selections for DXCC entity");
};
#endif
			char buf[256];
			tqsl_getLocationFieldCharData(loc, field, buf, sizeof buf);
			if (strlen(buf) > 0)
				valMsg = wxGetTranslation(wxString::FromUTF8(buf));
			(reinterpret_cast<wxStaticText *>(p1_controls[i]))->SetLabel(valMsg);
		}
	}
	// Do the second page
	if (second_page > 0) {
		tqsl_setStationLocationCapturePage(loc, second_page);
	}
	for (int i = 0; second_page > 0 && i < static_cast<int>(p2_controls.size()); i++) {
		char gabbi_name[40];
		int in_type;
		int flags;
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		tqsl_getLocationFieldFlags(loc, i, &flags);
		if (flags == TQSL_LOCATION_FIELD_SELNXT) {
			int index;
			tqsl_getLocationFieldIndex(loc, i, &index);
			if (index <= 0) {
				char label[256];
				tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
				valMsg = wxString::Format(_("You must select a %hs"), label);
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE && p2_controls[i]) {
			char buf[256];
			tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
			if (strlen(buf) > 0)
				valMsg = wxGetTranslation(wxString::FromUTF8(buf));
			(reinterpret_cast<wxStaticText *>(p2_controls[i]))->SetLabel(valMsg);
		}
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	tqsl_setStationLocationCapturePage(loc, initial_page);
	return 0;
}

bool
TQSLWizLocPage::TransferDataFromWindow() {
	tqslTrace("TQSLWizLocPage::TransferDataFromWindow", NULL);

	tqsl_setStationLocationCapturePage(loc, loc_page);
	if (loc_page != second_page) {
		for (int i = 0; i < static_cast<int>(p1_controls.size()); i++) {
			int in_type;
			tqsl_getLocationFieldInputType(loc, i, &in_type);
			switch(in_type) {
				case TQSL_LOCATION_FIELD_DDLIST:
				case TQSL_LOCATION_FIELD_LIST:
					break;
				case TQSL_LOCATION_FIELD_TEXT:
					tqsl_setLocationFieldCharData(loc, i, (reinterpret_cast<wxTextCtrl *>(p1_controls[i]))->GetValue().ToUTF8());
					break;
			}
		}
	} else {
		for (int i = 0; i < static_cast<int>(p2_controls.size()); i++) {
			int in_type;
			tqsl_getLocationFieldInputType(loc, i, &in_type);
			switch(in_type) {
				case TQSL_LOCATION_FIELD_DDLIST:
				case TQSL_LOCATION_FIELD_LIST:
					break;
				case TQSL_LOCATION_FIELD_TEXT:
					tqsl_setLocationFieldCharData(loc, i, (reinterpret_cast<wxTextCtrl *>(p2_controls[i]))->GetValue().ToUTF8());
					break;
			}
		}
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	return true;
}
void
TQSLWizLocPage::OnPageChanging(wxWizardEvent& ev) {
	tqslTrace("TQSLWizLocPage::OnPageChanging", "Direction=", ev.GetDirection());

	validate();
	if (valMsg.Len() > 0 && ev.GetDirection()) {
		if (!allowBadGrid) {
			ev.Veto();
			allowBadGrid = true;		// Don't allow going forward once
		} else {
			allowBadGrid = false;
		}
		if (!invalidGrid) {
			wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
		}
	}
}

BEGIN_EVENT_TABLE(TQSLWizFinalPage, TQSLWizPage)
	EVT_LISTBOX(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_LISTBOX_DCLICK(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_TEXT(TQSL_ID_LOW+1, TQSLWizFinalPage::check_valid)
	EVT_WIZARD_PAGE_CHANGING(wxID_ANY, TQSLWizFinalPage::OnPageChanging)
END_EVENT_TABLE()

void
TQSLWizFinalPage::OnListbox(wxCommandEvent &) {
	tqslTrace("TQSLWizFinalPage::OnListbox", NULL);
	if (namelist->GetSelection() >= 0) {
		const char *cp = (const char *)(namelist->GetClientData(namelist->GetSelection()));
		if (cp)
			newname->SetValue(wxString::FromUTF8(cp).Trim(true).Trim(false));
	}
}

TQSLWizFinalPage::TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev)
	: TQSLWizPage(parent, locp), prev(i_prev) {
	tqslTrace("TQSLWizFinalPage::TQSLWizFinalPage", "parent=0x%lx, locp=0x%lx, i_prev=0x%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(locp), reinterpret_cast<void *>(i_prev));
	initialized = false;
	errlbl = NULL;
	valMsg = wxT("");
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*40;

	int y = text_size.GetHeight();
	sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, _("Station Data input complete"));
	sizer->Add(st, 0, wxALIGN_CENTER|wxTOP, 10);

	// Title
	st = new wxStaticText(this, -1, _("Select or enter name of this station location"));
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);

	// List of existing location names
	namelist = new wxListBox(this, TQSL_ID_LOW, wxDefaultPosition, wxSize(control_width, text_size.GetHeight()*10),
		0, 0, wxLB_SINGLE|wxLB_HSCROLL|wxLB_NEEDED_SB);
	sizer->Add(namelist, 0, wxEXPAND);
	int n;
	tqsl_getNumStationLocations(locp, &n);
	for (int i = 0; i < n; i++) {
		char buf[256];
		tqsl_getStationLocationName(loc, i, buf, sizeof buf);
		item_data.push_back(strdup(buf));
		char cbuf[256];
		tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf);
		wxString s = wxString::FromUTF8(buf);
		s += (wxString(wxT(" (")) + wxString::FromUTF8(cbuf) + wxString(wxT(")")));
		const void *v = item_data.back();
		namelist->Append(s, const_cast<void *>(v));
	}
	if (namelist->GetCount() > 0)
		namelist->SetSelection(0, FALSE);
	// New name
	st = new wxStaticText(this, -1, _("Station Location Name"));
	sizer->Add(st, 0, wxALIGN_LEFT|wxTOP, 10);
	newname = new wxTextCtrl(this, TQSL_ID_LOW+1, wxT(""), wxPoint(0, y), wxSize(control_width, -1));
	sizer->Add(newname, 0, wxEXPAND);
	newname->SetValue(parent->GetLocationName());
	errlbl = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxSize(control_width, -1),
				wxALIGN_LEFT/*|wxST_NO_AUTORESIZE*/);
	sizer->Add(errlbl, 0, wxTOP);
	initialized = true;
	AdjustPage(sizer, wxT("stnloc2.htm"));
}

bool
TQSLWizFinalPage::TransferDataFromWindow() {
	tqslTrace("TQSLWizFinalPage::TransferDataFromWindow", NULL);
	validate();
	if (valMsg.Len() > 0) // Must be a "back"
		return true;
	wxString s = newname->GetValue().Trim(true).Trim(false);
	(reinterpret_cast<TQSLWizard *>(GetParent()))->SetLocationName(s);
	return true;
}

void
TQSLWizFinalPage::OnPageChanging(wxWizardEvent& ev) {
	tqslTrace("TQSLWizFinalPage::OnPageChanging", "Direction=", ev.GetDirection());

	validate();
	if (!valMsg.IsEmpty() && ev.GetDirection()) {
		ev.Veto();
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
	}
}

const char *
TQSLWizFinalPage::validate() {
	tqslTrace("TQSLWizFinalPage::validate", NULL);
	if (!initialized) return 0;
	wxString val = newname->GetValue().Trim(true).Trim(false);
	valMsg = wxT("");

	if (val.IsEmpty()) {
		valMsg = wxGetTranslation(_("Station name must be provided"));
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	return 0;
}

TQSLWizFinalPage::~TQSLWizFinalPage() {
	for (int i = 0; i < static_cast<int>(item_data.size()); i++) {
		free(item_data[i]);
	}
}
