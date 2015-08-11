//gcc sem-ex2.c -Wall -pedantic -std=c99 -D_XOPEN_SOURCE=700 -pthread
/* Includes */
#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */ 
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <semaphore.h>  /* Semaphore */

/* prototype for thread routine */
void handler ( void *ptr );

/* global vars */
/* semaphores are declared global so they can be accessed 
   in main() and in thread routine,
   here, the semaphore is used as a mutex */
sem_t mutex;
int counter; /* shared variable */

int main()
{
    int i[4];
    pthread_t thread_a;
    pthread_t thread_b;
    pthread_t thread_c;
    pthread_t thread_d;
    
    i[0] = 0; /* argument to threads */
    i[1] = 1;
    i[2] = 2;
    i[3]=3;
    sem_init(&mutex, 0, 2);      /* initialize mutex to 2 - binary semaphore */
                                 /* second param = 0 - semaphore is local */
                                 
    /* Note: you can check if thread has been successfully created by checking return value of
       pthread_create */                                 
    pthread_create (&thread_a, NULL, (void *) &handler, (void *) &i[0]);
    pthread_create (&thread_b, NULL, (void *) &handler, (void *) &i[1]);
    
        pthread_create (&thread_c, NULL, (void *) &handler, (void *) &i[2]);
    pthread_create (&thread_d, NULL, (void *) &handler, (void *) &i[3]);

    pthread_join(thread_a, NULL);
    pthread_join(thread_b, NULL);
    pthread_join(thread_c, NULL);
    pthread_join(thread_d, NULL);

    sem_destroy(&mutex); /* destroy semaphore */
                  
    /* exit */  
    exit(0);
} /* main() */

void handler ( void *ptr )
{
    int x; 
    x = *((int *) ptr);
    printf("Thread %d: Waiting to enter critical region...\n", x);
    sem_wait(&mutex);       /* down semaphore */
    /* START CRITICAL REGION */
    printf("Thread %d: Now in critical region...\n", x);
    printf("Thread %d: Counter Value: %d\n", x, counter);
    printf("Thread %d: Incrementing Counter...\n", x);
    counter++;
    printf("Thread %d: Incrementing Counter...\n", x);
    counter++;
    printf("Thread %d: Incrementing Counter...\n", x);
    counter++;
    printf("Thread %d: Incrementing Counter...\n", x);
    counter++;
    printf("Thread %d: Incrementing Counter...\n", x);
    counter++;
    printf("Thread %d: New Counter Value: %d\n", x, counter);
    sleep(3);
    printf("Thread %d: Exiting critical region...\n", x);
    counter++;
    counter++;
    counter++;
    /* END CRITICAL REGION */    
    sem_post(&mutex);       /* up semaphore */
    
    pthread_exit(0); /* exit thread */
}