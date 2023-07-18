/***************************************************************************
                          util.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __util_h
#define __util_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

class CertTreeItemData;
class LocTreeItemData;
void displayCertProperties(CertTreeItemData *item, wxWindow *parent = 0);
void displayLocProperties(LocTreeItemData *item, wxWindow *parent = 0);
int getPassword(char *buf, int bufsiz, void *);
void displayTQSLError(const char *pre);
wxMenu *makeCertificateMenu(bool enable, bool keyonly = false, const char* callsign = NULL);
wxMenu *makeLocationMenu(bool enable);

#endif	// __util_h
