#include "isotp.h"

static isotp_receive_handle *isotp_receive_handles[ISOTP_RECEIVE_HANDLES_SIZE];

static isotp_send_handle *isotp_send_handles[ISOTP_SEND_HANDLES_SIZE];

static isotp_shims shims;

#define INC_INDEX(i) if (++(*i) > 15) *i = 0;

bool isotp_init(isotp_shims new_shims)
{
    shims = new_shims;
    return true;
}

static uint32_t get_real_timer(uint16_t time)
{
    if (time == 0) return 1000000;
    if(time <= 0x7F)
            return time * 1000000;
        else
            return (time - 0xF0) * 1000;
}

static void clear_receive_handle(isotp_receive_handle *handle)
{
    handle->success = false;
    handle->completed = false;
    handle->state = ISOTP_IDLE;
    handle->current_message_size = 0;
    handle->index = 0;
    handle->payload = NULL;
    handle->message_size = 0;
}

static void clear_send_handle(isotp_send_handle *handle)
{
    handle->success = false;
    handle->completed = false;
    handle->state = ISOTP_IDLE;
    handle->current_message_size = 0;
    handle->index = 0;
    handle->blocksize = 0;
    handle->stmin = 0;
}

bool isotp_receive_init(isotp_receive_handle *handle)
{
    int i;
    for(i = 0; i < ISOTP_RECEIVE_HANDLES_SIZE; i++){
        if (isotp_receive_handles[i] == NULL){

            isotp_receive_handles[i] = handle;

            clear_receive_handle(isotp_receive_handles[i]);

            shims.debug("id: %x Receive message init\n", handle->receive_id);

            return true;
        }
    }

    shims.debug("isotp_receive_handles is full [max: %d]!\n", 
            ISOTP_RECEIVE_HANDLES_SIZE);
    return false;
}

static void isotp_send_next_frame(isotp_send_handle **phandler)
{
    isotp_send_handle* handle = *phandler;

    uint8_t data[8];

    INC_INDEX(&handle->index);

    data[0] = (N_PCI_CF | (handle->index & 0x0F));
    uint16_t remaining_size = handle->message_size - 
            handle->current_message_size;
    if (remaining_size < 8) {
        memcpy(data + 1, handle->payload + handle->current_message_size, 
                remaining_size);

        if (!shims.send_can_message(handle->send_id, data, remaining_size + 1)){
            shims.debug("Can`t send CF\n");
            handle->state = ISOTP_ERROR;
            handle->message_sent_callback(handle);
            *phandler = NULL;

            return;
        }

        handle->success = true;
        handle->completed = true;
        handle->state = ISOTP_OK;
        handle->current_message_size += remaining_size;
        handle->message_sent_callback(handle);
        *phandler = NULL;

        return;
    }
    memcpy(data + 1, handle->payload + handle->current_message_size, 7);
    if (!shims.send_can_message(handle->send_id, data, CAN_MAX_DLEN)){
        shims.debug("Can`t send CF\n");
        handle->state = ISOTP_ERROR;
        handle->message_sent_callback(handle);
        *phandler = NULL;
    }
    handle->current_message_size += 7;

    return;
}

void isotp_continue_send(isotp_send_handle **phandler)
{
    isotp_send_handle* handle = *phandler;

    if (handle->state != ISOTP_SENDING) {
        return;
    }

    isotp_send_next_frame(phandler);

    if (handle->state == ISOTP_SENDING) {
        shims.set_timer(get_real_timer(handle->stmin), 
                (void*) isotp_continue_send, phandler);
    }
    return;
}

void isotp_wait_fc(void *point)
{
    isotp_send_handle **phandler = (isotp_send_handle **) point;
    isotp_send_handle* handle = *phandler;

    if (handle->state != ISOTP_WAIT_FC) {
        return;
    }

    shims.debug("Oops! Timeout FC receive!\n");
    handle->state = ISOTP_TIMEOUT;
    handle->message_sent_callback(handle);
    *phandler = NULL;

    return;
}

