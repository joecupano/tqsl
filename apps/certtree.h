/***************************************************************************
                          certtree.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __certtree_h
#define __certtree_h

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

class CertTreeItemData : public wxTreeItemData {
 public:
	explicit CertTreeItemData(tQSL_Cert cert) : _cert(cert) {path = wxEmptyString; basename = wxEmptyString;}
	~CertTreeItemData();
	tQSL_Cert getCert() { return _cert; }
	wxString path;
	wxString basename;
 private:
	tQSL_Cert _cert;
};

class CertTree : public wxTreeCtrl {
 public:
	CertTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style);
	virtual ~CertTree();
	int Build(int flags = TQSL_SELECT_CERT_WITHKEYS, const TQSL_PROVIDER *provider = 0);
	void OnItemActivated(wxTreeEvent& event);
	void OnRightDown(wxMouseEvent& event);
	bool useContextMenu;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	CertTreeItemData *GetItemData(wxTreeItemId id) { return reinterpret_cast<CertTreeItemData *>(wxTreeCtrl::GetItemData(id)); }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	int GetNumCerts() const { return _ncerts; }
	int GetNumIssuers() const { return _nissuers; }
	void SelectCert(tQSL_Cert cert);

 private:
        tQSL_Cert *_certs;
	int _ncerts;
	int _nissuers;
	DECLARE_EVENT_TABLE()
};

#endif	// __certtree_h
