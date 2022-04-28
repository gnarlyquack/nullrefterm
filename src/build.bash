#!/usr/bin/env bash

# Set application-specific stuff here
EXE_NAME=nullrefterm
EXE_SOURCES=(main.c)
EXE_LIBS=(x11 xft)

# Things that could also be modified but are probably fine
BUILD_DIR=../build/
CC=clang
C_VERSION=c17
COMPILER_FLAGS+="-g "
#COMPILER_FLAGS+="-O3 "
COMPILER_FLAGS+="-DDEBUG "
COMPILER_FLAGS+="-Wall -Wextra -Wpedantic "
COMPILER_FLAGS+="-Wcast-qual "
COMPILER_FLAGS+="-Wconversion "
COMPILER_FLAGS+="-Wshadow "
COMPILER_FLAGS+="-Wunused "
COMPILER_FLAGS+="-Werror=vla "
COMPILER_FLAGS+="-Wl,-z,defs "


##############################################################################
#######################     Don't touch below here!     ######################
##############################################################################

if [ ! -z "$EXE_LIBS" ]
then
    COMPILER_FLAGS+=`pkg-config --cflags --libs ${EXE_LIBS[@]}`
fi

SOURCE_DIR=${PWD}/
mkdir -p $BUILD_DIR
pushd $BUILD_DIR > /dev/null


START=$(date +"%s.%N")


echo "Building ${EXE_NAME}..."
SOURCES=""
for SOURCE in ${EXE_SOURCES[@]}
do
    SOURCES+="${SOURCE_DIR}${SOURCE} "
done
$CC $COMPILER_FLAGS $SOURCES -o $EXE_NAME -std=$C_VERSION
RESULT=$?


if [ $RESULT -eq 0 ]
then
    END=$(date +"%s.%N")
    ELAPSED=$(echo "$END - $START" | bc)
    printf "Finished in %.2f secs\n" $ELAPSED
fi

popd > /dev/null
exit $RESULT
