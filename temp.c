#include <stdlib.h> 
                   #include <stdio.h> 
                   #include <pthread.h> 
                   #include <unistd.h>
int main() {
int x=0;if (1){x=(x+1);x=(x+1);}printf("%d", x);return 0;}