/***************************************************************************
                          adif.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by ARRL
    email                : MSimcik@localhost.localdomain
    revision             : $Id$
 ***************************************************************************/

#define TQSLLIB_DEF

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif
#include "tqsllib.h"
#include "tqslerrno.h"
#include "winstrdefs.h"

typedef enum {
	TQSL_ADIF_STATE_BEGIN,
	TQSL_ADIF_STATE_GET_NAME,
	TQSL_ADIF_STATE_GET_SIZE,
	TQSL_ADIF_STATE_GET_TYPE,
	TQSL_ADIF_STATE_GET_DATA,
	TQSL_ADIF_STATE_DONE
}  TQSL_ADIF_STATE;

struct TQSL_ADIF {
	int sentinel;
	FILE *fp;
	char *filename;
	int line_no;
};

#define CAST_TQSL_ADIF(p) ((struct TQSL_ADIF *)p)

static char ADIF_ErrorField[TQSL_ADIF_FIELD_NAME_LENGTH_MAX + 1];

static TQSL_ADIF *
check_adif(tQSL_ADIF adif) {
	if (tqsl_init())
		return 0;
	if (adif == 0)
		return 0;
	if (CAST_TQSL_ADIF(adif)->sentinel != 0x3345) {
		tqslTrace("check_adif", "adif no valid sentinel");
		return 0;
	}
	return CAST_TQSL_ADIF(adif);
}

static void
free_adif(TQSL_ADIF *adif) {
	tqslTrace("free_adif", NULL);
	if (adif && adif->sentinel == 0x3345) {
		adif->sentinel = 0;
		if (adif->filename)
			free(adif->filename);
		if (adif->fp)
			fclose(adif->fp);
		free(adif);
	}
}

