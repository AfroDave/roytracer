@echo off

set CFLAGS=/I..\include /I..\extern /nologo /fp:fast /GL /O2 /Ox /Oi /Zi /W4 /FC
set LDFLAGS=/LIBPATH:..\lib /DEBUG /incremental:no /opt:ref kernel32.lib user32.lib gdi32.lib opengl32.lib glfw3dll.lib cudart.lib

if "%1" == "run" (
    .\bin\roytracer.exe
)

if "%1" == "build" (
    if not exist ".\bin" mkdir ".\bin"
    pushd ".\bin"
    if "%2" == "cuda" (
        nvcc -I..\include -I..\extern -lineinfo -O3 -c ..\src\cuda\raytrace.cu -o raytrace.obj
    ) else if "%2" == "omp" (
        cl %CFLAGS% /openmp -DRT_OMP ..\src\cpu\raytrace.c /c
    ) else (
        cl %CFLAGS% ..\src\cpu\raytrace.c /c
    )
    cl %CFLAGS% raytrace.obj ..\src\roytracer.c ..\extern\glad\glad.c /link %LDFLAGS% /out:roytracer.exe
    popd
)
