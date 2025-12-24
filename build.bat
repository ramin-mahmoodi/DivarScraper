@echo off
setlocal
echo Setting up Visual Studio Environment...
call "C:\Studio\Product\VC\Auxiliary\Build\vcvars64.bat"

if %errorlevel% neq 0 (
    echo Error: Failed to setup VS environment.
    pause
    exit /b
)

if not exist bin mkdir bin

echo Compiling Resources...
rc.exe /fo assets\resources.res assets\resources.rc

if %errorlevel% neq 0 (
    echo Resource compilation failed!
    exit /b
)

echo Compiling DivarScraper (Optimized for Size)...
cl /O1 /Os /Gy /GL /utf-8 /I src /I assets /D_UNICODE /DUNICODE /DNDEBUG ^
    /DSQLITE_OMIT_LOAD_EXTENSION /DSQLITE_OMIT_DEPRECATED /DSQLITE_OMIT_PROGRESS_CALLBACK ^
    /DSQLITE_OMIT_SHARED_CACHE /DSQLITE_OMIT_TCL_VARIABLE ^
    src\main.cpp src\Database.cpp src\NetworkClient.cpp src\LayoutEngine.cpp ^
    src\HeaderBar.cpp src\HeaderPopup.cpp src\DrawHelper.cpp src\sqlite3.c ^
    assets\resources.res ^
    User32.lib Gdi32.lib Winhttp.lib rpcrt4.lib Comctl32.lib Shlwapi.lib Ole32.lib D2d1.lib Dwrite.lib ^
    /Fe:DivarScraper.exe /Fo:bin\ ^
    /link /LTCG /OPT:REF /OPT:ICF

if %errorlevel% neq 0 (
    echo Build Failed!
    exit /b
)

echo.
echo Build Success! DivarScraper.exe created.
endlocal
