@echo off
setlocal
set CXX=C:\MinGW\bin\g++.exe
if not exist "%CXX%" set CXX=g++
set SRC=main.cpp main_app.cpp v8k_globals.cpp v8k_util.cpp v8k_license.cpp v8k_hw.cpp v8k_input_raw.cpp v8k_hook.cpp v8k_engine.cpp v8k_ui.cpp
set FLAGS=-std=c++11 -O2 -mwindows -DUNICODE -D_UNICODE -static-libgcc -static-libstdc++ -lcrypt32 -ladvapi32 -luser32 -lgdi32 -lcomctl32
echo Building Virtual8K...
"%CXX%" %SRC% %FLAGS% -o Virtual8K.exe
if errorlevel 1 exit /b 1
echo OK: Virtual8K.exe
exit /b 0
