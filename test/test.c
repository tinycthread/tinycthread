/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; -*-
Copyright (c) 2012 Marcus Geelnard
Copyright (c) 2013-2014 Evan Nemerson

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

static void test_sleep(void);

/* Mutex + global count variable */
mtx_t gMutex;
int gCount;

/* Condition variable */
cnd_t gCond;

once_flag onceFlags[10000];

typedef void(*TestFunc)(void);

typedef struct {
  const char* name;
  TestFunc func;
} Test;


static int thread_test_args (void * aArg)
{
  return *(int*)aArg;
}

#define TEST_THREAD_ARGS_N_THREADS 4

static void test_thread_arg_and_retval(void)
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
static int thread_test_local_storage(void * aArg)
{
  (void)aArg;
  gLocalVar = rand();
  return 0;
}

static void test_thread_local_storage(void)
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

#define TEST_THREAD_LOCK_ITERATIONS_PER_THREAD 10000

static int thread_lock(void * aArg)
{
  int i;
  mtx_t try_mutex;

  (void)aArg;

  for (i = 0; i < TEST_THREAD_LOCK_ITERATIONS_PER_THREAD; ++ i)
  {
    mtx_lock(&gMutex);
    assert(mtx_trylock(&gMutex) == thrd_busy);
    ++ gCount;
    mtx_unlock(&gMutex);
  }

  mtx_init(&try_mutex, mtx_plain);

  mtx_lock(&gMutex);
  for (i = 0; i < TEST_THREAD_LOCK_ITERATIONS_PER_THREAD; ++ i)
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

static void test_mutex_locking(void)
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

  assert(gCount == (TEST_MUTEX_LOCKING_N_THREADS * TEST_THREAD_LOCK_ITERATIONS_PER_THREAD * 2));
}

struct TestMutexData {
  mtx_t mtx;
  volatile int i;
  volatile int completed;
};

static int test_mutex_recursive_cb(void* data)
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

static void test_mutex_recursive(void)
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

static int timespec_compare (struct timespec* a, struct timespec* b)
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

#define NSECS_PER_SECOND 1000000000
static void timespec_add_nsec (struct timespec* ts, long tv_nsec)
{
  ts->tv_sec += tv_nsec / NSECS_PER_SECOND;
  ts->tv_nsec += tv_nsec % NSECS_PER_SECOND;
  if (ts->tv_nsec >= NSECS_PER_SECOND)
  {
    ts->tv_sec++;
    ts->tv_nsec -= NSECS_PER_SECOND;
  }
}

struct TestMutexTimedData {
  mtx_t mutex;
  struct timespec start;
  struct timespec timeout;
  struct timespec end;
  struct timespec upper;
};

static int test_mutex_timed_thread_func(void* arg)
{
  int ret;
  struct timespec ts;
  struct TestMutexTimedData* data = (struct TestMutexTimedData*) arg;

  ret = mtx_timedlock(&(data->mutex), &(data->timeout));
  assert (ret == thrd_timedout);

  timespec_get(&ts, TIME_UTC);
  ret = timespec_compare(&ts, &(data->start));
  assert (ret >= 0);
  ret = timespec_compare(&ts, &(data->timeout));
  assert (ret >= 0);
  ret = timespec_compare(&ts, &(data->end));
  assert (ret < 0);

  ret = mtx_lock(&(data->mutex));
  assert (ret == thrd_success);

  timespec_get(&ts, TIME_UTC);
  ret = timespec_compare(&ts, &(data->end));
  assert (ret >= 0);

  ret = timespec_compare(&ts, &(data->upper));
  assert (ret < 0);

  mtx_unlock(&(data->mutex));

  return 0;
}

static void test_mutex_timed(void)
{
  struct TestMutexTimedData data;
  thrd_t thread;
  struct timespec interval = { 0, };
  struct timespec start;
  struct timespec end;

  interval.tv_sec = 0;
  interval.tv_nsec = (NSECS_PER_SECOND / 10) * 2;

  mtx_init(&(data.mutex), mtx_timed);
  mtx_lock(&(data.mutex));

  timespec_get(&(data.start), TIME_UTC);
  data.timeout = data.start;
  timespec_add_nsec(&(data.timeout), NSECS_PER_SECOND / 10);
  data.end = data.timeout;
  timespec_add_nsec(&(data.end), NSECS_PER_SECOND / 10);
  data.upper = data.end;
  timespec_add_nsec(&(data.upper), NSECS_PER_SECOND / 10);

  thrd_create(&thread, test_mutex_timed_thread_func, &data);

  timespec_get(&start, TIME_UTC);
  assert (thrd_sleep(&interval, &interval) == 0);
  timespec_get(&end, TIME_UTC);
  mtx_unlock(&(data.mutex));

  thrd_join(thread, NULL);
}

static int test_thrd_exit_func(void* arg)
{
  test_sleep();
  thrd_exit(2);
  return 1;
}

