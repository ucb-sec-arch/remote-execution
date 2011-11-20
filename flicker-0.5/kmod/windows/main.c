// 
// main.c
//
// Code for the FlickerDrv driver.
//

#include "ntddk.h"
#include "../Common/KernUser.h"
#include "Globals.h"
#include "ioctl.h"
#include "flicker.h"
#include "log.h" /* for dbg() */
#include "txt.h" /* for txt_get_error() */
#include "heap.h"
#include "mtrrs.h"

extern acmod_t* g_acmod;
static void* g_pal_region = NULL;
static char* allocated_g_acmod = NULL;
static char* allocated_g_pal_region = NULL;

#define SIZE_128K (128*1024)
#define SIZE_4K (4*1024)

////////////////////////////////////////////////////////////////////////////
//                               MACROS
////////////////////////////////////////////////////////////////////////////

#define SYMBOLIC_LINK_NAME                       L"\\DosDevices\\FlickerDrv"

////////////////////////////////////////////////////////////////////////////
//                               GLOBALS
////////////////////////////////////////////////////////////////////////////

// All the global variables are in one neat structure:

FLICKER_GLOBALS globals;

////////////////////////////////////////////////////////////////////////////
//                           FORWARD DEFINES
////////////////////////////////////////////////////////////////////////////

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
FreeAllocations(
    VOID
    );

NTSTATUS
DoAllocations(
    VOID
    );

NTSTATUS
FlickerDrvDispatchControl(
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    );

NTSTATUS
FlickerDrvDispatchCreate(
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    );

NTSTATUS
FlickerDrvDispatchCleanup(
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    );

NTSTATUS
FlickerDrvDispatchClose(
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    );


VOID
FlickerDrvUnload(
    PDRIVER_OBJECT DriverObject
    );

////////////////////////////////////////////////////////////////////////////
//                                SECTIONS
////////////////////////////////////////////////////////////////////////////

// Define the sections that allow for discarding (i.e. paging) some of
// the code.

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

////////////////////////////////////////////////////////////////////////////
//                             DRIVER ENTRY
////////////////////////////////////////////////////////////////////////////

// DriverEntry is the installable driver initialization.  Here we set up
// the driver, create our one FlickerDrv device, and do all sorts of
// miscellaneous initialization.

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING flickerDeviceNameUnicodeString;
    UNICODE_STRING flickerDeviceLinkUnicodeString;

    dbg("******************************************************\nFlickerDrv: Entering DriverEntry %d\n", 0);

    // Initialize global variables.

    globals.flickerDeviceObject = NULL;
    globals.createdSymbolicLinkToDevice = FALSE;
    KeInitializeSpinLock(&globals.mainMutex);

	// Allocate expensive resources
	status = DoAllocations();
    if (status != STATUS_SUCCESS) {
        FreeAllocations();
        return status;
    }

	// XXX TODO: Verify that the local platform includes the necessary hardware support.  
	// Right now that means Flicker on Linux has been manually tested and confirmed to work.
	// 2010.09.18 Jon

    // Create dispatch points for all the IRPs we handle.
    DriverObject->MajorFunction[IRP_MJ_CREATE] = FlickerDrvDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = FlickerDrvDispatchCleanup;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = FlickerDrvDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FlickerDrvDispatchControl;

    DriverObject->DriverUnload = FlickerDrvUnload;

    // Create the one and only FlickerDrv device    
    RtlInitUnicodeString(&flickerDeviceNameUnicodeString, L"\\Device\\FlickerDrv");
    status = IoCreateDevice(DriverObject,
                            0,
                            &flickerDeviceNameUnicodeString,
                            FILE_DEVICE_FLICKERDRV,
                            0,
                            FALSE,
                            &globals.flickerDeviceObject);
    if (status != STATUS_SUCCESS) {
        FreeAllocations();
        return status;
    }

    // Create a symbolic link for the FlickerDrv device so that it can be accessed
    // from user level.    
    RtlInitUnicodeString(&flickerDeviceLinkUnicodeString, SYMBOLIC_LINK_NAME);
    status = IoCreateSymbolicLink(&flickerDeviceLinkUnicodeString,
                                  &flickerDeviceNameUnicodeString);
    if (status != STATUS_SUCCESS) {
        FreeAllocations();
        return status;
    }
    globals.createdSymbolicLinkToDevice = TRUE;

	// Check for errors from our last run
	if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
		if (!txt_get_error()) {
			dbg("*** There were SENTER errors last time! ***\n");
			serial_out_string("*** There were SENTER errors last time! ***\n");
		}
	}

	// Check the size of various structs/unions
	dbg("sizeof(os_sinit_data_t) = %d, sizeof(txt_caps_t) = %d, sizeof(os_mle_data_t) = %d, \nsizeof(mtrr_state_t) = %d, sizeof(mtrr_physmask_t) = %d, sizeof(mtrr_def_type_t) = %d\n",
		sizeof(os_sinit_data_t),
		sizeof(txt_caps_t),
		sizeof(os_mle_data_t),
		sizeof(mtrr_state_t),
		sizeof(mtrr_physmask_t),
		sizeof(mtrr_def_type_t));

    return STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////
