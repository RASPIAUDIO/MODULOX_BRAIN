@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
pushd "%~dp0.."
if not exist build mkdir build
set "json_source=%USERPROFILE%\esp\v5.4.1\esp-idf\components\json\cJSON"
if defined IDF_PATH set "json_source=%IDF_PATH%\components\json\cJSON"
cl /nologo /std:c++20 /EHsc /O2 /I main /I "%json_source%" tests\catalog_writer_test.cpp "%json_source%\cJSON.c" /Fo:build\ /Fe:build\catalog_writer_test.exe
if errorlevel 1 (
  popd
  exit /b 1
)
build\catalog_writer_test.exe
set test_result=%errorlevel%
popd
exit /b %test_result%
