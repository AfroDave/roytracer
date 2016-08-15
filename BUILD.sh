#! /usr/bin/env bash

CFLAGS='-I../include -I../extern -GL -O3 -Wall -Wpedantic'
LDFLAGS='-L../lib'

if [[ "${1}" == "run" ]]; then
    ./bin/roytracer
fi

if [[ "${1}" == "build" ]]; then
    [[ ! -d "./bin" ]] && mkdir "./bin"
    pushd "./bin" > /dev/null
    if [[ "${2}" == "cuda" ]]; then
        nvcc -I../include -I../extern -lineinfo -O3 -c ../src/cuda/raytrace.cu -o raytrace.obj
    elif [[ "${2}" == "omp" ]]; then
        clang ${CFLAGS} /openmp -DRT_OMP ../src/cpu/raytrace.c /c
    else
        clang ${CFLAGS} ../src/cpu/raytrace.c /c
    fi
    clang ${CFLAGS} ${LDFLAGS} raytrace.o ../src/roytracer.c ../extern/glad/glad.c
    pushd "./bin" > /dev/null
fi
