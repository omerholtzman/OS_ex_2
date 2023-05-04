#include <map>
#include <iostream>
#include <queue>
#include "uthreads.h"
#include <sys/time.h>
#include <signal.h>
#include "thread.h"


#define RETURN_ERROR -1
#define RETURN_SUCCESS 0
#define MAIN_TID 0
#define STARTING_RUNNING_QUANTUMS 0
#define BLOCK_RUNNING -101

#define WRONG_ID_TERMINATING_THREAD_ERROR "Error: tried to terminate ID doesn't exist"
#define REACHED_MAX_THREAD_NUM_ERROR "Error: reached max thread number"
#define NO_SUCH_ID_ERROR "Error: No such id is running. id: "
#define EMPTY_QUEUE_ERROR "Error: Reached an empty queue!"
#define BLOCKING_MAIN_ERROR "Tried blocking the main thread. That is an error."
#define SIGACTION_ERROR "Error: sigaction error."
#define ITIMER_ERROR "Error: an error was raised setting the itimer."
#define SYSCALL_ERROR "Error: sys call failed."
#define QUANTUM_SEC_ERROR "Error: Quantum usec must be positive number!"
#define UNINITIALIZED_THREAD_ERROR "Error: Thread was not initialized"
#define UNBLOCK_SLEEPING_THREAD_ERROR "Error: Tried to unblock sleeping thread!"


static std::map<int, Thread*> id_to_thread_map = std::map<int, Thread*>();
static std::map<int, int>sleeping_treads_map = std::map<int, int>();
static std::queue<int> thread_queue = std::queue<int>();
static struct itimerval timer;
static struct sigaction signal_handler;

static int global_quantum_usecs;
static int quantums_passed;
static sigset_t set;

void delete_from_queue (int tid);
void update_sleeping_quantums ();

void sig_block(){
  if (sigprocmask(SIG_BLOCK, &set ,nullptr) == -1){
    std::cerr << SYSCALL_ERROR << std::endl;
    exit(EXIT_FAILURE);
  }
}

void sig_unblock(){
  if (sigprocmask(SIG_UNBLOCK, &set, nullptr) == -1){
      std::cerr << SYSCALL_ERROR << std::endl;
      exit(EXIT_FAILURE);
  }
}


void run_thread(int thread_number){
  quantums_passed++;
  id_to_thread_map[thread_number]->run_thread();
}

bool id_not_found(int tid){
  if (id_to_thread_map.find(tid) == id_to_thread_map.end()){
      std::cerr << NO_SUCH_ID_ERROR << tid << std::endl;
      return true;
    }
  return false;
}

void change_function(int sig){
  sig_block();
  if (sig != TRUE){
      id_to_thread_map[thread_queue.front()]->save_thread_frame();
  }
  // move the last thread in queue to ready if he is running
  if (id_to_thread_map[thread_queue.front()]->get_state() == RUNNING){
    id_to_thread_map[thread_queue.front()]->set_state(READY);
    thread_queue.push(thread_queue.front());
    thread_queue.pop();
  }
  // finds the first ready thread
  while(id_to_thread_map[thread_queue.front()]->get_state() != READY){
      thread_queue.push(thread_queue.front());
      thread_queue.pop();
  }
  update_sleeping_quantums ();
  sig_unblock();
  run_thread(thread_queue.front());
}

void update_sleeping_quantums ()
{
  for (const auto &tid : sleeping_treads_map){
    sleeping_treads_map[tid.first]--;
    if (id_to_thread_map.find(tid.first) != id_to_thread_map.end() && tid.second == 0){
        if (id_to_thread_map[tid.first]->get_end_of_sleeping() != BLOCKED){
            id_to_thread_map[tid.first]->set_state(READY);
          }
        else{
            id_to_thread_map[tid.first]->set_end_of_sleeping(READY);
        }
    }
  }
}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0){
    std::cerr << QUANTUM_SEC_ERROR << std::endl;
    return RETURN_ERROR;
  }
  global_quantum_usecs = quantum_usecs;
  quantums_passed = 1;

  signal_handler.sa_handler = &change_function;

  sigemptyset(&set);
  sigfillset(&set);  // fill the set of sigs to block

  if(sigaction(SIGVTALRM, &signal_handler, nullptr) < 0){
    std::cerr << SIGACTION_ERROR << std::endl;
    return EXIT_FAILURE;
  }

  // Configuring timer.
  timer.it_value.tv_sec = 0;
  // TODO: should we start here different value?
  timer.it_value.tv_usec = quantum_usecs;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = quantum_usecs;

  if(setitimer(ITIMER_VIRTUAL, &timer, nullptr)){
    std::cerr << ITIMER_ERROR << std::endl;
  }

//  sigjmp_buf *buf = (sigjmp_buf *)new sigjmp_buf();
//  int ret_val = sigsetjmp(*buf, TRUE);
//  if (ret_val == 0)
//    {
//
//    }
  Thread *main_thread = new Thread (MAIN_TID, nullptr);
  id_to_thread_map.insert({MAIN_TID, main_thread});
  thread_queue.push(MAIN_TID);
  main_thread->set_state(RUNNING);
  main_thread->set_quantums_ran(1);
  return RETURN_SUCCESS;
}