bool isotp_send(isotp_send_handle *handle)
{
    clear_send_handle(handle);

    if (handle->payload == NULL || handle->message_size < 1) {
        shims.debug("Send data not foud!\n");
        handle->state = ISOTP_ERROR;
        handle->message_sent_callback(handle);
        return false;
    }

    if (handle->message_size > ISOTP_MAX_DLEN) {
        shims.debug("Too large size! Maximum: %d\n", ISOTP_MAX_DLEN);
        handle->state = ISOTP_ERROR;
        handle->message_sent_callback(handle);
        return false;
    }

    uint8_t data[8];

    if (handle->message_size < 8){
        /* SEND SF */
        data[0] = (N_PCI_SF | handle->message_size);
        shims.debug("FIRST BYTE: %.2x\n", data[0]);
        memcpy(data + 1, handle->payload, handle->message_size);
        if (!shims.send_can_message(handle->send_id, data, 
                handle->message_size + 1)){
            shims.debug("Can`t send single frame\n");
            handle->state = ISOTP_ERROR;
            handle->message_sent_callback(handle);
            return false;
        }

        shims.debug("Single frame was sended. Size: %d\n", 
                handle->message_size);

        handle->success = true;
        handle->completed = true;
        handle->state = ISOTP_OK;
        handle->current_message_size = handle->message_size;

        handle->message_sent_callback(handle);

        return true;
    }

    int i;
    for(i = 0; i < ISOTP_SEND_HANDLES_SIZE; i++) {
        if (isotp_send_handles[i] == NULL) {
            isotp_send_handles[i] = handle;

            shims.debug("id: %x Send message init\n", handle->send_id);
            break;
        }
    }

    if (i == ISOTP_SEND_HANDLES_SIZE) {
        shims.debug("isotp_send_handles is full [max: %d]!\n", 
                ISOTP_SEND_HANDLES_SIZE);
        handle->state = ISOTP_ERROR;
        handle->message_sent_callback(handle);

        return false;
    }

    /* SEND FF */

    data[0] = (N_PCI_FF |
                ((handle->message_size & 0x0F00) >> 8));
    data[1] = (handle->message_size & 0x00FF);
    memcpy(data + 2, handle->payload, 6);
    if (!shims.send_can_message(handle->send_id, data, CAN_MAX_DLEN)){
        shims.debug("Can`t send first frame\n");
        handle->state = ISOTP_ERROR;
        handle->message_sent_callback(handle);
        isotp_send_handles[i] = NULL;
        return false;
    }
    handle->current_message_size = 6;
    handle->state = ISOTP_WAIT_FC;
    shims.set_timer(get_real_timer(handle->receive_timeout_ms),
                     (void*)isotp_wait_fc, &isotp_send_handles[i]);

    shims.debug("Wait FC...\n");

    return true;
}

static bool pci_sf_manage(isotp_receive_handle *handle, const uint16_t id,
                   const uint8_t data[], const uint8_t size)
{
    uint16_t PCIsize;
    PCIsize = data[0] & 0x0F;

    handle->payload = shims.get_buffer(PCIsize);
    if (handle->payload == NULL) {
        handle->state = ISOTP_BUFFER_OVFLW;
        handle->message_received_callback(handle);
        return false;
    }

    handle->message_size = PCIsize;
    handle->current_message_size = PCIsize;
    handle->completed = true;
    handle->success = true;
    memcpy(handle->payload, data + 1, PCIsize);
    handle->state = ISOTP_OK;

    shims.debug("Single frame was received. Length: %d\n", 
            handle->message_size);

    handle->message_received_callback(handle);
    clear_receive_handle(handle);

    return true;
}

static bool send_fc_frame(uint16_t id,
                   isotp_fc_flag flag, uint8_t bs, uint8_t stmin)
{
    uint8_t fc_frame[3];

    fc_frame[0] = (N_PCI_FC | flag);
    fc_frame[1] = bs;
    fc_frame[2] = stmin;

    return shims.send_can_message(id, fc_frame, 3);
}

static bool pci_ff_manage(isotp_receive_handle *handle, const uint16_t id,
                   const uint8_t data[], const uint8_t size)
{
    uint16_t PCIsize;

    if (handle->state == ISOTP_WAIT_DATA) {
        shims.debug("Again FF?.. Okey, clear and go!\n");
        clear_receive_handle(handle);
        return false;
    }

    PCIsize = (data[0] & 0x0F) << 8;
    PCIsize += data[1];
    handle->message_size = PCIsize;
    handle->payload = shims.get_buffer(PCIsize);
    if (handle->payload == NULL) {
        handle->state = ISOTP_BUFFER_OVFLW;
        handle->message_received_callback(handle);
        clear_receive_handle(handle);
        return false;
    }
    memcpy(handle->payload, data + 2, 6);
    handle->current_message_size += 6;
    shims.debug("[id: %x] First frame was received. Length: %d\n",
            handle->receive_id , handle->message_size);
    handle->state = ISOTP_WAIT_DATA;

    if (!send_fc_frame(handle->send_id, FC_CTS, 0, handle->timeout_ms)) {
        shims.debug("Can`t send fc frame! \n");
        handle->state = ISOTP_ERROR;
        handle->message_received_callback(handle);
        clear_receive_handle(handle);
        return false;
    }
    shims.debug("Send FC with id: %x\n", handle->send_id);

    return true;
}

