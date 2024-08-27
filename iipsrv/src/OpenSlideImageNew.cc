/*  IIPImage Server: OpenSlide handler

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

//#define DEBUG 1

#include "OpenSlideImageNew.h"
#include "Logger.h"
#include <sstream>
#include <cmath>
#ifdef DEBUG
#include "Timer.h"
#endif


using namespace std;


// Reference our logging object
extern Logger logfile;


// Handle info, warning and error messages from OpenSlide
static void error_callback( const char* msg, void* ){
  stringstream ss;
  ss << "OpenSlide error :: " << msg;
  throw file_error( ss.str() );
}

#ifdef DEBUG
static void warning_callback( const char* msg, void* ){
  if( IIPImage::logging ) logfile << "OpenSlide warning :: " << msg << endl;
}
static void info_callback( const char* msg, void* ){
  if( IIPImage::logging ) logfile << "OpenSlide info :: " << msg;
}
#endif



void OpenSlideImage::openImage()
{
  string filename = getFileName( currentX, currentY );

  // Update our timestamp
  updateTimestamp( filename );

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif
  _osr = openslide_open(filename.c_str())
  colourspace = sRGB;
  // Error if fail; "If the file is not recognized by OpenSlide, NULL, If the file is recognized but an error occurred, an OpenSlide object in error state.""
  if (_osr == NULL){
    throw file_error( "OpenSlide :: Unable to open '" + filename + "'" );
  } else if (openslide_get_error(_osr) != NULL){
    throw file_error( "OpenSlide :: Unable to open '" + filename + "'" );
  }

#ifdef DEBUG
  logfile << "OpenSlide :: openslide_open() :: " << "Opened file" << endl;
#endif

  // Load our metadata if not already loaded
  if( ! _imageLoaded ) loadImageInfo( currentX, currentY );

#ifdef DEBUG
  logfile << "OpenSlide :: openslide_open() :: " << timer.getTime() << " microseconds" << endl;
#endif

}



void OpenSlideImage::closeImage()
{
#ifdef DEBUG
  Timer timer;
  timer.start();
#endif
  openslide_close(_osr);
#ifdef DEBUG
  logfile << "OpenSlide :: openslide_close() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



void OpenSlideImage::loadImageInfo( int seq, int ang )
{

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  _imageLoaded = true;
  numLevels = openslide_get_level_count(_osr);


  // Empty any existing list of available resolution sizes
  image_widths.clear();
  image_heights.clear();

  // Save first resolution level
  int64_t w, h;
  openslide_get_level0_dimensions(_osr, &w, &h);
  image_widths.push_back(w);
  image_heights.push_back(h);

#ifdef DEBUG
  logfile << "OpenSlide :: Levels: " << numLevels << endl;
  logfile << "OpenSlide :: Resolution : " << w << "x" << h << endl;
#endif

  // Get all image dimensions
  for (int i = 1; i < level_count; i++) {
    int64_t level_width, level_height;
    openslide_get_level_dimensions(_osr, i, &level_width, &level_height);
    image_widths.push_back(level_width);
    image_heights.push_back(level_height);
  }


  // If we don't have enough resolutions to fit a whole image into a single tile
  // we need to generate them ourselves virtually.
  unsigned int n = 1;
  w = image_widths[0];
  h = image_heights[0];
  while( (w>tile_widths[0]) || (h>tile_heights[0]) ){
    n++;
    w = floor( w/2.0 );
    h = floor( h/2.0 );
    if( n > numLevels ){
      image_widths.push_back(w);
      image_heights.push_back(h);
    }
  }

  if( n > numLevels ){
#ifdef DEBUG
    logfile << "OpenSlide :: Warning! Insufficient resolution levels. Will generate "
	    << n-numLevels << " extra levels dynamically -" << endl
	    << "OpenSlide :: However, you are advised to regenerate the file with at least " << n << " levels" << endl;
#endif
    virtual_levels = n-numLevels;
  }
  numLevels = n;

  // Indicate that our metadata has been read
  _imageLoaded = true;


#ifdef DEBUG
  logfile << "OpenSlide :: loadImageInfo() :: " << timer.getTime() << " microseconds" << endl;
#endif
}



RawTile OpenSlideImage::getTile(int seq, int ang, unsigned int res, int layers, unsigned int tile){

#ifdef DEBUG
  Timer timer;
  timer.start();
#endif

  if (res > (numResolutions-1)) {
    ostringstream tile_no;
    tile_no << "OpenSlide :: Asked for non-existant resolution: " << res;
    throw file_error(tile_no.str());
    return 0;
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getTile() :: res=" << res << " tile= " << tile  << " is_zoom= " << osi_level << endl;

#endif
  int vipsres = getNativeResolution(res);
  unsigned int tw = tile_widths[0];
  unsigned int th = tile_heights[0];
  
  // Get the width and height for last row and column tiles
  unsigned int rem_x = image_widths[vipsres] % tile_widths[0];
  unsigned int rem_y = image_heights[vipsres] % tile_heights[0];

  // Calculate the number of tiles in each direction
  unsigned int ntlx = (image_widths[vipsres] / tile_widths[0]) + (rem_x == 0 ? 0 : 1);
  unsigned int ntly = (image_heights[vipsres] / tile_heights[0]) + (rem_y == 0 ? 0 : 1);

  // Check whether requested tile exists
  if( tile >= ntlx*ntly ){
    ostringstream tile_no;
    tile_no << "OpenSlide :: Asked for non-existent tile: " << tile;
    throw file_error( tile_no.str() );
  }

  // Alter the tile size if it's in the last column
  if( ( tile % ntlx == ntlx - 1 ) && ( rem_x != 0 ) ) {
    tw = rem_x;
  }

  // Alter the tile size if it's in the bottom row
  if( ( tile / ntlx == ntly - 1 ) && rem_y != 0 ) {
    th = rem_y;
  }

  // Calculate the pixel offsets for this tile
  int xoffset = (tile % ntlx) * tile_widths[0];
  int yoffset = (unsigned int) floor((double)(tile/ntlx)) * tile_heights[0];

  // TODO this is bits per channel. I think it's 8, but coult be wrong.
  int obpc = 8;
  // Create our Rawtile object and initialize with data
  RawTile rawtile( tile, res, seq, ang, tw, th, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();
  // Process the tile
  process( res, layers, xoffset, yoffset, tw, th, rawtile.data );

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getTile() :: total " << timer.getTime() << " microseconds" << endl << flush;
  logfile << "TILE RENDERED" << std::endl;
#endif
  return ttt;  // return cached instance.  TileManager's job to copy it..
}



// Get an entire region and not just a tile
RawTile OpenSlideImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ){

  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;
  
#ifdef DEBUG
  Timer timer;
  timer.start();
#endif
  RawTile rawtile( 0, res, ha, va, w, h, channels, obpc );
  rawtile.filename = getImagePath();
  rawtile.timestamp = timestamp;
  rawtile.allocate();

  process( res, layers, x, y, w, h, rawtile.data );

#ifdef DEBUG
  logfile << "OpenSlide :: getRegion() :: " << timer.getTime() << " microseconds" << endl;
#endif

  return rawtile;
}



// Main processing function
void OpenSlideImage::process( unsigned int res, int layers, int xoffset, int yoffset, unsigned int tw, unsigned int th, void *d )
{
  // open if not opened
  if( !_image ) openImage();

  // Scale up our output bit depth to the nearest factor of 8
  unsigned int obpc = bpc;
  if( bpc <= 16 && bpc > 8 ) obpc = 16;
  else if( bpc <= 8 ) obpc = 8;

  unsigned int factor = 1;                  // Downsampling factor - set it to default value
  int vipsres = (numLevels - 1) - res; // Reverse resolution number

  // Calculate number of extra resolutions needed that have not been encoded in the image
  if( res < virtual_levels ){
    factor = 2 * (virtual_levels - res);
    xoffset *= factor;
    yoffset *= factor;
    tw *= factor;
    th *= factor;
    // Set our resolution level back to the smallest original resolution
    vipsres = numLevels - 1 - virtual_levels;
#ifdef DEBUG
  logfile << "OpenSlide :: using smallest existing resolution " << virtual_levels << endl;
#endif
  }

  // Set the number of layers to half of the number of detected layers if we have not set the
  // layers parameter manually. If layers is set to less than 0, use all layers.
  if( layers < 0 ) layers = quality_layers;
  else if( layers == 0 ) layers = ceil( quality_layers/2.0 );

  // Also make sure we have at least 1 layer
  if( layers < 1 ) layers = 1;


  // Set number of quality layer and resolution
  opj_dparameters_t params;
  params.cp_layer = layers;
  params.cp_reduce = vipsres;


  // Set resolution - hack for OpenSlide up to 2.2.0
  for( OPJ_UINT32 i = 0; i < _image->numcomps; i++ ){
    _image->comps[i].factor = vipsres;
  }

  // Image location and size at requested resolution
  unsigned int x0 = xoffset << vipsres;
  unsigned int y0 = yoffset << vipsres;
  unsigned int w0 = (xoffset + tw) << vipsres;
  unsigned int h0 = (yoffset + th) << vipsres;

#ifdef DEBUG
  logfile << "OpenSlide :: decoding " << layers << " quality layers" << endl;
  logfile << "OpenSlide :: requested region at requested resolution: position: "
	  << xoffset << "x" << yoffset << ". size: " << tw << "x" << th << endl;
  logfile << "OpenSlide :: region size mapped to full resolution: " << (tw<<vipsres) << "x" << (th<<vipsres) << endl;
#endif


  // Define our decoding region
  if( !opj_set_decode_area( _codec, _image, x0, y0, w0, h0 ) ){
    throw file_error( "OpenSlide :: process() :: opj_set_decode_area() failed" );
  }

  // Perform decoding
  if( !opj_decode( _codec, _stream, _image ) ){
    throw file_error( "OpenSlide :: process() :: opj_decode() failed" );
  }

  // Extract any ICC profile - unfortunately, can only get ICC profile after decoding
  int icc_length = _image->icc_profile_len;
  const char* icc = (const char*) _image->icc_profile_buf;
  if( icc_length > 0 ) metadata["icc"] = string( icc, icc_length );
#ifdef DEBUG
  if( icc_length > 0 ){
    logfile << "OpenSlide :: ICC profile detected with size " << icc_length << endl;
  }
#endif

  // Copy our decoded data by looping over all pixels
  size_t n = 0;

  for( unsigned int j=0; j < th; j += factor ){
    for( unsigned int i = 0; i < tw; i += factor ){
      size_t index = tw*j + i;
      for( unsigned int k = 0; k < channels; k++ ){
        // Handle 16 and 8 bit data:
	// OpenSlide's output data is 32 bit unsigned int, so just mask of the bottom 2 bytes
	// for 16 bit output or bottom 1 byte for 8 bit
	if( obpc == 16 ){
	  ((unsigned short*)d)[n++] =(  (_image->comps[k].data[index]) & 0x0000ffff );
	}
	// Binary (bi-level) images need to be scaled up to 8 bits
	else if( bpc == 1 ){
	  ((unsigned char*)d)[n++] = ((_image->comps[k].data[index]) & 0x000000f) * 255;
	}
	else{
	  ((unsigned char*)d)[n++] = (_image->comps[k].data[index]) & 0x000000ff;
	}
      }
    }
  }

  // We need to close the image here in case we try to use the OpenSlide
  // stream or image structures multiple times in the same request pipeline
  closeImage();

}
