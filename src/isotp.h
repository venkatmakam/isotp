#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* The state of the program during the frame exchange */
typedef enum {
    /* Work state: */
    ISOTP_IDLE, 
    ISOTP_WAIT_FC,
    ISOTP_WAIT_DATA,
    ISOTP_SENDING,
    ISOTP_OK,
    /* Errors: */
    ISOTP_TIMEOUT,
    ISOTP_BUFFER_OVFLW,
    ISOTP_WRONG_INDEX,
    ISOTP_ERROR
} isotp_state;



/* Flow Status given in FC frame */
typedef enum {
    FC_CTS = 0,   /* continue to send */
    FC_WAIT = 1,        /* wait next frame */
    FC_OVFLW = 2      /* overflow */
} isotp_fc_flag;



/* N_PCI type values in bits 7-4 of N_PCI bytes */
typedef enum {
    N_PCI_SF = 0x00,  /* single frame */
    N_PCI_FF = 0x10,  /* first frame */
    N_PCI_CF = 0x20,  /* consecutive frame */
    N_PCI_FC = 0x30,  /* flow control frame */
} isotp_npci_type;


/* CAN payload length and DLC definitions according to ISO 11898-1 */
#define CAN_MAX_DLEN 8

/* ISO 15765-2 can carry up to 2^12bytes of payload per message packet. */
#define ISOTP_MAX_DLEN 4095

/* Limit for handles array */
#define ISOTP_RECEIVE_HANDLES_SIZE 10
#define ISOTP_SEND_HANDLES_SIZE 10

typedef void (*timer_callback)(void *params);

typedef void (*debug_shim)(const char* message, ...);

typedef bool (*send_can_frame_shim)(const uint16_t id,
        const uint8_t* data, const uint8_t size);

typedef bool (*set_time_shim)(uint32_t time_ms,
        void (*callback)(void *params), void *params);

typedef uint8_t* (*isotp_get_buffer)(const uint16_t size);

typedef struct {

    debug_shim debug;

    send_can_frame_shim send_can_message;

    set_time_shim set_timer;

    isotp_get_buffer get_buffer;

} isotp_shims;

bool isotp_init(isotp_shims new_shims);

/* SEND */

typedef struct isotp_send_handle isotp_send_handle;

typedef void (*isotp_message_sent_handler)(const isotp_send_handle* handle);

struct isotp_send_handle{

    bool completed;
    bool success;

    uint16_t receive_id;
    uint16_t receive_timeout_ms;

    uint16_t blocksize;
    uint16_t stmin;

    uint16_t send_id;
    uint8_t *payload;
    uint16_t message_size;
    uint16_t current_message_size;

    uint8_t index;

    isotp_state state;

    isotp_message_sent_handler message_sent_callback;

};

bool isotp_send(isotp_send_handle* handle);

void isotp_continue_send(isotp_send_handle **handle);

/* RECEIVE*/

typedef struct isotp_receive_handle isotp_receive_handle;

typedef void (*isotp_message_received_handler)
                (const isotp_receive_handle* handle);

struct isotp_receive_handle {

    bool completed;
    bool success;

    uint16_t send_id;
    uint16_t timeout_ms;

    isotp_message_received_handler message_received_callback;

    uint16_t receive_id;
    uint8_t *payload;
    uint16_t message_size;
    uint16_t current_message_size;

    uint8_t index;

    isotp_state state;
};

bool isotp_receive_init(isotp_receive_handle *handle);

bool isotp_continue_receive(const uint16_t id,
        const uint8_t data[], const uint8_t size);


#endif // ISOTP_H
