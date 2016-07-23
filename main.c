#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

#include "src/isotp_linux.h"
#include "src/isotp.h"

void my_message_received_handler(const isotp_receive_handle* handle)
{
    int i;
    if (handle->success) {
        printf("Success: Receive data [Length = %d]: ", handle->message_size);
        for (i = 0; i < handle->message_size; i++)
            printf("%x ",handle->payload[i]);
        printf("\n");
    } else {
        printf("Receive failed.\n");
    }
}

void my_sent_message(const isotp_send_handle *handle)
{
    int i;
    if (handle->success) {
        printf("Success: Send data [Length = %d]: ", handle->message_size);
        for (i = 0; i < handle->message_size; i++)
            printf("%x ",handle->payload[i]);
        printf("\n");
    }else {
        printf("Send failed\n");
    }
}

int main(int argc, char *argv[])
{
    isotp_start("can0");

    printf("BEGIN!\n");
    /* RECEIVE INIT */
    isotp_receive_handle my_handle[10];
    int i = 0;
    for(i = 0; i < 10; i++) {
        my_handle[i].send_id = 0x0 + i;
        my_handle[i].receive_id = 0x7 + i;
        my_handle[i].timeout_ms = 0x7F;
        my_handle[i].message_received_callback = my_message_received_handler;

        isotp_receive_init(&my_handle[i]);
    }
    /* END RECEIVE INIT */

    /* SEND DATA */
    uint8_t data1[20] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                         0x07, 0x08, 0x09, 0x00, 0x01, 0x02, 0x03,
                         0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

    isotp_send_handle send_handle;
    send_handle.message_size = 19;
    send_handle.payload = data1;
    send_handle.receive_timeout_ms = 0x7F;
    send_handle.send_id = 0x9;
    send_handle.receive_id = 0x2;
    send_handle.message_sent_callback = my_sent_message;

    isotp_send(&send_handle);

    /* END SEND DATA */

    while(true); /* Working... */

    isotp_finish();
    
    return 0;
}