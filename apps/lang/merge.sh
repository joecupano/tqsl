for lang in $(find . -name '??*' -type d -print |sort| sed -e 's/\.\///')
do
	/bin/echo -n "$lang: "
	msgmerge -N -U $lang/tqslapp.po ../tqslapp.pot
done
