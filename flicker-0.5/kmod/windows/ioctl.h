//
// Ioctl.h
//
// Function prototypes for Ioctl.c
//

NTSTATUS
DoFlickerDrvIoControl(
    ULONG ioControlCode,
    PCHAR inBuf,
    ULONG inBufLength,
    PCHAR outBuf,
    ULONG outBufLength,
    PVOID *contextPtr,
    PULONG returnedBytesPtr
    );
