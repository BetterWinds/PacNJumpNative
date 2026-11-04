@echo off
rem Builds a sideloadable release APK and copies it to native\dist\
rem   package.bat        -> personal build with the game data bundled
rem   package.bat byo    -> shareable build with NO game data; the player picks their own original APK on first launch
rem   package.bat template -> no-data APK, unsigned, copied to ..\web\template.apk for the browser builder
rem Needs Android Studio installed (uses its bundled JDK and the SDK from local.properties).
cd /d "%~dp0"
rem Gradle 8.13 cannot run on JDK 25. Use Android Studio's JDK, but prefer a separately installed JDK 21 or 17 if there is one.
if exist "C:\Program Files\Android\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
if exist "%LOCALAPPDATA%\Programs\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=%LOCALAPPDATA%\Programs\Android Studio\jbr"
for /d %%D in ("C:\Program Files\Java\jdk-17*" "C:\Program Files\Eclipse Adoptium\jdk-17*" "C:\Program Files\Microsoft\jdk-17*") do if exist "%%~D\bin\java.exe" set "JAVA_HOME=%%~D"
for /d %%D in ("C:\Program Files\Java\jdk-21*" "C:\Program Files\Eclipse Adoptium\jdk-21*" "C:\Program Files\Microsoft\jdk-21*") do if exist "%%~D\bin\java.exe" set "JAVA_HOME=%%~D"
echo Using JAVA_HOME=%JAVA_HOME%
"%JAVA_HOME%\bin\java.exe" -version
set "EXTRA="
if /i "%~1"=="byo" set "EXTRA=-PBYO"
if /i "%~1"=="template" set "EXTRA=-PBYO -PUNSIGNED"
call gradlew.bat %EXTRA% assembleRelease
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
if /i "%~1"=="template" (
  for %%F in ("app\build\outputs\apk\release\PacNJump-byo-template-*-release.apk") do copy /y "%%F" "..\web\template.apk" >nul
  for %%A in ("..\web\template.apk") do if %%~zA LSS 100000 (echo ERROR: web\template.apk is only %%~zA bytes, the copy failed & exit /b 1)
  echo Done. ..\web\template.apk
  exit /b 0
)
if not exist "..\dist" mkdir "..\dist"
copy /y "app\build\outputs\apk\release\PacNJump-*-release.apk" "..\dist\" >nul
echo.
echo Done. APK is in native\dist\
dir /b "..\dist\*.apk"
