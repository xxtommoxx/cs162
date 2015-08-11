#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#define MAX_THREAD 100

 sem_t *semaphore;


typedef struct {
 int start,end;
} param;

void *count(void *arg){
 sem_wait(&semaphore);
 int i =0;
 param *p=(param *)arg;
 printf("\nprintfrom %d  to  %d\n",p->start,p->end);
 for(i =p->start ; i< p->end ; i++){
  printf(" i = %d",i);sleep(1);
 }
 
 sem_post(&semaphore);
 return (void*)(1);
}


int main(int argc, char *argv[])
{
   

    if ((semaphore = sem_open("/semaphore", O_CREAT, 0644, 2)) == SEM_FAILED ) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // sem_wait(semaphore);
    // sem_post(semaphore);

   


 int n,i;
 pthread_t *threads;
 param *p;

     if (argc != 2) {
      printf ("Usage: %s n\n",argv[0]);
      printf ("\twhere n is no. of threads\n");
      exit(1);
     }

 n=atoi(argv[1]);

    if ((n < 1) || (n > MAX_THREAD)) {
      printf ("arg[1] should be 1 - %d.\n",MAX_THREAD);
      exit(1);
    }

 threads=(pthread_t *)malloc(n*sizeof(*threads));

 p=(param *)malloc(sizeof(param)*n);

 for (i=0; i<n; i++) {
  p[i].start=i*100;
  p[i].end=(i+1)*100;
  pthread_create(&threads[i],NULL,count,(void *)(p+i));
 }
 
 printf("\nWait threads\n");
 sleep(1);

 int *x = malloc(sizeof(int));
for (i=0; i<n; i++) {
  pthread_join(threads[i],(void*)x);
 }

 sem_close(semaphore);
sem_unlink("/semaphore");
 
 free(p);
 puts("Done");
 exit(0);




    

}


