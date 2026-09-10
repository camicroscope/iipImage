// Member functions for TPTImage.h

/*  IIP Server: Tiled Pyramidal TIFF handler

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


#include "TPTImage.h"
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstring>


using namespace std;


// Reference our logging object
extern std::ofstream logfile;


void TPTImage::openImage() throw (file_error)
{

  // Insist that the tiff and tile_buf be non-NULL
  if( tiff || tile_buf ){
    throw file_error( "TPT::openImage: tiff or tile_buf is not NULL" );
  }

  string filename = getFileName( currentX, currentY );
  logfile << "TPT :: openImage() :: filename: " << filename << endl << flush;

  // Update our timestamp
  updateTimestamp( filename );

  // Try to open and allocate a buffer
  if( ( tiff = TIFFOpen( filename.c_str(), "r" ) ) == NULL ){
    throw file_error( "tiff open failed for: " + filename );
  }

  // Load our metadata if not already loaded.
  // The level tables must be populated too: an object copy-constructed from a
  // cached IIPImage carries bpc across but not the tables, and getTile()
  // indexes them on every request.
  if( bpc == 0 || tile_widths.empty() ) loadImageInfo( currentX, currentY );

  // Insist on a tiled image. This has to be OR, not AND: an image reporting a
  // tile width but no tile length (or the reverse) is not usable either, and
  // used to pass straight through to the division in getTile().
  if( (tile_width == 0) || (tile_height == 0) ){
    throw file_error( "TIFF image is not tiled: " + getFileName( currentX, currentY ) );
  }

  isSet = true;

}


void TPTImage::loadImageInfo( int seq, int ang ) throw(file_error)
{
  tdir_t current_dir;
  // Initialise: TIFFGetField() leaves its output argument untouched when the
  // tag is absent, so an unset variable here silently keeps stack garbage.
  uint16 colour = 0, samplesperpixel = 1, bitspersample = 8, sampleformat = 1;
  double sminvaluearr[4] = {0.0}, smaxvaluearr[4] = {0.0};
  double *sminvalue = NULL, *smaxvalue = NULL;
  string filename;
  char *tmp = NULL;
  // TIFFTAG_XMLPACKET returns its length through a uint32, not an int
  uint32 count = 0;

  currentX = seq;
  currentY = ang;

  current_dir = TIFFCurrentDirectory( tiff );

  // Enumerate the TIFF directories and pick out the ones that form the
  // resolution pyramid.
  //
  // Two things have to be separated here, and the original code did neither.
  //
  // 1. Tiled vs striped. A directory for which libtiff does not report
  //    TileWidth/TileLength is striped. The original code read the tile
  //    geometry with an unchecked TIFFGetField(), so on a striped directory
  //    tw/th kept stack garbage - usually zero - and the modulo below raised
  //    SIGFPE, aborting the whole FastCGI worker. Striped directories are NOT
  //    discarded though: whole-slide writers routinely emit the top levels
  //    tiled and the reduced levels striped, so dropping them punches a hole
  //    in the pyramid. They are recorded here and assembled from strips in
  //    getTile() instead.
  //
  // 2. Pyramid levels vs ancillary images. The same files also carry label and
  //    macro sub-images, which are not part of the pyramid at all. Those are
  //    identified by aspect ratio: a genuine reduced level keeps the aspect of
  //    the full-resolution image, while a label or macro does not. Counting
  //    them as levels made the resolution list non-monotonic and handed the
  //    DeepZoom layer nonsense dimensions.
  //
  // These vectors were also never cleared before being appended to, so a
  // second call to this function (getTile() re-invokes it whenever the
  // sequence or angle changes) grew them without bound and desynchronised
  // them from numResolutions.
  image_widths.clear();
  image_heights.clear();
  tile_widths.clear();
  tile_heights.clear();
  resolution_ids.clear();
  level_tiled.clear();

  if( !TIFFSetDirectory( tiff, 0 ) ){
    throw file_error( "TPTImage :: TIFFSetDirectory( 0 ) failed for " + getFileName( seq, ang ) );
  }

  // Aspect ratio of the base image, and the previous accepted level, so each
  // candidate can be tested against both.
  double base_aspect = 0.0;
  uint32 prev_w = 0, prev_h = 0;

  tdir_t dir = 0;
  do {
    uint32 dw = 0, dh = 0, dtw = 0, dth = 0;
    bool tiled = false;

    if( ( TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH,  &dw ) == 1 ) &&
        ( TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &dh ) == 1 ) &&
        ( dw > 0 ) && ( dh > 0 ) ){

      tiled = ( TIFFGetField( tiff, TIFFTAG_TILEWIDTH,  &dtw ) == 1 ) &&
              ( TIFFGetField( tiff, TIFFTAG_TILELENGTH, &dth ) == 1 ) &&
              ( dtw > 0 ) && ( dth > 0 );
      if( !tiled ){ dtw = 0; dth = 0; }

      bool accept = false;

      if( image_widths.empty() ){
        // First usable directory is the base of the pyramid
        accept = true;
        base_aspect = (double)dw / (double)dh;
      }
      else if( dw < prev_w && dh < prev_h ){
        // A reduced level must keep the aspect ratio of the base image.
        // 5% covers the rounding a halving chain accumulates; a label or
        // macro sub-image is far outside it.
        double aspect = (double)dw / (double)dh;
        if( fabs( aspect - base_aspect ) <= 0.05 * base_aspect ) accept = true;
      }

      if( accept ){
        image_widths.push_back( dw );
        image_heights.push_back( dh );
        tile_widths.push_back( dtw );
        tile_heights.push_back( dth );
        resolution_ids.push_back( dir );
        level_tiled.push_back( tiled );
        prev_w = dw;
        prev_h = dh;
      }
    }
    dir++;
  } while( TIFFReadDirectory( tiff ) );

  numResolutions = (unsigned int) image_widths.size();

  if( numResolutions == 0 ){
    throw file_error( "TPTImage :: no usable resolution levels found in " + getFileName( seq, ang ) );
  }

  // Every level has to advertise the same tile size, because TileManager::crop()
  // strides edge tiles by the image-level tile width. Striped levels have no
  // tile geometry of their own, so give them the size the tiled levels use -
  // or a sane default if this file is striped throughout.
  uint32 virtual_tw = 0, virtual_th = 0;
  for( size_t i = 0; i < tile_widths.size(); i++ ){
    if( level_tiled[i] ){ virtual_tw = tile_widths[i]; virtual_th = tile_heights[i]; break; }
  }
  if( virtual_tw == 0 || virtual_th == 0 ){ virtual_tw = 256; virtual_th = 256; }

  for( size_t i = 0; i < tile_widths.size(); i++ ){
    if( !level_tiled[i] || tile_widths[i] == 0 || tile_heights[i] == 0 ){
      tile_widths[i]  = virtual_tw;
      tile_heights[i] = virtual_th;
    }
  }

  // Read the image metadata from the base level rather than from whichever
  // directory happened to be current, which may be a label or macro image
  // with unrelated depth and colour.
  if( !TIFFSetDirectory( tiff, resolution_ids[0] ) ){
    throw file_error( "TPTImage :: TIFFSetDirectory() failed for " + getFileName( seq, ang ) );
  }

  tile_width  = virtual_tw;
  tile_height = virtual_th;

  TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel );
  TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
  TIFFGetField( tiff, TIFFTAG_SAMPLEFORMAT, &sampleformat );

  // We have to do this conversion explicitly to avoid problems on Mac OS X
  channels = (unsigned int) samplesperpixel;
  bpc = (unsigned int) bitspersample;
  sampleType = (sampleformat==3) ? FLOATINGPOINT : FIXEDPOINT;

  // Reset the TIFF directory
  TIFFSetDirectory( tiff, current_dir );

  // Handle various colour spaces
  if( colour == PHOTOMETRIC_CIELAB ) colourspace = CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ) colourspace = GREYSCALE;
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. These are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colourspace = sRGB;
    channels = 3;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colourspace = sRGB;
  }
  else colourspace = sRGB;

  // Get the max and min values for our data type - required for floats
  // This are usually single values per image, but can also be per channel
  // in libtiff > 4.0.2 via http://www.asmail.be/msg0055458208.html

#ifdef TIFFTAG_PERSAMPLE
  if( channels > 1 ){
    TIFFSetField(tiff, TIFFTAG_PERSAMPLE, PERSAMPLE_MULTI);
    TIFFGetFieldDefaulted( tiff, TIFFTAG_SMINSAMPLEVALUE, &sminvalue );
    TIFFGetFieldDefaulted( tiff, TIFFTAG_SMAXSAMPLEVALUE, &smaxvalue );
    TIFFSetField(tiff, TIFFTAG_PERSAMPLE, PERSAMPLE_MERGED);
    if (!sminvalue) sminvalue = sminvaluearr;
    if (!smaxvalue) smaxvalue = smaxvaluearr;
  }
  else{
#endif
    sminvalue = sminvaluearr;
    smaxvalue = smaxvaluearr;
    TIFFGetFieldDefaulted( tiff, TIFFTAG_SMINSAMPLEVALUE, sminvalue );
    TIFFGetFieldDefaulted( tiff, TIFFTAG_SMAXSAMPLEVALUE, smaxvalue );
#ifdef TIFFTAG_PERSAMPLE
  }
#endif

  // Clear our arrays
  min.clear();
  max.clear();

  for( unsigned int i=0; i<channels; i++ ){
    if( (!sminvalue) == smaxvalue[i] ){
      // Set default values if values not included in header
      if( bpc == 8 ) smaxvalue[i] = 255.0;
      else if( bpc == 16 ) smaxvalue[i] = 65535.0;
      else if( bpc == 32 && sampleType == FIXEDPOINT ) smaxvalue[i] = 4294967295.0;
    }
    min.push_back( (float)sminvalue[i] );
    max.push_back( (float)smaxvalue[i] );
  }

  // Also get some basic metadata
  if( TIFFGetField( tiff, TIFFTAG_ARTIST, &tmp ) ) metadata["author"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_COPYRIGHT, &tmp ) ) metadata["copyright"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_DATETIME, &tmp ) ) metadata["create-dtm"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_IMAGEDESCRIPTION, &tmp ) ) metadata["subject"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_SOFTWARE, &tmp ) ) metadata["app-name"] = tmp;
  if( TIFFGetField( tiff, TIFFTAG_XMLPACKET, &count, &tmp ) ) metadata["xmp"] = string(tmp,count);

}


void TPTImage::closeImage()
{
  if( tiff != NULL ){
    TIFFClose( tiff );
    tiff = NULL;
  }
  if( tile_buf != NULL ){
    _TIFFfree( tile_buf );
    tile_buf = NULL;
  }
  tile_buf_size = 0;
}


RawTilePtr TPTImage::getTile( int seq, int ang, unsigned int res, int layers, unsigned int tile ) throw (file_error)
{
  uint32 im_width = 0, im_height = 0, tw = 0, th = 0, ntlx = 0, ntly = 0;
  uint32 rem_x = 0, rem_y = 0;
  uint16 colour = 0;
  string filename;


  // Check the resolution exists
  if( res >= numResolutions ){
    ostringstream error;
    error << "TPTImage :: Asked for non-existant resolution: " << res;
    throw file_error( error.str() );
  }


  // If we are currently working on a different sequence number, then
  //  close and reload the image.
  if( (currentX != seq) || (currentY != ang) ){
    closeImage();
  }


  // Open the TIFF if it's not already open
  if( !tiff ){
    filename = getFileName( seq, ang );
    if( ( tiff = TIFFOpen( filename.c_str(), "r" ) ) == NULL ){
      throw file_error( "tiff open failed for:" + filename );
    }
  }


  // Reload our image information in case the tile size etc is different
  if( (currentX != seq) || (currentY != ang) || tile_widths.empty() ){
    loadImageInfo( seq, ang );
  }


  // The first resolution is the highest, so we need to invert 
  //  the resolution - can avoid this if we store our images with
  //  the smallest image first. 
  int vipsres = ( numResolutions - 1 ) - res;
  

  // Resolution levels are not necessarily TIFF directory numbers: only tiled
  // directories become levels, so map through resolution_ids.
  if( vipsres < 0 || (size_t)vipsres >= resolution_ids.size() ){
    ostringstream error;
    error << "TPTImage :: resolution " << res << " has no backing TIFF directory in "
          << getFileName( seq, ang );
    throw file_error( error.str() );
  }

  // Change to the right directory for the resolution
  if( !TIFFSetDirectory( tiff, resolution_ids[vipsres] ) ) {
    throw file_error( "TIFFSetDirectory failed" );
  }


  // Tile geometry comes from the tables built in loadImageInfo(). For a tiled
  // level this is the file's own tile size; for a striped level it is the
  // virtual tile size the whole image advertises, and the tile is assembled
  // from strips further down.
  tw = tile_widths[vipsres];
  th = tile_heights[vipsres];

  // Get the size of the current image and the colourspace.
  TIFFGetField( tiff, TIFFTAG_IMAGEWIDTH, &im_width );
  TIFFGetField( tiff, TIFFTAG_IMAGELENGTH, &im_height );
  TIFFGetField( tiff, TIFFTAG_PHOTOMETRIC, &colour );
//   TIFFGetField( tiff, TIFFTAG_SAMPLESPERPIXEL, &channels );
//   TIFFGetField( tiff, TIFFTAG_BITSPERSAMPLE, &bpc );


  // Every value below is used as a divisor. Refuse the request rather than
  // dividing by zero: an unguarded modulo here used to raise SIGFPE and abort
  // the entire FastCGI worker, taking every other in-flight request with it.
  if( tw == 0 || th == 0 || im_width == 0 || im_height == 0 ){
    ostringstream error;
    error << "TPTImage :: refusing tile " << tile << " at resolution " << res
          << " (TIFF directory " << resolution_ids[vipsres] << "): invalid geometry"
          << " tile=" << tw << "x" << th << " image=" << im_width << "x" << im_height
          << " in " << getFileName( seq, ang );
    throw file_error( error.str() );
  }


  // Get the width and height for last row and column tiles
  rem_x = im_width % tw;
  rem_y = im_height % th;


  // Calculate the number of tiles in each direction
  ntlx = (im_width / tw) + (rem_x == 0 ? 0 : 1);
  ntly = (im_height / th) + (rem_y == 0 ? 0 : 1);

  // ntlx is a divisor in the two tests below
  if( ntlx == 0 || ntly == 0 ){
    ostringstream error;
    error << "TPTImage :: resolution " << res << " yields no tiles ("
          << ntlx << "x" << ntly << ") in " << getFileName( seq, ang );
    throw file_error( error.str() );
  }


  // Check that a valid tile number was given. This is checked against the grid
  // we just computed rather than TIFFNumberOfTiles(), which reports zero for a
  // striped directory and would reject every tile of a striped level.
  if( tile >= ntlx * ntly ){
    ostringstream tile_no;
    tile_no << "TPTImage :: Asked for non-existant tile: " << tile
            << " (resolution " << res << " has " << ntlx << "x" << ntly << " tiles)";
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


  // Handle various colour spaces
  if( colour == PHOTOMETRIC_CIELAB ) colourspace = CIELAB;
  else if( colour == PHOTOMETRIC_MINISBLACK ) colourspace = GREYSCALE;
  else if( colour == PHOTOMETRIC_PALETTE ){
    // Watch out for colourmapped images. There are stored as 1 sample per pixel,
    // but are decoded to 3 channels by libtiff, so declare them as sRGB
    colourspace = GREYSCALE;
    channels = 1;
  }
  else if( colour == PHOTOMETRIC_YCBCR ){
    // JPEG encoded tiles can be subsampled YCbCr encoded. Ask to decode these to RGB
    TIFFSetField( tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
    colourspace = sRGB;
  }
  else colourspace = sRGB;


  // Consumers (TileManager::crop() in particular) stride edge tiles by the
  // image-level tile size, so the buffer must always be a full padded tile.
  size_t padded_bytes = (size_t)tile_width * (size_t)tile_height
                      * (size_t)channels * (size_t)(bpc/8);
  size_t needed = padded_bytes;
  if( level_tiled[vipsres] ){
    tsize_t ts = TIFFTileSize( tiff );
    if( ts > 0 && (size_t)ts > needed ) needed = (size_t)ts;
  }

  // Allocate memory for our tile, growing the buffer if a level needs more
  // than the one we allocated for previously.
  if( !tile_buf || tile_buf_size < (tsize_t)needed ){
    if( tile_buf ) _TIFFfree( tile_buf );
    if( ( tile_buf = _TIFFmalloc( (tsize_t)needed ) ) == NULL ){
      tile_buf_size = 0;
      throw file_error( "tiff malloc tile failed" );
    }
    tile_buf_size = (tsize_t)needed;
  }

  int length = 0;

  if( level_tiled[vipsres] ){

    // Decode and read the tile
    length = TIFFReadEncodedTile( tiff, (ttile_t) tile,
				  tile_buf, (tsize_t) - 1 );
    if( length == -1 ) {
      throw file_error( "TIFFReadEncodedTile failed for " + getFileName( seq, ang ) );
    }
  }
  else {

    // Striped level: libtiff has no tile to hand us, so assemble one from the
    // strips covering this tile's rows. Reduced levels of whole-slide images
    // are frequently written striped even when the top levels are tiled, and
    // they are needed - dropping them leaves a hole at the bottom of the
    // pyramid that the DeepZoom layer cannot fill, which shows up as a
    // partly-black image until the viewer zooms into the tiled levels.
    uint16 planar = PLANARCONFIG_CONTIG;
    TIFFGetFieldDefaulted( tiff, TIFFTAG_PLANARCONFIG, &planar );
    if( planar != PLANARCONFIG_CONTIG ){
      throw file_error( "TPTImage :: striped level with separate planar configuration is not supported in "
                        + getFileName( seq, ang ) );
    }

    unsigned int bytes_pp = channels * (bpc/8);
    if( bytes_pp == 0 ){
      throw file_error( "TPTImage :: cannot determine bytes per pixel for " + getFileName( seq, ang ) );
    }

    // channels and bpc were read from the base level. If this level disagrees
    // the pixel stride would be wrong and we would emit corrupt image data, so
    // refuse rather than guess.
    uint16 level_spp = 0, level_bps = 0;
    TIFFGetFieldDefaulted( tiff, TIFFTAG_SAMPLESPERPIXEL, &level_spp );
    TIFFGetFieldDefaulted( tiff, TIFFTAG_BITSPERSAMPLE, &level_bps );
    if( (unsigned int)level_spp != channels || (unsigned int)level_bps != bpc ){
      ostringstream error;
      error << "TPTImage :: striped level " << res << " has " << level_spp << " samples/pixel at "
            << level_bps << " bits, but the base level has " << channels << " at " << bpc
            << " in " << getFileName( seq, ang );
      throw file_error( error.str() );
    }

    uint32 rows_per_strip = 0;
    TIFFGetFieldDefaulted( tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip );
    if( rows_per_strip == 0 || rows_per_strip > im_height ) rows_per_strip = im_height;

    tsize_t scanline = TIFFScanlineSize( tiff );
    tsize_t strip_size = TIFFStripSize( tiff );
    if( scanline <= 0 || strip_size <= 0 ){
      throw file_error( "TPTImage :: bad strip geometry in " + getFileName( seq, ang ) );
    }

    unsigned char* strip_buf = (unsigned char*) _TIFFmalloc( strip_size );
    if( !strip_buf ) throw file_error( "tiff malloc strip failed" );

    // Blank the padded tile so any region we do not fill reads as background
    // rather than as whatever the previous request left behind.
    memset( tile_buf, 0, padded_bytes );

    uint32 x0 = ( tile % ntlx ) * tile_widths[vipsres];
    uint32 y0 = ( tile / ntlx ) * tile_heights[vipsres];
    size_t dst_stride = (size_t)tile_width * bytes_pp;
    size_t copy_bytes = (size_t)tw * bytes_pp;

    uint32 loaded = (uint32)-1;
    tsize_t loaded_bytes = 0;

    for( uint32 r = 0; r < th; r++ ){

      uint32 row = y0 + r;
      if( row >= im_height ) break;

      uint32 strip = row / rows_per_strip;
      if( strip != loaded ){
        loaded_bytes = TIFFReadEncodedStrip( tiff, (tstrip_t)strip, strip_buf, strip_size );
        if( loaded_bytes == -1 ){
          _TIFFfree( strip_buf );
          ostringstream error;
          error << "TPTImage :: TIFFReadEncodedStrip failed for strip " << strip
                << " at resolution " << res << " in " << getFileName( seq, ang );
          throw file_error( error.str() );
        }
        loaded = strip;
      }

      size_t offset = (size_t)( row % rows_per_strip ) * (size_t)scanline
                    + (size_t)x0 * bytes_pp;

      // Bound against what this strip actually decoded, not the nominal strip
      // size: the final strip of an image usually holds fewer rows, and the
      // tail of the buffer is then uninitialised.
      if( offset + copy_bytes > (size_t)loaded_bytes ) break;

      memcpy( (unsigned char*)tile_buf + (size_t)r * dst_stride,
              strip_buf + offset, copy_bytes );
    }

    _TIFFfree( strip_buf );
    length = (int) padded_bytes;
  }


  RawTilePtr rawtile(new RawTile( tile, res, seq, ang, tw, th, channels, bpc ));
  rawtile->data = tile_buf;
  rawtile->dataLength = length;
  rawtile->filename = getImagePath();
  rawtile->timestamp = timestamp;
  rawtile->memoryManaged = 0;
  rawtile->padded = true;
  rawtile->sampleType = sampleType;

  return( rawtile );

}

