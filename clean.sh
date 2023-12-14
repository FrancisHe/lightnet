#! /bin/sh
LONG_OPTIONS=cache
OPTIONS=c
WITH_CACHE=0

ARGS=$(getopt -l $LONG_OPTIONS -o $OPTIONS -- $@) || exit
eval set -- "$ARGS"
while true; do
    case "$1" in
        -c | --cache) ((WITH_CACHE++)); shift;;
        --) shift; break;;
        *) echo "getopt error"; exit 1;;
    esac
done

if [ ! -e cmake-build ]; then
    echo "cmake-build not exist, no need to clean."
    exit 0
fi

# clean object files, archive files and dynamic libraries
CLEAN_CMD="cmake --build . --target clean"
cd cmake-build && echo "$CLEAN_CMD" && $CLEAN_CMD

# cmake options will saved into CMakeCache.txt, we MUST remove
# CMakeCache.txt to make new options work.
RET_CODE=$?; if [ $RET_CODE -ne 0 ]; then exit $RET_CODE; fi
if [ $WITH_CACHE -ne 0 ]; then
    rm -vf CMakeCache.txt
fi
