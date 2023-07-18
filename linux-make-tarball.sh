#!/bin/sh

if [ $# -eq 0 ] || [ $# -gt 2 ]; then
    echo "usage: $0 [tree-ish] name"
    echo " This script will create a git-free tarball of the source directory"
    echo " with the build id appropriately substituted, just like CMake"
    echo " does if it can find git."
    echo
    echo " 'name' will become a tarball called name.tar.gz"
    echo " containing the archive"
    exit 0
fi;

#is git directory?

if [ ! -d ".git" ]; then
    echo "Working directory is not a git directory!"
    exit 1
fi

# we will extract to a temp dir to make build id
TEMPDIR=`mktemp -d` || exit 1

TREEISH="HEAD"
if [ $# -eq 2 ]; then
    # we use this as a tree-ish
    TREEISH="$1"
    shift;
fi

OUTNAME=$1

git archive --format=tar --prefix="$OUTNAME/" $TREEISH | (cd $TEMPDIR && tar xf -) || exit 1

DESC=`git describe $TREEISH`
VER=`git show $TREEISH:apps/tqslversion.ver`

sed -e s/@BUILD@/pkg-$DESC/ -e s/@TQSLVERSION@/$VER/ $TEMPDIR/$OUTNAME/apps/tqslbuild.h.in > $TEMPDIR/$OUTNAME/apps/tqslbuild.h
dos2unix -q $TEMPDIR/$OUTNAME/README
rm $TEMPDIR/$OUTNAME/apps/help/tqslapp/tqslapp.chm

WD=`pwd`

(cd $TEMPDIR && tar zcf $WD/$OUTNAME.tar.gz $OUTNAME)

rm -R $TEMPDIR
