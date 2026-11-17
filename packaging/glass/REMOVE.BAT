@ECHO OFF
IF "%OS%"=="Windows_NT" GOTO XP
REM Win98 COMMAND.COM: %0\.. locates files beside this batch, including other drives.
IF EXIST C:\Glass98\G98SETUP.EXE GOTO RUN
IF EXIST C:\Glass98\GLASS.HTM GOTO RUN
ECHO Glass98 is already removed. Saved INI files are retained.
GOTO DONE
:RUN
IF "%TEMP%"=="" GOTO NOTEMP
COPY /Y %0\..\G98SETUP.EXE "%TEMP%\G98RUN.EXE" > NUL
IF ERRORLEVEL 1 GOTO FAILED
"%TEMP%\G98RUN.EXE" remove "%0"
IF ERRORLEVEL 1 GOTO FAILED
DEL "%TEMP%\G98RUN.EXE"
GOTO DONE
:NOTEMP
ECHO TEMP is not set. Set it to a writable temporary folder and retry.
GOTO DONE
:FAILED
ECHO Glass98 setup failed. Keep the package and INI files, and review the error above.
:DONE
IF "%1"=="/NOPAUSE" GOTO END
PAUSE
GOTO END

:XP
IF EXIST C:\Glass98\G98SETUP.EXE GOTO XPRUN
IF EXIST C:\Glass98\GLASS.HTM GOTO XPRUN
ECHO Glass98 is already removed. Saved INI files are retained.
IF "%1"=="/NOPAUSE" EXIT /B 0
PAUSE
EXIT /B 0
:XPRUN
IF "%TEMP%"=="" GOTO NOTEMP
COPY /Y "%~dp0G98SETUP.EXE" "%TEMP%\G98RUN.EXE" > NUL
IF ERRORLEVEL 1 GOTO FAILED
"%TEMP%\G98RUN.EXE" remove "%~f0"
IF ERRORLEVEL 1 GOTO FAILED
DEL "%TEMP%\G98RUN.EXE"
IF "%1"=="/NOPAUSE" EXIT /B
PAUSE
EXIT /B

:END
