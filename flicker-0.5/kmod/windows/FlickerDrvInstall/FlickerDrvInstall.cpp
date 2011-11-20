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

bool InstallFlickerDrv ()
{
    // Make sure it's a 32-bit x86 platform.

    SYSTEM_INFO systemInfo;
    ZeroMemory(&systemInfo, sizeof(systemInfo));
    GetNativeSystemInfo(&systemInfo);
    if (systemInfo.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_INTEL) {
        cout << "We're sorry.  FlickerDrv is currently only supported on 32-bit x86 platforms." << endl;
        return false;
    }

    // Make sure the driver file exists.

    string driverPath = "FlickerDrv.sys";
    if (GetFileAttributesA(driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        cout << "Could not find required driver file " << driverPath << endl;
        return false;
    }

    // Find out where the system root directory is.

    char systemRoot[MAX_PATH];
    if (GetEnvironmentVariableA("SystemRoot", systemRoot, MAX_PATH) == 0) {
        DisplayError("Could not obtain the system root directory environment variable");
        return false;
    }

    // Copy the driver file (ending in .SYS) to the drivers subdirectory of
    // the system32 subdirectory of the system root directory.  This is the
    // traditional place for it.

    char targetFileLocation[MAX_PATH];
    sprintf_s(targetFileLocation,
              MAX_PATH,
              "%s\\SYSTEM32\\DRIVERS\\FlickerDrv.sys",
              systemRoot);
    if (!CopyFileA(driverPath.c_str(), targetFileLocation, FALSE)) {
        DisplayError("Could not copy " + driverPath + " to the drivers directory");
        return false;
    }

    // Get a handle to the service control manager.

    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL) {
        DisplayError("Could not open the service control manager");
        return false;
    }

    // Check whether the service for this driver already exists.

    SC_HANDLE schService = OpenServiceA(schSCManager, "FlickerDrv", SERVICE_ALL_ACCESS);
    if (schService != NULL) {
        cerr << "FlickerDrv driver already installed." << endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return true;
    }

    // Create the service.  Set the 'target file location' to be the name
    // of the driver location in general terms (using %systemroot% to
    // denote the system root)

    sprintf_s(targetFileLocation, MAX_PATH, "%%systemroot%%\\system32\\drivers\\FlickerDrv.sys");
    schService = CreateServiceA(schSCManager,
                                "FlickerDrv",
                                "FlickerDrv",
                                SERVICE_ALL_ACCESS,
                                SERVICE_KERNEL_DRIVER,
                                SERVICE_DEMAND_START,
                                SERVICE_ERROR_NORMAL,
                                targetFileLocation,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
    if (schService == NULL) {
        DisplayError("Could not create FlickerDrv service");
        CloseServiceHandle(schSCManager);
        return false;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

int main (int argc, char **argv)
{
    if (InstallFlickerDrv()) {
        cout << "FlickerDrv installed successfully." << endl;
    }
    else {
        cout << "FlickerDrv failed to install." << endl;
    }

    return 0;
}
