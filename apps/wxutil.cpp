/***************************************************************************
                          wxutil.cpp  -  description
                             -------------------
    begin                : Thu Aug 14 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wxutil.h"
#include <wx/dir.h>
#include <wx/config.h>
#include <wx/filename.h>
#include "tqsllib.h"
#include "tqslerrno.h"

#if wxMAJOR_VERSION == 3 && wxMINOR_VERSION > 0
#define WX31		// wxWidgets isn't done until stuff doesn't run
#endif

wxSize
getTextSize(wxWindow *win) {
	wxClientDC dc(win);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);
	return wxSize(char_width, char_height);
}

// isspace() called on extended chars in UTF-8 raises asserts in
// the windows C++ libs. Don't call isspace() if out of range.

static inline int isspc(int c) {
	if (c < 0 || c > 255)
		return 0;
	return isspace(c);
}

wxString
wrapString(wxWindow *win, wxString in, int length) {
	wxClientDC dc(win);
	wxCoord textwidth, textheight;
	wxString out = wxT("");
	wxString str = in;
	do {
		dc.GetTextExtent(str, &textwidth, &textheight);
		if (textwidth > length) {
			int index = 0;
				do {
					str = str.Left(str.Length()-1);
					index++;
					dc.GetTextExtent(str, &textwidth, &textheight);

					wxString c = wxString(str.Last());

					if (textwidth < length && isspc(c[0]))
						break;
				} while (1);
			if (!out.IsEmpty())
				out += wxT("\n");
			out += str;
			str = in.Right(index);
		} else {
			break;
		}
	} while (1);
	if (!out.IsEmpty())
		out += wxT("\n");
	out += str;
	return out;
}

// Strip special characters from a string prior to writing to XML
wxString
urlEncode(wxString& str) {
	str.Replace(wxT("&"), wxT("&amp;"), true);
	str.Replace(wxT("\""), wxT("&quot;"), true);
	str.Replace(wxT("'"), wxT("&apos;"), true);
	str.Replace(wxT("<"), wxT("&lt;"), true);
	str.Replace(wxT(">"), wxT("&gt;"), true);
	return str;
}

// Convert UTF-8 string to UCS-2 (MS Unicode default)
int
utf8_to_ucs2(const char *in, char *out, size_t buflen) {
	size_t len = 0;

	while (len < buflen) {
		if ((unsigned char)*in < 0x80) {		// ASCII range
			*out++ = *in;
			if (*in++ == '\0')			// End of string
				break;
			len++;
		} else if (((unsigned char)*in & 0xc0) == 0xc0) {  // Two-byte
			*out++ = ((in[0] & 0x1f) << 6) | (in[1] & 0x3f);
			in += 2;
			len++;
		} else if (((unsigned char)*in & 0xe0) == 0xe0) {  // Three-byte
			unsigned short three =	((in[0] & 0x0f) << 12) |
						 ((in[1] & 0x3f) << 6) |
						 (in[2] & 0x3f);
			*out++ = (three & 0xff00) >> 8;
			len++;
			if (len < buflen) {
				*out++ = (three & 0xff);
				len++;
			}
			in += 3;
		} else {
			in++;		// Unknown. Skip input.
		}
	}
	out[len-1] = '\0';
	return len;
}
int
getPasswordFromUser(wxString& result, const wxString& message, const wxString& caption, const wxString& defaultValue, wxWindow *parent) {
	long style = wxTextEntryDialogStyle;

	wxPasswordEntryDialog dialog(parent, message, caption, defaultValue, style);

	int ret = dialog.ShowModal();
	if (ret == wxID_OK)
		result = dialog.GetValue();

	return ret;
}

static const char *error_strings[] = {
	__("Memory allocation failure"),			/* TQSL_ALLOC_ERROR */
	__("Unable to initialize random number generator"),	/* TQSL_RANDOM_ERROR */
	__("Invalid argument"),					/* TQSL_ARGUMENT_ERROR */
	__("Operator aborted operation"),			/* TQSL_OPERATOR_ABORT */
	__("No Certificate Request matches the selected Callsign Certificate"),/* TQSL_NOKEY_ERROR */
	__("Buffer too small"),					/* TQSL_BUFFER_ERROR */
	__("Invalid date format"),				/* TQSL_INVALID_DATE */
	__("Certificate not initialized for signing"),		/* TQSL_SIGNINIT_ERROR */
	__("Passphrase not correct"),				/* TQSL_PASSWORD_ERROR */
	__("Expected name"),					/* TQSL_EXPECTED_NAME */
	__("Name exists"),					/* TQSL_NAME_EXISTS */
	__("Data for this DXCC entity could not be found"),	/* TQSL_NAME_NOT_FOUND */
	__("Invalid time format"),				/* TQSL_INVALID_TIME */
	__("QSO date is not within the date range specified on your Callsign Certificate"),	/* TQSL_CERT_DATE_MISMATCH */
	__("Certificate provider not found"),			/* TQSL_PROVIDER_NOT_FOUND */
	__("No callsign certificate for key"),			/* TQSL_CERT_KEY_ONLY */
	__("Configuration file cannot be opened"),		/* TQSL_CONFIG_ERROR */
	__("The private key for this Callsign Certificate is not present on this computer; you can obtain it by loading a .tbk or .p12 file"),				      /* TQSL_CERT_NOT_FOUND */
	__("PKCS#12 file not TQSL compatible"),			/* TQSL_PKCS12_ERROR */
	__("Callsign Certificate not TQSL compatible"),		/* TQSL_CERT_TYPE_ERROR */
	__("Date out of range"),				/* TQSL_DATE_OUT_OF_RANGE */
	__("Already Uploaded QSO suppressed"),			/* TQSL_DUPLICATE_QSO */
	__("Database error"),					/* TQSL_DB_ERROR */
	__("The selected station location could not be found"),	/* TQSL_LOCATION_NOT_FOUND */
	__("The selected callsign could not be found"),		/* TQSL_CALL_NOT_FOUND */
	__("The TQSL configuration file cannot be parsed"),	/* TQSL_CONFIG_SYNTAX_ERROR */
	__("This file can not be processed due to a system error"),	/* TQSL_FILE_SYSTEM_ERROR */
	__("The format of this file is incorrect."),		/* TQSL_FILE_SYNTAX_ERROR */
	__("Callsign certificate could not be installed"),	/* TQSL_CERT_ERROR */
        __("Callsign Certificate does not match QSO details"),	/* TQSL_CERT_MISMATCH */
        __("Station Location does not match QSO details"),	/* TQSL_LOCATION_MISMATCH */
};