static void test_thrd_exit(void)
{
  thrd_t thread;
  int res;
  thrd_create(&thread, test_thrd_exit_func, NULL);
  assert(thrd_join(thread, &res));
  assert(res == 2);
}

/* Thread function: Condition notifier */
static int thread_condition_notifier(void * aArg)
{
  (void)aArg;

  mtx_lock(&gMutex);
  -- gCount;
  cnd_broadcast(&gCond);
  mtx_unlock(&gMutex);
  return 0;
}

/* Thread function: Condition waiter */
static int thread_condition_waiter(void * aArg)
{
  (void)aArg;

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

static void test_condition_variables (void)
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
static int thread_yield(void * aArg)
{
  (void)aArg;

  /* Yield... */
  thrd_yield();
  return 0;
}

static void test_yield (void)
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

static void test_sleep(void)
{
  struct timespec ts;
  struct timespec interval;
  struct timespec end_ts;

  interval.tv_sec = 0;
  interval.tv_nsec = NSECS_PER_SECOND / 10;

  /* Calculate current time + 100ms */
  timespec_get(&ts, TIME_UTC);
  timespec_add_nsec(&ts, NSECS_PER_SECOND / 10);

  /* Sleep... */
  thrd_sleep(&interval, NULL);

  timespec_get(&end_ts, TIME_UTC);

  assert(timespec_compare(&ts, &end_ts) <= 0);
}

static void test_time(void)
{
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
}

/* Once function */
static void thread_once_func(void)
{
  mtx_lock(&gMutex);
  ++ gCount;
  mtx_unlock(&gMutex);
}

/* Once thread function */
static int thread_once(void* data)
{
  int i;

  (void)data;

  for (i = 0; i < 10000; i++)
  {
    call_once(&(onceFlags[i]), thread_once_func);
  }

  return 0;
}

#define TEST_ONCE_N_THREADS 16

static void test_once (void)
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

struct TestThreadSpecificData {
  tss_t key;
  mtx_t mutex;
  int values_freed;
} test_tss_data;

static void test_tss_free (void* val)
{
  mtx_lock(&(test_tss_data.mutex));
  test_tss_data.values_freed++;
  mtx_unlock(&(test_tss_data.mutex));
  free(val);
}

static int test_tss_thread_func (void* data)
{
  int* value = (int*)malloc(sizeof(int));

  (void)data;

  *value = rand();

  assert(tss_get(test_tss_data.key) == NULL);
  tss_set(test_tss_data.key, value);
  assert(tss_get(test_tss_data.key) == value);

  tss_set(test_tss_data.key, NULL);
  assert(tss_get(test_tss_data.key) == NULL);
  tss_set(test_tss_data.key, value);
  assert(tss_get(test_tss_data.key) == value);

  return 0;
}

#define TEST_TSS_N_THREADS 256

static void test_tss (void)
{
  thrd_t threads[TEST_TSS_N_THREADS];
  int* value = (int*)malloc(sizeof(int));
  int i;

  *value = rand();

  tss_create(&(test_tss_data.key), test_tss_free);
  mtx_init(&(test_tss_data.mutex), mtx_plain);
  test_tss_data.values_freed = 0;

  assert(tss_get(test_tss_data.key) == NULL);
  tss_set(test_tss_data.key, value);
  assert(tss_get(test_tss_data.key) == value);

  for (i = 0; i < TEST_TSS_N_THREADS; i++)
  {
    thrd_create(&(threads[i]), test_tss_thread_func, NULL);
  }

  for (i = 0; i < TEST_TSS_N_THREADS; i++)
  {
    thrd_join(threads[i], NULL);
  }

  assert(test_tss_data.values_freed == TEST_TSS_N_THREADS);
  assert(tss_get(test_tss_data.key) == value);
  tss_delete(test_tss_data.key);
  assert(tss_get(test_tss_data.key) == NULL);
  assert(test_tss_data.values_freed == TEST_TSS_N_THREADS);

  free(value);
}



const Test unit_tests[] =
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
  { "thread-specific-storage", test_tss },
  { "mutex-timed", test_mutex_timed },
  { "thread-exit", test_thrd_exit },
  { NULL, }
};

static void test_config_print_and_exit(const Test* tests, int argc, char** argv)
{
  int test_n;

  (void)argc;

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

static void test_run(const Test* test, unsigned int seed)
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

static int tests_run(const Test* tests, int argc, char** argv)
{
  int opt;
  unsigned long int seed;
  char* endptr;
  struct timespec tv;
  int test_n;
  int found;

  timespec_get(&tv, TIME_UTC);
  srand(tv.tv_nsec);
  seed = rand();

  #if !defined(_TTHREAD_WIN32_)
  while ((opt = getopt(argc, argv, "s:h")) != -1)
  {
    switch (opt)
    {
      case 's':
        {
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

  fprintf(stdout, "Random seed: %lu\n", seed);

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

  res = tests_run(unit_tests, argc, argv);

  mtx_destroy(&gMutex);
  cnd_destroy(&gCond);

  return res;
}
