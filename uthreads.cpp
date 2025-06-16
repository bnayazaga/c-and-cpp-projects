#include <iostream>
//#include "uthreads.h"
#include <queue>
#include "thread_t.h"
#include <list>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#define LIBRARY_ERROR "thread library error: "
#define NEGATIVE_QUANTUM "negative quantum was given\n"
#define SYSTEM_ERROR "system error: "
#define MALLOC_ERROR "malloc didn't succeed\n"
#define FAILURE (-1)
#define BLOCK_MAIN "cannot block main thread\n"
#define NOT_FOUND "this thread doesn't exist\n"
#define SLEEP_MAIN "main thread cannot go to sleep\n"

#define SECOND 1000000
#define SIGACTION_FAIL "sigaction failed\n"
#define SETTIMER_FAIL "setitimer error\n"
sigset_t set;
int current_thread_id;
int quantum_size;
int quantums;
int empty = MAX_THREAD_NUM - 1;
thread_t *threads[MAX_THREAD_NUM];
sigjmp_buf env[MAX_THREAD_NUM];
std::deque<int> ready;
struct sigaction sa = {0};
struct itimerval timer;

void free_all ();
void erase_from_ready(int tid){
  if (!ready.empty())
  {
    for (auto it = begin (ready); it != end (ready); ++it)
    {
      if (*it == tid)
      {
//        threads[*it]->ready = false;
        ready.erase (it);
        break;
      }
    }
  }
}

void maintain_sleeping()
{
  for (auto &thread: threads)
  {
    if (thread != nullptr && thread->sleeping)
    {
      if (thread->sleep_quantum == 0)
      {
        thread->sleeping = false;
        if (!thread->blocked)
        {
          ready.push_back (thread->id);
        }
      }
      else {
        thread->sleep_quantum--;
      }
    }
  }
}


int scheduler () {
  maintain_sleeping();
  int id = 0;
  if (!ready.empty())
  {
    id = ready.front ();
    ready.pop_front ();
  }
  return id;
}

void timer_handler(int sig){
//  sigprocmask (SIG_BLOCK, &set, NULL);
  if (threads[current_thread_id] != nullptr)
  {
    if (!threads[current_thread_id]->blocked &&
        !threads[current_thread_id]->sleeping)
    {
      threads[current_thread_id]->ready = true;
      ready.push_back (current_thread_id);
    }
    if (sigsetjmp(env[current_thread_id], 1))
    {
      if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
      {
        std::cerr << SYSTEM_ERROR << SETTIMER_FAIL;
        free_all();
        exit (1);
      }
      sigprocmask (SIG_UNBLOCK, &set, NULL);
      return;
    }
  }
  current_thread_id = scheduler();
  threads[current_thread_id]->ready = false;
  threads[current_thread_id]->quantums++;
  quantums++;
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    std::cerr << SYSTEM_ERROR << SETTIMER_FAIL;
    free_all();
    exit (1);
  }
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  siglongjmp (env[current_thread_id], 1);
}

