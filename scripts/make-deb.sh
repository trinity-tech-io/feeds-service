#!/bin/bash

FEEDSD_VER="$1";
FEEDSD_DIST_DIR="$2";
FEEDSD_INSTALL_PREFIX="$3";
OSTYPE=$(cat /etc/os-release 2>&1 |grep "^ID=");
set -o errexit;
set -o nounset;

if [[ "$OSTYPE" != *"ubuntu"*
&& "$OSTYPE" != *"raspbian"* ]]; then
	echo "ERROR: Unsupport system, only ubuntu and raspbian can do this!";
	exit 1;
fi

if [ -z "$FEEDSD_VER" ]; then
	echo "ERROR: Please set version by argument 1";
	exit 1;
fi
if [ -z "$FEEDSD_DIST_DIR" ]; then
	echo "ERROR: Please set dist dir by argument 3";
	exit 1;
fi
OSTYPE=${OSTYPE/ID=/};
echo "System Type: $OSTYPE";
echo "Feeds Service Version: $FEEDSD_VER";
echo "Feeds Service Dist: $FEEDSD_DIST_DIR";
FEEDSD_DEB_DIR="$FEEDSD_DIST_DIR/feedsd";

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd);
PROJECT_DIR=$(dirname "$SCRIPT_DIR");

rm -rf "$FEEDSD_DIST_DIR" && mkdir -p "$FEEDSD_DEB_DIR";
export DESTDIR="$FEEDSD_DIST_DIR";
make install;
cp -rv "$FEEDSD_DIST_DIR/$FEEDSD_INSTALL_PREFIX/"* "$FEEDSD_DEB_DIR";
mkdir "$FEEDSD_DEB_DIR/DEBIAN" && cp -rv "$PROJECT_DIR/debian/"* "$FEEDSD_DEB_DIR/DEBIAN";

echo "Feeds Service Configuring...";
### config control
FEEDSD_SIZE=$(du -sb deb/feedsd/);
FEEDSD_SIZE=${FEEDSD_SIZE%%[$'\t\r\n ']*};
sed -i "s|Version:.*|Version: $FEEDSD_VER|"                "$FEEDSD_DEB_DIR/DEBIAN/control";
sed -i "s|Installed-Size:.*|Installed-Size: $FEEDSD_SIZE|" "$FEEDSD_DEB_DIR/DEBIAN/control";
### config feedsd.conf
sed -i "s|~/.feedsd|/var/lib/feedsd|"                      "$FEEDSD_DEB_DIR/etc/feedsd/feedsd.conf";

echo "Feeds Service Generating...";
cd "$FEEDSD_DIST_DIR" && dpkg-deb --build feedsd;

if [[ "$OSTYPE" == "ubuntu" ]]; then
	OSREV=$(cat /etc/os-release 2>&1 |grep "^VERSION_ID=");
	OSREV=${OSREV/VERSION_ID=/};
	OSREV=${OSREV//\"/};
	FEEDSD_DESC="amd64_${OSTYPE}_${OSREV}"
elif [[ "$OSTYPE" == "raspbian" ]]; then
	FEEDSD_DESC="armhf_${OSTYPE}";
else 
	echo "ERROR: Unsupport system!";
	exit 1;
fi
mv feedsd.deb feedsd_${FEEDSD_VER}_${FEEDSD_DESC}.deb;

echo "Done!";
