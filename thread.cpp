#include <map>
#include <iostream>
#include <queue>
#include "uthreads.h"
#include "thread.h"
#include <sys/time.h>
#include <signal.h>


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif


Thread::Thread (int tid, thread_entry_point entry_point)
{
  this->tid = tid;
  this->entry_point = entry_point;
  this->quantums_ran = STARTING_RUNNING_QUANTUMS;
  this->state = READY;
  this->end_of_sleeping_state = READY;

  this->stack = new char[STACK_SIZE];
  address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp(this->jump_buffer, TRUE);
  (this->jump_buffer->__jmpbuf)[JB_SP] = translate_address(sp);
  (this->jump_buffer->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&(this->jump_buffer->__saved_mask));
}


void Thread::run_thread ()
{
  this->quantums_ran++;
  this->state = RUNNING;

  siglongjmp(this->jump_buffer, TRUE);
}

void Thread::block_thread ()
{
  // TODO: should we return this?
  this->state = BLOCKED;
  sigsetjmp(this->jump_buffer, TRUE);
}

Thread::~Thread ()
{
  delete[] this->stack;
}

int Thread::save_thread_frame ()
{
  return sigsetjmp(this->jump_buffer, TRUE);
}
