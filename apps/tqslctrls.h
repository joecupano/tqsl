/***************************************************************************
                          tqslctrls.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslctrls_h
#define __tqslctrls_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

enum {		// Menu items
	tc_c_Properties = (wxID_HIGHEST+1),
	tc_c_Sign,
	tc_c_Renew,
	tc_c_Import,
	tc_c_Load,
	tc_c_Export,
	tc_c_Delete,
	tc_c_Undelete,
	tc_c_New,
	tc_f_Delete,
	tc_h_Contents,
	tc_h_About,
	tl_c_Properties,
	tm_s_Properties,
	tl_c_Edit,
	tl_c_Delete,
};

enum {		// Window IDs
	tc_CertTree = (tl_c_Delete+1),
	tc_CRQWizard,
	tc_Load,
	tc_CertPropDial,
	tl_LocPropDial,
	tc_LocTree,
	tc_LogGrid,
	tl_Upload,
	tl_Login,
	tl_Save,
	tl_Edit,
	tl_AddLoc,
	tl_EditLoc,
	tl_DeleteLoc,
	tl_PropLoc,
	tc_CertSave,
	tc_CertRenew,
	tc_CertDelete,
	tc_CertProp,
	ID_CRQ_PROVIDER,
	ID_CRQ_PROVIDER_INFO,
	ID_CRQ_COUNTRY,
	ID_CRQ_ZIP,
	ID_CRQ_NAME,
	ID_CRQ_CITY,
	ID_CRQ_ADDR1,
	ID_CRQ_ADDR2,
	ID_CRQ_EMAIL,
	ID_CRQ_STATE,
	ID_CRQ_CALL,
	ID_CRQ_QBYEAR,
	ID_CRQ_QBMONTH,
	ID_CRQ_QBDAY,
	ID_CRQ_QEYEAR,
	ID_CRQ_QEMONTH,
	ID_CRQ_QEDAY,
	ID_CRQ_DXCC,
	ID_CRQ_TYPE,
	ID_CRQ_CERT,
	ID_CRQ_PW1,
	ID_CRQ_PW2,
	ID_CRQ_SIGN,
	ID_LCW_P12,
	ID_LCW_TQ6,
	ID_PREF_ROOT_CB,
	ID_PREF_CA_CB,
	ID_PREF_USER_CB,
	ID_PREF_ALLCERT_CB,
	ID_CERT_OK_BUT,
	ID_CERT_CAN_BUT,
	ID_CERT_HELP_BUT
};


#endif	// __tqslctrls_h
