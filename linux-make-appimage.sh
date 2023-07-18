#!/bin/sh
#
# Make a Linux AppImage for tqsl
# 
# Derived from a build script from Pavel, CO7WT
#
# we will extract to a temp dir to make build id
TEMPDIR=`mktemp -d` || exit 1
HERE=$(pwd)
cd $TEMPDIR

# download & set all needed tools
if [ -z `which linuxdeploy-x86_64.AppImage` ] ; then 
    wget -c -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod a+x linuxdeploy-x86_64.AppImage
    sudo cp linuxdeploy-x86_64.AppImage /usr/local/bin
    rm linuxdeploy-x86_64.AppImage
fi
if [ -z `which linuxdeploy-plugin-appimage-x86_64.AppImage` ] ; then 
    wget -c -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage"
    chmod a+x linuxdeploy-plugin-appimage-x86_64.AppImage
    sudo cp linuxdeploy-plugin-appimage-x86_64.AppImage /usr/local/bin
    rm linuxdeploy-plugin-appimage-x86_64.AppImage
fi

# 
# Get the code
#
git clone https://git.code.sf.net/p/trustedqsl/tqsl
cd tqsl
export VERSION=`cat apps/tqslversion.ver|sed -e 's/\.0$//'`
echo "Building AppImage for TQSL $VERSION"

mkdir build && 
cd build &&
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr &&
	make && \
	make install DESTDIR=../AppDir && \
	cd .. && \
	cp ./build/apps/tqsl AppDir/usr/bin/ && \
	cp ./apps/icons/key48.png AppDir/TrustedQSL.png && \
	cp ./apps/tqsl.desktop AppDir &&
	ln -s usr/bin/tqsl AppDir/AppRun &&
	sed -i s/"TrustedQSL.png"/"TrustedQSL"/ AppDir/tqsl.desktop && \
	linuxdeploy-x86_64.AppImage -e AppDir/usr/bin/tqsl \
		-d AppDir/tqsl.desktop \
		-i AppDir/TrustedQSL.png \
		--output appimage \
		--appdir AppDir && \
	chmod 755 TQSL-*x86_64.AppImage  && \
	cp TQSL-*.AppImage $HERE/TQSL-x86_64.AppImage && \
	cd $HERE && \
	rm -rf $TEMPDIR
