#ifndef _THREAD_H_
#define _THREAD_H_

#include <setjmp.h>

#define STARTING_RUNNING_QUANTUMS 0
#define STACK_SIZE 4096
#define TRUE 1

enum STATE{READY, RUNNING, BLOCKED};

class Thread {
 private:
  thread_entry_point entry_point;
  int quantums_ran;
  int tid;
  STATE state;
  sigjmp_buf jump_buffer;
  STATE end_of_sleeping_state;
  char *stack;

 public:
  Thread(int tid, thread_entry_point entry_point);
  ~Thread();

  STATE get_state() const {return this->state;}
  void set_state(STATE new_state) {this->state = new_state;}
  void set_quantums_ran (int new_quantums_ran) {this->quantums_ran =
  new_quantums_ran;}
  int get_quantums_ran () const {return this->quantums_ran;}
  thread_entry_point get_entry_point () const {return this->entry_point;}
  void set_entry_point (thread_entry_point new_entry_point)
  {this->entry_point = new_entry_point;}
  int get_tid () const{return this->tid;}
  void set_tid (int new_tid) {this->tid = new_tid;}
  STATE get_end_of_sleeping () const {return end_of_sleeping_state;}
  void set_end_of_sleeping (STATE was_blocked){this->end_of_sleeping_state =
      was_blocked;}


  int save_thread_frame();
  void block_thread();
  void run_thread();
};

#endif //_THREAD_H_