//                           RESOURCE MANAGEMENT
////////////////////////////////////////////////////////////////////////////

// Releases any resources we have allocated for FlickerDrv so far.
VOID
FreeAllocations (
    VOID
    )
{
    UNICODE_STRING deviceLinkUnicodeString;
	dbg("Entered FlickerDrvReleaseResources");
    if (globals.createdSymbolicLinkToDevice) {
        RtlInitUnicodeString(&deviceLinkUnicodeString, SYMBOLIC_LINK_NAME);
        IoDeleteSymbolicLink(&deviceLinkUnicodeString);
        globals.createdSymbolicLinkToDevice = FALSE;
    }

    if (globals.flickerDeviceObject != NULL) {
        IoDeleteDevice(globals.flickerDeviceObject);
        globals.flickerDeviceObject = NULL;
    }


	if(allocated_g_acmod) {
		MmFreeContiguousMemory(allocated_g_acmod);
		allocated_g_acmod = NULL;
	}

	if(allocated_g_pal_region) {
		MmFreeContiguousMemory(allocated_g_pal_region);
		allocated_g_pal_region = NULL;
	}
	dbg("Returning from FlickerDrvReleaseResources");
}


// DoAllocations allocates the large, contiguous 
// memory regions that will be used to facilitate Flicker sessions.
NTSTATUS 
DoAllocations(VOID)
{
	NTSTATUS status = STATUS_SUCCESS;
	PHYSICAL_ADDRESS low, high, boundary;
	uint32_t needed_alloc_size = sizeof(pal_t);
	uint32_t padded_size = 0;

	low.QuadPart = 0ULL;
	high.QuadPart = 0xffffffffULL; // 4GB  

    assert(NULL == g_pal_region);
    assert(NULL == g_pal);
    assert(NULL == g_mle_ptab);
    assert(NULL == g_acmod);

    dbg("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");

    if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
        dbg("Intel CPU detected; doing additional allocation for ACMod and MLE PTs");
        /* We need more space on Intel for the MLE page tables. */
        needed_alloc_size += sizeof(mle_pt_t);

        /* We need a buffer into which the Authenticated Code module
         * can be stored.  From MLE developer's guide: "System
         * software should not utilize the memory immediately after
         * the SINIT AC module up to the next 4KByte boundary."  We
         * get this for free since kmalloc uses __get_free_pages
         * internally, and always returns 4K-aligned chunks.
         *
         * XXX TODO: Copy it directly into the TXT-specified location
         * without additional buffering. (i.e., the above comment is
         * irrelevant since the AC Mod is copied again before use. XXX
         * is it?) */

		// Make sure we're 128K aligned by over allocating
		padded_size = sizeof(acmod_t) + SIZE_4K - 1;
		allocated_g_acmod = MmAllocateContiguousMemory(padded_size, high);
		if(allocated_g_acmod == NULL) {
			error("alloc of %d bytes failed!", padded_size);			
			return STATUS_NO_MEMORY;
		}

		// Compute an allocated address
		g_acmod = (acmod_t*) (allocated_g_acmod + (SIZE_4K - ((uint32_t)allocated_g_acmod % SIZE_4K)));


		//boundary.QuadPart = sizeof(acmod_t); /* hopefully this makes us aligned */
		//g_acmod = MmAllocateContiguousMemorySpecifyCache(sizeof(acmod_t), low, high, boundary, MmNonCached);

  //      if(g_acmod == NULL) {
  //          error("alloc of 0x%08x bytes failed!", sizeof(acmod_t));
  //          return STATUS_NO_MEMORY;
  //      }

        dbg("alloc of 0x%08x bytes for acmod at virt 0x%08x.", sizeof(acmod_t), (uint32_t)g_acmod);
        dbg("g_mle_ptab @ 0x%p", g_mle_ptab);
        dbg("->pdpt  @ 0x%p", g_mle_ptab->pdpt);
        dbg("->pd    @ 0x%p", g_mle_ptab->pd);
        dbg("->pt    @ 0x%p", g_mle_ptab->pt);

    }

    dbg("PAGE_SIZE = 0x%08lx (%ld)", PAGE_SIZE, PAGE_SIZE);
    dbg("sizeof(pal_t)= 0x%08x (%d)", sizeof(pal_t), sizeof(pal_t));

	// Make sure we're 128K aligned by over allocating
	padded_size = needed_alloc_size + SIZE_128K - 1;
	allocated_g_pal_region = MmAllocateContiguousMemory(padded_size, high);
    if(allocated_g_pal_region == NULL) {
        error("alloc of %d bytes failed!", padded_size);
        if(g_acmod) { MmFreeContiguousMemory(allocated_g_acmod); g_acmod = NULL; }
        return STATUS_NO_MEMORY;
    }

	// Compute an allocated address
	g_pal_region = allocated_g_pal_region + (SIZE_128K - ((uint32_t)allocated_g_pal_region % SIZE_128K));

	//boundary.QuadPart = 0xbc * PAGE_SIZE; /* hopefully this makes us aligned */
	//g_pal_region = MmAllocateContiguousMemorySpecifyCache(needed_alloc_size, low, high, boundary, MmNonCached);
	//g_pal_region = MmAllocateContiguousMemory(needed_alloc_size, high);

    //if(g_pal_region == NULL) {
    //    error("alloc of %d bytes failed!", needed_alloc_size);
    //    if(g_acmod) { MmFreeContiguousMemory(g_acmod); g_acmod = NULL; }
    //    return STATUS_NO_MEMORY;
    //}

	 dbg("alloc of allocated_g_pal_region at virt 0x%08x.",
        (uint32_t)allocated_g_pal_region);

	 dbg("remainder is %d", (uint32_t)allocated_g_pal_region % SIZE_128K);

    dbg("alloc of %d bytes at virt 0x%08x.",
        needed_alloc_size, (uint32_t)g_pal_region);

    /* Verify that we have at least a 128K aligned block */
    if((unsigned long)(g_pal_region) != ((unsigned long)(g_pal_region) & ALIGN_128K)) {
        error("ERROR: memory not aligned!");
        MmFreeContiguousMemory(allocated_g_pal_region); allocated_g_pal_region = NULL;
        if(allocated_g_acmod) { MmFreeContiguousMemory(allocated_g_acmod); allocated_g_acmod = NULL; }
        return STATUS_NO_MEMORY;
    }

    /* zero the PAL container */ /* slow? necessary? */
    memset(g_pal_region, 0, needed_alloc_size);

    if(needed_alloc_size > sizeof(pal_t)) {
        /* Intel system; assign g_mle_ptab, then g_pal */
        g_mle_ptab = (mle_pt_t*)g_pal_region;
        g_pal = (pal_t*)((char*)g_pal_region + sizeof(mle_pt_t));
    } else {
        /* AMD system; just assign g_pal */
        g_pal = (pal_t*)g_pal_region;
    }

    dbg("g_pal           @ 0x%p", g_pal);
    dbg("g_pal->pal      @ 0x%p", g_pal->pal);
    dbg("&g_pal->reload  @ 0x%p", &(g_pal->reload));
    dbg("g_pal->inputs   @ 0x%p", g_pal->inputs);
    dbg("g_pal->outputs  @ 0x%p", g_pal->outputs);

	build_resume_page_tables(g_pal->pal, g_pal->resume_pagetabs);

    return 0;
}



