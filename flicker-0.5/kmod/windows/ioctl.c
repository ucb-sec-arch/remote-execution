//
// ioctl.c
//
// Code for handling I/O control requests to the FlickerDrv device.
//

#include <ntddk.h>
#include <ntstrsafe.h>
#include "../Common/KernUser.h"
#include "Globals.h"
#include "ioctl.h"
#include "flicker.h"
#include "log.h" /* for dbg() */
#include "acmod.h"

extern acmod_t *g_acmod;
extern size_t g_acmod_size;

/////////////////////////////////////////////////////////////////////////////
//                             I/O CONTROL
/////////////////////////////////////////////////////////////////////////////

NTSTATUS
DoFlickerDrvIoControl(
    ULONG ioControlCode,
    PCHAR inBuf,
    ULONG inBufLength,
    PCHAR outBuf,
    ULONG outBufLength,
    PVOID *contextPtr,
    PULONG returnedBytesPtr
    )
{
     dbg("FlickerDrv: Received ioControlCode 0x%08x.", ioControlCode);

	switch (ioControlCode) {
      case FLICKERDRV_CTRL_OPERATION_GO :
      {
          ULONG inputValue, outputValue;

          dbg("FlickerDrv: Received go command.  About to launch DRTM.");	  

		  launch_drtm(); // Go for it!!!
			
		  if (returnedBytesPtr != NULL) {
			  *returnedBytesPtr = 0;
		  }
          return STATUS_SUCCESS;
      }
      break;

      case FLICKERDRV_CTRL_WRITE_SINIT :
	  {
		  PHYSICAL_ADDRESS sinitPhys;
		  dbg("FlickerDrv: Received SINIT write request with %d bytes.", inBufLength);
		  dump_bytes(inBuf, inBufLength < 256 ? inBufLength : 256);

		  if(inBufLength > ACMOD_SIZE_MAX) {
			error("FlickerDrv: ACMOD size (%d bytes) is TOO BIG; maximum %d bytes.", inBufLength, ACMOD_SIZE_MAX);
			return STATUS_BUFFER_TOO_SMALL;
		  }

		  memcpy(g_acmod->acm.raw, inBuf, inBufLength);
		  g_acmod_size = inBufLength;

		  sinitPhys = MmGetPhysicalAddress(g_acmod->acm.raw);
 		  dbg("FlickerDrv: sinitPhys 0x%016x bytes.", sinitPhys.QuadPart);

		  //verify_acmod();

		  return STATUS_SUCCESS;
	  }
	  break;

      case FLICKERDRV_CTRL_WRITE_PAL :
	  {
		  dbg("FlickerDrv: Received PAL write request with %d bytes.", inBufLength);
		  dump_bytes(inBuf, inBufLength < 256 ? inBufLength : 256);

		  if(inBufLength > MAX_PAL_SIZE) {
			error("FlickerDrv: PAL size (%d bytes) is TOO BIG; maximum %d bytes.", inBufLength, MAX_PAL_SIZE);
			return STATUS_BUFFER_TOO_SMALL;
		  }

		  memcpy(g_pal->pal, inBuf, inBufLength);

		  dbg("FlickerDrv: palPhys 0x%016x", MmGetPhysicalAddress(g_pal->pal).QuadPart);

		  return STATUS_SUCCESS;
	  }
	  break;

	  case FLICKERDRV_CTRL_WRITE_INPUTS :
	  {
		  dbg("FlickerDrv: Received inputs with %d bytes.", inBufLength);
		  dump_bytes(inBuf, inBufLength < 256 ? inBufLength : 256);

		  if (inBufLength > MAX_INPUT_SIZE) {
			error("FlickerDrv: Input size (%d bytes) is TOO BIG; maximum %d bytes.", inBufLength, MAX_INPUT_SIZE);
			return STATUS_BUFFER_TOO_SMALL;
		  }

		  memcpy(g_pal->inputs, inBuf, inBufLength);

		  dbg("FlickerDrv: Inputs placed at physical address %016x\n", MmGetPhysicalAddress(g_pal->inputs));

		  return STATUS_SUCCESS;
	  }
	  break;

	  case FLICKERDRV_CTRL_READ_OUTPUTS :
	  {
		  unsigned long offset = 0;

		  if (inBufLength != 4) {
			  return STATUS_INVALID_PARAMETER;
		  }
		  offset = *((unsigned long *) inBuf);
		  if (offset >= MAX_OUTPUT_SIZE) { // Don't need to check for < 0, since offset is unsigned
			  return STATUS_INVALID_PARAMETER;
		  }

		  dbg("FlickerDrv: Received request for outputs at offset %d with a buffer of %d bytes.", offset, outBufLength);

		  memcpy(outBuf, g_pal->outputs + offset, min(outBufLength, MAX_OUTPUT_SIZE - offset));

		  return STATUS_SUCCESS;
	  }
	  break;



      default :
        return STATUS_NOT_IMPLEMENTED;
        break;
    }
}