static wxString
getLocalizedErrorString_v(int err) {
	int adjusted_err;
	wxString buf;

	if (err == 0)
		return _("NO ERROR");
	if (err == TQSL_CUSTOM_ERROR) {
		if (tQSL_CustomError[0] == 0) {
			return _("Unknown custom error");
		} else {
			return wxString::FromUTF8(tQSL_CustomError);
		}
	}
	if (err == TQSL_DB_ERROR && tQSL_CustomError[0] != 0) {
		return wxString::Format(_("Database Error: %hs"), tQSL_CustomError);
	}

	if (err == TQSL_SYSTEM_ERROR || err == TQSL_FILE_SYSTEM_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(_("System error: %hs : %hs"),
				tQSL_ErrorFile, strerror(tQSL_Errno));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::Format(_("System error: %hs"),
				strerror(tQSL_Errno));
		}
		return buf;
	}
	if (err == TQSL_FILE_SYNTAX_ERROR) {
		tqslTrace("SyntaxError", "File (partial) content '%s'", tQSL_CustomError);
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(_("File syntax error: %hs"),
				tQSL_ErrorFile);
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = _("File syntax error");
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_ERROR) {
		// Get error details from tqsllib as we have
		// no visibility into the tqsllib openssl context
		const char *msg = tqsl_getErrorString();
		return wxString::FromUTF8(msg);
	}
	if (err == TQSL_ADIF_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(wxT("%hs: %hs"), tQSL_ErrorFile, tqsl_adifGetError(tQSL_ADIF_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::FromUTF8(tqsl_adifGetError(tQSL_ADIF_Error));
		}
		return buf;
	}
	if (err == TQSL_CABRILLO_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(wxT("%hs: %hs"),
				tQSL_ErrorFile, tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::FromUTF8(tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_VERSION_ERROR) {
		// No visibility into the tqsllib openssl context
		const char *msg = tqsl_getErrorString();
		return wxString::FromUTF8(msg);
	}
	if (err == TQSL_CERT_NOT_FOUND && tQSL_ImportCall[0] != '\0') {
		return wxString::Format(
			_("The private key for callsign %hs serial %ld is not present on this computer; you can obtain it by loading a .tbk or .p12 file"),
			tQSL_ImportCall, tQSL_ImportSerial);
	}
	adjusted_err = (err - TQSL_ERROR_ENUM_BASE) & ~0x1000;
	if (adjusted_err < 0 ||
	    adjusted_err >=
		static_cast<int>(sizeof error_strings / sizeof error_strings[0])) {
		return wxString::Format(_("Invalid error code: %d"), err);
	}

	if (err == TQSL_CERT_MISMATCH || err == TQSL_LOCATION_MISMATCH) {
		const char *fld, *cert, *qso;
		fld = strtok(tQSL_CustomError, "|");
		cert = strtok(NULL, "|");
		qso = strtok(NULL, "|");
		if (qso == NULL) {		// Nothing in the cert
			qso = cert;
			cert = "none";
		}
		wxString tp(_("Callsign Certificate"));
		if (err == TQSL_LOCATION_MISMATCH)
			tp = wxString(_("Station Location"));
	 	wxString composed = wxGetTranslation(wxString::FromUTF8(error_strings[adjusted_err]));
		// TRANSLATORS: This message is for QSO details. For example, 'The Station Location GRIDSQUARE has value FM18ju while QSO has FM18jt'
		composed = composed + wxT("\n") + wxString::Format(_("The %s '%hs' has value '%hs' while QSO has '%hs'"), tp.c_str(), fld, cert, qso);
		return composed;
	}
	if (err == (TQSL_LOCATION_MISMATCH | 0x1000)) {
		const char *fld, *val;
		fld = strtok(tQSL_CustomError, "|");
		val = strtok(NULL, "|");
	 	wxString composed(_("This log has invalid QSO information"));
		// TRANSLATORS: This message is for QSO details. For example, 'The log being signed has 'US County' set to Foobar which is not valid'
		composed = composed + wxT("\n") + wxString::Format(_("The log being signed has '%hs' set to value '%hs' which is not valid"), fld, val);
		return composed;
	}
	if (err == (TQSL_CERT_NOT_FOUND | 0x1000)) {
		err = TQSL_CERT_NOT_FOUND;
		const char *call, *ent;
		call = strtok(tQSL_CustomError, "|");
		ent = strtok(NULL, "|");
		wxString composed = wxString::Format(_("There is no valid callsign certificate for %hs in entity %hs available. This QSO cannot be signed"), call, ent);
		return composed;
	}
	return wxGetTranslation(wxString::FromUTF8(error_strings[adjusted_err]));
}

wxString
getLocalizedErrorString() {
	wxString cp = getLocalizedErrorString_v(tQSL_Error);
	tQSL_Error = TQSL_NO_ERROR;
	tQSL_Errno = 0;
	tQSL_ErrorFile[0] = 0;
	tQSL_CustomError[0] = 0;
	return cp;
}
/*
 * wxWidgets used to keep language IDs stable, by adding to the end of the
 * enum. wx3 seems to have broken that promise by injecting several languages
 * in the middle. But we use language IDs to specify the local language. Horrid.
 * So, here's a map from old to new.
 */

#if wxMAJOR_VERSION > 2
	struct langMap {
		wxLanguage wx3;
		int wx2;
	};

	struct langMap mapping[] = {
	    { wxLANGUAGE_DEFAULT, 0 },
	    { wxLANGUAGE_UNKNOWN, 1 },
	    { wxLANGUAGE_ABKHAZIAN, 2 },
	    { wxLANGUAGE_AFAR, 3 },
	    { wxLANGUAGE_AFRIKAANS, 4 },
	    { wxLANGUAGE_ALBANIAN, 5 },
	    { wxLANGUAGE_AMHARIC, 6 },
	    { wxLANGUAGE_ARABIC, 7 },
	    { wxLANGUAGE_ARABIC_ALGERIA, 8 },
	    { wxLANGUAGE_ARABIC_BAHRAIN, 9 },
	    { wxLANGUAGE_ARABIC_EGYPT, 10 },
	    { wxLANGUAGE_ARABIC_IRAQ, 11 },
	    { wxLANGUAGE_ARABIC_JORDAN, 12 },
	    { wxLANGUAGE_ARABIC_KUWAIT, 13 },
	    { wxLANGUAGE_ARABIC_LEBANON, 14 },
	    { wxLANGUAGE_ARABIC_LIBYA, 15 },
	    { wxLANGUAGE_ARABIC_MOROCCO, 16 },
	    { wxLANGUAGE_ARABIC_OMAN, 17 },
	    { wxLANGUAGE_ARABIC_QATAR, 18 },
	    { wxLANGUAGE_ARABIC_SAUDI_ARABIA, 19 },
	    { wxLANGUAGE_ARABIC_SUDAN, 20 },
	    { wxLANGUAGE_ARABIC_SYRIA, 21 },
	    { wxLANGUAGE_ARABIC_TUNISIA, 22 },
	    { wxLANGUAGE_ARABIC_UAE, 23 },
	    { wxLANGUAGE_ARABIC_YEMEN, 24 },
	    { wxLANGUAGE_ARMENIAN, 25 },
	    { wxLANGUAGE_ASSAMESE, 26 },
	    { wxLANGUAGE_ASTURIAN, wxLANGUAGE_ASTURIAN + 0x8000 },
	    { wxLANGUAGE_AYMARA, 27 },
	    { wxLANGUAGE_AZERI, 28 },
	    { wxLANGUAGE_AZERI_CYRILLIC, 29 },
	    { wxLANGUAGE_AZERI_LATIN, 30 },
	    { wxLANGUAGE_BASHKIR, 31 },
	    { wxLANGUAGE_BASQUE, 32 },
	    { wxLANGUAGE_BELARUSIAN, 33 },
	    { wxLANGUAGE_BENGALI, 34 },
	    { wxLANGUAGE_BHUTANI, 35 },
	    { wxLANGUAGE_BIHARI, 36 },
	    { wxLANGUAGE_BISLAMA, 37 },
	    { wxLANGUAGE_BOSNIAN, wxLANGUAGE_BOSNIAN + 0x8000 },
	    { wxLANGUAGE_BRETON, 38 },
	    { wxLANGUAGE_BULGARIAN, 39 },
	    { wxLANGUAGE_BURMESE, 40 },
#ifndef WX31
	    { wxLANGUAGE_CAMBODIAN, 41 },
#endif
	    { wxLANGUAGE_CATALAN, 42 },
	    { wxLANGUAGE_CHINESE, 43 },
	    { wxLANGUAGE_CHINESE_SIMPLIFIED, 44 },
	    { wxLANGUAGE_CHINESE_TRADITIONAL, 45 },
	    { wxLANGUAGE_CHINESE_HONGKONG, 46 },
	    { wxLANGUAGE_CHINESE_MACAU, 47 },
	    { wxLANGUAGE_CHINESE_SINGAPORE, 48 },
	    { wxLANGUAGE_CHINESE_TAIWAN, 49 },
	    { wxLANGUAGE_CORSICAN, 50 },
	    { wxLANGUAGE_CROATIAN, 51 },
	    { wxLANGUAGE_CZECH, 52 },
	    { wxLANGUAGE_DANISH, 53 },
	    { wxLANGUAGE_DUTCH, 54 },
	    { wxLANGUAGE_DUTCH_BELGIAN, 55 },
	    { wxLANGUAGE_ENGLISH, 56 },
	    { wxLANGUAGE_ENGLISH_UK, 57 },
	    { wxLANGUAGE_ENGLISH_US, 58 },
	    { wxLANGUAGE_ENGLISH_AUSTRALIA, 59 },
	    { wxLANGUAGE_ENGLISH_BELIZE, 60 },
	    { wxLANGUAGE_ENGLISH_BOTSWANA, 61 },
	    { wxLANGUAGE_ENGLISH_CANADA, 62 },
	    { wxLANGUAGE_ENGLISH_CARIBBEAN, 63 },
	    { wxLANGUAGE_ENGLISH_DENMARK, 64 },
	    { wxLANGUAGE_ENGLISH_EIRE, 65 },
	    { wxLANGUAGE_ENGLISH_JAMAICA, 66 },
	    { wxLANGUAGE_ENGLISH_NEW_ZEALAND, 67 },
	    { wxLANGUAGE_ENGLISH_PHILIPPINES, 68 },
	    { wxLANGUAGE_ENGLISH_SOUTH_AFRICA, 69 },
	    { wxLANGUAGE_ENGLISH_TRINIDAD, 70 },
	    { wxLANGUAGE_ENGLISH_ZIMBABWE, 71 },
	    { wxLANGUAGE_ESPERANTO, 72 },
	    { wxLANGUAGE_ESTONIAN, 73 },
	    { wxLANGUAGE_FAEROESE, 74 },
	    { wxLANGUAGE_FARSI, 75 },
	    { wxLANGUAGE_FIJI, 76 },
	    { wxLANGUAGE_FINNISH, 77 },
	    { wxLANGUAGE_FRENCH, 78 },
	    { wxLANGUAGE_FRENCH_BELGIAN, 79 },
	    { wxLANGUAGE_FRENCH_CANADIAN, 80 },
	    { wxLANGUAGE_FRENCH_LUXEMBOURG, 81 },
	    { wxLANGUAGE_FRENCH_MONACO, 82 },
	    { wxLANGUAGE_FRENCH_SWISS, 83 },
	    { wxLANGUAGE_FRISIAN, 84 },
	    { wxLANGUAGE_GALICIAN, 85 },
	    { wxLANGUAGE_GEORGIAN, 86 },
	    { wxLANGUAGE_GERMAN, 87 },
	    { wxLANGUAGE_GERMAN_AUSTRIAN, 88 },
	    { wxLANGUAGE_GERMAN_BELGIUM, 89 },
	    { wxLANGUAGE_GERMAN_LIECHTENSTEIN, 90 },
	    { wxLANGUAGE_GERMAN_LUXEMBOURG, 91 },
	    { wxLANGUAGE_GERMAN_SWISS, 92 },
	    { wxLANGUAGE_GREEK, 93 },
	    { wxLANGUAGE_GREENLANDIC, 94 },
	    { wxLANGUAGE_GUARANI, 95 },
	    { wxLANGUAGE_GUJARATI, 96 },
	    { wxLANGUAGE_HAUSA, 97 },
	    { wxLANGUAGE_HEBREW, 98 },
	    { wxLANGUAGE_HINDI, 99 },
	    { wxLANGUAGE_HUNGARIAN, 100 },
	    { wxLANGUAGE_ICELANDIC, 101 },
	    { wxLANGUAGE_INDONESIAN, 102 },
	    { wxLANGUAGE_INTERLINGUA, 103 },
	    { wxLANGUAGE_INTERLINGUE, 104 },
	    { wxLANGUAGE_INUKTITUT, 105 },
	    { wxLANGUAGE_INUPIAK, 106 },
	    { wxLANGUAGE_IRISH, 107 },
	    { wxLANGUAGE_ITALIAN, 108 },
	    { wxLANGUAGE_ITALIAN_SWISS, 109 },
	    { wxLANGUAGE_JAPANESE, 110 },
	    { wxLANGUAGE_JAVANESE, 111 },
#ifdef WX31
	    { wxLANGUAGE_KABYLE, wxLANGUAGE_KABYLE + 0x8000 },
#endif
	    { wxLANGUAGE_KANNADA, 112 },
	    { wxLANGUAGE_KASHMIRI, 113 },
	    { wxLANGUAGE_KASHMIRI_INDIA, 114 },
	    { wxLANGUAGE_KAZAKH, 115 },
	    { wxLANGUAGE_KERNEWEK, 116 },
#ifdef WX31
	    { wxLANGUAGE_KHMER, 41 },
#endif
	    { wxLANGUAGE_KINYARWANDA, 117 },
	    { wxLANGUAGE_KIRGHIZ, 118 },
	    { wxLANGUAGE_KIRUNDI, 119 },
	    { wxLANGUAGE_KONKANI, 120 },
	    { wxLANGUAGE_KOREAN, 121 },
	    { wxLANGUAGE_KURDISH, 122 },
	    { wxLANGUAGE_LAOTHIAN, 123 },
	    { wxLANGUAGE_LATIN, 124 },
	    { wxLANGUAGE_LATVIAN, 125 },
	    { wxLANGUAGE_LINGALA, 126 },
	    { wxLANGUAGE_LITHUANIAN, 127 },
	    { wxLANGUAGE_MACEDONIAN, 128 },
	    { wxLANGUAGE_MALAGASY, 129 },
	    { wxLANGUAGE_MALAY, 130 },
	    { wxLANGUAGE_MALAYALAM, 131 },
	    { wxLANGUAGE_MALAY_BRUNEI_DARUSSALAM, 132 },
	    { wxLANGUAGE_MALAY_MALAYSIA, 133 },
	    { wxLANGUAGE_MALTESE, 134 },
	    { wxLANGUAGE_MANIPURI, 135 },
	    { wxLANGUAGE_MAORI, 136 },
	    { wxLANGUAGE_MARATHI, 137 },
	    { wxLANGUAGE_MOLDAVIAN, 138 },
	    { wxLANGUAGE_MONGOLIAN, 139 },
	    { wxLANGUAGE_NAURU, 140 },
	    { wxLANGUAGE_NEPALI, 141 },
	    { wxLANGUAGE_NEPALI_INDIA, 142 },
	    { wxLANGUAGE_NORWEGIAN_BOKMAL, 143 },
	    { wxLANGUAGE_NORWEGIAN_NYNORSK, 144 },
	    { wxLANGUAGE_OCCITAN, 145 },
	    { wxLANGUAGE_ORIYA, 146 },
	    { wxLANGUAGE_OROMO, 147 },
	    { wxLANGUAGE_PASHTO, 148 },
	    { wxLANGUAGE_POLISH, 149 },
	    { wxLANGUAGE_PORTUGUESE, 150 },
	    { wxLANGUAGE_PORTUGUESE_BRAZILIAN, 151 },
	    { wxLANGUAGE_PUNJABI, 152 },
	    { wxLANGUAGE_QUECHUA, 153 },
	    { wxLANGUAGE_RHAETO_ROMANCE, 154 },
	    { wxLANGUAGE_ROMANIAN, 155 },
	    { wxLANGUAGE_RUSSIAN, 156 },
	    { wxLANGUAGE_RUSSIAN_UKRAINE, 157 },
	    { wxLANGUAGE_SAMI, 0x20000000 },
	    { wxLANGUAGE_SAMOAN, 158 },
	    { wxLANGUAGE_SANGHO, 159 },
	    { wxLANGUAGE_SANSKRIT, 160 },
	    { wxLANGUAGE_SCOTS_GAELIC, 161 },
	    { wxLANGUAGE_SERBIAN, 162 },
	    { wxLANGUAGE_SERBIAN_CYRILLIC, 163 },
	    { wxLANGUAGE_SERBIAN_LATIN, 164 },
	    { wxLANGUAGE_SERBO_CROATIAN, 165 },
	    { wxLANGUAGE_SESOTHO, 166 },
	    { wxLANGUAGE_SETSWANA, 167 },
	    { wxLANGUAGE_SHONA, 168 },
	    { wxLANGUAGE_SINDHI, 169 },
	    { wxLANGUAGE_SINHALESE, 170 },
	    { wxLANGUAGE_SISWATI, 171 },
	    { wxLANGUAGE_SLOVAK, 172 },
	    { wxLANGUAGE_SLOVENIAN, 173 },
	    { wxLANGUAGE_SOMALI, 174 },
	    { wxLANGUAGE_SPANISH, 175 },
	    { wxLANGUAGE_SPANISH_ARGENTINA, 176 },
	    { wxLANGUAGE_SPANISH_BOLIVIA, 177 },
	    { wxLANGUAGE_SPANISH_CHILE, 178 },
	    { wxLANGUAGE_SPANISH_COLOMBIA, 179 },
	    { wxLANGUAGE_SPANISH_COSTA_RICA, 180 },
	    { wxLANGUAGE_SPANISH_DOMINICAN_REPUBLIC, 181 },
	    { wxLANGUAGE_SPANISH_ECUADOR, 182 },
	    { wxLANGUAGE_SPANISH_EL_SALVADOR, 183 },
	    { wxLANGUAGE_SPANISH_GUATEMALA, 184 },
	    { wxLANGUAGE_SPANISH_HONDURAS, 185 },
	    { wxLANGUAGE_SPANISH_MEXICAN, 186 },
	    { wxLANGUAGE_SPANISH_MODERN, 187 },
	    { wxLANGUAGE_SPANISH_NICARAGUA, 188 },
	    { wxLANGUAGE_SPANISH_PANAMA, 189 },
	    { wxLANGUAGE_SPANISH_PARAGUAY, 190 },
	    { wxLANGUAGE_SPANISH_PERU, 191 },
	    { wxLANGUAGE_SPANISH_PUERTO_RICO, 192 },
	    { wxLANGUAGE_SPANISH_URUGUAY, 193 },
	    { wxLANGUAGE_SPANISH_US, 194 },
	    { wxLANGUAGE_SPANISH_VENEZUELA, 195 },
	    { wxLANGUAGE_SUNDANESE, 196 },
	    { wxLANGUAGE_SWAHILI, 197 },
	    { wxLANGUAGE_SWEDISH, 198 },
	    { wxLANGUAGE_SWEDISH_FINLAND, 199 },
	    { wxLANGUAGE_TAGALOG, 200 },
	    { wxLANGUAGE_TAJIK, 201 },
	    { wxLANGUAGE_TAMIL, 202 },
	    { wxLANGUAGE_TATAR, 203 },
	    { wxLANGUAGE_TELUGU, 204 },
	    { wxLANGUAGE_THAI, 205 },
	    { wxLANGUAGE_TIBETAN, 206 },
	    { wxLANGUAGE_TIGRINYA, 207 },
	    { wxLANGUAGE_TONGA, 208 },
	    { wxLANGUAGE_TSONGA, 209 },
	    { wxLANGUAGE_TURKISH, 210 },
	    { wxLANGUAGE_TURKMEN, 211 },
	    { wxLANGUAGE_TWI, 212 },
	    { wxLANGUAGE_UIGHUR, 213 },
	    { wxLANGUAGE_UKRAINIAN, 214 },
	    { wxLANGUAGE_URDU, 215 },
	    { wxLANGUAGE_URDU_INDIA, 216 },
	    { wxLANGUAGE_URDU_PAKISTAN, 217 },
	    { wxLANGUAGE_UZBEK, 218 },
	    { wxLANGUAGE_UZBEK_CYRILLIC, 219 },
	    { wxLANGUAGE_UZBEK_LATIN, 220 },
	    { wxLANGUAGE_VALENCIAN, 0x1fffffff },
	    { wxLANGUAGE_VIETNAMESE, 221 },
	    { wxLANGUAGE_VOLAPUK, 222 },
	    { wxLANGUAGE_WELSH, 223 },
	    { wxLANGUAGE_WOLOF, 224 },
	    { wxLANGUAGE_XHOSA, 225 },
	    { wxLANGUAGE_YIDDISH, 226 },
	    { wxLANGUAGE_YORUBA, 227 },
	    { wxLANGUAGE_ZHUANG, 228 },
	    { wxLANGUAGE_ZULU, 229 },
#ifndef WX31
	    { wxLANGUAGE_KABYLE, wxLANGUAGE_KABYLE + 0x8000 },	// wx3.1 moved this to after Javanese.
#endif
	    { wxLANGUAGE_USER_DEFINED, 230 },
	    { wxLANGUAGE_DEFAULT, -1}
	};

#endif // WX3

wxLanguage langWX2toWX3(wxLanguage wx2) {
#if wxMAJOR_VERSION > 2
	for (unsigned int i = 0; i < WXSIZEOF(mapping); i++) {
		if (mapping[i].wx2 == -1) return wx2;
		if (mapping[i].wx2 == wx2) return mapping[i].wx3;
	}
#endif
	return wx2;
}
