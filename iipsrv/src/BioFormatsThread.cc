/*
 * File:   BioFormatsThread.cc
 */

#include "BioFormatsThread.h"
#include <stdexcept>
#include <iostream>

BioFormatsThread::BioFormatsThread()
{
  // In our Docker caMicroscpe deployment we pass these using fcgid.conf
  // and other conf files
  // Required:
  char *cpdir = getenv("BFBRIDGE_CLASSPATH");
  if (!cpdir || cpdir[0] == '\0')
  {
    std::cerr << "Please set BFBRIDGE_CLASSPATH to a single directory where jar files can be found.\n";
    throw std::runtime_error("Please set BFBRIDGE_CLASSPATH to a single directory where jar files can be found.\n");
  }

  // Optional:
  char *cachedir = getenv("BFBRIDGE_CACHEDIR");
  if (cachedir && cachedir[0] == '\0')
  {
    cachedir = NULL;
  }
  bfbridge_error_t *error = bfbridge_make_vm(&bfvm, cpdir, cachedir);
  if (error)
  {
    std::cerr << "BioFormatsThread.cc bfbridge_make_vm error: " << error->description << std::endl;
    throw std::runtime_error("BioFormatsThread.cc bfbridge_make_vm error: \n" + std::string(error->description));
  }

  // Expensive function being used from a header-only library.
  // Shouldn't be called from a header file
  error = bfbridge_make_thread(&bfthread, &bfvm);
  if (error)
  {
    std::cerr << "BioFormatsThread.cc bfbridge_make_thread error: " << error->description << std::endl;
    throw "BioFormatsThread.cc bfbridge_make_thread error: \n" + std::string(error->description);
  }
}
