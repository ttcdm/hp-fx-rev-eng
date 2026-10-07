@echo off
rem
rem - Do an "unattended" update of the HP Visualize fx drivers
rem
rem - Options:
rem		
rem		/n - do not reboot after installation.  Default is to reboot.
rem		/s <dir> - specify source directory.  Default is current directory.

	set REBOOT=129
	set SOURCE=.

:process_switches

	if "%0" == "" goto done_with_switches
	if "%0" == "/n" set REBOOT=128
	if NOT "%0" == "/s"  goto continue_process
	shift
	set SOURCE=%0

:continue_process
	shift
	goto process_switches

:done_with_switches

	rundll32 setupapi,InstallHinfSection hpVisualizeFX 128 %SOURCE%\hpfxlw2k.inf
	rundll32 setupapi,InstallHinfSection hpVisualizeFX.Services 128 %SOURCE%\hpfxlw2k.inf
	rundll32 setupapi,InstallHinfSection hpVisualizeFX.SoftwareSettings %REBOOT% %SOURCE%\hpfxlw2k.inf
