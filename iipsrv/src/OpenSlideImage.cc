#include "OpenSlideImage.h"
#include "Timer.h"
#include <tiff.h>
#include <tiffio.h>
#include <cmath>
#include <sstream>

#include <cstdlib>
#include <cassert>

#include <limits>
//#define DEBUG_OSI 1
using namespace std;

extern std::ofstream logfile;

/// Overloaded function for opening a TIFF image
void OpenSlideImage::openImage() throw (file_error) {

  string filename = getFileName(currentX, currentY);

  // get the file modification date/time.   return false if not changed, return true if change compared to the stored info.
  bool modified = updateTimestamp(filename);

  //    if (modified && isSet) {
#ifdef DEBUG_OSI
  Timer timer;
  timer.start();

  logfile << "OpenSlide :: openImage() :: start" << endl << flush;

#endif
  // close previous
  closeImage();

  osr = openslide_open(filename.c_str());

  const char* error = openslide_get_error(osr);
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: openImage() get error :: completed " << filename << endl << flush;
#endif

  if (error) {
    logfile << "ERROR: encountered error: " << error << " while opening " << filename << " with OpenSlide: " << endl << flush;
    throw file_error(string("Error opening '" + filename + "' with OpenSlide, error " + error));
  }
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: openImage() :: " << timer.getTime() << " microseconds" << endl << flush;
#endif


#ifdef DEBUG_OSI
  logfile << "OpenSlide :: openImage() :: completed " << filename << endl << flush;
#endif
  if (osr == NULL) {
    logfile << "ERROR: can't open " << filename << " with OpenSlide" << endl << flush;
    throw file_error(string("Error opening '" + filename + "' with OpenSlide"));
  }



  if (bpc == 0) {
    loadImageInfo(currentX, currentY);
  }



  isSet = true;
  //    } else {
  //#ifdef DEBUG_OSI
  //    logfile << "OpenSlide :: openImage() :: not newer.  reuse openslide object." << endl << flush;
  //#endif
  //    }
}