////////////////////////////////////////////////////////////////////////////
//                          DISPATCH ROUTINES
////////////////////////////////////////////////////////////////////////////

// FlickerDrvDispatchControl is the dispatch routine called to handle any
// device control IRP sent to our FlickerDrv device.  The main thing to
// do here is check what kind of device control request it is and handle
// it accordingly.

NTSTATUS
FlickerDrvDispatchControl(
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    )
{
    PIO_STACK_LOCATION  irpStack;
    PCHAR               inBuf, outBuf;
    ULONG               inBufLength, outBufLength, ioControlCode;
    NTSTATUS            status;

    // Set the request up as successful, by default.

    status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;

    // Get a pointer to our part of the IRP stack.

    irpStack = IoGetCurrentIrpStackLocation(irp);

    // Get the control code (what kind of request this is).

    ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    dbg("FlickerDrvDispatchControl: ioControlCode 0x%08x", ioControlCode);

    // Get the pointer to the input/output buffer and its length.
    // These buffers are in different places depending on the method
    // used (the low two bits of ioControlCode).
    
    if ((ioControlCode & 0x3) == METHOD_NEITHER) {
		dbg("Buffer METHOD_MEITHER (ioControlCode 0x%08x)", ioControlCode);
        inBuf = irpStack->Parameters.DeviceIoControl.Type3InputBuffer;
        outBuf = irp->UserBuffer;
    }
    else {
		dbg("Buffer (in direct? out direct? buffered? (ioControlCode 0x%08x)", ioControlCode);
        inBuf = irp->AssociatedIrp.SystemBuffer;
        outBuf = irp->AssociatedIrp.SystemBuffer;
    }
    inBufLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    
    // Do the appropriate thing, depending on the control code

    if (irpStack->FileObject != NULL) {
        status = DoFlickerDrvIoControl(ioControlCode,
                                   inBuf,
                                   inBufLength,
                                   outBuf,
                                   outBufLength,
                                   &irpStack->FileObject->FsContext,
                                   &irp->IoStatus.Information);
	dbg("returned from DoFlickerDrvIoControl");
    } else {
		dbg("Error: irpStack->FileObject is NULL");
        status = STATUS_INVALID_DEVICE_REQUEST;
    }
  
    // Complete the IRP
    irp->IoStatus.Status = status;
	dbg("About to call IoCompleteRequest");
    IoCompleteRequest(irp, IO_NO_INCREMENT);
	dbg("Returned from call to IoCompleteRequest");
	dbg("About to return from FlickerDrvDispatchControl with status 0x%08x", status);

	
	return status;
}