DLLEXPORT int CALLCONVENTION
tqsl_beginADIF(tQSL_ADIF *adifp, const char *filename) {
	tqslTrace("tqsl_beginADIF", "adifp=0x%lx, filename=%s", adifp, filename);
	if (filename == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	struct TQSL_ADIF *adif;
	adif = (struct TQSL_ADIF *)calloc(1, sizeof(struct TQSL_ADIF));
	if (adif == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	adif->sentinel = 0x3345;
	ADIF_ErrorField[0] = '\0';
	tqslTrace("tqsl_beginADIF", "Preparing to open file");
#ifdef _WIN32
	wchar_t *wfilename = utf8_to_wchar(filename);
	if ((adif->fp = _wfopen(wfilename, L"rb, ccs=UTF-8")) == NULL) {
		free_wchar(wfilename);
#else
	if ((adif->fp = fopen(filename, "rb")) == NULL) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
		tqslTrace("tqsl_beginADIF", "Error %d errno %d file %s", tQSL_Error, tQSL_Errno, filename);
		goto err;
	}
#ifdef _WIN32
	free_wchar(wfilename);
#endif
	if ((adif->filename = strdup(filename)) == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	*((struct TQSL_ADIF **)adifp) = adif;
	return 0;
 err:
	free_adif(adif);
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_endADIF(tQSL_ADIF *adifp) {
	tqslTrace("tqsl_endADIF", "adifp=0x%lx", adifp);
	TQSL_ADIF *adif;
	if (adifp == 0)
		return 0;
	adif = CAST_TQSL_ADIF(*adifp);
	free_adif(adif);
	*adifp = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFLine(tQSL_ADIF adifp, int *lineno) {
	TQSL_ADIF *adif;
	if (!(adif = check_adif(adifp)))
		return 1;
	if (lineno == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*lineno = adif->line_no;
	return 0;
}

static
void
strCatChar(char *str, int character) {
	while (*str)
		str++;
	*str++ = character;
	*str = '\0';
}

DLLEXPORT const char* CALLCONVENTION
tqsl_adifGetError(TQSL_ADIF_GET_FIELD_ERROR status) {
	const char *fmt;
	static char errorText[512];
	switch( status ) {
		case TQSL_ADIF_GET_FIELD_SUCCESS:
			fmt = "ADIF success";
			break;

		case TQSL_ADIF_GET_FIELD_NO_NAME_MATCH:
			fmt = "ADIF field %s no name match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH:
			fmt = "ADIF field %s no type match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_RANGE_MATCH:
			fmt = "ADIF field %s no range match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH:
			fmt = "ADIF field %s no enumeration match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_RESULT_ALLOCATION:
			fmt = "ADIF field %s no result allocation";
			break;

		case TQSL_ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW:
			fmt = "ADIF field %s name length overflow";
			break;

		case TQSL_ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW:
			fmt = "ADIF field %s data length overflow";
			break;

		case TQSL_ADIF_GET_FIELD_SIZE_OVERFLOW:
			fmt = "ADIF field %s size overflow";
			break;

		case TQSL_ADIF_GET_FIELD_TYPE_OVERFLOW:
			fmt = "ADIF field %s type overflow";
			break;

		case TQSL_ADIF_GET_FIELD_ERRONEOUS_STATE:
			fmt = "ADIF erroneously executing default state";
			break;

		case TQSL_ADIF_GET_FIELD_EOF:
			fmt = "ADIF field %s reached End of File";
			break;

		default:
			fmt = "ADIF unknown error";
			break;
	}
	snprintf(errorText, sizeof errorText, fmt, ADIF_ErrorField);
	tqslTrace("tqsl_getADIFError", "error=%s", errorText);
	return( errorText );
}

static TQSL_ADIF_GET_FIELD_ERROR
tqsl_adifGetField(tqsl_adifFieldResults *field, FILE *filehandle,
		  const tqsl_adifFieldDefinitions *adifFields,
		  const char * const *typesDefined,
		  unsigned char *(*allocator)(size_t), int *line_no) {
	TQSL_ADIF_GET_FIELD_ERROR status;
	TQSL_ADIF_STATE adifState;
	int currentCharacter;
	unsigned int iIndex;
	unsigned int dataLength;
	unsigned int dataIndex = 0;
	TQSL_ADIF_BOOLEAN recordData;


	/* get the next name value pair */
	status = TQSL_ADIF_GET_FIELD_SUCCESS;
	adifState = TQSL_ADIF_STATE_BEGIN;

	/* assume that we do not wish to record this data */
	recordData = TQSL_FALSE;

	/* clear the field buffers */
	field->name[0] = '\0';
	field->size[0] = '\0';
	field->type[0] = '\0';
	field->data = NULL;
	field->adifNameIndex = 0;
	field->userPointer = NULL;
	field->line_no = -1;

	while(adifState != TQSL_ADIF_STATE_DONE) {
		if (EOF != (currentCharacter = fgetc(filehandle))) {
			if (*line_no == 0)
				*line_no = 1;
			if (currentCharacter == '\n')
				(*line_no)++;
			switch(adifState) {
				case TQSL_ADIF_STATE_BEGIN:
					/* GET STARTED */
					/* find the field opening "<", ignoring everything else */
					if ('<' == currentCharacter) {
						adifState = TQSL_ADIF_STATE_GET_NAME;
					}
					break;

				case TQSL_ADIF_STATE_GET_NAME:
					/* GET FIELD NAME */
					/* add field name characters to buffer, until '>' or ':' found */
					if (('>' == currentCharacter) || (':' == currentCharacter)) {
						/* find if the name is a match to a LoTW supported field name */
						field->line_no = *line_no;
						status = TQSL_ADIF_GET_FIELD_NO_NAME_MATCH;
						adifState = TQSL_ADIF_STATE_GET_SIZE;

						for(iIndex = 0;
							(TQSL_ADIF_GET_FIELD_NO_NAME_MATCH == status) &&
							(0 != adifFields[iIndex].name[0]);
							iIndex++) {
							/* case insensitive compare */
							if (0 == strcasecmp(field->name, adifFields[iIndex].name) ||
							    0 == strcasecmp(adifFields[iIndex].name, "*")) {
								/* set name index */
								field->adifNameIndex = iIndex;

								/* copy user pointer */
								field->userPointer = adifFields[iIndex].userPointer;

								/* since we know the name, record the data */
								recordData = TQSL_TRUE;
								status = TQSL_ADIF_GET_FIELD_SUCCESS;
							}
							if ('>' == currentCharacter) {
								adifState = TQSL_ADIF_STATE_DONE;
							}
						}
					} else if (strlen(field->name) < TQSL_ADIF_FIELD_NAME_LENGTH_MAX) {
						/* add to field match string */
						strCatChar(field->name, currentCharacter);
					} else {
						status = TQSL_ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW;
						adifState = TQSL_ADIF_STATE_DONE;
					}
					break;

				case TQSL_ADIF_STATE_GET_SIZE:
					/* GET FIELD SIZE */
					/* adding field size characters to buffer, until ':' or '>' found */
					if ((':' == currentCharacter) || ('>' == currentCharacter)) {
						/* reset data copy offset */
						dataIndex = 0;

						/* see if any size was read in */
						if (0 != field->size[0]) {
							/* convert data size to integer */
							dataLength = strtol(field->size, NULL, 10);
						} else {
							dataLength = 0;
						}

						if (':' == currentCharacter) {
							/* get the type */
							adifState = TQSL_ADIF_STATE_GET_TYPE;
						} else {
							/* no explicit type, set to LoTW default */
							strncpy(field->type, adifFields[(field->adifNameIndex)].type, sizeof field->type);
							/* get the data */
							adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
						}

						/* only allocate if we care about the data */
						if (recordData) {
							if (dataLength <= adifFields[(field->adifNameIndex)].max_length) {
								/* allocate space for data results, and ASCIIZ */
								if (NULL != (field->data = (*allocator)(dataLength + 1))) {
									/* ASCIIZ terminator */
									field->data[dataIndex] = 0;
								} else {
									status = TQSL_ADIF_GET_FIELD_NO_RESULT_ALLOCATION;
									adifState = TQSL_ADIF_STATE_DONE;
								}
							} else {
								strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
								status = TQSL_ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW;
								adifState = TQSL_ADIF_STATE_DONE;
							}
						}
					} else if (strlen(field->size) < TQSL_ADIF_FIELD_SIZE_LENGTH_MAX) {
						/* add to field size string */
						strCatChar(field->size, currentCharacter);
					} else {
						strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
						status = TQSL_ADIF_GET_FIELD_SIZE_OVERFLOW;
						adifState = TQSL_ADIF_STATE_DONE;
					}
					break;

				case TQSL_ADIF_STATE_GET_TYPE:
					/* GET FIELD TYPE */
					/* get the number of characters in the value data */
					if ('>' == currentCharacter) {
						/* check what type of field this is */
						/* place default type in, if necessary */
						if (0 == field->type[0]) {
							strncpy(field->type, adifFields[(field->adifNameIndex)].type, sizeof field->type);
							adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
						} else {
							/* find if the type is a match to a LoTW supported data type */
							strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
							status = TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH;
							adifState = TQSL_ADIF_STATE_DONE;
							for( iIndex = 0;
								(TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH == status) &&
								(0 != typesDefined[iIndex][0]);
								iIndex++ ) {
								/* case insensitive compare */
								if (0 == strcasecmp(field->type, typesDefined[iIndex])) {
									status = TQSL_ADIF_GET_FIELD_SUCCESS;
									adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
								}
							}
						}
					} else if (strlen(field->type) < TQSL_ADIF_FIELD_TYPE_LENGTH_MAX) {
						/* add to field type string */
						strCatChar(field->type, currentCharacter);
					} else {
						strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
						status = TQSL_ADIF_GET_FIELD_TYPE_OVERFLOW;
						adifState = TQSL_ADIF_STATE_DONE;
					}
					break;

				case TQSL_ADIF_STATE_GET_DATA:
					/* GET DATA */
					/* read in the prescribed number of characters to form the value */
					if (0 != dataLength--) {
						/* only record if we care about the data */
						if (recordData) {
							/* ASCIIZ copy that is tolerant of binary data too */
							field->data[dataIndex++] = (unsigned char)currentCharacter;
							field->data[dataIndex] = 0;
						}
						if (0 == dataLength)
							adifState = TQSL_ADIF_STATE_DONE;
						} else {
							adifState = TQSL_ADIF_STATE_DONE;
						}
					break;

				case TQSL_ADIF_STATE_DONE:
					/* DONE, should never get here */
				default:
					strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
					status = TQSL_ADIF_GET_FIELD_ERRONEOUS_STATE;
					adifState = TQSL_ADIF_STATE_DONE;
					break;
			}
		} else {
			status = TQSL_ADIF_GET_FIELD_EOF;
			adifState = TQSL_ADIF_STATE_DONE;
		}
	}

	if (TQSL_ADIF_GET_FIELD_SUCCESS == status) {
		/* check data for enumeration match and range errors */
		/* match enumeration */
		signed long dataValue;
		switch(adifFields[(field->adifNameIndex)].rangeType) {
			case TQSL_ADIF_RANGE_TYPE_NONE:
				break;

			case TQSL_ADIF_RANGE_TYPE_MINMAX:
				dataValue = strtol((const char *)field->data, NULL, 10);
				if ((dataValue < adifFields[(field->adifNameIndex)].min_value) ||
						(dataValue > adifFields[(field->adifNameIndex)].max_value)) {
					strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
					status = TQSL_ADIF_GET_FIELD_NO_RANGE_MATCH;
				}
				break;

			case TQSL_ADIF_RANGE_TYPE_ENUMERATION:
				strncpy(ADIF_ErrorField, field->name, sizeof(ADIF_ErrorField));
				status = TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH;
				for( iIndex = 0;
					(status == TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH) &&
					(0 != adifFields[(field->adifNameIndex)].enumStrings[iIndex][0]);
					iIndex++) {
						/* case insensitive compare */
						if (field->data && (0 == strcasecmp((const char *)field->data, adifFields[(field->adifNameIndex)].enumStrings[iIndex]))) {
							status = TQSL_ADIF_GET_FIELD_SUCCESS;
							ADIF_ErrorField[0] = '\0';
					}
				}
				break;
		}
	}
	return( status );
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFField(tQSL_ADIF adifp, tqsl_adifFieldResults *field, TQSL_ADIF_GET_FIELD_ERROR *status,
	const tqsl_adifFieldDefinitions *adifFields, const char * const *typesDefined,
	unsigned char *(*allocator)(size_t) ) {
	TQSL_ADIF *adif;
	if (!(adif = check_adif(adifp)))
		return 1;
	if (field == NULL || status == NULL || adifFields == NULL || typesDefined == NULL
		|| allocator == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*status = tqsl_adifGetField(field, adif->fp, adifFields, typesDefined, allocator, &(adif->line_no));
	return 0;
}

static unsigned char *
tqsl_condx_copy(const unsigned char *src, int slen, unsigned char *dest, int *len) {
	if (slen == 0)
		return dest;
	if (slen < 0)
		slen = strlen((const char *)src);
	if (*len < slen) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return NULL;
	}
	memcpy(dest, src, slen);
	*len -= slen;
	return dest+slen;
}

/* Output an ADIF field to a file descriptor.
 */
DLLEXPORT int CALLCONVENTION
tqsl_adifMakeField(const char *fieldname, char type, const unsigned char *value, int len,
	unsigned char *buf, int buflen) {
	if (fieldname == NULL || buf == NULL || buflen <= 0) {	/* Silly caller */
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	unsigned char *cp;
	if ((cp = tqsl_condx_copy((const unsigned char *)"<", 1, buf, &buflen)) == NULL)
		return 1;
	if ((cp = tqsl_condx_copy((const unsigned char *)fieldname, -1, cp, &buflen)) == NULL)
		return 1;
	if (value != NULL && len < 0)
		len = strlen((const char *)value);
	if (value != NULL && len != 0) {
		char nbuf[20];
		if ((cp = tqsl_condx_copy((const unsigned char *)":", 1, cp, &buflen)) == NULL)
			return 1;
		snprintf(nbuf, sizeof nbuf, "%d", len);
		if ((cp = tqsl_condx_copy((const unsigned char *)nbuf, -1, cp, &buflen)) == NULL)
			return 1;
		if (type && type != ' ' && type != '\0') {
			if ((cp = tqsl_condx_copy((const unsigned char *)":", 1, cp, &buflen)) == NULL)
				return 1;
			if ((cp = tqsl_condx_copy((const unsigned char *)&type, 1, cp, &buflen)) == NULL)
				return 1;
		}
		if ((cp = tqsl_condx_copy((const unsigned char *)">", 1, cp, &buflen)) == NULL)
			return 1;
		if ((cp = tqsl_condx_copy(value, len, cp, &buflen)) == NULL)
			return 1;
	} else {
		if ((cp = tqsl_condx_copy((const unsigned char *)">", 1, cp, &buflen)) == NULL)
			return 1;
	}
	if ((cp = tqsl_condx_copy((const unsigned char *)"", 1, cp, &buflen)) == NULL)
		return 1;
	return 0;
}
