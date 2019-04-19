// Tile Cache Class

/*  IIP Image Server

    Copyright (C) 2005-2014 Ruven Pillay.
    Based on an LRU cache by Patrick Audley <http://blackcat.ca/lifeline/query.php/tag=LRU_CACHE>
    Copyright (C) 2004 by Patrick Audley

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*
 * Cache Modified by Tony Pan, 2014.
 * templated cache to support caching both tile pointers and image pointers.
 */

#ifndef _CACHE_H
#define _CACHE_H


// Fix missing snprintf in Windows
#if _MSC_VER
#define snprintf _snprintf
#endif

#include <cmath>

// Test for available map types. Try to use an efficient hashed map type if possible
// and define this as HASHMAP, which we can then use elsewhere.
#if defined(HAVE_UNORDERED_MAP)
#include <unordered_map>
#define HASHMAP std::unordered_map

#elif defined(HAVE_TR1_UNORDERED_MAP)
#include <tr1/unordered_map>
#define HASHMAP std::tr1::unordered_map

// Use the gcc hash_map extension if we have it
#elif defined(HAVE_EXT_HASH_MAP)
#include <ext/hash_map>
#define HASHMAP __gnu_cxx::hash_map

/* Explicit template specialization of hash of a string class,
   which just uses the internal char* representation as a wrapper.
   Required for older versions of gcc as hashing on a string is
   not supported.
 */
namespace __gnu_cxx {
  template <>
    struct hash<std::string> {
      size_t operator() (const std::string& x) const {
      // TCP: compare content.
				return  __stl_hash_string(x.c_str());
      }
    };
}

// If no hash type available, just use map
#else
#include <map>
#define HASHMAP std::map

#endif // End of #if defined



// Try to use the gcc high performance memory pool allocator (available in gcc >= 3.4)
#ifdef HAVE_EXT_POOL_ALLOCATOR
#include <ext/pool_allocator.h>
#endif



#include <iostream>
#include <list>
#include <string>
#include "RawTile.h"
#include "IIPImage.h"



/// Cache to store raw tile data
template <typename Key, typename Value>
class Cache {

  protected:

#if defined(HAS_SHARED_PTR)
  typedef std::shared_ptr<Value>  ValuePtr;
#else
  typedef Value*  ValuePtr;
#endif


   /// Max memory size in bytes
   const size_t maxSize;

   /// Current memory running total
   size_t currentSize;

   /// Main cache storage typedef  Don't need to store key in list again.
 #ifdef HAVE_EXT_POOL_ALLOCATOR
 //  typedef std::list < std::pair<std::string, ValuePtr >,
 //    __gnu_cxx::__pool_alloc< std::pair<std::string, ValuePtr > > > ObjectList;
   typedef std::list < ValuePtr ,
     __gnu_cxx::__pool_alloc< ValuePtr > > ObjectList;
 #else
 //  typedef std::list < std::pair<std::string, ValuePtr > > ObjectList;
   typedef std::list < ValuePtr > ObjectList;
 #endif

   /// Main cache list iterator typedef
   typedef typename ObjectList::iterator List_Iter;

   // can store list iterators in map because list iterators are not affected by insert/delete etc to list.

   /// Index typedef
 #ifdef HAVE_EXT_POOL_ALLOCATOR
   typedef HASHMAP < std::string, List_Iter,
     __gnu_cxx::hash< std::string >,
     std::equal_to< std::string >,
     __gnu_cxx::__pool_alloc< std::pair<std::string, List_Iter> >
     > ObjectMap;
 #else
   typedef HASHMAP < std::string, List_Iter > ObjectMap;
 #endif


   /// Main cache storage object
   ObjectList objList;

   /// Main Cache storage index object
   ObjectMap objMap;


