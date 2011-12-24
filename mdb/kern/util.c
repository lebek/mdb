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

#include <Python.h>

#include "kern.h"
#include "util.h"

void
kern_handle_kr(kern_return_t kr)
{
    static struct {
        kern_return_t code;
        const char *msg;
    } *msgp, msgs [] = {
        { KERN_INVALID_ADDRESS,
          "Specified address is not currently valid." },
        { KERN_PROTECTION_FAILURE,
          "Specified memory is valid, but does not permit the"
          " required forms of access." },
        { KERN_NO_SPACE,
          "The address range specified is already in use, or"
          " no address range of the size specified could be"
          " found." },
        { KERN_INVALID_ARGUMENT,
          "The function requested was not applicable to this"
          " type of argument, or an argument is invalid." },
        { KERN_FAILURE,
          "The function could not be performed. A catch-all." },
        { KERN_RESOURCE_SHORTAGE,
          "A system resource could not be allocated to fulfill"
          " this request. This failure may not be permanent." },
        { KERN_NOT_RECEIVER,
          "The task in question does not hold receive rights"
          " for the port argument." },
        { KERN_NO_ACCESS,
          "Bogus access restriction." },
        { KERN_MEMORY_FAILURE,
          "During a page fault, the target address refers to a"
          " memory object that has been destroyed. This"
          " failure is permanent." },
        { KERN_MEMORY_ERROR,
          "During a page fault, the memory object indicated"
          " that the data could not be returned. This failure"
          " may be temporary; future attempts to access this"
          " same data may succeed, as defined by the memory"
          " object." },
        { KERN_ALREADY_IN_SET,
          "The receive right is already a member of the portset." },
        { KERN_NOT_IN_SET,
          "The receive right is not a member of a port set." },
        { KERN_NAME_EXISTS,
          "The name already denotes a right in the task." },
        { KERN_ABORTED,
          "The operation was aborted. Ipc code will"
          " catch this and reflect it as a message error." },
        { KERN_INVALID_NAME,
          "The name doesn't denote a right in the task." },
        { KERN_INVALID_TASK,
          "Target task isn't an active task." },
        { KERN_INVALID_RIGHT,
          "The name denotes a right, but not an appropriate right." },
        { KERN_INVALID_VALUE,
          "A blatant range error." },
        { KERN_UREFS_OVERFLOW,
          "Operation would overflow limit on user-references." },
        { KERN_INVALID_CAPABILITY,
          "The supplied (port) capability is improper." },
        { KERN_RIGHT_EXISTS,
          "The task already has send or receive rights"
          " for the port under another name." },
        { KERN_INVALID_HOST,
          "Target host isn't actually a host." },
        { KERN_MEMORY_PRESENT,
          "An attempt was made to supply 'precious' data"
          " for memory that is already present in a"
          " memory object." },
        { KERN_MEMORY_DATA_MOVED,
          "A page was requested of a memory manager via"
          " memory_object_data_request for an object using"
          " a MEMORY_OBJECT_COPY_CALL strategy, with the"
          " VM_PROT_WANTS_COPY flag being used to specify"
          " that the page desired is for a copy of the"
          " object, and the memory manager has detected"
          " the page was pushed into a copy of the object"
          " while the kernel was walking the shadow chain"
          " from the copy to the object. This error code"
          " is delivered via memory_object_data_error"
          " and is handled by the kernel (it forces the"
          " kernel to restart the fault). It will not be"
          " seen by users." },
        { KERN_MEMORY_RESTART_COPY,
          "A strategic copy was attempted of an object"
          " upon which a quicker copy is now possible."
          " The caller should retry the copy using"
          " vm_object_copy_quickly. This error code"
          " is seen only by the kernel." },
        { KERN_INVALID_PROCESSOR_SET,
          "An argument applied to assert processor set privilege"
          " was not a processor set control port." },
        { KERN_POLICY_LIMIT,
          "The specified scheduling attributes exceed the thread's"
          " limits." },
        { KERN_INVALID_POLICY,
          "The specified scheduling policy is not currently"
          " enabled for the processor set." },
        { KERN_INVALID_OBJECT,
          "The external memory manager failed to initialize the"
          " memory object." },
        { KERN_ALREADY_WAITING,
          "A thread is attempting to wait for an event for which"
          " there is already a waiting thread." },
        { KERN_DEFAULT_SET,
          "An attempt was made to destroy the default processor"
          " set." },
        { KERN_EXCEPTION_PROTECTED,
          "An attempt was made to fetch an exception port that is"
          " protected, or to abort a thread while processing a"
          " protected exception." },
        { KERN_INVALID_LEDGER,
          "A ledger was required but not supplied." },
        { KERN_INVALID_MEMORY_CONTROL,
          "The port was not a memory cache control port." },
        { KERN_INVALID_SECURITY,
          "An argument supplied to assert security privilege"
          " was not a host security port." },
        { KERN_NOT_DEPRESSED,
          "thread_depress_abort was called on a thread which"
          " was not currently depressed." },
        { KERN_TERMINATED,
          "Object has been terminated and is no longer available." },
        { KERN_LOCK_SET_DESTROYED,
          "Lock set has been destroyed and is no longer available." },
        { KERN_LOCK_UNSTABLE,
          "The thread holding the lock terminated before releasing"
          " the lock." },
        { KERN_LOCK_OWNED,
          "The lock is already owned by another thread." },
        { KERN_LOCK_OWNED_SELF,
          "The lock is already owned by the calling thread." },
        { KERN_SEMAPHORE_DESTROYED,
          "Semaphore has been destroyed and is no longer available." },
        { KERN_RPC_SERVER_TERMINATED,
          "Return from RPC indicating the target server was"
          " terminated before it successfully replied." },
        { KERN_RPC_TERMINATE_ORPHAN,
          "Terminate an orphaned activation." },
        { KERN_RPC_CONTINUE_ORPHAN,
          "Allow an orphaned activation to continue executing." },
        { KERN_NOT_SUPPORTED,
          "Empty thread activation (No thread linked to it)." },
        { KERN_NODE_DOWN,
          "Remote node down or inaccessible." },
        { KERN_NOT_WAITING,
          "A signalled thread was not actually waiting." },
        { KERN_OPERATION_TIMED_OUT,
          "Some thread-oriented operation (semaphore_wait) timed out." },
        { KERN_CODESIGN_ERROR,
          "During a page fault, indicates that the page was rejected"
          " as a result of a signature check." },
        { 0, NULL }
    };

    const char *msg = "Kernel error.";

    for (msgp = msgs; msgp->msg; msgp++) {

        if (kr == msgp->code) {
            msg = msgp->msg;
            break;
        }
    }

    PyErr_SetString(kern_KernelError, msg);
}
