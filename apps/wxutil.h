/***************************************************************************
                          wxutil.h  -  description
                             -------------------
    begin                : Thu Aug 14 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __wxutil_h
#define __wxutil_h


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

#include <wx/intl.h>

// This macro marks a c-string to be extracted for translation. xgettext is directed to look for _() and __().

#define __(x) (x)

#if wxCHECK_VERSION(2, 5, 0)
	#define TQ_WXCLOSEEVENT wxCloseEvent
	#define TQ_WXTEXTEVENT wxCommandEvent
	#define TQ_WXCOOKIE wxTreeItemIdValue
#else
	#define TQ_WXCLOSEEVENT wxCommandEvent
	#define TQ_WXTEXTEVENT wxEvent
	#define TQ_WXCOOKIE long
#endif

wxSize getTextSize(wxWindow *win);
wxString wrapString(wxWindow *win, wxString in, int length);
wxString urlEncode(wxString& str);
int utf8_to_ucs2(const char *in, char *out, size_t buflen);
int getPasswordFromUser(wxString& result, const wxString& message, const wxString& caption, const wxString& defaultValue, wxWindow *parent);
wxString getLocalizedErrorString(void);
wxLanguage langWX2toWX3(wxLanguage wx2);

#endif	// __wxutil_h