   /// Internal touch function
   /** Touches a key in the Cache and makes it the most recently used
    *  @param key to be touched
    *  @return a Map_Iter pointing to the key that was touched.
    */
   typename ObjectMap::iterator _touch( const std::string &key ) {
     typename ObjectMap::iterator miter = objMap.find( key );
     if( miter == objMap.end() ) return miter;
     // Move the found node to the head of the list.
     objList.splice( objList.begin(), objList, miter->second );
     return miter;
   }


   /// Internal remove function
   /**
    *  @param miter Map_Iter that points to the key to remove
    *  @warning miter is no longer usable after being passed to this function.
    */
   void _remove( const typename ObjectMap::iterator &miter ) {
     // Reduce our current size counter
     currentSize -= getRecordSize(miter);
     objList.erase( miter->second );

#if !defined(HAS_SHARED_PTR)
     delete *(miter->second);
#endif

     objMap.erase( miter );

     // internal shared pointer should have reference count decremented automatically.
   }

   /// Interal remove function
   /** @param key to remove */
   void _remove( const std::string &key ) {
     typename ObjectMap::iterator miter = objMap.find( key );
     this->_remove( miter );
   }

   // remember to add objSize.
   size_t getRecordSize( const typename ObjectMap::iterator &miter ) {
     return getRecordSize(miter->first, *(miter->second));
   }

   virtual size_t getRecordSize( const std::string &key, const ValuePtr val ) = 0;

   virtual std::string getIndex( const ValuePtr r ) = 0;

   virtual time_t getTimestamp ( const ValuePtr r ) = 0;

   /// Constructor
   /** @param max Maximum cache size in bytes or count */
   explicit Cache( const size_t max ) :
       maxSize(max),
       currentSize(0),
       objList(),
       objMap()
   {};


  public:



   /// Destructor
   virtual ~Cache() {
     clear();
   }

   void clear() {
#if !defined(HAS_SHARED_PTR)
     for (List_Iter it = objList.begin(); it != objList.end(); ++it) {
       delete *it;
     }
#endif

     objList.clear();
     objMap.clear();

     // shared pointers deleter called automatically.
   }

   /// Insert a tile
   /** @param r Tile to be inserted.  shared pointer copied in.*/
   void insert( const ValuePtr rt ) {

     if( maxSize == 0 ) return;

     if (!rt) return;  // pointer expired.

     // make a local copy of the POINTER
     ValuePtr r(rt);

     std::string key = this->getIndex( r );

     // Touch the key, if it exists
     typename ObjectMap::iterator miter = this->_touch( key );

     // Check whether this tile exists in our cache
     if( miter != objMap.end() ){
       // Check the timestamp and delete if necessary
       if( getTimestamp(*(miter->second)) < getTimestamp(r) ){
         this->_remove( miter );  // old RawTile will be destroyed when no one else is using it.
       }
       // If this index already exists and it is up to date, do nothing
       else return;  // r will be destroyed properly, leaving miter.
     }

     // Update our total current size variable BEFORE moving it. Use the string::capacity function
     // rather than length() as std::string can allocate slightly more than necessary
     // The +1 is for the terminating null byte
     currentSize += getRecordSize(key, r);

     // Store the key if it doesn't already exist in our cache
     // Ok, do the actual insert at the head of the list
     objList.push_front( r );

     // And store this in our map
     List_Iter liter = objList.begin();
     objMap[ key ] = liter;



     // Check to see if we need to remove an element due to exceeding max_size
     while( currentSize > maxSize ) {
       // Remove the last element
       liter = objList.end();
       --liter;
       key = this->getIndex( *liter );
       this->_remove( key );
     }

   }


   /// Return the number of tiles in the cache
   unsigned int getNumElements() { return objList.size(); }


   /// Get a tile from the cache
   /**
    *  @param f filename
    *  @param r resolution number
    *  @param t tile number
    *  @param h horizontal sequence number
    *  @param v vertical sequence number
    *  @param c compression type
    *  @param q compression quality
    *  @return pointer to data or NULL on error
    */
   ValuePtr getObject( const std::string &key ) {

     if( maxSize == 0 ) return NULL;

     typename ObjectMap::iterator miter = objMap.find( key );
     if( miter == objMap.end() ) return ValuePtr();

     this->_touch( key );

     return ValuePtr(*(miter->second));
   }


