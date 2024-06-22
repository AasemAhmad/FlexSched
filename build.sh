#! /bin/sh

DIR="./build"
if [ -d "$DIR" ]; then
    # Take action if $DIR exists. #
    echo "build files into ${DIR}..."
    cd build;
    make -j
else
    echo "creating ${DIR}"
    mkdir -p ${DIR}
    cd build
    cmake  -DCMAKE_BUILD_TYPE=Debug -Wno-dev .. 
    make -j
fi