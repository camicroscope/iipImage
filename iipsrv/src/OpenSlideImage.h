/* 
 * File:   OpenSlideImage.h
 * Author: Tony Pan
 *
 * rewrite, loosely based on https://github.com/cytomine/iipsrv
 *
 *
 */


#ifndef OPENSLIDEIMAGE_H
#define	OPENSLIDEIMAGE_H

#include "IIPImage.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <inttypes.h>
#include <iostream>
#include <fstream>

#include "Cache.h"  // for local cache of raw tiles.


extern "C" {
#include "openslide.h"
#include "openslide-features.h"
}


#define OPENSLIDE_TILESIZE 256
#define OPENSLIDE_TILE_CACHE_SIZE 32

/// Image class for OpenSlide supported Images: Inherits from IIPImage. Uses the OpenSlide library.

class OpenSlideImage : public IIPImage {
private:
    openslide_t* osr; //the openslide reader
    /// Tile data buffer pointer

    TileCache *tileCache;
 
    //uint32_t *osr_buf;
    // tdata_t tile_buf;

    std::vector<size_t> numTilesX, numTilesY;
    std::vector<size_t> lastTileXDim, lastTileYDim;
    std::vector<uint32_t> openslide_level_to_use, openslide_downsample_in_level;
 
//    /// get a region from file - downsample_region. color convert
//    void read(const uint32_t zoom, const uint32_t w, const uint32_t h, const uint64_t x, const uint64_t y, void* data);
//
//    /// downsample  - read from file region, downsample
//    void downsample_region(uint32_t *buf, const uint64_t x, const uint64_t y, const int32_t z, const uint32_t w, const uint32_t h);

    /**
     * @brief get cached tile.
     * @detail  return from the local cache a tile.
     *          The tile may be native (directly from file),
     *                previously cached,
     *                or downsampled (recursively called.
     *  note that tilex and tiley can be found by multiplying by 2 raised to power of the difference in levels.
     *  2 versions - direct and recursive.  direct should have slightly lower latency.
     *
     * @param tilex
     * @param tiley
     * @param res
     * @return
     */
    /// check if cache has tile.  if yes, return it.  if not, and is a native layer, getNativeTile, else call halfsampleAndComposeTile
    RawTilePtr getCachedTile(const size_t tilex, const size_t tiley, const uint32_t iipres);

    /// read from file, color convert, store in cache, and return tile.
    RawTilePtr getNativeTile(const size_t tilex, const size_t tiley, const uint32_t iipres);

    /// call 4x (getCachedTile at next res, downsample, compose), store in cache, and return tile.  (causes recursion, stops at native layer or in cache.)
    RawTilePtr halfsampleAndComposeTile(const size_t tilex, const size_t tiley, const uint32_t iipres);



    inline uint32_t bgra2rgb_kernel(const uint32_t& bgra) {
      uint32_t t = bgra;
      t = (t & 0x000000ff)<<24 | (t & 0x0000ff00)<<8 | (t & 0x00ff0000)>>8 | (t & 0xff000000)>>24;
      return t >> 8;
    }

    /**
     * @brief in place bgra to rgb conversion.
     * @details  relies on compiler to generate bswap code.  uses endian conversion to swap bgra to argb, then bit shift by 8 to get rid of a.
     *
     * @param data
     */
    void bgra2rgb(uint8_t* data, const size_t w, const size_t h);

    /// average 2 rows, then average 2 pixels
    inline uint32_t halfsample_kernel_3(const uint8_t *r1, const uint8_t *r2) {
    	uint64_t a = *(reinterpret_cast<const uint64_t*>(r1));
    	uint64_t c = *(reinterpret_cast<const uint64_t*>(r2));
    	uint64_t t1 = (((a ^ c) & 0xFEFEFEFEFEFEFEFE) >> 1) + (a & c);
    	uint32_t t2 = t1 & 0x0000000000FFFFFF;
    	uint32_t t3 = (t1 >> 24) & 0x0000000000FFFFFF;
    	return (((t3 ^ t2) & 0xFEFEFEFE) >> 1) + (t3 & t2);
    }

    /// downsample by 2
    void halfsample_3(const uint8_t* in, const size_t in_w, const size_t in_h,
                     uint8_t* out, size_t& out_w, size_t& out_h);


    void compose(const uint8_t *in, const size_t in_w, const size_t in_h,
    		const size_t& xoffset, const size_t& yoffset,
    		uint8_t* out, const size_t& out_w, const size_t& out_h);

    /// Constructor
    OpenSlideImage() : IIPImage() {
        tile_width = OPENSLIDE_TILESIZE;
        tile_height = OPENSLIDE_TILESIZE;
        osr = NULL;
    };

public:


    /// Constructor
    /** \param path image path
     */
    OpenSlideImage(const std::string& path, TileCache* tile_cache) : IIPImage(path), tileCache(tile_cache) {
        tile_width = OPENSLIDE_TILESIZE;
        tile_height = OPENSLIDE_TILESIZE;
        osr = NULL;
    };

    /// Copy Constructor

    /** \param image IIPImage object
     */
    OpenSlideImage(const IIPImage& image, TileCache* tile_cache) : IIPImage(image), tileCache(tile_cache) {
        osr = NULL;
    };


    /** \param image IIPImage object
     */
    explicit OpenSlideImage(const OpenSlideImage& image) : IIPImage(image),
    		osr(image.osr), tileCache(image.tileCache),
    		numTilesX(image.numTilesX),
    		numTilesY(image.numTilesY),
    		lastTileXDim(image.lastTileXDim),
    		lastTileYDim(image.lastTileYDim),
    		openslide_level_to_use(image.openslide_level_to_use),
    		openslide_downsample_in_level(image.openslide_downsample_in_level)
	{};
    /// Destructor

    virtual ~OpenSlideImage() {
        closeImage();
    };

    /// Overloaded function for opening a TIFF image
    virtual void openImage() throw (file_error);


    /// Overloaded function for loading TIFF image information
    /** \param x horizontal sequence angle
        \param y vertical sequence angle
     */
    virtual void loadImageInfo(int x, int y) throw (file_error);

    /// Overloaded function for closing a TIFF image
    virtual void closeImage();


    /// Overloaded function for getting a particular tile
    /** \param x horizontal sequence angle
        \param y vertical sequence angle
        \param r resolution
        \param l number of quality layers to decode
        \param t tile number
     */
	virtual RawTilePtr getTile(int x, int y, unsigned int r, int l, unsigned int t) throw (file_error);

//    // TCP: turn on region decoding.  problem is that this bypasses tile caching, so overall it's not faster.
//    /// Return whether this image type directly handles region decoding
//    virtual bool regionDecoding(){ return false; };
//	// TCP: support getGegion (internally, openslide already is composing regions.  however, this bypasses tile caching so is slower)
//    /// Overloaded function for returning a region for a given angle and resolution
//    /** Return a RawTile object: Overloaded by child class.
//        \param ha horizontal angle
//        \param va vertical angle
//        \param r resolution
//        \param layers number of quality layers to decode
//        \param x x coordinate
//        \param y y coordinate
//        \param w width of region
//        \param h height of region
//     */
//	virtual RawTile getRegion( int ha, int va, unsigned int r, int layers, int x, int y, unsigned int w, unsigned int h );



//    void readProperties(openslide_t* osr);
};

#endif	/* OPENSLIDEIMAGE_H */

