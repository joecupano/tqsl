/***************************************************************************
                          loadcertwiz.h  -  description
                             -------------------
    begin                : Wed Aug 6 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef __loadcertwiz_h
#define __loadcertwiz_h

#include "extwizard.h"

class LCW_Page;
class notifyData;

class LoadCertWiz : public ExtWizard {
 public:
	explicit LoadCertWiz(wxWindow *parent, wxHtmlHelpController *help = 0, const wxString& title = wxEmptyString, const wxString& ext = wxT("tq6"));
	~LoadCertWiz();
	LCW_Page *GetCurrentPage() { return reinterpret_cast<LCW_Page *>(wxWizard::GetCurrentPage()); }
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	bool RunWizard();
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	void ResetNotifyData();
	notifyData *GetNotifyData() { return _nd; }
	wxWindow *Parent() { return _parent; }
	LCW_Page *Final() { return _final; }
	LCW_Page *P12PW() { return _p12pw; }
 private:
	LCW_Page *_first;
	LCW_Page *_final;
	LCW_Page *_p12pw;
	class notifyData *_nd;
	wxWindow *_parent;
};

class LCW_Page : public ExtWizard_Page {
 public:
	explicit LCW_Page(LoadCertWiz *parent) : ExtWizard_Page(parent) {}
	LoadCertWiz *Parent() { return reinterpret_cast<LoadCertWiz *>(_parent); }
};

class LCW_P12PasswordPage : public LCW_Page {
 public:
	explicit LCW_P12PasswordPage(LoadCertWiz *parent);
	virtual bool TransferDataFromWindow();
	wxString GetPassword() const;
	void SetFilename(const wxString& filename) { _filename = filename; }
 private:
	wxTextCtrl *_pwin;
	wxString _filename;
	wxStaticText *tc_status;
};

class LCW_FinalPage : public LCW_Page {
 public:
	explicit LCW_FinalPage(LoadCertWiz *parent);
	virtual void refresh();
 private:
	wxTextCtrl *tc_status;
};

class notifyData {
 public:
	struct counts {
		int loaded, error, duplicate;
	};
	struct counts root, ca, user, pkey, config;
	wxString status;
	notifyData() {
		root.loaded = root.error = root.duplicate = 0;
		ca.loaded = ca.error = ca.duplicate = 0;
		user.loaded = user.error = user.duplicate = 0;
		pkey.loaded = pkey.error = pkey.duplicate = 0;
		config.loaded = config.error = config.duplicate = 0;
		status = wxT("");
	}
	wxString Message() const;
};

inline bool
LoadCertWiz::RunWizard() {
	if (_first) {
		return wxWizard::RunWizard(_first);
	} else {
		return false;
	}
}

inline void
LoadCertWiz::ResetNotifyData() {
	if (_nd)
		delete _nd;
	_nd = new notifyData;
}

inline
LoadCertWiz::~LoadCertWiz() {
	if (_nd)
		delete _nd;
}

int notifyImport(int type, const char *message, void *);


#endif	// __loadcertwiz_h

