/*
 * File:   BioFormatsThread.h
 */

#ifndef BIOFORMATSTHREAD_H
#define BIOFORMATSTHREAD_H

#ifndef BFBRIDGE_INLINE
#define BFBRIDGE_INLINE
#endif

#ifndef BFBRIDGE_KNOW_BUFFER_LEN
#define BFBRIDGE_KNOW_BUFFER_LEN
#endif

#include <jni.h>
#include <string>
#include <stdlib.h>
#include "../../BFBridge/c/bfbridge_basiclib.h"

class BioFormatsThread
{
public:
  bfbridge_vm_t bfvm;
  bfbridge_thread_t bfthread;

  BioFormatsThread();

  // Copying a BioFormatsThread means copying a JVM and this is not
  // possible. The attempt to do that is a sign of faulty code
  // so a compile time error.
  BioFormatsThread(const BioFormatsThread &other) = delete;
  BioFormatsThread &operator=(const BioFormatsThread &other) = delete;

  ~BioFormatsThread()
  {
    bfbridge_free_thread(&bfthread);

    // Must run only once, on app termination
    // Any other time, it breaks JVM and won't run again
    // Therefore even if JVM is unused for some time, it must be kept alive
    bfbridge_free_vm(&bfvm);
  }
};

#endif /* BIOFORMATSTHREAD_H */
