@echo off

if not defined DevEnvDir (
  rem Change this to your visual studio install location
  call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist bin mkdir bin

pushd src
cl /O2 /W3 all.c /Fo:..\bin\ /link /out:..\bin\iolambda.exe
popd
