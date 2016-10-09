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

/* This is the child thread function */
int HelloThread(void * aArg)
{
  (void)aArg;

  printf("Hello world!\n");
  return 0;
}

/* This is the main program (i.e. the main thread) */
int main()
{
  /* Start the child thread */
  thrd_t t;
  if (thrd_create(&t, 0, HelloThread, (void*)0) == thrd_success)
  {
    /* Wait for the thread to finish */
    thrd_join(t, NULL);
  }

  return 0;
}