int uthread_init(int quantum_usecs) {
  if (quantum_usecs <= 0) {
    std::cerr << LIBRARY_ERROR << NEGATIVE_QUANTUM;
    return FAILURE;
  }
  sigaddset (&set, SIGVTALRM);
  quantum_size = quantum_usecs;
  quantums = 1;
  current_thread_id = 0;
  auto* thread = static_cast<thread_t *>(malloc (sizeof (thread_t)));
  if (thread == nullptr){
    std::cerr << SYSTEM_ERROR << MALLOC_ERROR;
    exit (1);
  }
  thread->id = 0;
  thread->quantums++;
  threads[0] = thread;
  sa.sa_handler = &timer_handler;
  if (sigaction(SIGVTALRM, &sa, NULL) < 0)
  {
    std::cerr << SYSTEM_ERROR << SIGACTION_FAIL;
    free(thread);
    exit (1);
  }
  timer.it_value.tv_sec = quantum_size/SECOND;
  timer.it_value.tv_usec = quantum_size % SECOND;
  timer.it_interval.tv_sec = quantum_size/SECOND;
  timer.it_interval.tv_usec = quantum_size % SECOND;
  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    std::cerr << SYSTEM_ERROR << SETTIMER_FAIL;
    free(thread);
    exit (1);
  }
  return EXIT_SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point){
  if (entry_point == nullptr) {
    return FAILURE;
    }
  if (empty == 0){
    return FAILURE;
  }
  sigprocmask (SIG_BLOCK, &set, NULL);
  auto* thread = static_cast<thread_t *>(malloc (sizeof (thread_t)));
  if (thread == nullptr){
    std::cerr << SYSTEM_ERROR << MALLOC_ERROR;
    free_all();
    exit (1);
  }
  thread->entry_point = entry_point;
  thread->stack = (char *)(calloc (STACK_SIZE, sizeof (char*)));
  if (thread->stack == nullptr){
    free(thread);
    std::cerr << SYSTEM_ERROR << MALLOC_ERROR;
    free_all();
    exit (1);
  }
  for (int i = 1; i < MAX_THREAD_NUM; ++i)
  {
    if (threads[i] == nullptr){
      thread->id = i;
      threads[i] = thread;
      break;
    }
  }
  thread->sp = (address_t) thread->stack + STACK_SIZE - sizeof(address_t);
  thread->pc = (address_t) thread->entry_point;
  empty--;
  ready.push_back (thread->id);
  sigsetjmp(env[thread->id], 1);
  (env[thread->id]->__jmpbuf)[JB_SP] = translate_address(thread->sp);
  (env[thread->id]->__jmpbuf)[JB_PC] = translate_address(thread->pc);
  sigemptyset(&env[thread->id]->__saved_mask);
//  sigaddset (&env[thread->id]->__saved_mask, SIGVTALRM);
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  return thread->id;
}


int uthread_terminate(int tid){
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr) {
    std::cerr << LIBRARY_ERROR << NOT_FOUND;
    return FAILURE;
  }
  sigprocmask (SIG_BLOCK, &set, NULL);
  if (tid == 0){
    free_all();
    exit (0);
  }
  free(threads[tid]->stack);
  free(threads[tid]);
  threads[tid] = nullptr;
  erase_from_ready(tid);
  empty++;
  if (tid == current_thread_id)
  {
    timer_handler (0);
  }
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  return EXIT_SUCCESS;
}

void free_all ()
{
  for (int i = 1; i < MAX_THREAD_NUM; i++)
  {
    if (threads[i] != nullptr){
      free(threads[i]->stack);
      free(threads[i]);
    }
  }
}

int uthread_block(int tid){
  if (tid == 0) {
    std::cerr << LIBRARY_ERROR << BLOCK_MAIN;
    return FAILURE;
  }
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr){
    std::cerr << LIBRARY_ERROR << NOT_FOUND;
    return FAILURE;
  }
  sigprocmask (SIG_BLOCK, &set, NULL);
  threads[tid]->blocked = true;
  threads[tid]->ready = false;
  erase_from_ready (tid);
  if (tid == current_thread_id){
    timer_handler (0);
  }
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  return EXIT_SUCCESS;
}

int uthread_resume(int tid){
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr){
    std::cerr << LIBRARY_ERROR << NOT_FOUND;
    return FAILURE;
  }
  sigprocmask (SIG_BLOCK, &set, NULL);
  if (threads[tid]->blocked)
  {
    threads[tid]->blocked = false;
  }
  if (!threads[tid]->sleeping && !threads[tid]->ready){
    ready.push_back (tid);
    threads[tid]->ready = true;
  }
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  return EXIT_SUCCESS;
}

int uthread_sleep(int num_quantums){
  if (current_thread_id == 0) {
    std::cerr << LIBRARY_ERROR << SLEEP_MAIN;
    return FAILURE;
  }
  sigprocmask (SIG_BLOCK, &set, NULL);
  threads[current_thread_id]->sleeping = true;
  threads[current_thread_id]->sleep_quantum += num_quantums;
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  timer_handler (0);
  return EXIT_SUCCESS;
}

int uthread_get_tid(){
  return current_thread_id;
}

int uthread_get_total_quantums(){
  return quantums;
}

int uthread_get_quantums(int tid){
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr){
    std::cerr << LIBRARY_ERROR << NOT_FOUND;
    return FAILURE;
  }
  return threads[tid]->quantums;
}





