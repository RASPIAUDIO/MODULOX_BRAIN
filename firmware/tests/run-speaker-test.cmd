@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
pushd "%~dp0.."
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /O2 /I tests\host /I main tests\speaker_test.cpp /Fo:build\ /Fe:build\speaker_test.exe
if errorlevel 1 (
  popd
  exit /b 1
)
build\speaker_test.exe
set test_result=%errorlevel%
popd
exit /b %test_result%
