#include <iostream>
#include <fstream>
#include <windows.h>
#include <winioctl.h>
#include <winsvc.h>
#include <string.h>
#include <windows.h>
#include "../Common/KernUser.h"
using namespace std;

void DisplayError (const char *generalReason)
{
    LPSTR lpMsgBuf = NULL;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL,
                   GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR) &lpMsgBuf,
                   0,
                   NULL);
    cerr << generalReason << " because " << lpMsgBuf << endl;
    
    LocalFree(lpMsgBuf);
}

void PrintUsage(const char *programName)
{
    cerr << "Usage:  " << programName << " [go | sinit | pal] <param>" << endl;
	cerr << "\tgo : Starts a Flicker session" << endl;
	cerr << "\tsinit [Path to file containing SINIT module (Intel only)]" << endl;
	cerr << "\tpal [Path to file containing PAL]" << endl;
	cerr << "\tin [Path to file containing inputs]" << endl;
	cerr << "\tout [Path to file that should contain outputs] [offset at which to read] [# bytes to read]" << endl;
}

DWORD Go()
{
    HANDLE hFlickerDrvDevice = CreateFileA("\\\\.\\FlickerDrv",
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
    if (hFlickerDrvDevice == INVALID_HANDLE_VALUE) {
        DisplayError("Could not connect to FlickerDrv device");
        return 0;
    }

    DWORD resultValue = 0;
    DWORD returnedBytes = 0;
    BOOL success = DeviceIoControl(hFlickerDrvDevice,
                                   FLICKERDRV_CTRL_OPERATION_GO,
                                   NULL,
                                   0,
                                   &resultValue,
                                   sizeof(resultValue),
                                   &returnedBytes,
                                   NULL);
    if (!success) {
        DisplayError("Could not perform device I/O control");
    }
    else {
        if (GetLastError() != ERROR_SUCCESS) {
            DisplayError("Device I/O control for operation two failed");
        }
    }

    CloseHandle(hFlickerDrvDevice);

    return resultValue;
}

//
// Read in a file and pass it into kernel land.
// flickerOp specifies the control code to give the driver, so it knows how to process the file
// 
DWORD FeedFileToFlicker(const char *filename, DWORD flickerOp)
{
    DWORD resultValue = 0;

	// First, make sure filename actually exists and can be read in
    ifstream::pos_type pos = 0;
    char *buf = NULL;
	int size = 0;

	if(NULL == filename) { return -1; }

    ifstream theFile (filename, ios::in|ios::binary|ios::ate);
    if (theFile.is_open()) {
		pos = theFile.tellg();
		size = (int) pos;
		buf = new char [size];
		if(NULL == buf) { theFile.close(); cerr << "Can't allocate memory!" << endl; return -2; }
		theFile.seekg (0, ios::beg);
		theFile.read (buf, size);
		theFile.close();

        cout << "Successfully read file " << filename << " (size: " << size << " bytes)." << endl;
	} else {
		cerr << "Unable to open file " << filename << endl;
		return -3;
	}

	// Second, open a handle to the device driver
    HANDLE hFlickerDrvDevice = CreateFileA("\\\\.\\FlickerDrv",
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
    if (hFlickerDrvDevice == INVALID_HANDLE_VALUE) {
        DisplayError("Could not connect to FlickerDrv device");
        return -4;
    }

    DWORD returnedBytes = 0;
    BOOL success = DeviceIoControl(hFlickerDrvDevice,
                                   flickerOp,
                                   buf,
                                   size,
                                   &resultValue,
                                   sizeof(resultValue),
                                   &returnedBytes,
                                   NULL);
    if (!success) {
        DisplayError("Could not perform device I/O control");
    }
    else {
        if (GetLastError() != ERROR_SUCCESS) {
            DisplayError("Device I/O control for file write failed");
        }
    }

    CloseHandle(hFlickerDrvDevice);

    return resultValue;
}


// Read SINIT in from a file and pass it into kernel land.
DWORD DoOperationReadSinit(const char *sinitFilename)
{
   return FeedFileToFlicker(sinitFilename, FLICKERDRV_CTRL_WRITE_SINIT);
}

// Read MLE in from a file and pass it into kernel land.
DWORD DoOperationReadPal(const char *mleFilename)
{
	return FeedFileToFlicker(mleFilename, FLICKERDRV_CTRL_WRITE_PAL);
}

// Read inputs in from a file and pass it into kernel land.
DWORD DoOperationWriteInputs(const char *inputFilename)
{
	return FeedFileToFlicker(inputFilename, FLICKERDRV_CTRL_WRITE_INPUTS);
}

// Write outputs to a file
DWORD DoOperationReadOutputs(const char *outputFilename, long offset, long numBytes)
{
    DWORD resultValue = 0;
	DWORD returnedBytes = 0;
	char* buffer = (char*)malloc(numBytes);

	if (!buffer) {
		DisplayError("Could not allocate enough memory!\n");
		return -9;
	}

	// Open a handle to the device driver
    HANDLE hFlickerDrvDevice = CreateFileA("\\\\.\\FlickerDrv",
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
    if (hFlickerDrvDevice == INVALID_HANDLE_VALUE) {
        DisplayError("Could not connect to FlickerDrv device");
		free(buffer);
        return -4;
    }

    // Attempt to read the outputs into our buffer
    BOOL success = DeviceIoControl(hFlickerDrvDevice,
                                   FLICKERDRV_CTRL_READ_OUTPUTS,
                                   &offset,
                                   sizeof(offset),
                                   buffer,
                                   numBytes,
                                   &returnedBytes,
                                   NULL);
    if (!success) {
        DisplayError("Could not perform device I/O control");
		CloseHandle(hFlickerDrvDevice);
		return -3;
    } else {
        if (GetLastError() != ERROR_SUCCESS) {
            DisplayError("Device I/O control for file write failed");
			CloseHandle(hFlickerDrvDevice);
			return -7;
        }
    }

    CloseHandle(hFlickerDrvDevice);


	// Now, write the buffer out to the file
	ofstream theFile (outputFilename, ios::out|ios::binary);
	if (theFile.is_open()) {
		theFile.write(buffer, numBytes);
		theFile.close();
		free(buffer);

		cout << "Successfully wrote outputs to file " << outputFilename << endl;
	} else {
		cerr << "Unable to open file " << outputFilename << endl;
		free(buffer);
		return -3;
	}

    return resultValue;
}


int
main (
    int argc,
    char **argv
    )
{

    if (argc == 2 && !strcmp(argv[1], "go")) {  
        cout << "Result of go is " << Go() << endl;
    } else if (argc == 5 && !strcmp(argv[1], "out")) {
		cout << "Result of operation out is " << DoOperationReadOutputs(argv[2], atoi(argv[3]), atoi(argv[4])) << endl;
	} else if (argc != 3) { // All other operations require exactly 1 argument		
        PrintUsage(argv[0]);
        return -1;
    } else if (!strcmp(argv[1], "sinit")) {
        cout << "Result of operation sinit is " << DoOperationReadSinit(argv[2]) << endl;
    } else if (!strcmp(argv[1], "pal")) {
        cout << "Result of operation pal is " << DoOperationReadPal(argv[2]) << endl;
    } else if (!strcmp(argv[1], "in")) {
        cout << "Result of operation in is " << DoOperationWriteInputs(argv[2]) << endl;
    } else {
        PrintUsage(argv[0]);
        return -1;
    }

    return 0;
}
