/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; -*-
Copyright (c) 2012 Marcus Geelnard
Copyright (c) 2013 Evan Nemerson

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
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <string.h>

#if !defined(_TTHREAD_WIN32_)
#include <unistd.h>
#include <strings.h>
#endif

/* HACK: Mac OS X, early MinGW, and TCC do not support compile time
   thread-local storage */
#if defined(__APPLE__) || (defined(__MINGW32__) && (__GNUC__ < 4)) || defined(__TINYC__)
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

once_flag onceFlags[10000];

typedef void(*TestFunc)();

typedef struct {
  const char* name;
  TestFunc func;
} Test;


int thread_test_args (void * aArg)
{
  return *(int*)aArg;
}

#define TEST_THREAD_ARGS_N_THREADS 4

void test_thread_arg_and_retval()
{
  thrd_t threads[TEST_THREAD_ARGS_N_THREADS];
  int ids[TEST_THREAD_ARGS_N_THREADS];
  int retval;
  int i;

  for (i = 0; i < TEST_THREAD_ARGS_N_THREADS; i++)
  {
    ids[i] = rand();
    thrd_create(&(threads[i]), thread_test_args, (void*) &(ids[i]));
  }

  for (i = 0; i < TEST_THREAD_ARGS_N_THREADS; i++)
  {
    thrd_join(threads[i], &retval);
    assert (retval == ids[i]);
  }
}

#ifndef NO_CT_TLS
/* Thread function: Compile time thread-local storage */
int thread_test_local_storage(void * aArg)
{
  gLocalVar = rand();
  return 0;
}

void test_thread_local_storage()
{
  thrd_t t1;

  /* Clear the TLS variable (it should keep this value after all
     threads are finished). */
  gLocalVar = 1;

  /* Start a child thread that modifies gLocalVar */
  thrd_create(&t1, thread_test_local_storage, NULL);
  thrd_join(t1, NULL);

  /* Check if the TLS variable has changed */
  assert(gLocalVar == 1);
}
#endif

int thread_lock(void * aArg)
{
  int i;
  mtx_t try_mutex;

  for (i = 0; i < 10000; ++ i)
  {
    mtx_lock(&gMutex);
    assert(mtx_trylock(&gMutex) == thrd_busy);
    ++ gCount;
    mtx_unlock(&gMutex);
  }

  mtx_init(&try_mutex, mtx_try);

  mtx_lock(&gMutex);
  for (i = 0; i < 10000; ++ i)
  {
    assert (mtx_trylock(&try_mutex) == thrd_success);
    assert (mtx_trylock(&try_mutex) == thrd_busy);
    ++ gCount;
    mtx_unlock(&try_mutex);
  }
  mtx_unlock(&gMutex);

  return 0;
}

#define TEST_MUTEX_LOCKING_N_THREADS 128

void test_mutex_locking()
{
  thrd_t t[TEST_MUTEX_LOCKING_N_THREADS];
  int i;

  gCount = 0;

  for (i = 0; i < TEST_MUTEX_LOCKING_N_THREADS; ++ i)
  {
    thrd_create(&(t[i]), thread_lock, NULL);
  }

  for (i = 0; i < TEST_MUTEX_LOCKING_N_THREADS; ++ i)
  {
    thrd_join(t[i], NULL);
  }

  assert(gCount == (TEST_MUTEX_LOCKING_N_THREADS * 10000 * 2));
}

struct TestMutexData {
  mtx_t mtx;
  volatile int i;
  volatile int completed;
};

int test_mutex_recursive_cb(void* data)
{
  const int iterations = 10000;
  int i;
  struct TestMutexData* mutex_data = (struct TestMutexData*) data;

  assert (mtx_lock (&(mutex_data->mtx)) == thrd_success);

  for ( i = 0 ; i < iterations ; i++ )
  {
    mtx_lock (&(mutex_data->mtx));
    assert (mutex_data->i++ == i);
  }

  for ( i = iterations - 1 ; i >= 0 ; i-- )
  {
    mtx_unlock (&(mutex_data->mtx));
    assert (--(mutex_data->i) == i);
  }

  assert (mutex_data->i == 0);

  mutex_data->completed++;

  mtx_unlock (&(mutex_data->mtx));

  return 0;
}

