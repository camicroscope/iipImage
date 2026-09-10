// Tiled Pyramidal Tiff class interface

/*  IIPImage Tiled Pyramidal TIFF Class

    Copyright (C) 2000-2014 Ruven Pillay.

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


#ifndef _TPTIMAGE_H
#define _TPTIMAGE_H


#include "IIPImage.h"
#include <tiff.h>
#include <tiffio.h>




/// Image class for Tiled Pyramidal Images: Inherits from IIPImage. Uses libtiff
class TPTImage : public IIPImage {

 private:

  /// Pointer to the TIFF library struct
  TIFF *tiff;

  /// Tile data buffer pointer
  tdata_t tile_buf;

  /// Tile width for each resolution level
  /** Tile geometry can legitimately differ between levels, so it must be
      recorded per level rather than read once and assumed to apply to all. */
  std::vector<uint32> tile_widths;

  /// Tile height for each resolution level
  std::vector<uint32> tile_heights;

  /// TIFF directory index backing each resolution level
  /** Level N is not necessarily TIFF directory N: whole-slide files routinely
      carry label and macro sub-images interleaved with the real pyramid, and
      those are excluded. */
  std::vector<tdir_t> resolution_ids;

  /// Whether each resolution level is tiled (false = striped)
  /** Striped levels are read strip by strip and assembled into a tile. They
      are genuine pyramid levels in many whole-slide files - reduced levels are
      frequently written striped while the top levels are tiled - so they
      cannot simply be discarded without leaving a hole in the pyramid. */
  std::vector<bool> level_tiled;

  /// Allocated size of tile_buf, so it can be grown when a level needs more
  tsize_t tile_buf_size;


 public:

  /// Constructor
  TPTImage():IIPImage(), tiff( NULL ), tile_buf( NULL ), tile_buf_size( 0 ) {};

  /// Constructor
  /** @param path image path
   */
  TPTImage( const std::string& path ): IIPImage( path ), tiff( NULL ), tile_buf( NULL ),
    tile_buf_size( 0 ) {};

  /// Copy Constructor
  /** @param image IIPImage object
   */
  TPTImage( const TPTImage& image ): IIPImage( image ), tiff( NULL ), tile_buf( NULL ),
    tile_widths( image.tile_widths ), tile_heights( image.tile_heights ),
    resolution_ids( image.resolution_ids ), level_tiled( image.level_tiled ),
    tile_buf_size( 0 ) {};

  /// Assignment Operator
  /** @param TPTImage object
   */
  TPTImage& operator = ( TPTImage image ) {
    if( this != &image ){
      closeImage();
      IIPImage::operator=(image);
      tiff = image.tiff;
      tile_buf = image.tile_buf;
      tile_buf_size = image.tile_buf_size;
      tile_widths = image.tile_widths;
      tile_heights = image.tile_heights;
      resolution_ids = image.resolution_ids;
      level_tiled = image.level_tiled;
    }
    return *this;
  }

  /// Construct from an IIPImage object
  /** @param image IIPImage object
   */
  TPTImage( const IIPImage& image ): IIPImage( image ) {
    tiff = NULL; tile_buf = NULL; tile_buf_size = 0;
  };

  /// Destructor
  ~TPTImage() { closeImage(); };

  /// Overloaded function for opening a TIFF image
  void openImage() throw (file_error);

  /// Overloaded function for loading TIFF image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
   */
  void loadImageInfo( int x, int y ) throw (file_error);

  /// Overloaded function for closing a TIFF image
  void closeImage();

  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l quality layers
      @param t tile number
   */
  virtual RawTilePtr getTile( int x, int y, unsigned int r, int l, unsigned int t ) throw (file_error);

};


#endif
