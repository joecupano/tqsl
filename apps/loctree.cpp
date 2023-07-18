/***************************************************************************
                          loctree.cpp  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "loctree.h"
#include <errno.h>
#include <wx/imaglist.h>
#include <map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <utility>

#include "tqslctrls.h"
#include "tqslerrno.h"
#include "tqsltrace.h"

#include "util.h"
#include "wxutil.h"

#include "folder.xpm"
#include "home.xpm"

using std::pair;
using std::vector;
using std::map;
using std::make_pair;

enum {
	FOLDER_ICON = 0,
	HOME_ICON = 1
};

///////////// Location Tree Control ////////////////


BEGIN_EVENT_TABLE(LocTree, wxTreeCtrl)
	EVT_TREE_ITEM_ACTIVATED(-1, LocTree::OnItemActivated)
	EVT_RIGHT_DOWN(LocTree::OnRightDown)
END_EVENT_TABLE()

LocTree::LocTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style)
		: wxTreeCtrl(parent, id, pos, size, style), _nloc(0) {
	tqslTrace("LocTree::LocTree", "parent=0x%lx, id=0x%lx, style=%d", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(id), style);
	useContextMenu = true;
	wxBitmap homebm(home_xpm);
	wxBitmap folderbm(folder_xpm);
	wxImageList *il = new wxImageList(16, 16, false, 2);
	il->Add(folderbm);
	il->Add(homebm);
	SetImageList(il);
}


LocTree::~LocTree() {
}

typedef pair<wxString, int> locitem;
typedef vector<locitem> loclist;

static bool
cl_cmp(const locitem& i1, const locitem& i2) {
	return i1.first < i2.first;
}
static void
check_tqsl_error(int rval) {
	if (rval == 0)
		return;
	wxLogError(getLocalizedErrorString());
}

int
LocTree::Build(int flags, const TQSL_PROVIDER *provider) {
	tqslTrace("LocTree::Build", "provider=0x%lx", reinterpret_cast<void *>(const_cast<TQSL_PROVIDER *>(provider)));
	typedef map<wxString, loclist> locmap;
	locmap callsigns;

	DeleteAllItems();
	wxTreeItemId rootId = AddRoot(_("Station Locations"), FOLDER_ICON);
	tQSL_Location loc;
	check_tqsl_error(tqsl_initStationLocationCapture(&loc));
	check_tqsl_error(tqsl_getNumStationLocations(loc, &_nloc));
	for (int i = 0; i < _nloc && i < 2000; i++) {
		char locname[256];
		check_tqsl_error(tqsl_getStationLocationName(loc, i, locname, sizeof locname));
		if (!strcmp(locname, ".empty"))				// Dummy station location
			continue;
		char callsign[256];
		check_tqsl_error(tqsl_getStationLocationCallSign(loc, i, callsign, sizeof callsign));
		wxString locutf8 = wxString::FromUTF8(locname);
		callsigns[wxString::FromUTF8(callsign)].push_back(make_pair(locutf8, i));
	}
	// Sort each callsign list and add items to tree
	locmap::iterator loc_it;
	for (loc_it = callsigns.begin(); loc_it != callsigns.end(); loc_it++) {
		wxTreeItemId id = AppendItem(rootId, loc_it->first, FOLDER_ICON);
		loclist& list = loc_it->second;
		sort(list.begin(), list.end(), cl_cmp);
		for (int i = 0; i < static_cast<int>(list.size()); i++) {
			LocTreeItemData *loc = new LocTreeItemData(list[i].first, loc_it->first);
			AppendItem(id, list[i].first, HOME_ICON, -1, loc);
		}
		Expand(id);
	}
	Expand(rootId);
	check_tqsl_error(tqsl_endStationLocationCapture(&loc));
	return _nloc;
}

void
LocTree::OnItemActivated(wxTreeEvent& event) {
	tqslTrace("LocTree::OnItemActivated", NULL);
	wxTreeItemId id = event.GetItem();
	displayLocProperties(reinterpret_cast<LocTreeItemData *>(GetItemData(id)), this);
}

void
LocTree::OnRightDown(wxMouseEvent& event) {
	tqslTrace("LocTree::OnRightDown", NULL);
	if (!useContextMenu)
		return;
	wxTreeItemId id = HitTest(event.GetPosition());
	if (id && GetItemData(id)) {
		SelectItem(id);
		wxString locname = GetItemData(id)->getLocname();
		wxMenu *cm = makeLocationMenu(true);
		PopupMenu(cm, event.GetPosition());
		delete cm;
	}
}

