#include <stdlib.h> 
                   #include <stdio.h> 
                   #include <pthread.h> 
                   #include <unistd.h>
int _exp(int a) {return (a*a);}
int main() {
int res=_exp(10);printf("%d", res);
  return 0;
}
