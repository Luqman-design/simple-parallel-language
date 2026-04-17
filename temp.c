#include <stdlib.h> 
                   #include <stdio.h> 
                   #include <pthread.h> 
                   #include <unistd.h>
void *for_loop_worker_1(void *arg) { int start_index = *(int *)arg + 0; for (int i = start_index; i < 100; i = i + 2) { printf("%d", i);printf("%s"," - ");} return NULL; }int main() {
pthread_t threads[2]; int starts[2]; for (int i = 0; i < 2; i++) { starts[i] = i; pthread_create(&threads[i], NULL, for_loop_worker_1, &starts[i]); }for (int i = 0; i <2; i++) { pthread_join(threads[i], NULL); }
  return 0;
}
