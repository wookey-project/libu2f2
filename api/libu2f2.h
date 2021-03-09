#ifndef LIBU2F2_H_
#define LIBU2F2_H_

#include "autoconf.h"
#include "libc/types.h"
#include "libc/sanhandlers.h"
#include "libc/sys/msg.h"
#include "libc/errno.h"

#if CONFIG_USR_LIB_U2F2_DEBUG
# define log_printf(...) printf(__VA_ARGS__)
#else
# define log_printf(...)
#endif


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
#define MAGIC_PETPIN_INSERT            0x4513df85UL
#define MAGIC_PETPIN_INSERTED          0xf32e5a7dUL

#define MAGIC_USERPIN_INSERT           0x257fdf45UL
#define MAGIC_USERPIN_INSERTED         0x532efa7dUL

#define MAGIC_PASSPHRASE_CONFIRM       0x415468dfUL
#define MAGIC_PASSPHRASE_RESULT        0x4f8c517dUL


#define MAGIC_IS_BACKEND_READY 0xa46f8c5UL
#define MAGIC_BACKEND_IS_READY 0x6e9f851UL

#define MAGIC_USER_PRESENCE_REQ 0xae5d497fUL
#define MAGIC_USER_PRESENCE_ACK 0xa97fe5d4UL

/* to be removed ... */
#define MAGIC_PIN_CONFIRM_UNLOCK 1UL
#define MAGIC_PIN_UNLOCK_CONFIRMED 2UL


/*
 * Transmitting data to a remote task, and getting back another data in response.
 * Fragmentation is not handled here.
 * @target the target message queue, associated to the target
 * @sig    the message queue type to emit
 * @resp   the message queue type to receive as acknowedgement
 * @data_sent the data to be sent (can be NULL: no data to send)
 * @data_sent_len the len of data to be sent (can be 0)
 * @data_recv the data to be recv (can be NULL: no data to receive)
 * @data_recv the effective size of received data (can be 0)
 */
static inline mbed_error_t exchange_data(int target, uint32_t sig, uint32_t resp, msg_mtext_union_t *data_sent, size_t data_sent_len, msg_mtext_union_t *data_recv, size_t *data_recv_len)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf;

    /* sanitize */
    if (data_sent == NULL && data_sent_len != 0) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;
    }
    if (data_recv_len == NULL) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;
    }
    if ((*data_recv_len) > 0 && data_recv == NULL) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;
    }
    if ((*data_recv_len) > sizeof(msg_mtext_union_t) || data_sent_len > sizeof(msg_mtext_union_t)) {
        errcode = MBED_ERROR_INVPARAM;
        goto err;
    }

    msgbuf.mtype = sig;
    if (data_sent_len > 0) {
        memcpy((void*)&msgbuf.mtext, data_sent, data_sent_len);
    }

    log_printf("%s: send data %x (len %d) to %d\n", __func__, sig, data_sent_len, target);
    /* TODO errno/errcode */
    /* syncrhonously send request */
    msgsnd(target, &msgbuf, data_sent_len, 0);
    /* and get back response */
    msgrcv(target, data_recv, *data_recv_len, resp, 0);

    log_printf("%s: receiving data %x (len %d) from %d\n", __func__, resp, *data_recv_len, target);
err:
    return errcode;
}


/*
 * Sending a syncrhonous signal to a target, wait for its response (specificed in resp)
 * @target the target message queue, associated to the target
 * @sig    the message queue type to emit
 * @resp   the message queue type to receive as acknowedgement
 */
static inline mbed_error_t send_signal_with_acknowledge(int target, uint32_t sig, uint32_t resp)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf;
    size_t msgsz = 0;
    msgbuf.mtype = sig;

    log_printf("%s: send signal %x to %d\n", __func__, sig, target);
    /* TODO errno/errcode */
    /* syncrhonously send request */
    msgsnd(target, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(target, &msgbuf.mtext, msgsz, resp, 0);

    log_printf("%s: receiving signal %x from %d\n", __func__, resp, target);

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
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    log_printf("%s: receiving signal %x from %d\n", __func__, sig, source);
    /* TODO errno/errcode */
    msgrcv(source, &msgbuf.mtext, msgsz, sig, 0);
    /* syncrhonously transfer to backend */
    log_printf("%s: send signal %x to %d\n", __func__, sig, backend);
    msgsnd(backend, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(backend, &msgbuf.mtext, msgsz, resp, 0);
    log_printf("%s: receiving signal %x from %d\n", __func__, resp, backend);
    /* then transmit back to source */
    msgbuf.mtype = resp;
    log_printf("%s: sending back signal %x from %d\n", __func__, resp, source);
    msgsnd(source, &msgbuf, 0, 0);

    return errcode;
}

typedef mbed_error_t (*u2f2_transmit_signal_prehook_t)(void);
typedef mbed_error_t (*u2f2_transmit_signal_posthook_t)(void);

/*
 * Receiving a signal, syncrhonously transfer it to backend, getting back acknowledge and
 * transfer  the acknowledge to original signal source.
 * @source   the source message queue, from which the signal is originaly received
 * @backend  the target message queue, to which the signal is transfered
 * @sig      the message queue type to emit
 * @resp     the message queue type to receive as acknowedgement
 * @prehook  hook to execute before transmiting to backend
 * @posthook hook to execute before returning back to source
 */
static inline mbed_error_t transmit_signal_to_backend_with_hooks(int source, int backend, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t prehook, u2f2_transmit_signal_posthook_t posthook)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    /* TODO errno/errcode */
    msgrcv(source, &msgbuf.mtext, msgsz, sig, 0);
    /* prehook ? */
    if (prehook != NULL) {
        handler_sanity_check_with_panic((physaddr_t)prehook);
    }
    prehook();

    /* syncrhonously transfer to backend */
    msgsnd(backend, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(backend, &msgbuf.mtext, msgsz, resp, 0);
    /* posthook ? */
    if (posthook != NULL) {
        handler_sanity_check_with_panic((physaddr_t)posthook);
    }
    posthook();

    /* then transmit back to source */
    msgbuf.mtype = resp;
    msgsnd(source, &msgbuf, 0, 0);

    return errcode;
}

/*
 * Receiving a signal, syncrhonously execute a hook, and acknowledge it.
 * @source  the source message queue, from which the signal is originaly received
 * @sig     the message queue type to receive
 * @resp    the message queue type to send back
 * @hook    the hook to execute between reception and transmition
 */
static inline mbed_error_t handle_signal(int source, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t hook)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf;
    size_t msgsz = 0;

    msgbuf.mtype = sig;

    log_printf("%s: receiving signal %x from %d\n", __func__, sig, source);
    /* TODO errno/errcode */
    msgrcv(source, &msgbuf.mtext, msgsz, sig, 0);
    /* prehook ? */
    if (hook != NULL) {
        handler_sanity_check_with_panic((physaddr_t)hook);
        log_printf("%s: executing hook\n", __func__);
        errcode = hook();
        if (errcode != MBED_ERROR_NONE) {
            goto err;
        }
    }

    /* then transmit back to source */
    msgbuf.mtype = resp;
    log_printf("%s: sending back signal %x from %d\n", __func__, resp, source);
    msgsnd(source, &msgbuf, 0, 0);
err:
    return errcode;
}




#endif/*!LIBU2F2_H_*/
