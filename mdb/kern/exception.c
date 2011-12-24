/*-
 * Copyright (c) 2011 Peter Le Bek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/mach_types.h>

#include "kern.h"
#include "mach_exc.h"
#include "exception.h"

extern boolean_t mach_exc_server(mach_msg_header_t *InHeadP,
                                 mach_msg_header_t *OutHeadP);

kern_exc_event saved_event;

const char *
kern_exc_string (unsigned int i)
{
    switch (i)
        {
        case EXC_BAD_ACCESS:
            return "EXC_BAD_ACCESS";
        case EXC_BAD_INSTRUCTION:
            return "EXC_BAD_INSTRUCTION";
        case EXC_ARITHMETIC:
            return "EXC_ARITHMETIC";
        case EXC_EMULATION:
            return "EXC_EMULATION";
        case EXC_SOFTWARE:
            return "EXC_SOFTWARE";
        case EXC_BREAKPOINT:
            return "EXC_BREAKPOINT";
        case EXC_SYSCALL:
            return "EXC_SYSCALL";
        case EXC_MACH_SYSCALL:
            return "EXC_MACH_SYSCALL";
        case EXC_RPC_ALERT:
            return "EXC_RPC_ALERT";
        default:
            return "EXC_UNKOWN";
        }
}

extern kern_return_t catch_mach_exception_raise (
       mach_port_t             exception_port,
       mach_port_t             thread,
       mach_port_t             task,
       exception_type_t        exception,
       exception_data_t        code,
       mach_msg_type_number_t  codeCnt)
{
    kern_return_t kr;

    saved_event.thread = thread;
    saved_event.type = exception;

    if ( (kr = thread_suspend(thread)) != KERN_SUCCESS) {
        return KERN_FAILURE;
    }

    return KERN_SUCCESS;
}

extern kern_return_t catch_mach_exception_raise_state (
       mach_port_t             exception_port,
       exception_type_t        exception,
       const exception_data_t  code,
       mach_msg_type_number_t  codeCnt,
       int *                   flavor,
       const thread_state_t    old_state,
       mach_msg_type_number_t  old_stateCnt,
       thread_state_t          new_state,
       mach_msg_type_number_t *new_stateCnt)
{
    return KERN_FAILURE;
}

extern kern_return_t catch_mach_exception_raise_state_identity (
       mach_port_t             exception_port,
       mach_port_t             thread,
       mach_port_t             task,
       exception_type_t        exception,
       exception_data_t        code,
       mach_msg_type_number_t  codeCnt,
       int *                   flavor,
       thread_state_t          old_state,
       mach_msg_type_number_t  old_stateCnt,
       thread_state_t          new_state,
       mach_msg_type_number_t *new_stateCnt)
{
    return KERN_FAILURE;
}

kern_return_t
kern_excserv_init (mach_port_t task, mach_port_t *exc_port)
{
    kern_return_t kr;
    exception_mask_t  mask = EXC_MASK_BAD_ACCESS
                           | EXC_MASK_BAD_INSTRUCTION
                           | EXC_MASK_ARITHMETIC
                           | EXC_MASK_SOFTWARE
                           | EXC_MASK_BREAKPOINT
                           | EXC_MASK_SYSCALL;

    /* Allocate exception port */
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                            exc_port);

    if (kr != KERN_SUCCESS)
        return kr;

    /* Give the remote task send rights to our exception port */
    kr = mach_port_insert_right(mach_task_self(), *exc_port, *exc_port,
                                MACH_MSG_TYPE_MAKE_SEND);

    if (kr != KERN_SUCCESS)
        return kr;

    /* Set remote task's exception port */
    kr = task_set_exception_ports(task, mask, *exc_port,
                                  EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                  THREAD_STATE_NONE);

    return kr;
}

/* Returns 1 on event, 0 on timeout, -1 on fail */
int
kern_excserv_poll (mach_port_t exc_port, int milliseconds,
                   kern_exc_event *event)
{
    mach_msg_return_t mr;
    static kern_exc_request request;
    static kern_exc_reply reply;

    mr = mach_msg(&request.head,
                  MACH_RCV_MSG|MACH_RCV_LARGE|MACH_RCV_TIMEOUT,
                  0, sizeof(request), exc_port,
                  milliseconds, MACH_PORT_NULL);

    if (mr == MACH_RCV_TIMED_OUT)
        return 0;
    else if (mr != MACH_MSG_SUCCESS)
        return -1;

    /* Dispatch to exception handler */
    if(! mach_exc_server(&request.head, &reply.head))
        return -1;

    /* Copy global */
    *event = saved_event;

    mr = mach_msg(&reply.head,
                  MACH_SEND_MSG|MACH_SEND_TIMEOUT,
                  reply.head.msgh_size, 0, MACH_PORT_NULL,
                  milliseconds, MACH_PORT_NULL);

    if (mr == MACH_RCV_TIMED_OUT)
        return 0;
    else if (mr != MACH_MSG_SUCCESS)
        return -1;

    return 1;
}