int uthread_spawn (thread_entry_point entry_point)
{
  sig_block();
  if (id_to_thread_map.size() == MAX_THREAD_NUM){
    std::cerr << REACHED_MAX_THREAD_NUM_ERROR << std::endl;
    sig_unblock();
    return RETURN_ERROR;
  }
  int new_thread_number = 1;
  while (new_thread_number <= MAX_THREAD_NUM){
    if (id_to_thread_map.find(new_thread_number) == id_to_thread_map.end()){
      id_to_thread_map.insert({new_thread_number, new Thread(new_thread_number,
        entry_point)});
      thread_queue.push(new_thread_number);
      sig_unblock();
      return new_thread_number;
    }
    new_thread_number++;
  }
  sig_unblock();
  return RETURN_ERROR;
}

int uthread_terminate (int tid)
{
  sig_block();
  if (tid == MAIN_TID){
    for (const auto &thread : id_to_thread_map){
      delete thread.second;
    }
    id_to_thread_map.clear();
    sleeping_treads_map.clear();
    while(!thread_queue.empty()) {thread_queue.pop();}
    sig_unblock();  // should this be here?
    exit(RETURN_SUCCESS);
  }
  else if (id_not_found(tid)) {
    sig_unblock();
    return RETURN_ERROR;
  }
  else{
    Thread *thread_to_delete = id_to_thread_map[tid];
      bool is_running = false;
      if (thread_to_delete != nullptr){
          is_running = thread_to_delete->get_state() == RUNNING;
          delete thread_to_delete;
        }
    id_to_thread_map.erase(tid);
    if (sleeping_treads_map.find(tid) != sleeping_treads_map.end()){
        sleeping_treads_map.erase(tid);
    }
    delete_from_queue(tid);
    if (is_running){
        sig_unblock();
        change_function(TRUE);
    }
  }
  sig_unblock();
  return RETURN_SUCCESS;

}

void delete_from_queue (int tid)
{
  std::queue<int> new_queue;
  while(!thread_queue.empty()){
    if (thread_queue.front() != tid){
      new_queue.push(thread_queue.front());
    }
    thread_queue.pop();
  }
  thread_queue = new_queue;
}

int uthread_block (int tid)
{
  if (id_not_found(tid)){
    return RETURN_ERROR;
  }
  if (tid == MAIN_TID){
    std::cerr << BLOCKING_MAIN_ERROR << std::endl;
    return RETURN_ERROR;
  }
  sig_block();
  switch (id_to_thread_map[tid]->get_state ())
    {
      case BLOCKED:
        if (sleeping_treads_map.find(tid) != sleeping_treads_map.end()){
            id_to_thread_map[tid]->set_end_of_sleeping(BLOCKED);

        }
        sig_unblock();
        return RETURN_SUCCESS;
      case READY:
        id_to_thread_map[tid]->set_state(BLOCKED);
        sig_unblock();
        return RETURN_SUCCESS;
      case RUNNING:
        id_to_thread_map[tid]->block_thread();
        change_function(BLOCK_RUNNING);
        sig_unblock();
        return RETURN_SUCCESS;
      default:
        sig_unblock();
        return RETURN_ERROR;
    }
}


void push_to_end(int tid){
  int size = (int) thread_queue.size();
  for (int i = 0; i < size; i++){
    if (thread_queue.front() != tid){
        thread_queue.push(thread_queue.front());
    }
      thread_queue.pop();
  }
  thread_queue.push(tid);
}

int uthread_resume (int tid)
{
  sig_block();
  if (id_not_found(tid)) {
    sig_unblock();
    return RETURN_ERROR;
  }
  if (id_to_thread_map[tid]->get_state() == BLOCKED){
      // thread is blocked
      if (sleeping_treads_map.find(tid) != sleeping_treads_map.end()){
          id_to_thread_map[tid]->set_end_of_sleeping(READY);
        }
      else{
          id_to_thread_map[tid]->set_state(READY);
          push_to_end(tid);
      }
  }
  else if (sleeping_treads_map.find(tid) != sleeping_treads_map.end()){
      std::cerr << UNBLOCK_SLEEPING_THREAD_ERROR << std::endl;
      sig_unblock();
      return RETURN_ERROR;
  }
  sig_unblock();
  return RETURN_SUCCESS;
}

int uthread_sleep (int num_quantums)
{
  sig_block();
  int running_thread = uthread_get_tid();
  if (running_thread == MAIN_TID){
    std::cerr << BLOCKING_MAIN_ERROR << std::endl;
    return EXIT_FAILURE;
  }
  if (sleeping_treads_map.find(running_thread) != sleeping_treads_map.end()){
    sig_unblock();
    return RETURN_SUCCESS;
  }
  sleeping_treads_map.insert({running_thread, num_quantums});
  id_to_thread_map[running_thread]->block_thread();
  sig_unblock();
  return RETURN_SUCCESS;
}

int uthread_get_tid ()
{
  int thread_number = 0;
  while (thread_number <= MAX_THREAD_NUM){
    if (!(id_to_thread_map.find(thread_number) == id_to_thread_map.end())){
        if (id_to_thread_map[thread_number]->get_state() == RUNNING){
            return thread_number;
          }
    }
      thread_number++;
    }
  return RETURN_ERROR;
}

int uthread_get_total_quantums ()
{
  return quantums_passed;
}

int uthread_get_quantums (int tid)
{
  sig_block();
  if (id_not_found(tid)) {
      sig_unblock();
      return RETURN_ERROR;
  }
  if (id_to_thread_map.find(tid) != id_to_thread_map.end()){
      if (id_to_thread_map[tid] != nullptr){
        sig_unblock();
          return id_to_thread_map[tid]->get_quantums_ran();
        }
      std::cerr << UNINITIALIZED_THREAD_ERROR << std::endl;
      sig_unblock();
      return RETURN_ERROR;
  }
  sig_unblock();
  return RETURN_ERROR;
}
