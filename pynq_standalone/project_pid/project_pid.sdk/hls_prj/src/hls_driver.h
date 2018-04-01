
#ifndef __HLS_DRIVER_H
#define __HLS_DRIVER_H

#include "xpid_regulator.h"

extern void hls_pid_return_isr(void *InstancePtr);
extern int hls_PID_init(XPid_regulator *hls_Ptr_pid);
extern void pid_input(void);

#endif
