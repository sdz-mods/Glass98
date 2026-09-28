@ECHO OFF
REM Win98 COMMAND.COM: %0\.. locates files beside this batch, including other drives.
IF "%TEMP%"=="" GOTO NOTEMP
COPY /Y %0\..\G98SETUP.EXE "%TEMP%\G98RUN.EXE" > NUL
IF ERRORLEVEL 1 GOTO FAILED
"%TEMP%\G98RUN.EXE" install "%0"
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
:END
