/***************************************************************************
                          tqslpaths.h  -  description
                             -------------------
    begin                : Mon Dec 9 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqslpaths.h,v 1.7 2013/03/01 13:09:28 k1mu Exp $
 ***************************************************************************/

#ifndef __tqslpaths_h
#define __tqslpaths_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#endif

#include <wx/filefn.h>
#if defined(__WIN32__)
	#include <windows.h>
#endif

#include "tqsllib.h"

class DocPaths : public wxPathList {
 public:
	explicit DocPaths(wxString subdir) : wxPathList() {
#ifdef _WIN32
		Add(wxGetHomeDir() + wxT("\\help\\") + subdir);
#else
		Add(wxGetHomeDir() + wxT("/help/") + subdir);
		Add(wxString::FromUTF8(tQSL_RsrcDir) + wxT("/help/"));
		Add(wxString::FromUTF8(tQSL_RsrcDir) + wxT("/help/") + subdir);
#endif
#if defined(_WIN32)
		HKEY hkey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\TrustedQSL",
			0, KEY_READ, &hkey) == ERROR_SUCCESS) {
			DWORD dtype;
			char path[TQSL_MAX_PATH_LEN];
			DWORD bsize = sizeof path;
			if (RegQueryValueEx(hkey, L"HelpDir", 0, &dtype, (LPBYTE)path, &bsize)
				== ERROR_SUCCESS) {
				Add(wxString::FromUTF8(path) + wxT("\\"));
				Add(wxString::FromUTF8(path) + wxT("\\") + subdir);
			}
		}
		Add(wxT("help\\") + subdir);
#elif defined(__APPLE__)
		CFBundleRef tqslBundle = CFBundleGetMainBundle();
		CFURLRef bundleURL = CFBundleCopyBundleURL(tqslBundle);
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(tqslBundle);
		if (bundleURL && resourcesURL) {
			CFStringRef bundleString = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
			CFStringRef resString = CFURLCopyFileSystemPath(resourcesURL, kCFURLPOSIXPathStyle);
			CFRelease(bundleURL);
			CFRelease(resourcesURL);

			CFIndex lenB = CFStringGetMaximumSizeForEncoding(CFStringGetLength(bundleString), kCFStringEncodingUTF8);
			CFIndex lenR = CFStringGetMaximumSizeForEncoding(CFStringGetLength(resString), kCFStringEncodingUTF8);

			char *npath = reinterpret_cast<char *>(malloc(lenB  + lenR + 10));
			if (npath) {
				CFStringGetCString(bundleString, npath, lenB + 1, kCFStringEncodingASCII);
				CFRelease(bundleString);

                		// if last char is not a /, append one
                		if ((strlen(npath) > 0) && (npath[strlen(npath)-1] != '/')) {
                        		strncat(npath, "/", 2);
                		}

				CFStringGetCString(resString, npath + strlen(npath), lenR + 1, kCFStringEncodingUTF8);

                		if ((strlen(npath) > 0) && (npath[strlen(npath)-1] != '/')) {
                        		strncat(npath, "/", 2);
                		}
                		Add(wxString::FromUTF8(npath));
                		Add(wxString::FromUTF8(npath) + wxT("Help/"));
				Add(wxT("/Applications/") + subdir + wxT(".app/Contents/Resources/"));
				Add(wxT("/Applications/") + subdir + wxT(".app/Contents/Resources/Help/"));
				free(npath);
			}
		}
#else
		Add(wxT(CONFDIR) wxT("help/"));
		Add(wxT(CONFDIR) wxT("help/") + subdir);
		Add(subdir);
#endif
	}
};
#endif		// tqslpaths_h
