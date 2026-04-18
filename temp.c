#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
int result1 = 0;
int result2 = 0;
int add(int a, int b) {return (a+b);}

void* thread_call_1(void* arg) {intptr_t* args=(intptr_t*)arg;result1=add((int)args[0], (int)args[1]);return NULL;}
void* thread_call_2(void* arg) {intptr_t* args=(intptr_t*)arg;result2=add((int)args[0], (int)args[1]);return NULL;}
int main() {
pthread_t _thread_result1;intptr_t _args_result1[2]={4, 7};pthread_create(&_thread_result1,NULL,thread_call_1,_args_result1);pthread_t _thread_result2;intptr_t _args_result2[2]={20, 13};pthread_create(&_thread_result2,NULL,thread_call_2,_args_result2);pthread_join(_thread_result1, NULL);pthread_join(_thread_result2, NULL);printf("%d",result1);printf("%d",result2);
  return 0;
}
