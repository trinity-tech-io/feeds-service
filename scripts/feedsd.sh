#! /bin/sh

set -o errexit

SCRIPT_DIR=$(cd $(dirname "$0") && pwd);
ROOT_DIR=$(dirname "$SCRIPT_DIR");

RUNTIME="/var/lib/feedsd/runtime/current/bin/feedsd";
if [ ! -f "$RUNTIME" ]; then
	RUNTIME="$ROOT_DIR/$RUNTIME";
fi

if [ ! -f "$RUNTIME" ]; then
	echo "Failed to found feeds service runtime.";
	exit 1;
fi

RUNTIME_DIR=$(dirname "$RUNTIME");
cd "$RUNTIME_DIR";
"$RUNTIME" $@;
exit 0;
