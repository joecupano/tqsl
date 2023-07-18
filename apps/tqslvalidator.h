/***************************************************************************
                          tqslvalidator.h  -  description
                             -------------------
    begin                : Sun Dec 8 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef TQSLVALIDATOR_H
#define TQSLVALIDATOR_H

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "tqsllib.h"

#include "wx/grid.h"

#include <wx/validate.h>

/** Classes work with wxWindows wxTextCtrl to validate TQSL Date/Time entries.
  *
  *@author Jon Bloom
  */

class TQSLValidator : public wxValidator {
 public:
	explicit TQSLValidator(void *objp);
	TQSLValidator(const TQSLValidator& val) { Copy(val); }
	virtual ~TQSLValidator() {}
	virtual bool Copy(const TQSLValidator&);
	virtual bool Validate(wxWindow* win);
	virtual bool TransferToWindow();
	virtual bool TransferFromWindow();
	virtual wxString ToString() = 0;
	virtual void FromString(const wxString&) = 0;
	virtual bool IsValid(const wxString&) = 0;
 protected:
	wxString _type;
	void *_objp;
};

class TQSLDateValidator : public TQSLValidator {
 public:
	explicit TQSLDateValidator(tQSL_Date *date) : TQSLValidator(date) { _type = _("Date"); }
	TQSLDateValidator(const TQSLDateValidator& val) : TQSLValidator(val) {}
	virtual ~TQSLDateValidator() {}
	virtual wxObject *Clone() const { return new TQSLDateValidator(*this); }
	virtual wxString ToString();
	virtual void FromString(const wxString&);
	virtual bool IsValid(const wxString&);
};

class TQSLTimeValidator : public TQSLValidator {
 public:
	explicit TQSLTimeValidator(tQSL_Time *time) : TQSLValidator(time) { _type = _("Time"); }
	TQSLTimeValidator(const TQSLTimeValidator& val) : TQSLValidator(val) {}
	virtual ~TQSLTimeValidator() {}
	virtual wxObject *Clone() const { return new TQSLTimeValidator(*this); }
	virtual wxString ToString();
	virtual void FromString(const wxString&);
	virtual bool IsValid(const wxString&);
};

#endif	// TQSLVALIDATOR_H
