/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; -*-
Copyright (c) 2012 Marcus Geelnard

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/

#include <stdio.h>
#include <tinycthread.h>

/* HACK: Mac OS X and early MinGW do not support compile time thread-local
   storage */
#if defined(__APPLE__) || (defined(__MINGW32__) && (__GNUC__ < 4))
 #define NO_CT_TLS
#endif

/* Compile time thread local storage variable */
#ifndef NO_CT_TLS
_Thread_local int gLocalVar;
#endif

/* Mutex + global count variable */
mtx_t gMutex;
int gCount;

/* Condition variable */
cnd_t gCond;

/* Thread function: Thread ID */
int ThreadIDs(void * aArg)
{
  int myId = *(int*)aArg;
  printf(" Hello, I'm thread %d.\n", myId);
  fflush(stdout);
  return myId;
}

#ifndef NO_CT_TLS
/* Thread function: Compile time thread-local storage */
int ThreadTLS(void * aArg)
{
  gLocalVar = 2;
  printf(" My gLocalVar is %d.\n", gLocalVar);
  return 0;
}
#endif

/* Thread function: Mutex locking */
int ThreadLock(void * aArg)
{
  int i;
  for (i = 0; i < 10000; ++ i)
  {
    mtx_lock(&gMutex);
    ++ gCount;
    mtx_unlock(&gMutex);
  }
  return 0;
}

/* Thread function: Condition notifier */
int ThreadCondition1(void * aArg)
{
  mtx_lock(&gMutex);
  -- gCount;
  cnd_broadcast(&gCond);
  mtx_unlock(&gMutex);
  return 0;
}

/* Thread function: Condition waiter */
int ThreadCondition2(void * aArg)
{
  printf(" Wating...");
  fflush(stdout);
  mtx_lock(&gMutex);
  while(gCount > 0)
  {
    printf(".");
    fflush(stdout);
    cnd_wait(&gCond, &gMutex);
  }
  printf(".\n");
  return 0;
}

/* Thread function: Yield */
int ThreadYield(void * aArg)
{
  /* Yield... */
  thrd_yield();
  return 0;
}


/* This is the main program (i.e. the main thread) */
int main(void)
{
  /* Initialization... */
  mtx_init(&gMutex, mtx_plain);
  cnd_init(&gCond);

  /* Test 1: thread arguments & return values */
  printf("PART I: Thread arguments & return values\n");
  {
    thrd_t t1, t2, t3, t4;
    int id1, id2, id3, id4, ret1, ret2, ret3, ret4;

    /* Start a bunch of child threads */
    id1 = 1;
    thrd_create(&t1, ThreadIDs, (void*)&id1);
    thrd_join(t1, &ret1);
    printf(" Thread 1 returned %d.\n", ret1);
    id2 = 2;
    thrd_create(&t2, ThreadIDs, (void*)&id2);
    thrd_join(t2, &ret2);
    printf(" Thread 2 returned %d.\n", ret2);
    id3 = 3;
    thrd_create(&t3, ThreadIDs, (void*)&id3);
    thrd_join(t3, &ret3);
    printf(" Thread 3 returned %d.\n", ret3);
    id4 = 4;
    thrd_create(&t4, ThreadIDs, (void*)&id4);
    thrd_join(t4, &ret4);
    printf(" Thread 4 returned %d.\n", ret4);
  }

  /* Test 2: compile time thread local storage */
  printf("PART II: Compile time thread local storage\n");
#ifndef NO_CT_TLS
  {
    thrd_t t1;

    /* Clear the TLS variable (it should keep this value after all threads are
       finished). */
    gLocalVar = 1;
    printf(" Main gLocalVar is %d.\n", gLocalVar);

    /* Start a child thread that modifies gLocalVar */
    thrd_create(&t1, ThreadTLS, NULL);
    thrd_join(t1, NULL);

    /* Check if the TLS variable has changed */
    if(gLocalVar == 1)
      printf(" Main gLocalID was not changed by the child thread - OK!\n");
    else
      printf(" Main gLocalID was changed by the child thread - FAIL!\n");
  }
#else
  printf(" Compile time TLS is not supported on this platform...\n");
#endif

  /* Test 3: mutex locking */
  printf("PART III: Mutex locking (100 threads x 10000 iterations)\n");
  {
    thrd_t t[100];
    int i;

    /* Clear the global counter. */
    gCount = 0;

    /* Start a bunch of child threads */
    for (i = 0; i < 100; ++ i)
    {
      thrd_create(&t[i], ThreadLock, NULL);
    }

    /* Wait for the threads to finish */
    for (i = 0; i < 100; ++ i)
    {
      thrd_join(t[i], NULL);
    }

    /* Check the global count */
    printf(" gCount = %d\n", gCount);
  }

  /* Test 4: condition variable */
  printf("PART IV: Condition variable (40 + 1 threads)\n");
  {
    thrd_t t1, t[40];
    int i;

    /* Set the global counter to the number of threads to run. */
    gCount = 40;

    /* Start the waiting thread (it will wait for gCount to reach zero). */
    thrd_create(&t1, ThreadCondition2, NULL);

    /* Start a bunch of child threads (these will decrease gCount by 1 when they
       finish) */
    for (i = 0; i < 40; ++ i)
    {
      thrd_create(&t[i], ThreadCondition1, NULL);
    }

    /* Wait for the waiting thread to finish */
    thrd_join(t1, NULL);

    /* Wait for the other threads to finish */
    for (i = 0; i < 40; ++ i)
    {
      thrd_join(t[i], NULL);
    }
  }

  /* Test 5: yield */
  printf("PART V: Yield (40 + 1 threads)\n");
  {
    thrd_t t[40];
    int i;

    /* Start a bunch of child threads */
    for (i = 0; i < 40; ++ i)
    {
      thrd_create(&t[i], ThreadYield, NULL);
    }

    /* Yield... */
    thrd_yield();

    /* Wait for the threads to finish */
    for (i = 0; i < 40; ++ i)
    {
      thrd_join(t[i], NULL);
    }
  }

  /* Test 6: Sleep */
  printf("PART VI: Sleep (10 x 100 ms)\n");
  {
    int i;
    struct timespec ts;

    printf(" Sleeping");
    fflush(stdout);
    for (i = 0; i < 10; ++ i)
    {
      /* Calculate current time + 100ms */
      clock_gettime(TIME_UTC, &ts);
      ts.tv_nsec += 100000000;
      if (ts.tv_nsec >= 1000000000)
      {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
      }

      /* Sleep... */
      thrd_sleep(&ts, NULL);

      printf(".");
      fflush(stdout);
    }
    printf("\n");
  }

  /* Test 7: Time */
  printf("PART VII: Current time (UTC), three times\n");
  {
    struct timespec ts;
    clock_gettime(TIME_UTC, &ts);
    printf(" Time = %ld.%09ld\n", (long)ts.tv_sec, ts.tv_nsec);
    clock_gettime(TIME_UTC, &ts);
    printf(" Time = %ld.%09ld\n", (long)ts.tv_sec, ts.tv_nsec);
    clock_gettime(TIME_UTC, &ts);
    printf(" Time = %ld.%09ld\n", (long)ts.tv_sec, ts.tv_nsec);
  }

  /* FIXME: Implement some more tests for the TinyCThread API... */

  /* Finalization... */
  mtx_destroy(&gMutex);
  cnd_destroy(&gCond);

  return 0;
}
