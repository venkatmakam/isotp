#include "isotp_linux.h"

static isotp_linux isotp_data;

bool my_send_can(const uint16_t id, const uint8_t* data,
        const uint8_t size)
{
    struct can_frame c_frame;
    c_frame.can_id  = id;
    c_frame.can_dlc = size;
    memcpy(c_frame.data, data, size);

    if (write(isotp_data.write_socket, &c_frame, sizeof(struct can_frame)) > 0)
        return true;
    else
        return false;
}

uint8_t* my_malloc(const uint16_t size)
{
    return (uint8_t *)malloc(size);
}

void my_free(uint8_t *buffer)
{
    free((void *)buffer);
}

void socket_can_init(int *sock)
{
    struct sockaddr_can addr;
    struct ifreq ifr;

    if((*sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        printf("Error while opening socket\n");
        pthread_exit(NULL);
    }

    strcpy(ifr.ifr_name, isotp_data.socketcan_name);
    ioctl(*sock, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if(bind(*sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error in socket bind\n");
        pthread_exit(NULL);
    }
}

/* Thread function for async reading */
void* linux_reading(void *arg)
{
	struct can_frame c_frame;
    while (true){
        if ((read(isotp_data.read_socket, &c_frame,
                  sizeof(struct can_frame))) > 0){
 			
 			isotp_continue_receive(c_frame.can_id, c_frame.data,
 			 c_frame.can_dlc);
        }
    }
}

void timer_handler(int sig, siginfo_t *si, void *uc)
{
    timer_params *t = (timer_params*) si->si_value.sival_ptr;
    t->callback(t->params);
    timer_delete(*t->timer);
    free(t->timer);
    free(t);
}

bool my_set_timer(uint32_t time_ms, timer_callback callback, void *params)
{
    timer_t *timerID = (timer_t *) malloc(sizeof(timer_t));

    struct sigevent se;

    timer_params *t = (timer_params *) malloc(sizeof(timer_params));
    t->params = params;
    t->callback = callback;
    t->timer = timerID;

    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = ISOTP_TIMER_SIGNAL;
    se.sigev_value.sival_ptr = t;

    timer_create(CLOCK_REALTIME, &se, timerID);

    struct itimerval param;
    bzero(&param, sizeof(param));
    param.it_value.tv_usec = time_ms;
    timer_settime(*timerID, 0, (const struct itimerspec *) &param, NULL);

    return true;
}

void debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\r\n");
    va_end(args);
}

bool isotp_start(char *socketcan_name)
{
	isotp_data.socketcan_name = socketcan_name;

	socket_can_init(&isotp_data.write_socket);
	socket_can_init(&isotp_data.read_socket);

    struct sigaction act1;
    act1.sa_flags = SA_SIGINFO;
    act1.sa_sigaction = timer_handler;
    sigemptyset(&act1.sa_mask);
    sigaction(ISOTP_TIMER_SIGNAL, &act1, 0);


    /* READING IN LINUX... */
    pthread_t thread;
    pthread_create(&thread, NULL, linux_reading, NULL);
    /* END READ */

    /* SHIMS INIT */
    isotp_shims shims;
    shims.debug = debug;
    shims.send_can_message = my_send_can;
    shims.set_timer = my_set_timer;
    shims.get_buffer = my_malloc;

    isotp_init(shims);
    /* END SHIMS INIT */

    return true;
}


bool isotp_finish()
{
	close(isotp_data.write_socket);
	close(isotp_data.read_socket);
	return true;
}






