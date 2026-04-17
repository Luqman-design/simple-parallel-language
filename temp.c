#include <stdlib.h> 
                   #include <stdio.h> 
                   #include <pthread.h> 
                   #include <unistd.h>
pthread_mutex_t lock_counter;
pthread_mutex_init(&lock_counter, NULL);
void* a(void* arg) {
pthread_mutex_lock(&lock_counter);
counter=(counter+1);pthread_mutex_unlock(&lock_counter);
pthread_mutex_lock(&lock_counter);
counter=(counter+1);pthread_mutex_unlock(&lock_counter);
  return NULL;
}

void* b(void* arg) {
pthread_mutex_lock(&lock_counter);
counter=(counter+1);pthread_mutex_unlock(&lock_counter);
  return NULL;
}

int main() {
int counter=0;printf("%d", counter);
  return 0;
}
