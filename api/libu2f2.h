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

#define MAGIC_TOKEN_UNLOCKED    0x4f8a5fedUL

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
mbed_error_t exchange_data(int target, uint32_t sig, uint32_t resp, msg_mtext_union_t *data_sent, size_t data_sent_len, msg_mtext_union_t *data_recv, size_t *data_recv_len);

/*
 * Sending a syncrhonous signal to a target, wait for its response (specificed in resp)
 * @target the target message queue, associated to the target
 * @sig    the message queue type to emit
 * @resp   the message queue type to receive as acknowedgement
 */
mbed_error_t send_signal_with_acknowledge(int target, uint32_t sig, uint32_t resp);

/*
 * Receiving a signal, syncrhonously transfer it to backend, getting back acknowledge and
 * transfer the acknowledge to original signal source.
 * @source  the source message queue, from which the signal is originaly received
 * @backend the target message queue, to which the signal is transfered
 * @sig     the message queue type to emit
 * @resp    the message queue type to receive as acknowedgement
 */
mbed_error_t transmit_signal_to_backend_with_acknowledge(int source, int backend, uint32_t sig, uint32_t resp);

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
mbed_error_t transmit_signal_to_backend_with_hooks(int source, int backend, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t prehook, u2f2_transmit_signal_posthook_t posthook);

/*
 * Receiving a signal, syncrhonously execute a hook, and acknowledge it.
 * @source  the source message queue, from which the signal is originaly received
 * @sig     the message queue type to receive
 * @resp    the message queue type to send back
 * @hook    the hook to execute between reception and transmition
 */
mbed_error_t handle_signal(int source, uint32_t sig, uint32_t resp, u2f2_transmit_signal_prehook_t hook);

#endif/*!LIBU2F2_H_*/
