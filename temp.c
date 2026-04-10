#include <stdlib.h> 
                   #include <stdio.h> 
                   #include <pthread.h> 
                   #include <unistd.h>
                    pthread_mutex_t global_lock; 
int func_name(int a) {
 pthread_mutex_lock(&global_lock);
for (int i=0; (i<5); i+=1) {printf("%d", i);}pthread_mutex_unlock(&global_lock);
}
int main() {
                  pthread_mutex_init(&global_lock, NULL);
if(fork()==0){func_name(4);fflush(stdout);_exit(0);};return 0;}