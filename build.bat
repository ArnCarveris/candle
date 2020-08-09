@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

CD /D %~dp0

CALL vcenv.bat

SET SAUCES=""
:PARSEPARAMS
	IF "%~1"=="" GOTO START
	IF "%~1"=="/SAUCES" (
		SET SAUCES=%~2
		SHIFT
	)
	SHIFT
GOTO :PARSEPARAMS
:START

set DIR=build
set GLFW=third_party\glfw\src
set TINYCTHREAD=third_party\tinycthread\source
set THIRD_PARTY_SRC= %GLFW%\context.c^
                     %GLFW%\init.c^
                     %GLFW%\input.c^
                     %GLFW%\monitor.c^
                     %GLFW%\vulkan.c^
                     %GLFW%\window.c^
                     %GLFW%\win32_init.c^
                     %GLFW%\win32_joystick.c^
                     %GLFW%\win32_monitor.c^
                     %GLFW%\win32_time.c^
                     %GLFW%\win32_thread.c^
                     %GLFW%\win32_window.c^
                     %GLFW%\wgl_context.c^
                     %GLFW%\egl_context.c^
                     %GLFW%\osmesa_context.c^
                     %TINYCTHREAD%\tinycthread.c^
                     third_party\miniz.c

::set CFLAGS=/O2 /I. /W1 /D_GLFW_WIN32 /Ithird_party\glfw\include /I%TINYCTHREAD% /DTHREADED^
	::/D_CRT_SECURE_NO_WARNINGS
set CFLAGS=/DEBUG:FULLi /Z7 /I. /W1 /D_GLFW_WIN32 /Ithird_party\glfw\include /I%TINYCTHREAD% /DTHREADED^
	/D_CRT_SECURE_NO_WARNINGS

set sources=%THIRD_PARTY_SRC% candle.c

set subdirs=components systems formats utils vil ecs

FOR %%a IN (%subdirs%) DO @IF EXIST "%%a" (
	FOR %%f in (%%a\*.c) DO @IF EXIST "%%f" set sources=!sources! %%f
)

mkdir %DIR%\components
mkdir %DIR%\systems
mkdir %DIR%\utils
mkdir %DIR%\formats
mkdir %DIR%\vil
mkdir %DIR%\ecs
mkdir %DIR%\%GLFW%
mkdir %DIR%\%TINYCTHREAD%

set objects=
FOR %%f IN (!sources!) DO @IF EXIST "%%f" (
	set src=%DIR%\%%f
	CALL set object=%%src:.c=.obj%%
	cl %CFLAGS% /c "%%f" /Fo"!object!" || (
		echo Error compiling %%f
		GOTO ERROR
	)
	CALL set objects=!objects! !object!
)

cl packager\packager.c %DIR%\third_party\miniz.obj /Fe%DIR%\packager.exe /O2
CALL %DIR%\packager.exe ..\!SAUCES!
ECHO 1 RCDATA "%DIR%\data.zip" > %DIR%\res.rc
rc %DIR%\res.rc

lib !objects! /out:"%DIR%\candle.lib"

GOTO END

:ERROR
ECHO ERROR
:END
