/*
 *
 * Copyright 2019 The wookey project team <wookey@ssi.gouv.fr>
 *   - Ryad     Benadjila
 *   - Arnauld  Michelizza
 *   - Mathieu  Renard
 *   - Philippe Thierry
 *   - Philippe Trebuchet
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * the Free Software Foundation; either version 3 of the License, or (at
 * ur option) any later version.
 *
 * This package is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this package; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include "api/libu2f2.h"
#include "libc/string.h"
#include "libc/stdio.h"

#if CONFIG_USR_LIB_U2F2_DEBUG
# define log_printf(...) printf(__VA_ARGS__)
#else
# define log_printf(...)
#endif



mbed_error_t exchange_data(int target, uint32_t sig, uint32_t resp, msg_mtext_union_t *data_sent, size_t data_sent_len, msg_mtext_union_t *data_recv, size_t *data_recv_len)
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



mbed_error_t send_signal_with_acknowledge(int target, uint32_t sig, uint32_t resp)
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


mbed_error_t transmit_signal_to_backend_with_acknowledge(int source, int backend, uint32_t sig, uint32_t resp)
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

mbed_error_t transmit_signal_to_backend_with_hooks(int source, int backend, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t prehook, u2f2_transmit_signal_posthook_t posthook)
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

mbed_error_t handle_signal(int source, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t hook)
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