#define TEST_MUTEX_RECURSIVE_N_THREADS 128

void test_mutex_recursive()
{
  thrd_t t[TEST_MUTEX_RECURSIVE_N_THREADS];
  int i;
  struct TestMutexData data;

  mtx_init(&(data.mtx), mtx_recursive);
  data.i = 0;
  data.completed = 0;

  for ( i = 0 ; i < TEST_MUTEX_RECURSIVE_N_THREADS ; i++ )
  {
    thrd_create (&(t[i]), test_mutex_recursive_cb, &data);
  }

  for ( i = 0 ; i < TEST_MUTEX_RECURSIVE_N_THREADS ; i++ )
  {
    thrd_join (t[i], NULL);
  }

  assert (data.completed == TEST_MUTEX_RECURSIVE_N_THREADS);
}

/* Thread function: Condition notifier */
int thread_condition_notifier(void * aArg)
{
  mtx_lock(&gMutex);
  -- gCount;
  cnd_broadcast(&gCond);
  mtx_unlock(&gMutex);
  return 0;
}

/* Thread function: Condition waiter */
int thread_condition_waiter(void * aArg)
{
  fflush(stdout);
  mtx_lock(&gMutex);
  while(gCount > 0)
  {
    fflush(stdout);
    cnd_wait(&gCond, &gMutex);
  }
  mtx_unlock(&gMutex);
  return 0;
}

void test_condition_variables ()
{
  thrd_t t1, t[40];
  int i;

  /* Set the global counter to the number of threads to run. */
  gCount = 40;

  /* Start the waiting thread (it will wait for gCount to reach
     zero). */
  thrd_create(&t1, thread_condition_waiter, NULL);

  /* Start a bunch of child threads (these will decrease gCount by 1
     when they finish) */
  for (i = 0; i < 40; ++ i)
  {
    thrd_create(&t[i], thread_condition_notifier, NULL);
  }

  /* Wait for the waiting thread to finish */
  thrd_join(t1, NULL);

  /* Wait for the other threads to finish */
  for (i = 0; i < 40; ++ i)
  {
    thrd_join(t[i], NULL);
  }
}

/* Thread function: Yield */
int thread_yield(void * aArg)
{
  /* Yield... */
  thrd_yield();
  return 0;
}

void test_yield ()
{
  thrd_t t[40];
  int i;

  /* Start a bunch of child threads */
  for (i = 0; i < 40; ++ i)
  {
    thrd_create(&t[i], thread_yield, NULL);
  }

  /* Yield... */
  thrd_yield();

  /* Wait for the threads to finish */
  for (i = 0; i < 40; ++ i)
  {
    thrd_join(t[i], NULL);
  }
}

int timespec_compare (struct timespec* a, struct timespec* b)
{
  if (a->tv_sec != b->tv_sec)
  {
    return a->tv_sec - b->tv_sec;
  }
  else if (a->tv_nsec != b->tv_nsec)
  {
    return a->tv_nsec - b->tv_nsec;
  }
  else
  {
    return 0;
  }
}

void test_sleep()
{
  int i;
  struct timespec ts;
  struct timespec end_ts;

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

  clock_gettime(TIME_UTC, &end_ts);

  assert(timespec_compare(&ts, &end_ts) <= 0);
}

void test_time()
{
  struct timespec ts;
  clock_gettime(TIME_UTC, &ts);
}

/* Once function */
void thread_once_func(void)
{
  mtx_lock(&gMutex);
  ++ gCount;
  mtx_unlock(&gMutex);
}

/* Once thread function */
int thread_once(void* data)
{
  int i;

  for (i = 0; i < 10000; i++)
  {
    call_once(&(onceFlags[i]), thread_once_func);
  }

  return 0;
}

#define TEST_ONCE_N_THREADS 16

