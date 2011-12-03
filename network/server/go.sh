#!/bin/bash

# Flicker session invocation script for Linux; copes with both AMD and Intel CPUs

# stop if anything goes wrong
set -e

# Just in case...
sync

# Verify flicker kernel module is installed
if [ `grep flicker /proc/modules | wc -l` = "0" ]
    then
    echo "Inserting flicker.ko module"
    insmod /home/tess/Downloads/remote-execution/flicker-0.5/kmod/flicker.ko
fi

# Disable (up to 8) APs
for i in `seq 1 9`
  do
  if [ -e /sys/devices/system/cpu/cpu$i ]
      then
      if [ `cat /sys/devices/system/cpu/cpu$i/online` = "1" ]
          then
          echo "Disabling CPU $i"
          echo 0 > /sys/devices/system/cpu/cpu$i/online
      fi
  fi
done

SYSFSPATH=/sys/kernel/flicker

# Try to be CPU-agnostic in this script. ASSUMPTION: There is only
# one SINIT module on the system, it is located in /boot, and it
# contains SINIT in the filename.
SINIT=""
for sinitfile in /boot/*SINIT*
do
    SINIT=$sinitfile
done
if [ -e $SINIT ]
then
    echo "Found SINIT $SINIT"
    # Load ACmod
    echo -n A > $SYSFSPATH/control
    cat $SINIT > $SYSFSPATH/data
    echo -n a > $SYSFSPATH/control
else
    # Could not find SINIT, check for Intel processor
    if [ `grep Intel /proc/cpuinfo | wc -l` -gt 0 ] ; then
        echo "FATAL ERROR: Intel processor detected but no SINIT module found."
        exit
    fi
    # We're still here.  Assume we have an AMD processor and proceed.
    echo "Proceeding for AMD processor"
fi

# Load PAL
echo -n M > $SYSFSPATH/control
cat pal.bin > $SYSFSPATH/data
echo -n m > $SYSFSPATH/control

# Load some test inputs (3 inputs: command 42, and two null-padded strings)
echo -n I > $SYSFSPATH/control
# Try this echo command from the bash prompt and pipe through hd to get:
#00000000  01 00 00 00 ca fe f0 0d  04 00 00 00 de ad be ef  |................|
# echo -ne \\x01\\x00\\x00\\x00\\x0d\\xf0\\xfe\\xca\\x04\\x00\\x00\\x00\\xef\\xbe\\xad\\xde > $SYSFSPATH/data
cat server_flicker_input > $SYSFSPATH/data
echo -n i > $SYSFSPATH/control

# Launch Flicker session
sleep 1
echo -n G > $SYSFSPATH/control

# Read outputs
echo "Retrieving outputs from Flicker session:"
cat $SYSFSPATH/data | hd

rmmod flicker

PCRS=`find /sys -name pcrs`
if [ ! -z $PCRS ]
  then
  echo PCRs found at $PCRS
  grep PCR-17 $PCRS
fi

# Re-enable (up to 8) APs
for i in `seq 1 9`
  do
  if [ -e /sys/devices/system/cpu/cpu$i ]
      then
      if [ `cat /sys/devices/system/cpu/cpu$i/online` = "0" ]
          then
          echo "Enabling CPU $i"
          echo 1 > /sys/devices/system/cpu/cpu$i/online
      fi
  fi
done

exit

