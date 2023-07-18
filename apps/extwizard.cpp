/***************************************************************************
                          extwizard.cpp  -  description
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

#include "extwizard.h"
#include "tqsltrace.h"

BEGIN_EVENT_TABLE(ExtWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, ExtWizard::OnPageChanged)
END_EVENT_TABLE()

void
ExtWizard::OnPageChanged(wxWizardEvent& ev) {
	tqslTrace("ExtWizard::OnPageChanged", "Direction=%d", ev.GetDirection());
	GetCurrentPage()->refresh();
	GetCurrentPage()->SetFocus();
	GetCurrentPage()->validate();
}

void
ExtWizard::ReportSize(const wxSize& size) {
	tqslTrace("ExtWizard::ReportSize", "size=%d %d", size.GetWidth(), size.GetHeight());
	if (size.GetWidth() > _minsize.GetWidth())
		_minsize.SetWidth(size.GetWidth());
	if (size.GetHeight() > _minsize.GetHeight())
		_minsize.SetHeight(size.GetHeight());
}

ExtWizard::ExtWizard(wxWindow *parent, wxHtmlHelpController *help, const wxString& title) {
	tqslTrace("ExtWizard::ExtWizard", "parent=%lx, title=%s", reinterpret_cast<void *>(parent), S(title));

	SetExtraStyle(wxWIZARD_EX_HELPBUTTON);
	Create(parent, wxID_ANY, title);
	_help = help;
	CenterOnParent();
}

BEGIN_EVENT_TABLE(ExtWizard_Page, wxWizardPageSimple)
	EVT_WIZARD_HELP(-1, ExtWizard_Page::OnHelp)
END_EVENT_TABLE()
// FindWindowById(wxID_FORWARD,this->GetParent())->Hide()//Or->Disable()


void
ExtWizard_Page::check_valid(TQ_WXTEXTEVENT&) {
	tqslTrace("ExtWizard_Page::check_valid", NULL);
	validate();
}

void
ExtWizard_Page::AdjustPage(wxBoxSizer *sizer, const wxString& helpfile) {
	tqslTrace("ExtWizard_Page::AdjustPage", NULL);

	_helpfile = helpfile;

	if (!_helpfile.IsEmpty() && _parent->HaveHelp()) {
		FindWindowById(wxID_HELP, this->GetParent())->Enable(); // or Show()
	} else {
		FindWindowById(wxID_HELP, this->GetParent())->Disable(); // or Hide()
	}

	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->SetSizeHints(this);
	Layout();
	_parent->ReportSize(sizer->CalcMin());
}
