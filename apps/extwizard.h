/***************************************************************************
                          extwizard.h  -  description
                             -------------------
    begin                : Thu Aug 7 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef __extwizard_h
#define __extwizard_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wx/wxprec.h"
#include "wxutil.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "wx/wizard.h"
#include "wx/wxhtml.h"

class ExtWizard_Page;

class ExtWizard : public wxWizard {
 public:
	explicit ExtWizard(wxWindow *parent, wxHtmlHelpController *help = 0, const wxString& title = wxEmptyString);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	ExtWizard_Page *GetCurrentPage() { return reinterpret_cast<ExtWizard_Page *>(wxWizard::GetCurrentPage()); }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	wxHtmlHelpController *GetHelp() { return _help; }
	void DisplayHelp(const wxString& file) { if (_help) _help->Display(file); }
	void ReportSize(const wxSize& size);
	void AdjustSize() { SetPageSize(_minsize); }
	bool HaveHelp() const { return _help != 0; }
 protected:
	void OnPageChanged(wxWizardEvent&);
	wxHtmlHelpController *_help;
	wxSize _minsize;

	DECLARE_EVENT_TABLE()
};

class ExtWizard_Page : public wxWizardPageSimple {
 public:
	explicit ExtWizard_Page(ExtWizard *parent) : wxWizardPageSimple(parent), _parent(parent), _helpfile(wxT("")) { }

	virtual const char *validate() { return NULL; }	// Returns error message string or NULL=no error
	virtual void refresh() { }	// Updates page contents based on page-specific criteria
	void check_valid(TQ_WXTEXTEVENT&);
 protected:
	ExtWizard *_parent;
	void AdjustPage(wxBoxSizer *sizer, const wxString& helpfile = wxT(""));
 private:
	void OnHelp(wxWizardEvent&) { if (_helpfile != wxT("")) _parent->DisplayHelp(_helpfile); }
	wxString _helpfile;

	DECLARE_EVENT_TABLE();
};

#endif	// __extwizard_h