/// given an open OSI file, get information from the image.
void OpenSlideImage::loadImageInfo(int x, int y) throw (file_error) {

#ifdef DEBUG_OSI
  logfile << "OpenSlideImage :: loadImageInfo()" << endl;

  if (!osr) {
    logfile << "loadImageInfo called before openImage()" << endl;
  }
#endif  

  int64_t w = 0, h = 0;
  currentX = x;
  currentY = y;

  const char* error;

#ifdef DEBUG_OSI
  const char* const* prop_names = openslide_get_property_names(osr);
  error = openslide_get_error(osr);
  if (error) {
    logfile << "ERROR: encountered error: " << error << " while getting property names: " << error << endl;
  }

  int i= 0;

  logfile << "Properties:" << endl;
  while (prop_names[i]) {
    logfile << "" << i << " : " << prop_names[i] << " = " << openslide_get_property_value(osr, prop_names[i]) << endl;
    error = openslide_get_error(osr);
    if (error) {
      logfile << "ERROR: encountered error: " << error << " while getting property: " << error << endl;
    }

    ++i;
  }
#endif


  const char* prop_val;
  //  // TODO:  use actual tile width.  default is 256
  //  prop_val = openslide_get_property_value(osr, "openslide.level[0].tile-width");
  //  error = openslide_get_error(osr);
  //  if (error) {
  //    logfile << "ERROR: encountered error: " << error << " while getting property: " << error << endl;
  //  }
  //
  //  if (prop_val)  tile_width = atoi(prop_val);
  //  else tile_width = 256;
  tile_width = 256;   // default to power of 2 to make downsample simpler.

  //  prop_val = openslide_get_property_value(osr, "openslide.level[0].tile-height");
  //  error = openslide_get_error(osr);
  //  if (error) {
  //    logfile << "ERROR: encountered error: " << error << " while getting property: " << error << endl;
  //  }
  //  if (prop_val) tile_height = atoi(prop_val);
  //  else tile_height = 256;
  tile_height = 256;  // default to power of 2 to make downsample simpler.


  openslide_get_level0_dimensions(osr, &w, &h);
  error = openslide_get_error(osr);
  if (error) {
    logfile << "ERROR: encountered error: " << error << " while getting level0 dim: " << error << endl;
  }

#ifdef DEBUG_OSI
  logfile << "dimensions :" << w << " x " << h << endl;
  //logfile << "comment : " << comment << endl;
#endif



  // TODO Openslide outputs 8 bit ABGR always.
  channels = 3;
  bpc = 8;
  colourspace = sRGB;

  // const char* comment = openslide_get_comment(osr);

  // save the openslide dimensions.
  std::vector<int64_t>  openslide_widths, openslide_heights;
  openslide_widths.clear();
  openslide_heights.clear();



  int32_t openslide_levels = openslide_get_level_count(osr);
  error = openslide_get_error(osr);
  if (error) {
    logfile << "ERROR: encountered error: " << error << " while getting level count: " << error << endl;
  }

#ifdef DEBUG_OSI
  logfile << "number of levels = " << openslide_levels << endl;
  double tempdownsample;
#endif
  int64_t ww, hh;
  for (int32_t i = 0; i < openslide_levels; i++) {
    openslide_get_level_dimensions(osr, i, &ww, &hh);
    error = openslide_get_error(osr);
    if (error) {
      logfile << "ERROR: encountered error: " << error << " while getting level dims: " << error << endl;
    }

    openslide_widths.push_back(ww);
    openslide_heights.push_back(hh);
#ifdef DEBUG_OSI
    tempdownsample = openslide_get_level_downsample(osr, i);
    error = openslide_get_error(osr);
    if (error) {
      logfile << "ERROR: encountered error: " << error << " while getting level downsamples: " << error << endl;
    }

    logfile << "\tlevel " << i << "\t(w,h) = (" << ww << "," << hh << ")\tdownsample=" << tempdownsample << endl;
#endif
  }

  openslide_widths.push_back(0);
  openslide_heights.push_back(0);


  image_widths.clear();
  image_heights.clear();

  //======== virtual levels because getTile specifies res as powers of 2.
  // precompute and store addition info about the tiles
  lastTileXDim.clear();
  lastTileYDim.clear();
  numTilesX.clear();
  numTilesY.clear();

  openslide_level_to_use.clear();
  openslide_downsample_in_level.clear();
  unsigned int os_level = 0;
  unsigned int os_downsample_in_level = 1;

  // store the original size.
  image_widths.push_back(w);
  image_heights.push_back(h);
  lastTileXDim.push_back(w % tile_width);
  lastTileYDim.push_back(h % tile_height);
  numTilesX.push_back((w + tile_width - 1) / tile_width);
  numTilesY.push_back((h + tile_height - 1) / tile_height);
  openslide_level_to_use.push_back(os_level);
  openslide_downsample_in_level.push_back(os_downsample_in_level);


  // what if there are openslide levels with dim smaller than this?

  // populate at 1/2 size steps
  while ((w > tile_width) || (h > tile_height)) {
    // need a level that's has image completely inside 1 tile.
    // (stop with both w and h less than tile_w/h,  previous iteration divided by 2.

    w >>= 1;  // divide by 2 and floor. losing 1 pixel from higher res.
    h >>= 1;

    // compare to next level width and height. if smaller, next level is better for supporting the w/h
    // so switch level.
    if (w <= openslide_widths[os_level+1] && h <= openslide_heights[os_level +1]) {
      ++os_level;
      os_downsample_in_level = 1;  // just went to next smaller level, don't downsample internally yet.
    } else {
      os_downsample_in_level <<= 1;  // next one, downsample internally by 2.
    }
    openslide_level_to_use.push_back(os_level);
    openslide_downsample_in_level.push_back(os_downsample_in_level);

    image_widths.push_back(w);
    image_heights.push_back(h);
    lastTileXDim.push_back(w % tile_width);
    lastTileYDim.push_back(h % tile_height);
    numTilesX.push_back((w + tile_width - 1) / tile_width);
    numTilesY.push_back((h + tile_height - 1) / tile_height);

#ifdef DEBUG_OSI
    logfile << "Create virtual layer : " << w << "x" << h << std::endl;
#endif
  }

#ifdef DEBUG_OSI
  for (int t = 0; t < image_widths.size(); t++) {
    logfile  << "virtual level " << t << " (w,h)=(" << image_widths[t] << "," << image_heights[t] << "),";
    logfile  << " (last_tw,last_th)=(" << lastTileXDim[t] << "," << lastTileYDim[t] << "),";
    logfile  << " (ntx,nty)=(" << numTilesX[t] << "," << numTilesY[t] << "),";
    logfile  << " os level=" << openslide_level_to_use[t] << " downsample from os_level=" << openslide_downsample_in_level[t] << endl;
  }
#endif

  numResolutions = numTilesX.size();

  // only support bpp of 8 (255 max), and 3 channels
  min.assign(channels, 0.0f);
  max.assign(channels, 255.0f);
}

