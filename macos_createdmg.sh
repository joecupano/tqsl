#!/bin/sh

TQSLVER=`cat apps/tqslversion.ver|sed -e 's/\.0$//'`
TQSLLIBPATH=`pwd`/src/libtqsllib.dylib
WORKDIR=`mktemp -d /tmp/tqsl.XXXXX` || exit 1
WINHELPFILE=$WORKDIR/TrustedQSL/$app.app/Contents/Resources/Help/tqslapp.chm
IMGNAME="tqsl"
KEYCHAIN="/Library/Keychains/System.keychain"
KEYCHAIN="$HOME/Library/Keychains/Login.keychain-db"
SIGNOPTS="--options runtime --timestamp"

if [ "x$1" = "x-legacy" ]; then
	SIGNOPTS=""			# Legacy can't have the hardened runtime
	shift
fi

file apps/tqsl.app/Contents/MacOS/tqsl | grep -q ppc && IMGNAME="tqsl-legacy"
file apps/tqsl.app/Contents/MacOS/tqsl | grep -q arm64 && IMGNAME="tqsl-arm64"

/bin/echo -n "Copying files to image directory... "

mkdir $WORKDIR/TrustedQSL
cp apps/ChangeLog.txt $WORKDIR/TrustedQSL/ChangeLog.txt
cp LICENSE.txt $WORKDIR/TrustedQSL/
cp apps/quick "$WORKDIR/TrustedQSL/Quick Start.txt"
cp -r apps/tqsl.app $WORKDIR/TrustedQSL

/bin/echo "done"

/bin/echo -n "Installing the libraries and tweaking the binaries to look for them... "

for app in tqsl
do
    cp $TQSLLIBPATH $WORKDIR/TrustedQSL/$app.app/Contents/MacOS
    [ -f $WINHELPFILE ] && rm $WINHELPFILE
    install_name_tool -change $TQSLLIBPATH @executable_path/libtqsllib.dylib $WORKDIR/TrustedQSL/$app.app/Contents/MacOS/$app
    cp src/config.xml $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/ca-bundle.crt $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/languages.dat $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    cp apps/cab_modes.dat $WORKDIR/TrustedQSL/$app.app/Contents/Resources
    for lang in ca_ES de es fi fr hi_IN it ja pl_PL pt ru sv_SE tr_TR zh_CN zh_TW
    do
	mkdir $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
	cp apps/lang/$lang/tqslapp.mo $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
	cp apps/lang/$lang/wxstd.mo $WORKDIR/TrustedQSL/$app.app/Contents/Resources/$lang.lproj
    done
# Make an empty 'en.lproj' folder so wx knows it's default
    mkdir $WORKDIR/TrustedQSL/$app.app/Contents/Resources/en.lproj
done

/bin/echo "done"

/bin/echo -n "Installing the help... "

cp -r apps/help/tqslapp $WORKDIR/TrustedQSL/tqsl.app/Contents/Resources/Help

/bin/echo "done"

/bin/echo "Creating the disk image..."

#hdiutil uses dots to show progress
hdiutil create -ov -srcfolder $WORKDIR -volname "TrustedQSL v$TQSLVER" "$IMGNAME-$TQSLVER.dmg"

if [ "x$1" != "x" ]; then
	echo "Codesigning as $1"
	plutil -replace CFBundleName -string tqsl ${WORKDIR}/TrustedQSL/tqsl.app/Contents/Info.plist
	echo codesign --deep --options runtime --timestamp --verbose --sign "$1" --keychain $KEYCHAIN $WORKDIR/TrustedQSL/tqsl.app
	codesign --deep --options runtime --timestamp --verbose --sign "$1" --keychain $KEYCHAIN $WORKDIR/TrustedQSL/tqsl.app
# Check that it signed OK
	codesign --verify $WORKDIR/TrustedQSL/tqsl.app || exit 1
fi
/bin/echo "Creating a package..."
pkgbuild --analyze --root $WORKDIR/TrustedQSL ${WORKDIR}/tqslapp.plist
plutil -replace BundleIsRelocatable -bool NO ${WORKDIR}/tqslapp.plist

if [ "x$2" != "x" ]; then
	pkgbuild --root ${WORKDIR}//TrustedQSL --component-plist ${WORKDIR}/tqslapp.plist --install-location /Applications/TrustedQSL `pwd`/${IMGNAME}-${TQSLVER}.pkg --keychain $KEYCHAIN --sign "$2"
else
	pkgbuild --root ${WORKDIR}//TrustedQSL --component-plist ${WORKDIR}/tqslapp.plist --install-location /Applications/TrustedQSL `pwd`/${IMGNAME}-${TQSLVER}.pkg
fi

/bin/echo -n "Cleaning up temporary files.. "
rm -r $WORKDIR
/bin/echo "Finished!"
