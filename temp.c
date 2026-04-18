#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
int result1 = 0;
int* result1_ptr;
int result2 = 0;
int* result2_ptr;
void *for_loop_worker_1(void *arg) { intptr_t* _args = (intptr_t*)arg; int start_index = (int)_args[0] + 0; int* _a = (int*)_args[1]; pthread_mutex_t* _lock_a = (pthread_mutex_t*)_args[2]; for (int i = start_index; i < 10; i = i + 2) { pthread_mutex_lock(_lock_a);(*_a)=(((*_a)*2)+i);pthread_mutex_unlock(_lock_a);} return NULL; }int add(int a) {pthread_mutex_t _lock_a;pthread_mutex_init(&_lock_a, NULL);intptr_t _fargs[2][3]; for (int _fi = 0; _fi < 2; _fi++) { _fargs[_fi][0] = _fi; _fargs[_fi][1] = (intptr_t)&a; _fargs[_fi][2] = (intptr_t)&_lock_a; } pthread_t threads[2]; for (int i = 0; i < 2; i++) { pthread_create(&threads[i], NULL, for_loop_worker_1, _fargs[i]); } for (int i = 0; i < 2; i++) { pthread_join(threads[i], NULL); }pthread_mutex_destroy(&_lock_a);return a;}

void process_call_1(int* result) {*result=add(10);exit(0);}
void process_call_2(int* result) {*result=add(20);exit(0);}
int main() {
result1_ptr = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); *result1_ptr = 0; pid_t _process_result1 = fork(); if (_process_result1 == 0) { process_call_1(result1_ptr); } pthread_t _thread_result1;result2_ptr = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); *result2_ptr = 0; pid_t _process_result2 = fork(); if (_process_result2 == 0) { process_call_2(result2_ptr); } pthread_t _thread_result2;waitpid(_process_result1, NULL, 0); result1 = *result1_ptr;waitpid(_process_result2, NULL, 0); result2 = *result2_ptr;printf("%d",result1);printf("%s","\n");printf("%d",result2);
  return 0;
}
