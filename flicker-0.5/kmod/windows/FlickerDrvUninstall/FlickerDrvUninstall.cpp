#include <iostream>
#include <windows.h>
#include <winioctl.h>
#include <winsvc.h>
#include <string>
using namespace std;

void DisplayError (string generalReason)
{
    char *lpMsgBuf;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &lpMsgBuf,
                   0, NULL);
    cout << generalReason << " because " << lpMsgBuf << endl;

    LocalFree(lpMsgBuf);
}

bool UninstallFlickerDrv ()
{
    // Get a handle to the service control manager.

    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL) {
        DisplayError("Could not open the service control manager");
        return false;
    }

    // Delete the FlickerDrv service if it exists.

    SC_HANDLE schService = OpenServiceA(schSCManager, "FlickerDrv", SERVICE_ALL_ACCESS);
    if (schService != NULL) {
        BOOL success = DeleteService(schService);
        CloseServiceHandle(schService);
        if (!success) {
            DisplayError("Could not delete FlickerDrv service");
            CloseServiceHandle(schSCManager);
            return false;
        }
    }
    CloseServiceHandle(schSCManager);

    // Find out where the system root directory is.

    char systemRoot[MAX_PATH];
    if (GetEnvironmentVariableA("SystemRoot", systemRoot, MAX_PATH) == 0) {
        DisplayError("Could not obtain the system root directory environment variable");
        return false;
    }

    // Delete the driver file (ending in .SYS) in the drivers subdirectory of
    // the system32 subdirectory of the system root directory.

    char driverFileLocation[MAX_PATH];
    sprintf_s(driverFileLocation,
              MAX_PATH,
              "%s\\SYSTEM32\\DRIVERS\\FlickerDrv.sys",
              systemRoot);
	if (!DeleteFileA(driverFileLocation)) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            DisplayError("Could not delete driver file");
            return false;
		}
    }

    return true;
}

int main (int argc, char **argv)
{
    if (UninstallFlickerDrv()) {
        cout << "FlickerDrv uninstalled successfully." << endl;
    }
    else {
        cout << "FlickerDrv failed to uninstall." << endl;
    }

    return 0;
}