/// Overloaded function for closing a TIFF image
void OpenSlideImage::closeImage() {
#ifdef DEBUG_OSI
  Timer timer;
  timer.start();
#endif

  if (osr != NULL) {
    openslide_close(osr);
    osr = NULL;
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: closeImage() :: " << timer.getTime() << " microseconds" << endl;
#endif
}




//
//// TCP: support get region (internally, already doing it.
///// Overloaded function for returning a region for a given angle and resolution
///** Return a RawTile object: Overloaded by child class.
//      \param ha horizontal angle
//      \param va vertical angle
//      \param res resolution
//      \param layers number of quality layers to decode
//      \param x x coordinate   at resolution r
//      \param y y coordinate   at resolution r
//      \param w width of region   at resolution r
//      \param h height of region  at resolution r
// */
//RawTile OpenSlideImage::getRegion( int ha, int va, unsigned int res, int layers, int x, int y, unsigned int w, unsigned int h ) {
//  // ignore ha, va, layers.  important things are r, x, y, w, and h.
//
//
//#ifdef DEBUG_OSI
//  Timer timer;
//  timer.start();
//#endif
//
//  if (res > (numResolutions-1)) {
//    ostringstream tile_no;
//    tile_no << "OpenSlide :: Asked for non-existant resolution: " << res;
//    throw tile_no.str();
//    return 0;
//  }
//
//  // convert r to zoom
//
//  // res is specified in opposite order from image levels: image level 0 has highest res,
//  // image level nRes-1 has res of 0.
//  uint32_t openslide_zoom = this->numResolutions - 1 - res;
//
//#ifdef DEBUG_OSI
//  logfile << "OpenSlide :: getRegion() :: res=" << res << " pos " << x << "x" << y << " size " << w << "x" << h << " is_zoom= " << openslide_zoom << endl;
//
//#endif
//
//
//
//  uint32_t lw, lh;
//  int32_t lx, ly;
//
//  unsigned int res_width = image_widths[openslide_zoom];
//  unsigned int res_height = image_heights[openslide_zoom];
//
//  // bound the x,y position
//  lx = std::max(0, x);  lx = std::min(lx, static_cast<int32_t>(res_width));
//  ly = std::max(0, y);  ly = std::min(ly, static_cast<int32_t>(res_height));
//
//  // next bound the w and height.
//  lw = std::min(w, res_width - lx);
//  lh = std::min(h, res_height - ly);
//
//
//
//  // convert x ,y, to the right coordinate system
//  if (lw == 0 || lh == 0) {
//    ostringstream tile_no;
//    tile_no << "OpenSlideImage :: Asked for zero-sized region " << x << "x" << y << ", size " << w << "x" << h << ", res dim=" << res_width << "x" << res_height;
//    throw tile_no.str();
//  }
//
//  // Create our raw tile buffer and initialize some values
//  RawTile region(0, res, ha, va, w, h, channels, bpp);
//  region.dataLength = w * h * channels * sizeof(unsigned char);
//  region.filename = getImagePath();
//  region.timestamp = timestamp;
//  // new a block that is larger for openslide library to directly copy in.
//  // then shuffle from BGRA to RGB.  relying on delete [] to do the right thing.
//  region.data = new unsigned char[w * h * 4 * sizeof(unsigned char)];  // TODO: avoid reallocation?
//  region.memoryManaged = 1;                 // allocated data, so use this flag to indicate that it needs to be cleared on destruction
//  //rawtile->padded = false;
//#ifdef DEBUG_OSI
//  logfile << "Allocating tw * th * channels * sizeof(char) : " << w << " * " << h << " * " << channels << " * sizeof(char) " << endl << flush;
//#endif
//
//  // call read
//  read(openslide_zoom, w, h, x, y, region.data);
//
//
//#ifdef DEBUG_OSI
//  logfile << "OpenSlide :: getRegion() :: " << timer.getTime() << " microseconds" << endl << flush;
//  logfile << "REGION RENDERED" << std::endl;
//#endif
//
//  return ( region );
//
//}


/// Overloaded function for getting a particular tile
/** \param x horizontal sequence angle (for microscopy, ignored.)
    \param y vertical sequence angle (for microscopy, ignored.)
    \param r resolution - specified as -log_2(mag factor), where mag_factor ~= highest res width / target res width.  0 to numResolutions - 1.
    \param l number of quality layers to decode - for jpeg2000
    \param t tile number  (within the resolution level.)	specified as a sequential number = y * width + x;
 */
RawTilePtr OpenSlideImage::getTile(int seq, int ang, unsigned int iipres, int layers, unsigned int tile) throw (file_error) {

#ifdef DEBUG_OSI
  Timer timer;
  timer.start();
#endif

  if (iipres > (numResolutions-1)) {
    ostringstream tile_no;
    tile_no << "OpenSlide :: Asked for non-existant resolution: " << iipres;
    throw file_error(tile_no.str());
    return 0;
  }

  // res is specified in opposite order from openslide virtual image levels: image level 0 has highest res,
  // image level nRes-1 has res of 0.
  uint32_t osi_level = numResolutions - 1 - iipres;

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getTile() :: res=" << iipres << " tile= " << tile  << " is_zoom= " << osi_level << endl;

#endif

  //======= get the dimensions in pixels and num tiles for the current resolution

  //    int64_t layer_width = image_widths[openslide_zoom];
  //    int64_t layer_height = image_heights[openslide_zoom];
  //openslide_get_layer_dimensions(osr, layers, &layer_width, &layer_height);


  // Calculate the number of tiles in each direction
  size_t ntlx = numTilesX[osi_level];
  size_t ntly = numTilesY[osi_level];

  if (tile >= ntlx * ntly) {
    ostringstream tile_no;
    tile_no << "OpenSlideImage :: Asked for non-existant tile: " << tile;
    throw file_error(tile_no.str());
  }

  // tile x.
  size_t tx = tile % ntlx;
  size_t ty = tile / ntlx;

  RawTilePtr ttt = getCachedTile(tx, ty, iipres);

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getTile() :: total " << timer.getTime() << " microseconds" << endl << flush;
  logfile << "TILE RENDERED" << std::endl;
#endif
  return ttt;  // return cached instance.  TileManager's job to copy it..
}

/**
 * check if cache has tile.
 *    if yes, return it.
 *    if not,
 *       if a native layer, getNativeTile,
 *       else call halfsampleAndComposeTile
 * @param res  	iipsrv's resolution id.  openslide's level is inverted from this.
 */
RawTilePtr OpenSlideImage::getCachedTile(const size_t tilex, const size_t tiley, const uint32_t iipres) {

#ifdef DEBUG_OSI
  Timer timer;
  timer.start();
#endif

  assert(tileCache);

  // check if cache has tile
  uint32_t osi_level = numResolutions - 1 - iipres;
  uint32_t tid = tiley * numTilesX[osi_level] + tilex;
  RawTilePtr ttt = tileCache->getObject(TileCache::getIndex(getImagePath(), iipres, tid, 0, 0, UNCOMPRESSED, 0));

  // if cache has file, return it
  if (ttt) {
#ifdef DEBUG_OSI
    logfile << "OpenSlide :: getCachedTile() :: Cache Hit " << tilex << "x" << tiley << "@" << iipres << " osi tile bounds: " << numTilesX[osi_level] << "x" << numTilesY[osi_level] << " " << timer.getTime() << " microseconds" << endl << flush;
#endif

    return ttt;
  }
  // else caches does not have it.
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getCachedTile() :: Cache Miss " << tilex << "x" << tiley << "@" << iipres << " osi tile bounds: " << numTilesX[osi_level] << "x" << numTilesY[osi_level] << " " << timer.getTime() << " microseconds" << endl << flush;
#endif

  // is this a native layer?
  if (openslide_downsample_in_level[osi_level] == 1) {
    // supported by native openslide layer
	// tile manager will cache if needed
    return getNativeTile(tilex, tiley, iipres);


  } else {
    // not supported by native openslide layer, so need to compose from next level up,
    return halfsampleAndComposeTile(tilex, tiley, iipres);

	// tile manager will cache this one.
  }

}


/**
 * read from file, color convert, store in cache, and return tile.
 *
 * @param res  	iipsrv's resolution id.  openslide's level is inverted from this.
 */
RawTilePtr OpenSlideImage::getNativeTile(const size_t tilex, const size_t tiley, const uint32_t iipres) {


#ifdef DEBUG_OSI
  Timer timer;
  timer.start();
#endif

  if (!osr) {
    logfile << "openslide image not yet loaded " << endl;
  }


  // compute the parameters (i.e. x and y offsets, w/h, and bestlayer to use.
  uint32_t osi_level = numResolutions - 1 - iipres;

  // find the next layer to downsample to desired zoom level z
  //
  uint32_t bestLayer = openslide_level_to_use[osi_level];

  size_t ntlx = numTilesX[osi_level];
  size_t ntly = numTilesY[osi_level];



  // compute the correct width and height
  size_t tw = tile_width;
  size_t th = tile_height;

  // Get the width and height for last row and column tiles
  size_t rem_x = this->lastTileXDim[osi_level];
  size_t rem_y = this->lastTileYDim[osi_level];

  // Alter the tile size if it's in the rightmost column
  if ((tilex == ntlx - 1) && (rem_x != 0)) {
    tw = rem_x;
  }
  // Alter the tile size if it's in the bottom row
  if ((tiley == ntly - 1) && (rem_y != 0)) {
    th = rem_y;
  }


  // create the RawTile object
  RawTilePtr rt(new RawTile(tiley * ntlx + tilex, iipres, 0, 0, tw, th, channels, bpc));

  // compute the size, etc
  rt->dataLength = tw * th * channels * sizeof(unsigned char);
  rt->filename = getImagePath();
  rt->timestamp = timestamp;

  // new a block that is larger for openslide library to directly copy in.
  // then shuffle from BGRA to RGB.  relying on delete [] to do the right thing.
  rt->data = new unsigned char[tw * th * 4 * sizeof(unsigned char)];
  rt->memoryManaged = 1;	// allocated data, so use this flag to indicate that it needs to be cleared on destruction
  //rawtile->padded = false;
#ifdef DEBUG_OSI
  logfile << "Allocating tw * th * channels * sizeof(char) : " << tw << " * " << th << " * " << 4 << " * sizeof(char) " << endl << flush;
#endif


  // READ FROM file

  //======= next compute the x and y coordinates (top left corner) in level 0 coordinates
  //======= expected by openslide_read_region.
  size_t tx0 = (tilex * tile_width) << osi_level;  // same as multiply by z power of 2
  size_t ty0 = (tiley * tile_height) << osi_level;

  openslide_read_region(osr, reinterpret_cast<uint32_t*>(rt->data), tx0, ty0, bestLayer, tw, th);
  const char* error = openslide_get_error(osr);
  if (error) {
    logfile << "ERROR: encountered error: " << error << " while reading region exact at  " << tx0 << "x" << ty0 << " dim " << tw << "x" << th << " with OpenSlide: " << error << endl;
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getNativeTile() :: read_region() :: " << tilex << "x" << tiley << "@" << iipres << " " << timer.getTime() << " microseconds" << endl << flush;
#endif

  if (!rt->data) throw string("FATAL : OpenSlideImage read_region => allocation memory ERROR");

#ifdef DEBUG_OSI
  timer.start();
#endif


  // COLOR CONVERT in place BGRA->RGB conversion
  this->bgra2rgb(reinterpret_cast<uint8_t*>(rt->data), tw, th);

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: getNativeTile() :: bgra2rgb() :: " << timer.getTime() << " microseconds" << endl << flush;
#endif


  // and return it.
  return rt;
}


/**
 * @detail  return from the local cache a tile.
 *          The tile may be native (directly from file),
 *                previously downsampled and cached,
 *                downsampled from existing cached tile (and cached now for later use), or
 *                downsampled from native tile (from file.  both native and downsampled tiles will be cached for later use)
 *
 *  note that tilex and tiley can be found by multiplying by 2 raised to power of the difference in levels.
 *  2 versions - direct and recursive.  direct should have slightly lower latency.

 * This function
 * automatically downsample a region in the missing zoom level z, if needed.
 * Arguments are exactly as what would be given to openslide_read_region().
 * Note that z is not the openslide layer, but the desired zoom level, because
 * the slide may not have all the layers that correspond to all the
 * zoom levels. The number of layers is equal or less than the number of
 * zoom levels in an equivalent zoomify format.
 * This downsampling method simply does area averaging. If interpolation is desired,
 * an image processing library could be used.

 * go to next level in size, get 4 tiles, downsample and compose.
 *
 * call 4x (getCachedTile at next res, downsample, compose),
 * store in cache, and return tile.  (causes recursion, stops at native layer or in cache.)
 */
RawTilePtr OpenSlideImage::halfsampleAndComposeTile(const size_t tilex, const size_t tiley, const uint32_t iipres) {
  // not in cache and not a native tile, so create one from higher sampling.
#ifdef DEBUG_OSI
  Timer timer;
  timer.start();
#endif

  // compute the parameters (i.e. x and y offsets, w/h, and bestlayer to use.
  uint32_t osi_level = numResolutions - 1 - iipres;


#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsampleAndComposeTile() :: zoom=" << osi_level << " from " << (osi_level -1) << endl;
#endif



  size_t ntlx = numTilesX[osi_level];
  size_t ntly = numTilesY[osi_level];

  // compute the correct width and height
  size_t tw = tile_width;
  size_t th = tile_height;

  // Get the width and height for last row and column tiles
  size_t rem_x = this->lastTileXDim[osi_level];
  size_t rem_y = this->lastTileYDim[osi_level];

  // Alter the tile size if it's in the rightmost column
  if ((tilex == ntlx - 1) && (rem_x != 0)) {
    tw = rem_x;
  }
  // Alter the tile size if it's in the bottom row
  if ((tiley == ntly - 1) && (rem_y != 0)) {
    th = rem_y;
  }



  // allocate raw tile.
  RawTilePtr rt(new RawTile(tiley * ntlx + tilex, iipres, 0, 0, tw, th, channels, bpc));

  // compute the size, etc
  rt->dataLength = tw * th * channels;
  rt->filename = getImagePath();
  rt->timestamp = timestamp;

  // new a block that is larger for openslide library to directly copy in.
  // then shuffle from BGRA to RGB.  relying on delete [] to do the right thing.
  rt->data = new unsigned char[rt->dataLength];
  rt->memoryManaged = 1;	// allocated data, so use this flag to indicate that it needs to be cleared on destruction
  //rawtile->padded = false;
#ifdef DEBUG_OSI
  logfile << "Allocating tw * th * channels * sizeof(char) : " << tw << " * " << th << " * " << channels << " * sizeof(char) " << endl << flush;
#endif

  // new iipres  - next res.  recall that larger res corresponds to higher mag, with largest res being max resolution.
  uint32_t tt_iipres = iipres + 1;
  RawTilePtr tt;
  // temp storage.
  uint8_t *tt_data = new uint8_t[(tile_width >> 1) * (tile_height >> 1) * channels];
  size_t tt_out_w, tt_out_h;

  // uses 4 tiles to create new.
  for (int j = 0; j < 2; ++j) {

    size_t tty = tiley * 2 + j;

    if (tty >= numTilesY[osi_level - 1]) break;  // at edge, this may not be a 2x2 block.

    for (int i = 0; i < 2; ++i) {
      // compute new tile x and y and iipres.
      size_t ttx = tilex * 2 + i;
      if (ttx >= numTilesX[osi_level - 1]) break;  // at edge, this may not be a 2x2 block.


#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsampleAndComposeTile() :: call getCachedTile " << endl << flush;
#endif


      // get the tile
      tt = getCachedTile(ttx, tty, tt_iipres);

      if (tt) {

	// cache the next res tile

#ifdef DEBUG_OSI
  timer.start();
#endif

  // cache it
  tileCache->insert(tt);   // copy is made?

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsampleAndComoseTile() :: cache insert res " << tt_iipres << " " << ttx << "x" << tty << " :: " << timer.getTime() << " microseconds" << endl << flush;
#endif


        // downsample into a temp storage.
        halfsample_3(reinterpret_cast<uint8_t*>(tt->data), tt->width, tt->height,
                     tt_data, tt_out_w, tt_out_h);

        // compose into raw tile.  note that tile 0,0 in a 2x2 block always have size tw/2 x th/2
        compose(tt_data, tt_out_w, tt_out_h, (tile_width / 2) * i, (tile_height / 2) * j,
                reinterpret_cast<uint8_t*>(rt->data), tw, th);
      }
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsampleAndComposeTile() :: called getCachedTile " << endl << flush;
#endif
    }

  }
  delete [] tt_data;
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsampleAndComposeTile() :: downsample " << osi_level << " from " << (osi_level -1) << " :: " << timer.getTime() << " microseconds" << endl << flush;
#endif


  // and return it.
  return rt;


}



// h is number of rows to process.  w is number of columns to process.
void OpenSlideImage::bgra2rgb(uint8_t* data, const size_t w, const size_t h) {
  // swap bytes in place.  we can because we are going from 4 bytes to 3, and because we are using a register to bswap
  //    0000111122223333
  // in:  BGRABGRABGRA
  // out: RGBRGBRGB
  //    000111222333


  // this version relies on compiler to generate bswap code.
  // bswap is only very slightly slower than SSSE3 and AVX2 code, and about 2x faster than naive single byte copies on core i5 ivy bridge.
  uint8_t *out = data;
  uint32_t* in = reinterpret_cast<uint32_t*>(data);
  uint32_t* end = in + w*h;

  uint32_t t;

  // remaining
  for (; in < end; ++in) {
    *(reinterpret_cast<uint32_t*>(out)) = bgra2rgb_kernel(*in);

    out += channels;
  }
}

/**
 * performs 1/2 size downsample on rgb 24bit images
 * @details 	usingg the following property,
 * 					(a + b) / 2 = ((a ^ b) >> 1) + (a & b)
 * 				which does not cause overflow.
 *
 * 				mask with 0xFEFEFEFE to revent underflow during bitshift, allowing 4 compoents
 * 				to be processed concurrently in same register without SSE instructions.
 *
 * 				output size is computed the same way as the virtual level dims - round down
 * 				if not even.
 *
 * @param in
 * @param in_w
 * @param in_h
 * @param out
 * @param out_w
 * @param out_h
 * @param downSamplingFactor      always power of 2.
 */
void OpenSlideImage::halfsample_3(const uint8_t* in, const size_t in_w, const size_t in_h,
                                  uint8_t* out, size_t& out_w, size_t& out_h) {


#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsample_3() :: start :: in " << (void*)in << " out " << (void*)out << endl << flush;
  logfile << "                            :: in wxh " << in_w << "x" << in_h << endl << flush;
#endif

  // do one 1/2 sample run
  out_w = in_w >> 1;
  out_h = in_h >> 1;

  if ((out_w == 0) || (out_h == 0)) {
      logfile << "OpenSlide :: halfsample_3() :: ERROR: zero output width or height " << endl << flush;
	  return;
  }

  if (!(in)) {
	  logfile << "OpenSlide :: halfsample_3() :: ERROR: null input " << endl << flush;
	  return;
  }

  uint8_t const *row1, *row2;
  uint8_t	*dest = out;  // if last recursion, put in out, else do it in place

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsample_3() :: top " << endl << flush;
#endif

  // walk through all pixels in output, except last.
  size_t max_h = out_h - 1,
      max_w = out_w;
  size_t inRowStride = in_w * channels;
  size_t inRowStride2 = 2 * inRowStride;
  size_t inColStride2 = 2 * channels;
  // skip last row, as the very last dest element may have overflow.
  for (size_t j = 0; j < max_h; ++j) {
	    // move row pointers forward 2 rows at a time - in_w may not be multiple of 2.
	  row1 = in + j * inRowStride2;
	  row2 = row1 + inRowStride;

    for (size_t i = 0; i < max_w; ++i) {
      *(reinterpret_cast<uint32_t*>(dest)) = halfsample_kernel_3(row1, row2);
      // output is contiguous.
      dest += channels ;
      row1 += inColStride2;
      row2 += inColStride2;
    }
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsample_3() :: last row " << endl << flush;
#endif

  // for last row, skip the last element
  row1 = in + max_h * inRowStride2;
  row2 = row1 + inRowStride;

  --max_w;
  for (size_t i = 0; i < max_w; ++i) {
    *(reinterpret_cast<uint32_t*>(dest)) = halfsample_kernel_3(row1, row2);
    dest += channels ;
    row1 += inColStride2;
    row2 += inColStride2;
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsample_3() :: last one " << endl << flush;
#endif

  // for last pixel, use memcpy to avoid writing out of bounds.
  uint32_t v = halfsample_kernel_3(row1, row2);
  memcpy(dest, reinterpret_cast<uint8_t*>(&v), channels);



  // at this point, in has been averaged and stored .
  // since we stride forward 2 col and rows at a time, we don't need to worry about overwriting an unread pixel.
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: halfsample_3() :: done" << endl << flush;
#endif

}

// in is contiguous, out will be when done.
void OpenSlideImage::compose(const uint8_t *in, const size_t in_w, const size_t in_h,
                             const size_t& xoffset, const size_t& yoffset,
                             uint8_t* out, const size_t& out_w, const size_t& out_h) {

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: compose() :: start " << endl << flush;
#endif

  if ((in_w == 0) || (in_h == 0)) {
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: compose() :: zero width or height " << endl << flush;
#endif
	  return;
  }
  if (!(in)) {
#ifdef DEBUG_OSI
  logfile << "OpenSlide :: compose() :: nullptr input " << endl << flush;
#endif
	  return;
  }

  if (out_h < yoffset + in_h) {
    logfile << "COMPOSE ERROR: out_h, yoffset, in_h: " << out_h << "," << yoffset << "," << in_h << endl;
    assert(out_h >= yoffset + in_h);
  }
  if (out_w < xoffset + in_w) {
    logfile << "COMPOSE ERROR: out_w, xoffset, in_w: " << out_w << "," << xoffset << "," << in_w << endl;
    assert(out_w >= xoffset + in_w);
  }

  size_t dest_stride = out_w*channels;
  size_t src_stride = in_w*channels;

  uint8_t *dest = out + yoffset * dest_stride + xoffset * channels;
  uint8_t const *src = in;

  for (int k = 0; k < in_h; ++k) {
    memcpy(dest, src, in_w * channels);
    dest += dest_stride;
    src += src_stride;
  }

#ifdef DEBUG_OSI
  logfile << "OpenSlide :: compose() :: start " << endl << flush;
#endif

}



