#! /bin/sh

LONG_OPTIONS=poller:,ns:,with_debug,with_test,job:,verbose
OPTIONS=p:n:dtj:v
WITH_DEBUG=0
WITH_TEST=0
JOB=8
VERBOSE=0

ARGS=$(getopt -l $LONG_OPTIONS -o $OPTIONS -- $@) || exit
eval set -- "$ARGS"
while true; do
    case "$1" in
        -p | --poller) POLLER=$2; shift 2;;
        -n | --ns) CPPNS=$2; shift 2;;
        -d | --with_debug) ((WITH_DEBUG++)); shift;;
        -t | --with_test) ((WITH_TEST++)); shift;;
        -j | --job) JOB=$2; shift 2;;
        -v | --verbose) ((VERBOSE++)); shift;;
        --) shift; break;;
        *) echo "getopt error"; exit 1;;
    esac
done

CMAKE_OPT=
if [ ! -z "$POLLER" ]; then
    CMAKE_OPT="$CMAKE_OPT -DLNET_POLLER=$POLLER"
fi
if [ ! -z "$CPPNS" ]; then
    CMAKE_OPT="$CMAKE_OPT -DCPPNS=$CPPNS"
fi
if [ $WITH_DEBUG -ne 0 ]; then
    CMAKE_OPT="$CMAKE_OPT -DLNET_DEBUG=ON"
fi
if [ $WITH_TEST -ne 0 ]; then
    CMAKE_OPT="$CMAKE_OPT -DLNET_BUILD_TESTS=ON"
fi

if [ ! -e cmake-build ]; then
    mkdir -v cmake-build
fi
CMAKE_CMD="cmake .."
if [ ! -z "$CMAKE_OPT" ]; then
    CMAKE_CMD="$CMAKE_CMD$CMAKE_OPT"
fi
cd cmake-build && echo "$CMAKE_CMD" && $CMAKE_CMD

RET_CODE=$?; if [ $RET_CODE -ne 0 ]; then exit $RET_CODE; fi
BUILD_OPT=
if [ ! -z "$JOB" ]; then
    BUILD_OPT="$BUILD_OPT -j$JOB"
fi
if [ $VERBOSE -ne 0 ]; then
    BUILD_OPT="$BUILD_OPT -v"
fi

BUILD_CMD="cmake --build ."
if [ ! -z "$BUILD_OPT" ]; then
    BUILD_CMD="$BUILD_CMD$BUILD_OPT"
fi
echo "$BUILD_CMD" && $BUILD_CMD
