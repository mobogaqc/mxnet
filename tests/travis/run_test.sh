#!/bin/bash

if [ ${TASK} == "lint" ]; then
    make lint
    exit $?
fi

if [ ${TASK} == "doc" ]; then
    make doc | tee 2>log.txt
    (cat log.txt|grep warning) && exit -1
    exit 0
fi

cp make/config.mk config.mk

if [ ${TRAVIS_OS_NAME} == "osx" ]; then
    echo "USE_BLAS=apple" >> config.mk
    echo "USE_OPENMP=0" >> config.mk
else
    # use g++-4.8 for linux
    if [ ${CXX} == "g++" ]; then
        export CXX=g++-4.8
    fi
    echo "USE_BLAS=blas" >> config.mk
fi
echo "CXX=${CXX}" >>config.mk

if [ ${TASK} == "build" ]; then
    if [ ${TRAVIS_OS_NAME} == "linux" ]; then
        echo "USE_CUDA=1" >> config.mk
        ./dmlc-core/scripts/setup_nvcc.sh $NVCC_PREFIX
    fi
    make all
    exit $?
fi

if [ ${TASK} == "cpp_test" ]; then
    make -f dmlc-core/scripts/packages.mk gtest
    echo "GTEST_PATH="${CACHE_PREFIX} >> config.mk
    make test || exit -1
    export MXNET_ENGINE_INFO=true
    for test in tests/cpp/*_test; do
        ./$test || exit -1
    done
    exit 0
fi

if [ ${TASK} == "r_test" ]; then
    make all || exit -1
    # use cached dir for storing data
    rm -rf ${PWD}/data
    mkdir -p ${CACHE_PREFIX}/data
    ln -s ${CACHE_PREFIX}/data ${PWD}/data

    set -e
    export _R_CHECK_TIMINGS_=0
    export R_BUILD_ARGS="--no-build-vignettes --no-manual"
    export R_CHECK_ARGS="--no-vignettes --no-manual"
    
    curl -OL http://raw.github.com/craigcitro/r-travis/master/scripts/travis-tool.sh
    chmod 755 ./travis-tool.sh
    ./travis-tool.sh bootstrap
    ./travis-tool.sh install_aptget r-cran-testthat r-cran-Rcpp r-cran-DiagrammeR r-cran-data.table r-cran-jsonlite r-cran-magrittr r-cran-stringr
    
    R CMD INSTALL R-package
    cd ./R-package
    ../travis-tool.sh install_deps
    ../travis-tool.sh run_tests
    exit 0
fi

if [ ${TASK} == "python_test" ]; then
    make all || exit -1
    # use cached dir for storing data
    rm -rf ${PWD}/data
    mkdir -p ${CACHE_PREFIX}/data
    ln -s ${CACHE_PREFIX}/data ${PWD}/data

    if [ ${TRAVIS_OS_NAME} == "osx" ]; then
        python -m nose tests/python/unittest || exit -1
        # python -m nose tests/python/train || exit -1
        python3 -m nose tests/python/unittest || exit -1
        python3 -m nose tests/python/train || exit -1
    else
        nosetests tests/python/unittest || exit -1
        # nosetests tests/python/train || exit -1
        nosetests3 tests/python/unittest || exit -1
        nosetests3 tests/python/train || exit -1
    fi
    exit 0
fi
