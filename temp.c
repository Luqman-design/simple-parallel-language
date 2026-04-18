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
int add(int a, int b) {return (a+b);}

void process_call_1(int* result) {*result=add(4, 7);exit(0);}
void process_call_2(int* result) {*result=add(20, 13);exit(0);}
int main() {
result1_ptr = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); *result1_ptr = 0; pid_t _process_result1 = fork(); if (_process_result1 == 0) { process_call_1(result1_ptr); } pthread_t _thread_result1;result2_ptr = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); *result2_ptr = 0; pid_t _process_result2 = fork(); if (_process_result2 == 0) { process_call_2(result2_ptr); } pthread_t _thread_result2;waitpid(_process_result1, NULL, 0); result1 = *result1_ptr;waitpid(_process_result2, NULL, 0); result2 = *result2_ptr;printf("%d",result1);printf("%d",result2);
  return 0;
}
