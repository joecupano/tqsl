/***************************************************************************
                          certtree.cpp  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "certtree.h"

#include <errno.h>
#include <wx/imaglist.h>
#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <iostream>

#include "tqslctrls.h"
#include "util.h"
#include "dxcc.h"
#include "tqslerrno.h"
#include "tqsltrace.h"
#include "wxutil.h"

#include "cert.xpm"
#include "nocert.xpm"
#include "broken-cert.xpm"
#include "folder.xpm"
#include "expired.xpm"
#include "replaced.xpm"

using std::pair;
using std::vector;
using std::map;
using std::make_pair;

enum {
	CERT_ICON = 0,
	NOCERT_ICON = 1,
	BROKEN_ICON = 2,
	EXPIRED_ICON = 3,
	REPLACED_ICON = 4,
	FOLDER_ICON = 5
};

///////////// Certificate Tree Control ////////////////


BEGIN_EVENT_TABLE(CertTree, wxTreeCtrl)
	EVT_TREE_ITEM_ACTIVATED(-1, CertTree::OnItemActivated)
	EVT_RIGHT_DOWN(CertTree::OnRightDown)
END_EVENT_TABLE()

CertTree::CertTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style)
		: wxTreeCtrl(parent, id, pos, size, style), _ncerts(0) {
	tqslTrace("CertTree::CertTree", NULL);
	useContextMenu = true;
	wxBitmap certbm(cert_xpm);
	wxBitmap no_certbm(nocert_xpm);
	wxBitmap broken_certbm(broken_cert_xpm);
	wxBitmap folderbm(folder_xpm);
	wxBitmap expiredbm(expired_xpm);
	wxBitmap replacedbm(replaced_xpm);
	wxImageList *il = new wxImageList(16, 16, false, 5);
	il->Add(certbm);
	il->Add(no_certbm);
	il->Add(broken_certbm);
	il->Add(expiredbm);
	il->Add(replacedbm);
	il->Add(folderbm);
	SetImageList(il);
}


CertTree::~CertTree() {
	tqslTrace("CertTree::~CertTree", NULL);
	if (_ncerts >0)
		tqsl_freeCertificateList(_certs, _ncerts);
	_ncerts = 0;
}

CertTreeItemData::~CertTreeItemData() {
}

typedef pair<wxString, int> certitem;
typedef vector<certitem> certlist;

static bool
cl_cmp(const certitem& i1, const certitem& i2) {
	return i1.first < i2.first;
}

int
CertTree::Build(int flags, const TQSL_PROVIDER *provider) {
	tqslTrace("CertTree::Build", "flags=%d, provider=%lx", flags, static_cast<void *>(const_cast<TQSL_PROVIDER *>(provider)));
	typedef map<wxString, certlist> issmap;
	issmap issuers;

	DeleteAllItems();
	wxTreeItemId rootId = AddRoot(_("Callsign Certificates"), FOLDER_ICON);
	if (tqsl_selectCertificates(&_certs, &_ncerts, 0, 0, 0, provider, flags)) {
		if (tQSL_Error != TQSL_SYSTEM_ERROR || tQSL_Errno != ENOENT)
			displayTQSLError(__("Error while accessing certificate store"));
	}
	// Separate certs into lists by issuer
	for (int i = 0; i < _ncerts; i++) {
		char issname[129];
		if (tqsl_getCertificateIssuerOrganization(_certs[i], issname, sizeof issname)) {
			displayTQSLError(__("Error parsing certificate for issuer"));
			return _ncerts;
		}
		char callsign[129] = "";
		if (tqsl_getCertificateCallSign(_certs[i], callsign, sizeof callsign - 4)) {
			displayTQSLError(__("Error parsing certificate for call sign"));
			return _ncerts;
		}
		strncat(callsign, " - ", sizeof callsign - strlen(callsign)-1);
		DXCC dxcc;
		int dxccEntity;
		if (tqsl_getCertificateDXCCEntity(_certs[i], &dxccEntity)) {
			displayTQSLError(__("Error parsing certificate for DXCC entity"));
			return _ncerts;
		}
		const char *entityName;
		if (dxcc.getByEntity(dxccEntity))
			entityName = dxcc.name();
		else
			entityName = "<UNKNOWN ENTITY>";
		strncat(callsign, entityName, sizeof callsign - strlen(callsign)-1);
		callsign[sizeof callsign-1] = 0;
		issuers[wxString::FromUTF8(issname)].push_back(make_pair(wxString::FromUTF8(callsign), i));
	}
	// Sort each issuer's list and add items to tree
	issmap::iterator iss_it;
	wxTreeItemId id = NULL;
	wxTreeItemId valid = NULL;
	wxTreeItemId replaced = NULL;
	wxTreeItemId invalid = NULL;
	wxTreeItemId pending = NULL;
	wxTreeItemId expired = NULL;
	_nissuers = issuers.size();
	for (iss_it = issuers.begin(); iss_it != issuers.end(); iss_it++) {
		if (_nissuers > 1) {
			id = AppendItem(rootId, iss_it->first, FOLDER_ICON);
		}
		certlist& list = iss_it->second;
		sort(list.begin(), list.end(), cl_cmp);
		valid = AppendItem(_nissuers > 1 ? id : rootId, _("Active, usable certificates"), FOLDER_ICON);
		for (int i = 0; i < static_cast<int>(list.size()); i++) {
			CertTreeItemData *cert = new CertTreeItemData(_certs[list[i].second]);
			int keyonly = 1;
			int exp = 0, sup = 0;
			int icon_type;
			int keytype = tqsl_getCertificatePrivateKeyType(_certs[list[i].second]);
			tqsl_isCertificateExpired(_certs[list[i].second], &exp);
			tqsl_isCertificateSuperceded(_certs[list[i].second], &sup);
			tqsl_getCertificateKeyOnly(_certs[list[i].second], &keyonly);
			if (keytype == TQSL_PK_TYPE_ERR) {
				icon_type = BROKEN_ICON;
				if (!invalid)
					invalid = AppendItem(_nissuers > 1 ? id : rootId, _("Invalid, unusable"), FOLDER_ICON);
				AppendItem(invalid, list[i].first, icon_type, -1, cert);
			} else if (keyonly) {
				icon_type = NOCERT_ICON;
				if (!pending)
					pending = AppendItem(_nissuers > 1 ? id : rootId, _("Certificates that are awaiting ARRL approval"), FOLDER_ICON);
				AppendItem(pending, list[i].first, icon_type, -1, cert);
			} else if (sup) {
				icon_type = REPLACED_ICON;
				if (!replaced)
					replaced = AppendItem(_nissuers > 1 ? id : rootId, _("Certificates replaced with a newer one"), FOLDER_ICON);
				AppendItem(replaced, list[i].first, icon_type, -1, cert);
			} else if (exp) {
				icon_type = EXPIRED_ICON;
				if (!expired)
					expired = AppendItem(_nissuers > 1 ? id : rootId, _("Certificates that have expired"), FOLDER_ICON);
				AppendItem(expired, list[i].first, icon_type, -1, cert);
			} else {
				icon_type = CERT_ICON;
				AppendItem(valid, list[i].first, icon_type, -1, cert);
			}
		}
		if (id)
			Expand(id);
	}
	if (!valid)		// Handle the no certificates case
		valid = AppendItem(_nissuers > 1 ? id : rootId, _("Active, usable certificates"), FOLDER_ICON);
	Expand(valid);
	Expand(rootId);
	return _ncerts;
}

void
CertTree::SelectCert(tQSL_Cert cert) {
	long serial;
	if (tqsl_getCertificateSerial(cert, &serial))
		return;
	// Iterate the tree, looking for a matching certificate
	wxTreeItemId root = GetRootItem();
	wxTreeItemIdValue issCookie;
	wxTreeItemIdValue listsCookie;
	wxTreeItemIdValue certCookie;
	wxTreeItemId top;
	if (_nissuers > 1) {
		top = GetFirstChild(root, issCookie);
	} else {
		top = root;
	}
	while (top.IsOk()) {
		wxTreeItemId item = GetFirstChild(top, listsCookie);
		while (item.IsOk()) {
			wxTreeItemId subitem = GetFirstChild(item, certCookie);
			while (subitem.IsOk()) {
				tQSL_Cert cert = GetItemData(subitem)->getCert();
				long s;
				tqsl_getCertificateSerial(cert, &s);
				if (s == serial) {	// found it
					SelectItem(subitem);
					return;
				}
				subitem = GetNextChild(item, certCookie);
			}
			item = GetNextChild(top, listsCookie);
		}
		if (_nissuers > 1) {
			top = GetNextChild(root, issCookie);
		} else {
			break;
		}
	}
	return;		// Not found
}

void
CertTree::OnItemActivated(wxTreeEvent& event) {
	tqslTrace("CertTree::OnItemActivated", NULL);
	wxTreeItemId id = event.GetItem();
	displayCertProperties(reinterpret_cast<CertTreeItemData *>(GetItemData(id)), this);
}

void
CertTree::OnRightDown(wxMouseEvent& event) {
	tqslTrace("CertTree::OnRightDown", NULL);
	if (!useContextMenu)
		return;
	wxTreeItemId id = HitTest(event.GetPosition());
	if (id && GetItemData(id)) {
		SelectItem(id);
		tQSL_Cert cert = GetItemData(id)->getCert();
		int keyonly = 1;
		int superseded = 1;
		int expired = 1;
		int enable = 1;
		wxMenu *cm;
		if (cert) {
			tqsl_getCertificateKeyOnly(cert, &keyonly);
                	tqsl_isCertificateSuperceded(cert, &superseded);
                	tqsl_isCertificateExpired(cert, &expired);
			if (expired || superseded) {
				enable = 0;
			}
			char callsign[129];
			if (tqsl_getCertificateCallSign(cert, callsign, sizeof callsign)) {
				cm = makeCertificateMenu((enable != 0), (keyonly != 0), NULL);
			} else {
				cm = makeCertificateMenu((enable != 0), (keyonly != 0), callsign);
			}
		} else {
			cm = makeCertificateMenu((enable != 0), (keyonly != 0), NULL);
		}
		PopupMenu(cm, event.GetPosition());
		delete cm;
	}
}
