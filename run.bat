@echo off
title run
::------------------------------------------------------::
echo ip_cmsis_dap为CMSIS-DAP的ip地址，需要手动确认;
echo name_device为目标板对应的型号文件，需从.\openocd\share\openocd\scripts\target\中选择;
echo dir_usbip为usbip.exe的地址，脚本会自动补全，无需修改;
echo dir_openocd为Openocd的地址，脚本会自动补全，无需修改;
echo.
::------------------------------------------------------::
set dir_usbip=%~dp0\usbip_bsod_fixed_certificated\
set dir_openocd=%~dp0\OpenOCD-20170821\bin\
set ip_cmsis_dap=192.168.0.110
set name_device=stm32f0x
::------------------------------------------------------::
echo -----------Checking params-----------
for /f "tokens=3 delims=，" %%i in ('@ping %ip_cmsis_dap% -4 -n 1^|find /v "字节"^|find /v "的"') do (echo %%i>>$)
for /f "tokens=2 delims=(%%" %%a in ('type $^|find "丢失"') do (set mis_srate=%%a)
for /f "tokens=2 delims==m " %%b in ('type $^|find "平均"') do (set avg_delay=%%b)
del $ /f /q
if ("%avg_delay%"=="") (goto exceptionNoDAP) else (echo dap_ip...check)
if exist %dir_usbip% (echo dir_usbip.exe...check) else (goto exceptionNoUSBIP)
if exist %dir_openocd% (echo dir_openocd.exe...check) else (goto exceptionNoOpenOCD)
if exist %dir_openocd%\..\share\openocd\scripts\target\%name_device%.cfg (echo target...check) else (goto exceptionNoTarget)
echo ----------------Done.----------------
echo.
echo All Checks complete, press any key to start debugging.
pause
::------------------------------------------------------::
path=%path%;%dir_usbip%;%dir_openocd%;
@start /min "usb/ip console" usbip.exe -a %ip_cmsis_dap% 1-1 
::use ping to delay 2 seconds
@ping 127.0.0.1 -4 -n 2 >nul
@start /min "openocd console" openocd.exe -f %dir_openocd%\..\share\openocd\scripts\interface\cmsis-dap.cfg -f %dir_openocd%\..\share\openocd\scripts\target\%name_device%.cfg
@telnet localhost 4444
::------------------------------------------------------::
@taskkill /f /im openocd.exe
@start /min usbip.exe -d 1
goto EOF
::------------------------------------------------------::
:exceptionEnd
echo .exe returns error!
goto EOF

:exceptionNoUSBIP
echo 请检查usbip.exe的路径，修改脚本后重试！
echo Please check the path of usbip.exe and retry!
goto EOF

:exceptionNoOpenOCD
echo 请检查OpenOCD.exe的路径，修改脚本后重试！
echo Please check the path of OpenOCD.exe and retry!
goto EOF

:exceptionNoTarget
echo 请检查目标板对应控制文件的路径，修改脚本后重试！
echo Please check the path of target.cfg and retry!
goto EOF

:exceptionNoDAP
echo 请检查DAP的网络连接，修改脚本后重试！
echo Please check the connection between PC and DAP, then retry!
goto EOF
::------------------------------------------------------::
:EOF
echo Press Any Key to End
pause
exit 0