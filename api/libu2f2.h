#ifndef LIBU2F2_H_
#define LIBU2F2_H_

#include "autoconf.h"
#include "libc/types.h"
#include "libc/sanhandlers.h"
#include "libc/sys/msg.h"
#include "libc/errno.h"

#define MAGIC_WINK_REQ          0x42420000UL

#define MAGIC_APDU_CMD_INIT     0xa5a50001UL /* ask for initiate APDU Cmd */
#define MAGIC_APDU_CMD_META     0xa5a50002UL /* send apdu cmd metadata */
#define MAGIC_APDU_CMD_MSG_LEN  0xa5a50003UL /* send apdu cmd buffer len (in bytes) */
#define MAGIC_APDU_CMD_MSG      0xa5a50004UL /* send apdu cmd buffer (len / 64 messages number + residual) */

#define MAGIC_APDU_RESP_INIT    0x5a5a0001UL /* ask for initiate APDU response */
#define MAGIC_APDU_RESP_MSG_LEN 0x5a5a0002UL /* send apdu response buffer len (in bytes) */
#define MAGIC_APDU_RESP_MSG     0x5a5a0003UL /* send apdu response buffer (len / 64 messages number + residual) */

#define MAGIC_CMD_RETURN        0xdeadbeefUL /* remote command return value */

#define MAGIC_ACKNOWLEDGE       0xeba42148UL /* acknowledge a command */

/* ask PIN: is user unlock through PIN done ? */
#define MAGIC_PIN_CONFIRM_UNLOCK    0x4513df85UL
#define MAGIC_PIN_UNLOCK_CONFIRMED  0xf32e5a7dUL


#define MAGIC_IS_BACKEND_READY 0xa46f8c5UL
#define MAGIC_BACKEND_IS_READY 0x6e9f851UL


/*
 * Sending a syncrhonous signal to a target, wait for its response (specificed in resp)
 * @target the target message queue, associated to the target
 * @sig    the message queue type to emit
 * @resp   the message queue type to receive as acknowedgement
 */
static inline mbed_error_t send_signal_with_acknowledge(int target, uint32_t sig, uint32_t resp)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    int pin_msq = get_pin_msq();
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    /* TODO errno/errcode */
    /* syncrhonously send request */
    msgsnd(target, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(target, &msgbuf.mtext, msgsz, resp, 0);

    return errcode;
}

/*
 * Receiving a signal, syncrhonously transfer it to backend, getting back acknowledge and
 * transfer the acknowledge to original signal source.
 * @source  the source message queue, from which the signal is originaly received
 * @backend the target message queue, to which the signal is transfered
 * @sig     the message queue type to emit
 * @resp    the message queue type to receive as acknowedgement
 */
static inline mbed_error_t transmit_signal_to_backend_with_acknowledge(int source, int backend, uint32_t sig, uint32_t resp)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    int pin_msq = get_pin_msq();
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    /* TODO errno/errcode */
    msgrcv(source, &msgbuf.mtext, msgsz, resp, 0);
    /* syncrhonously transfer to backend */
    msgsnd(backend, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(backend, &msgbuf.mtext, msgsz, resp, 0);
    /* then transmit back to source */
    msgbuf.mtype = resp;
    msgsnd(source, &msgbuf, 0, 0);

    return errcode;
}

typedef mbed_error_t (*u2f2_transmit_signal_prehook_t)(void);
typedef mbed_error_t (*u2f2_transmit_signal_posthook_t)(void);

/*
 * Receiving a signal, syncrhonously transfer it to backend, getting back acknowledge and
 * transfer the acknowledge to original signal source.
 * @source  the source message queue, from which the signal is originaly received
 * @backend the target message queue, to which the signal is transfered
 * @sig     the message queue type to emit
 * @resp    the message queue type to receive as acknowedgement
 */
static inline mbed_error_t transmit_signal_to_backend_with_hooks(int source, int backend, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t prehook, u2f2_transmit_signal_posthook_t posthook)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    int pin_msq = get_pin_msq();
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    /* TODO errno/errcode */
    msgrcv(source, &msgbuf.mtext, msgsz, resp, 0);
    /* prehook ? */
    if (prehook != NULL) {
        handler_sanity_check_with_panic(prehook);
    }
    prehook();

    /* syncrhonously transfer to backend */
    msgsnd(backend, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(backend, &msgbuf.mtext, msgsz, resp, 0);
    /* posthook ? */
    if (posthook != NULL) {
        handler_sanity_check_with_panic(posthook);
    }
    posthook();

    /* then transmit back to source */
    msgbuf.mtype = resp;
    msgsnd(source, &msgbuf, 0, 0);

    return errcode;
}




#endif/*!LIBU2F2_H_*/
