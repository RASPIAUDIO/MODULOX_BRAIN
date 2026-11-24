@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
pushd "%~dp0.."
cl /nologo /std:c++20 /EHsc /O2 /I main tests\navigation_test.cpp /Fo:build\ /Fe:build\navigation_test.exe
if errorlevel 1 (
  popd
  exit /b 1
)
build\navigation_test.exe
set test_result=%errorlevel%
popd
exit /b %test_result%
