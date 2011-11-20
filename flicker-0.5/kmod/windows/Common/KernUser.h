//
// KernUser.h
//
// Contains all the information about FlickerDrv that is used to communicate
// between kernel and user level.
//

///////////////////////////////////////////////////////////////////////////
//                            DEVICE CONTROL
///////////////////////////////////////////////////////////////////////////

// Device type values.  Note that values used by Microsoft Corporation
// are in the range 0-32767, and 32768-65535 are reserved for use
// by customers.

#define FILE_DEVICE_FLICKERDRV        0x0000A813

// Device control codes that can be sent to FlickerDrv

#define FLICKERDRV_CTRL_OPERATION_GO \
        (ULONG) CTL_CODE(FILE_DEVICE_FLICKERDRV, 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FLICKERDRV_CTRL_WRITE_SINIT \
        (ULONG) CTL_CODE(FILE_DEVICE_FLICKERDRV, 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FLICKERDRV_CTRL_WRITE_PAL \
        (ULONG) CTL_CODE(FILE_DEVICE_FLICKERDRV, 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FLICKERDRV_CTRL_WRITE_INPUTS \
        (ULONG) CTL_CODE(FILE_DEVICE_FLICKERDRV, 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FLICKERDRV_CTRL_READ_OUTPUTS \
        (ULONG) CTL_CODE(FILE_DEVICE_FLICKERDRV, 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS)


