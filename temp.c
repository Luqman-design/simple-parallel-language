#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
int counter = 0;
pthread_mutex_t lock_counter;
void* thread_call_1(void* arg) {pthread_mutex_lock(&lock_counter);counter+=10;pthread_mutex_unlock(&lock_counter);pthread_mutex_lock(&lock_counter);counter+=10;pthread_mutex_unlock(&lock_counter);pthread_mutex_lock(&lock_counter);counter+=10;pthread_mutex_unlock(&lock_counter);return NULL;}
int main() {
pthread_mutex_init(&lock_counter, NULL);
pthread_t _thread__t1;pthread_create(&_thread__t1,NULL,thread_call_1,NULL);pthread_join(_thread__t1, NULL);printf("%d",counter);
  return 0;
}
