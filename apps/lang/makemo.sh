for lang in $(find . -name '??*' -type d -print |sort| sed -e 's/\.\///')
do
	msgfmt $lang/tqslapp.po -o $lang/tqslapp.mo
done
