#!/bin/sh
# This script file processes the source files looking for strings
# marked as translatable and outputs them to a template (.pot) file.
# That template is then merged into existing translation-specific .po
# files, removing obsolete and adding new messages to those .po files.
#
xgettext --c++ --default-domain=tqslapp --output=tqslapp.pot \
	 --keyword=_  --keyword=__ --keyword=i18narg \
	 --escape --copyright-holder="The TrustedQSL Developers" \
	 --package-name="TQSL" --package-version="v2.6" \
	 --add-comments="TRANSLATORS:" *.cpp *.h
