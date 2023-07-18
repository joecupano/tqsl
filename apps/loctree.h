/***************************************************************************
                          loctree.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __loctree_h
#define __loctree_h

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

#include "tqsllib.h"

#include "wx/treectrl.h"

class LocTreeItemData : public wxTreeItemData {
 public:
	LocTreeItemData(wxString locname, wxString callsign) : _locname(locname), _callsign(callsign)  {}
	wxString getLocname() { return _locname; }
	wxString getCallSign() { return _callsign; }

 private:
	wxString _locname;
	wxString _callsign;
};

class LocTree : public wxTreeCtrl {
 public:
	LocTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style);
	virtual ~LocTree();
	int Build(int flags = TQSL_SELECT_CERT_WITHKEYS, const TQSL_PROVIDER *provider = 0);
	void OnItemActivated(wxTreeEvent& event);
	void OnRightDown(wxMouseEvent& event);
	bool useContextMenu;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	LocTreeItemData *GetItemData(wxTreeItemId id) { return reinterpret_cast<LocTreeItemData *>(wxTreeCtrl::GetItemData(id)); }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	int GetNumLocations() const { return _nloc; }

 private:
	int _nloc;
	DECLARE_EVENT_TABLE()
};

#endif	// __loctree_h
