/*  IIP Server: OpenSlide handler

    Copyright (C) 2024 Ryan Birmingham.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef _OpenSlideImage_H
#define _OpenSlideImage_H

#include "IIPImage.h"

extern "C" {
#include "openslide.h"
#include "openslide-features.h"
}


#define TILESIZE 256


/// Image class for OpenSlide Images.
/// Inherits from IIPImage. Uses the OpenSlide library.
class OpenSlideImage : public IIPImage {

 private:

  openslide_t* _osr;
  opj_image_t*  _image;
  bool _imageLoaded; 


  /// Main processing function
  /** @param r resolution
      @param l number of quality levels to decode
      @param x x coordinate
      @param y y coordinate
      @param w width of region
      @param h height of region
      @param d buffer to fill
   */
  void process( unsigned int r, int l, int x, int y, unsigned int w, unsigned int h, void* d );



 public:

  /// Constructor
  OpenSlideImage() : IIPImage(){
    _osr = NULL; _image = NULL; _imageLoaded=false;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Constructor
  /** @param path image path
   */
  OpenSlideImage( const std::string& path)  : IIPImage(path){
    _osr = NULL; _image = NULL; _imageLoaded=false;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Copy Constructor
  /** @param image OpenSlide object
   */
  OpenSlideImage( const OpenSlideImage& image ): IIPImage( image ) {};


  /// Copy Constructor
  /** @param image IIPImage object
   */
  OpenSlideImage( const IIPImage& image ) : IIPImage(image){
    _osr = NULL; _image = NULL; _imageLoaded=false;
    tile_widths.push_back(TILESIZE); tile_heights.push_back(TILESIZE);
  };


  /// Destructor
  ~OpenSlideImage(){ closeImage(); };


  /// Overloaded function for opening a TIFF image
  void openImage();


  /// Overloaded function for loading JP2 image information
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
  */
  void loadImageInfo( int x, int y );


  /// Overloaded function for closing a JP2 image
  void closeImage();


  /// Return whether this image type directly handles region decoding
  bool regionDecoding(){ return true; };


  /// Overloaded function for getting a particular tile
  /** @param x horizontal sequence angle
      @param y vertical sequence angle
      @param r resolution
      @param l number of quality layers to decode
      @param t tile number
   */
  RawTile getTile( int x, int y, unsigned int r, int l, unsigned int t );


  /// Overloaded function for returning a region from image
  /**
    @param ha       horizontal angle
    @param va       vertical angle
    @param res      resolution
    @param layers   number of quality layers to decode
    @param x        x coordinate
    @param y        y coordinate
    @param w        width of region
    @param h        height of region
    @return         a RawTile object
  */
  RawTile getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h );

};

#endif
