#include "uthreads.h"
#ifndef _THREAD_T_H_
#define _THREAD_T_H_

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

typedef struct thread_t{
  thread_entry_point entry_point;
  int id;
  int quantums = 0;
  bool blocked = false;
  bool ready = true;
  bool sleeping = false;
  int sleep_quantum = 0;
  char *stack;
  address_t sp;
  address_t pc;


} thread_t;
#endif //_THREAD_T_H_