NTSTATUS
FlickerDrvDispatchCreate (
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    )
{
	dbg("FlickerDrvDispatchCreate called");
    // Set the request up as successful

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    
    // Complete the IRP

    IoCompleteRequest(irp, IO_NO_INCREMENT);
	dbg("Survived IoCompleteRequest in main.c:FlickerDrvDispatchCreate");
    return STATUS_SUCCESS;
}

NTSTATUS
FlickerDrvDispatchCleanup (
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    )
{
	dbg("FlickerDrvDispatchCleanup called");
    // Set the request up as successful

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    
    // Complete the IRP

    IoCompleteRequest(irp, IO_NO_INCREMENT);
	dbg("Survived IoCompleteRequest in main.c:FlickerDrvDispatchCleanup");
    return STATUS_SUCCESS;
}

NTSTATUS
FlickerDrvDispatchClose (
    IN PDEVICE_OBJECT devObj,
    IN PIRP irp
    )
{
	dbg("FlickerDrvDispatchClose called");
    // Set the request up as successful

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    
    // Complete the IRP

    IoCompleteRequest(irp, IO_NO_INCREMENT);
	dbg("Survived IoCompleteRequest in main.c:FlickerDrvDispatchClose");
    return STATUS_SUCCESS;
}

// FlickerDrvUnload is called when the driver is about to be unloaded.

VOID
FlickerDrvUnload (
    PDRIVER_OBJECT DriverObject
    )
{
	dbg("FlickerDrvUnload called");
	FreeAllocations();
	dbg("FlickerDrvUnload survived call to FreeAllocations");
}
