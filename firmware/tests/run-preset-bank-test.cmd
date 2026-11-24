@echo off
setlocal
if not defined VCToolsInstallDir (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 1
)
pushd "%~dp0.."
node ..\editor\tests\presets.test.mjs
if errorlevel 1 (
  popd
  exit /b 1
)
cl /nologo /std:c++20 /EHsc /O2 /D_USE_MATH_DEFINES /TP /I tests\host /I main /I managed_components\espressif__esp-dsp\modules\iir\include /I managed_components\espressif__esp-dsp\modules\common\include tests\preset_bank_test.cpp main\synth_voice.cpp main\osc.cpp main\adsr.cpp main\filter.cpp managed_components\espressif__esp-dsp\modules\iir\biquad\dsps_biquad_gen_f32.c managed_components\espressif__esp-dsp\modules\iir\biquad\dsps_biquad_f32_ansi.c /Fo:build\ /Fe:build\preset_bank_test.exe
if errorlevel 1 (
  popd
  exit /b 1
)
build\preset_bank_test.exe
set test_result=%errorlevel%
popd
exit /b %test_result%
