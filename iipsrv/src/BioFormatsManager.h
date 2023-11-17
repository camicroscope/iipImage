/*
 * File:   BioFormatsManager.h
 */

#ifndef BIOFORMATSMANAGER_H
#define BIOFORMATSMANAGER_H

#include <vector>
#include "BioFormatsInstance.h"
#include <stdio.h>

class BioFormatsManager
{
private:
  static std::vector<BioFormatsInstance> free_list;

public:
  // call me with std::move - I think?
  static void free(BioFormatsInstance &&graal_isolate)
  {
    free_list.push_back(std::move(graal_isolate));
    free_list.back().refresh();
  }

  static BioFormatsInstance get_new()
  {
    // Make a new one if needed
    if (free_list.size() == 0)
    {
      BioFormatsInstance bfi;
      free_list.push_back(std::move(bfi));
    }

    // Pop one from the array
    BioFormatsInstance bfi = std::move(free_list.back());
    free_list.pop_back(); // calls destructuor
    return bfi;
  }
};

#endif /* BIOFORMATSMANAGER_H */
