/*
 * File:   BioFormatsInstance.h
 */

#ifndef BIOFORMATSINSTANCE_H
#define BIOFORMATSINSTANCE_H

#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <jni.h>
#include "BioFormatsThread.h"

/*
To use this library, do:
BioFormatsInstance bfi = BioFormatsManager::get_new();
bfi.bf_open(filename); // or any other methods
...
and when you're done:
BioFormatsManager::free(std::move(bfi));
*/

// Allow 2048*2048 four channels of 16 bits
#define bfi_communication_buffer_len 33554432

class BioFormatsInstance
{
public:
  // std::unique_ptr<BioFormatsThread> or shared ptr would also work
  static BioFormatsThread thread;

  bfbridge_instance_t bfinstance;

  BioFormatsInstance();

  // We want to keep alive 1 BioFormats instance
  // Copying is not allowed as it's not possible
  // Move semantics instead.
  // An alternative would be using unique pointer
  // Another would be reference counting, but destroying the previous is easier.
  // (https://ps.uci.edu/~cyu/p231C/LectureNotes/lecture13:referenceCounting/lecture13.pdf)
  // Our move semantics should replace the previous object's pointers with null pointers.
  // This is because the previous object will likely be destroyed (as they are already unusable)
  // so their destructor mustnt't free Java structures. So we set them to NULL
  // and make the destructor free them only if they're not NULL.
  // This way, only one copy is kept alive.

  BioFormatsInstance(const BioFormatsInstance &) = delete;
  BioFormatsInstance(BioFormatsInstance &&other)
  {
    // Copy both the Java instance class and the communication buffer
    // Moving removes the java class pointer from the previous
    // so that the destruction of it doesn't break the newer class
    bfbridge_move_instance(&bfinstance, &other.bfinstance);
  }
  BioFormatsInstance &operator=(const BioFormatsInstance &) = delete;
  BioFormatsInstance &operator=(BioFormatsInstance &&other)
  {
    bfbridge_move_instance(&bfinstance, &other.bfinstance);
    return *this;
  }

  char *communication_buffer()
  {
    return bfbridge_instance_get_communication_buffer(&bfinstance, NULL);
  }

  ~BioFormatsInstance()
  {
    char *buffer = communication_buffer();
    if (buffer)
    {
      delete[] buffer;
    }

    bfbridge_free_instance(&bfinstance, &thread.bfthread);
  }

  // changed ownership: user opened new file, etc.
  void refresh()
  {
#ifdef OSI_DEBUG
    cerr << "calling refresh\n";
#endif
    // Here is an example of calling a method manually without the C wrapper
    thread.bfthread.env->CallVoidMethod(bfinstance.bfbridge, thread.bfthread.BFClose);
#ifdef OSI_DEBUG
    cerr << "called refresh\n";
#endif
  }

  // To be called only just after a function returns an error code
  std::string get_error()
  {
    std::string err;
    char *buffer = communication_buffer();
    err.assign(communication_buffer(),
               bf_get_error_length(&bfinstance, &thread.bfthread));
    return err;
  }

  int is_compatible(std::string filepath)
  {
    return bf_is_compatible(&bfinstance, &thread.bfthread, &filepath[0], filepath.length());
  }

  int open(std::string filepath)
  {
    return bf_open(&bfinstance, &thread.bfthread, &filepath[0], filepath.length());
  }

  int close()
  {
    return bf_close(&bfinstance, &thread.bfthread);
  }

  int get_resolution_count()
  {
    return bf_get_resolution_count(&bfinstance, &thread.bfthread);
  }

  int set_current_resolution(int res)
  {
    return bf_set_current_resolution(&bfinstance, &thread.bfthread, res);
  }

  int get_size_x()
  {
    return bf_get_size_x(&bfinstance, &thread.bfthread);
  }

  int get_size_y()
  {
    return bf_get_size_y(&bfinstance, &thread.bfthread);
  }

  int get_size_z()
  {
    return bf_get_size_z(&bfinstance, &thread.bfthread);
  }

  int get_size_c()
  {
    return bf_get_size_c(&bfinstance, &thread.bfthread);
  }

  int get_size_t()
  {
    return bf_get_size_t(&bfinstance, &thread.bfthread);
  }

  int get_effective_size_c()
  {
    return bf_get_size_c(&bfinstance, &thread.bfthread);
  }

  int get_optimal_tile_width()
  {
    return bf_get_optimal_tile_width(&bfinstance, &thread.bfthread);
  }

  int get_optimal_tile_height()
  {
    return bf_get_optimal_tile_height(&bfinstance, &thread.bfthread);
  }

  int get_pixel_type()
  {
    return bf_get_pixel_type(&bfinstance, &thread.bfthread);
  }

  int get_bytes_per_pixel()
  {
    return bf_get_bytes_per_pixel(&bfinstance, &thread.bfthread);
  }

  int get_rgb_channel_count()
  {
    return bf_get_rgb_channel_count(&bfinstance, &thread.bfthread);
  }

  int get_image_count()
  {
    return bf_get_rgb_channel_count(&bfinstance, &thread.bfthread);
  }

  int is_rgb()
  {
    return bf_is_rgb(&bfinstance, &thread.bfthread);
  }

  int is_interleaved()
  {
    return bf_is_interleaved(&bfinstance, &thread.bfthread);
  }

  int is_little_endian()
  {
    return bf_is_little_endian(&bfinstance, &thread.bfthread);
  }

  int is_false_color()
  {
    return bf_is_false_color(&bfinstance, &thread.bfthread);
  }

  int is_indexed_color()
  {
    return bf_is_indexed_color(&bfinstance, &thread.bfthread);
  }

  std::string get_dimension_order()
  {
    int len = bf_get_dimension_order(&bfinstance, &thread.bfthread);
    if (len < 0)
    {
      return "";
    }
    std::string s;
    char *buffer = communication_buffer();
    s.assign(buffer, len);
    return s;
  }

  int is_order_certain()
  {
    return bf_is_order_certain(&bfinstance, &thread.bfthread);
  }

  int open_bytes(int x, int y, int w, int h)
  {
    return bf_open_bytes(&bfinstance, &thread.bfthread, 0, x, y, w, h);
  }
};

#endif /* BIOFORMATSINSTANCE_H */
