@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "CustomControlZ\CustomControlZ.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m > build.log 2>&1
echo Exit code: %ERRORLEVEL% >> build.log
