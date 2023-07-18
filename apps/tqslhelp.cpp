/***************************************************************************
                          tqslhelp.cpp  -  description
                             -------------------
          copyright (C) 2013-2017 by ARRL and the TrustedQSL Developers
 ***************************************************************************/

//Derived from wxWidgets fs_inet.cpp
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if !wxUSE_SOCKETS
    #undef wxUSE_FS_INET
    #define wxUSE_FS_INET 0
#endif

#if wxUSE_FILESYSTEM && wxUSE_FS_INET

#ifndef WXPRECOMP
    #include "wx/module.h"
#endif

#include "wx/wfstream.h"
#include "wx/url.h"
#include "wx/filesys.h"
#include "wx/utils.h"
#include "wx/mstream.h"
#include "tqslhelp.h"
#include "tqsltrace.h"

// ----------------------------------------------------------------------------
// tqslInternetFSHandler
// ----------------------------------------------------------------------------

static wxString StripProtocolAnchor(const wxString& location) {
	tqslTrace("StripProtocolAnchor", "location=%s", S(location));
	wxString myloc(location.BeforeLast(wxT('#')));
	if (myloc.empty())
		myloc = location.AfterFirst(wxT(':'));
	else
		myloc = myloc.AfterFirst(wxT(':'));

	// fix malformed url
	if (!myloc.Left(2).IsSameAs(wxT("//"))) {
		if (myloc.GetChar(0) != wxT('/'))
			myloc = wxT("//") + myloc;
		else
			myloc = wxT("/") + myloc;
	}
	if (myloc.Mid(2).Find(wxT('/')) == wxNOT_FOUND) myloc << wxT('/');

	return myloc;
}

//
// Simple filesystem "handler" that notices full URLs and
// opens them in the user's default browser.
//
// Always returns an error indication so that the help widget doesn't
// think it's actually been handled.
//
bool tqslInternetFSHandler::CanOpen(const wxString& location) {
	tqslTrace("tqslInternetFSHandler::CanOpen", "location=%s", S(location));
	static wxString lastLocation;
#if wxUSE_URL
	wxString p = GetProtocol(location);
	if ((p == wxT("http")) || (p == wxT("ftp"))) {
		// Keep track of the last location as we get
		// repeated attempts to open - only open
		// the page once.
		if (location != lastLocation) {
			wxString right = GetProtocol(location) + wxT(":") +
			StripProtocolAnchor(location);
			wxLaunchDefaultBrowser(right);
		}
		lastLocation = location;
	}
#endif
	return false;
}

wxFSFile* tqslInternetFSHandler::OpenFile(wxFileSystem& WXUNUSED(fs), const wxString& location) {
	return NULL; // We never actually return anything
}

class tqslFileSystemInternetModule : public wxModule {
	DECLARE_DYNAMIC_CLASS(tqslFileSystemInternetModule)

 public:
	tqslFileSystemInternetModule()
	    : wxModule(), m_handler(NULL) {
	}

	virtual bool OnInit() {
	    m_handler = new tqslInternetFSHandler;
	    wxFileSystem::AddHandler(m_handler);
	    return true;
	}

	virtual void OnExit() {
	    delete wxFileSystem::RemoveHandler(m_handler);
	}

 private:
	wxFileSystemHandler* m_handler;
};

IMPLEMENT_DYNAMIC_CLASS(tqslFileSystemInternetModule, wxModule)

#endif // wxUSE_FILESYSTEM && wxUSE_FS_INET