   void evict( const ValuePtr rt ) {
     std::string key = this->getIndex( rt );
     this->_remove( key );
   }

   /// Return the amount of cache used, in units defined by the subclass.
   virtual float getMemorySize() = 0;

};

class TileCache : public Cache<std::string, RawTile> {

  protected:

   typedef Cache<std::string, RawTile> BaseCacheType;

  /// Main cache storage typedef  Don't need to store key in list again.
  typedef BaseCacheType::ObjectList ObjectList;
  /// Main cache list iterator typedef
  typedef BaseCacheType::List_Iter List_Iter;
  /// Index typedef
  typedef BaseCacheType::ObjectMap ObjectMap;


  /// Basic object storage size
  const int objSize;

  // can store list iterators in map because list iterators are not affected by insert/delete etc to list.

  // remember to add objSize.
  virtual size_t getRecordSize( const std::string &key, const RawTilePtr val ) {
    return ( val->dataLength +
            ( val->filename.capacity() + key.capacity() ) * sizeof(char) +
        this->objSize );
  }


  virtual std::string getIndex( const RawTilePtr r ) {
    return TileCache::getIndex( r->filename, r->resolution, r->tileNum,
                     r->hSequence, r->vSequence, r->compressionType, r->quality );
  }

  virtual time_t getTimestamp ( const RawTilePtr r ) {
    return r->timestamp;
  }


 public:

  /// Constructor
  /** @param max Maximum cache size in MBs */
  explicit TileCache( float max ) :
   objSize(sizeof( RawTile ) +
           sizeof( RawTilePtr ) +
           sizeof( List_Iter ) +
           sizeof( std::pair<std::string, List_Iter> ) +
           sizeof(char) * 64), BaseCacheType(ceil(max * 1024.0 * 1024.0)) {};
  // 64 chars added at the end represents an average string length


  /// Destructor
  virtual ~TileCache() {}


  /// Create a hash index
  /** 
   *  @param f filename
   *  @param r resolution number
   *  @param t tile number
   *  @param h horizontal sequence number
   *  @param v vertical sequence number
   *  @param c compression type
   *  @param q compression quality
   *  @return string
   */
  static std::string getIndex( std::string f, int r, int t, int h, int v, CompressionType c, int q ) {
    char tmp[1024];
    snprintf( tmp, 1024, "%s:%d:%d:%d:%d:%d:%d", f.c_str(), r, t, h, v, c, q );
    return std::string( tmp );
  }

  virtual float getMemorySize() {
    return currentSize / (1024.0 * 1024.0);
  }


};




class ImageCache : public Cache<std::string, IIPImage> {

  protected:

   typedef Cache<std::string, IIPImage> BaseCacheType;


  /// Main cache storage typedef  Don't need to store key in list again.
  typedef BaseCacheType::ObjectList ObjectList;
  /// Main cache list iterator typedef
  typedef BaseCacheType::List_Iter List_Iter;
  /// Index typedef
  typedef BaseCacheType::ObjectMap ObjectMap;


  // can store list iterators in map because list iterators are not affected by insert/delete etc to list.

  // remember to add objSize.
  virtual size_t getRecordSize( const std::string &key, const IIPImagePtr val ) {
    return 1;
  }

  virtual std::string getIndex( const IIPImagePtr r ) {
    return r->getImagePath();
  }


  virtual time_t getTimestamp ( const IIPImagePtr r ) {
    return r->timestamp;
  }


 public:

  /// Constructor
  /** @param max Maximum cache size in number of IIPImage Objects */
  explicit ImageCache( int max ) : BaseCacheType(max) {};
  // 64 chars added at the end represents an average string length


  /// Destructor
  virtual ~ImageCache() {}

  // get number of image objects.
  virtual float getMemorySize() {
    return currentSize;
  }

};



#endif
