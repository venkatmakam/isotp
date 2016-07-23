#ifndef ISOTP_LINUX_H
#define ISOTP_LINUX_H

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#include <errno.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <unistd.h>

#include <pthread.h>

#include "isotp.h"


#define ISOTP_TIMER_SIGNAL 61


typedef struct {

	int read_socket;

	int write_socket;

	char *socketcan_name;

} isotp_linux;


typedef struct {
    
    timer_callback callback;
    
    void *params;
    
    timer_t *timer;

} timer_params;


bool isotp_start(char *socketcan_name);

bool isotp_finish();



#endif // ISOTP_LINUX_H
