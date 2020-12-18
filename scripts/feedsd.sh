#! /bin/sh

SCRIPT_DIR=$(cd "$(dirname $0)" && pwd);

RUNTIME="/var/lib/feedsd/runtime/current/bin/feedsd";
if [ ! -f "$RUNTIME" ]; then
	RUNTIME="$SCRIPT_DIR/../$RUNTIME";
fi

if [ ! -f "$RUNTIME" ]; then
	echo "Failed to found feeds service runtime.";
	exit 1;
fi

CONFIG_FILE="../etc/feedsd/feedsd.conf";
if [ -f "$SCRIPT_DIR/feedsd.conf" ]; then
	CONFIG_FILE="$SCRIPT_DIR/feedsd.conf";
fi

cd $(dirname "$RUNTIME");
"$RUNTIME" --config="$CONFIG_FILE";
exit 0;