void test_once ()
{
  const once_flag once_flag_init = ONCE_FLAG_INIT;
  thrd_t threads[TEST_ONCE_N_THREADS];
  int i;

  /* Initialize 10000 once_flags */
  for (i = 0; i < 10000 ; i++)
  {
    onceFlags[i] = once_flag_init;
  }

  /* Clear the global counter. */
  mtx_lock(&gMutex);
  gCount = 0;
  mtx_unlock(&gMutex);

  /* Create threads */
  for (i = 0; i < TEST_ONCE_N_THREADS; i++)
  {
    thrd_create(&(threads[i]), thread_once, NULL);
  }

  /* Wait for all threads to finish executing. */
  for (i = 0; i < TEST_ONCE_N_THREADS; i++)
  {
    thrd_join(threads[i], NULL);
  }

  /* Check the global count */
  assert(gCount == 10000);
}



const Test tests[] =
{
  { "thread-arg-and-retval", test_thread_arg_and_retval },
#ifndef NO_CT_TLS
  { "thread-local-storage", test_thread_local_storage },
#endif
  { "mutex-locking", test_mutex_locking },
  { "mutex-recursive", test_mutex_recursive },
  { "condition-variables", test_condition_variables },
  { "yield", test_yield },
  { "sleep", test_sleep },
  { "time", test_time },
  { "once", test_once },
  { NULL, }
};

void test_config_print_and_exit(const Test* tests, int argc, char** argv)
{
  int test_n;

  fprintf (stdout, "Usage: %s [OPTION]... [TEST]...\n", argv[0]);
  fprintf (stdout, "Tests for TinyCThread.\n");
  fprintf (stdout, "\n");
  fprintf (stdout, "Available tests:\n");
  for (test_n = 0; tests[test_n].name != NULL; test_n++)
  {
    fprintf (stdout, "  %s\n", tests[test_n].name);
  }
  fprintf (stdout, "\n");
  fprintf (stdout, "Options:\n");
  fprintf (stdout, "  -s seed       Seed for the random number generator.\n");
  fprintf (stdout, "  -h            Print this help screen and exit.\n");
}

void test_run(const Test* test, unsigned int seed)
{
  int i;
  fputs("  ", stdout);
  fputs(test->name, stdout);
  for (i = strlen(test->name); i < 48; i++)
  {
    fputc(' ', stdout);
  }
  fflush(stdout);
  srand(seed);
  test->func();
  fprintf(stdout, "OK\n");
}

int tests_run(const Test* tests, int argc, char** argv)
{
  int opt;
  int optc = 0;
  unsigned long int seed;
  char* endptr;
  struct timespec tv;
  int test_n;
  int found;

  clock_gettime(TIME_UTC, &tv);
  srand(tv.tv_nsec);
  seed = rand();

  #if !defined(_TTHREAD_WIN32_)
  while ((opt = getopt(argc, argv, "s:h")) != -1)
  {
    switch (opt)
    {
      case 's':
        {
          unsigned long int strtoul(const char *nptr, char **endptr, int base);
          seed = strtoul(optarg, &endptr, 0);
          if (*endptr != '\0' || seed > UINT_MAX)
          {
            fprintf (stdout, "Invalid seed `%s'.\n", optarg);
            exit(-1);
          }
        }
        break;
      case 'h':
        test_config_print_and_exit(tests, argc, argv);
        return 0;
      default:
        test_config_print_and_exit(tests, argc, argv);
        return -1;
    }
  }

  fprintf(stdout, "Random seed: %u\n", seed);

  if (optind < argc)
  {
    for (; optind < argc; optind++)
    {
      found = 0;
      for (test_n = 0; tests[test_n].name != NULL; test_n++)
      {
        if (strcasecmp(argv[optind], tests[test_n].name) == 0)
        {
          test_run (&(tests[test_n]), seed);
          found = 1;
          break;
        }
      }

      if (found == 0)
      {
        fprintf (stderr, "Could not find test `%s'.\n", argv[optind]);
        exit(-1);
      }
    }

    return 0;
  }
  #endif

  for (test_n = 0; tests[test_n].name != NULL; test_n++)
  {
    test_run (&(tests[test_n]), seed);
  }

  return 0;
}

int main(int argc, char** argv)
{
  int res;

  mtx_init(&gMutex, mtx_plain);
  cnd_init(&gCond);

  res = tests_run(tests, argc, argv);

  mtx_destroy(&gMutex);
  cnd_destroy(&gCond);

  return res;
}