static bool pci_cf_manage(isotp_receive_handle *handle, const uint16_t id,
                   const uint8_t data[], const uint8_t size)
{
    uint8_t frame_index;
    uint16_t remaining_size;

    if (handle->state != ISOTP_WAIT_DATA) {
        shims.debug("Wrong state!\n");
        return false;
    }

    INC_INDEX(&handle->index);
    frame_index = data[0] & 0x0F;
    if (frame_index != handle->index) {
        shims.debug("Wrong frame order (wait: %d)(receive: %d)!\n",
                     handle->index, frame_index);
        handle->state = ISOTP_WRONG_INDEX;
        handle->message_received_callback(handle);
        clear_receive_handle(handle);
        return false;
    }

    remaining_size = handle->message_size - handle->current_message_size;
    if (remaining_size < 8) {
        memcpy(handle->payload + handle->current_message_size, 
                data + 1, remaining_size);
        handle->current_message_size += remaining_size;
        handle->success = true;
        handle->completed = true;
        handle->state = ISOTP_OK;

        handle->message_received_callback(handle);
        clear_receive_handle(handle);

        return true;
    }

    memcpy(handle->payload + handle->current_message_size, data + 1, 7);
    handle->current_message_size += 7;

    return true;
}


static bool pci_fc_receive(const uint16_t id,
                    const uint8_t data[], const uint8_t size)
{
    int i;
    for(i = 0; i < ISOTP_SEND_HANDLES_SIZE; i++) {
        if (isotp_send_handles[i] != NULL)
            if (isotp_send_handles[i]->receive_id == id) {
                break;
            }
    }
    if (i == ISOTP_SEND_HANDLES_SIZE) {
        shims.debug("Not found FC reader...\n");
        return false;
    }

    if (isotp_send_handles[i]->state != ISOTP_WAIT_FC){
        shims.debug("Relevat isotp_send_handle is not ISOTP_WAIT_FC!\n");
        return false;
    }

    shims.debug("Received FC for id: %d \n", isotp_send_handles[i]->receive_id);

    isotp_send_handles[i]->blocksize = data[1];
    isotp_send_handles[i]->stmin = data[2];

    switch (data[0] & 0x0F)
    {
        case FC_CTS:
            shims.debug("Timer installed\n");
            isotp_send_handles[i]->state = ISOTP_SENDING;
            shims.set_timer(get_real_timer(isotp_send_handles[i]->stmin),
                        (void*) isotp_continue_send, &isotp_send_handles[i]);
            return true;
        break;
        case FC_OVFLW:
            isotp_send_handles[i]->state = ISOTP_BUFFER_OVFLW;
            isotp_send_handles[i]->message_sent_callback(isotp_send_handles[i]);
            isotp_send_handles[i] = NULL;
            return false;
        break;
        case FC_WAIT:
        default:
            return false;
        break;
    }
}

bool isotp_continue_receive(const uint16_t id,
        const uint8_t data[], const uint8_t size) {
    if(size < 1) {
        return false;
    }

    isotp_npci_type PCItype = data[0] & 0xF0;

    if (PCItype == N_PCI_FC){
        return pci_fc_receive(id, data, size);
    }

    isotp_receive_handle *handle = NULL;
    int i;
    for(i = 0; i < ISOTP_RECEIVE_HANDLES_SIZE; i++){
        if(isotp_receive_handles[i] != NULL)
            if(isotp_receive_handles[i]->receive_id == id) {
                handle = isotp_receive_handles[i];
                break;
            }
    }
    if (handle == NULL) {
        return false;
    }
    shims.debug("Got frame with id: %x\n", handle->receive_id);

    switch (PCItype) {
    case N_PCI_SF:
        return pci_sf_manage(handle, id, data, size);
        break;
    case N_PCI_FF:
        return pci_ff_manage(handle, id, data, size);
        break;
    case N_PCI_CF:
        return pci_cf_manage(handle, id, data, size);
        break;
    default:
        shims.debug("Unknown PCItype!\n");
        return false;
        break;
    }

}