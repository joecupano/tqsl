/***************************************************************************
                          tqslhelp.h  -  description
                             -------------------
          copyright (C) 2013-2017 by ARRL and the TrustedQSL Developers
 ***************************************************************************/

// Derived from wxWidgets fs_inet.h

#ifndef _tqslhelp_h
#define _tqslhelp_h

#include "wx/defs.h"

#include "wx/filesys.h"

// ----------------------------------------------------------------------------
// tqslInternetFSHandler
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_NET tqslInternetFSHandler : public wxFileSystemHandler {
 public:
        virtual bool CanOpen(const wxString& location);
        virtual wxFSFile* OpenFile(wxFileSystem& fs, const wxString& location);
};

#endif // _tqslhelp_h

