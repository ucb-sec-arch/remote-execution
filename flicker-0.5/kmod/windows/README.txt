The following projects are included here:

*.c,*.h: Windows-specific files for the kernel driver. 

FlickerDrv.sln:  Solution file for the Windows kernel driver.
FlickerDrv.vcxproj:  Project file for the Windows kernel driver.

Note:
      The FlickerDrv solution and project files are only to make code editing easier.  
      They cannot be used to build the driver.  Instead, follow the directions 
      below to build the driver using a DDK command window.

FlickerDrvTools.sln: User-space tools for interacting with the driver.  
		     Includes the following projects:
   FlickerDrvInstall:  The driver installer.
   FlickerDrvUninstall:  The driver uninstaller.
   FlickerDrvCtrl:  The user-level controller of the driver.

Requirements:
- Visual Studio.  Tested with Visual C++ 2010 Express and Visual Studio 2010 Premium.
- Windows Driver Development Kit (DDK).  Tested with v7.1.0


Examples below use the debug versions of everything.  
(Release versions will be faster, of course.)

Setup
-----
Build everything
  - Windows driver:
    Start a Windows DDK build window 
    (e.g., Start->Programs->Windows Driver Kit->WDK 7600->Build Environments->
           Windows 7->x86 Checked Build Environment)
    cd into the kmod directory (one below this one)
    Run 'build /cZ' (add /g for color output and /w to show warnings)

  - User-space programs:
    Open the FlickerDrvTools.sln file.
    Build the solution (F7 or Build->Build Solution).

Consolidate everything into one place

  mkdir DoAll
  copy objchk_win7_x86\i386\FlickerDrv.sys DoAll\ 
  copy Debug\FlickerDrvInstall.exe DoAll\
  copy Debug\FlickerDrvUninstall.exe DoAll\
  copy Debug\FlickerDrvCtrl.exe DoAll\

Install & start driver
----------------------
Start a command prompt with Administrator privilege
 - e.g., right click icon in Accessories and select "Run as Administrator"
cd DoAll
FlickerDrvInstall.exe  (to install FlickerDrv driver)
sc start FlickerDrv  (to start FlickerDrv driver)

Use the driver
-----------
cd DoAll
FlickerDrvCtrl.exe pal path_to_pal_binary
FlickerDrvCtrl.exe in path_to_input_file
[For Intel Only] FlickerDrvCtrl.exe sinit path_to_intel_sinit_file
FlickerDrvCtrl.exe go
FlickerDrvCtrl.exe out path_to_output_file


Stop & uninstall driver
-----------------------
Start a command prompt with Administrator privilege
cd DoAll
sc stop FlickerDrv
FlickerDrvUninstall.exe

Debugging
---------

We highly recommend you use a machine that has either two serial ports or a
serial port and a firewire port.  That way, you can direct Flicker's debug
output across one of the serial ports, and connect a Windows kernel debugger
(e.g., WinDBG) via the other serial port (or FireWire).  You can try to do
Windows debugging over USB, but your chances of success are slim.  If you only
have one serial port, you'll be stuck choosing, for each run, to see WinDBG
output _or_ Flicker output, since they speak different protocols.

To configure serial debugging with WinDBG, try:
http://msdn.microsoft.com/en-us/windows/hardware/gg487520


To Do
------
- Windows driver does not have code to disable the other processors 
  (late launch require non-bootstrap processors to be halted)
  Work around: Use "bcdedit set numproc 1" and reboot to run with only 1 proc active
- Windows driver for AMD does not have the code needed to clear the microcode
  This will result in incorrect values of PCR  17.
  Work around: None.  Need to write the necessary code.
