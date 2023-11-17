/*
    IIP FIF Command Handler Class Member Function

    Copyright (C) 2006-2014 Ruven Pillay.

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


#include <ctime>
#include "OpenSlideImage.h"
#include "BioFormatsImage.h"
#include <sys/stat.h>
#include <limits>

#include <algorithm>
#include "Task.h"
#include "URL.h"
#include "Environment.h"
#include "TPTImage.h"

#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif

#define MAXIMAGECACHE 500  // Max number of items in image cache



using namespace std;



void FIF::run( Session* session, const string& src ){

  if( session->loglevel >= 3 ) *(session->logfile) << "FIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();


  // Decode any URL-encoded characters from our path
  URL url( src );
  string argument = url.decode();


  // Filter out any ../ to prevent users by-passing any file system prefix
  unsigned int n;
  while( (n=argument.find("../")) < argument.length() ) argument.erase(n,3);

  if( session->loglevel >=1 ){
    if( url.warning().length() > 0 ) *(session->logfile) << "FIF :: " << url.warning() << endl;
    if( session->loglevel >= 5 ){
      *(session->logfile) << "FIF :: URL decoding/filtering: " << src << " => " << argument << endl;
    }
  }


  // Create our IIPImage object
  IIPImage test;

  // Get our image pattern variable
  string filesystem_prefix = Environment::getFileSystemPrefix();

  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();

  // Put the image setup into a try block as object creation can throw an exception
  try{

    auto temp = session->imageCache->getObject(argument);
    // Cache Hit
    if(  temp ){
      if( session->loglevel >= 2 ){
        *(session->logfile) << "FIF :: Image cache hit. Number of elements: " << session->imageCache->getNumElements() << endl;
      }

      // get the image, then check it's timestamp.
      if (difftime(IIPImage::getFileTimestamp(temp->getFileName(temp->currentX, temp->currentY)),
                   temp->timestamp)  >
          std::numeric_limits<double>::round_error()) {
        // file on filesystem newer. so reopen it.

          if( session->loglevel >= 2 ){
            *(session->logfile) << "FIF :: Newer file on FS.  reloading " << endl;
          }
        temp->closeImage();
        temp->openImage();
      }
    }
    // Cache Miss
    else{
      if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: Image cache miss" << endl;
      // eviction handled by ImageCache.

       //==== Create our test IIPImage object to get timestamp and image type.
        IIPImage test = IIPImage( argument );
        test.setFileNamePattern( filename_pattern );
        test.setFileSystemPrefix( filesystem_prefix );
        test.Initialise();  // also gathers the timestamp here.

        /***************************************************************
          Test for different image types - only TIFF is native for now
        ***************************************************************/

        ImageFormat format = test.getImageFormat();

        if( format == TIF ){
          if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: TIFF image detected" << endl;
          temp = IIPImagePtr(new TPTImage( test ));
        }
#pragma mark Adding in basic openslide functionality
        else if( format == OPENSLIDE ){
          if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: OpenSlide image detected" << endl;
          temp = IIPImagePtr(new OpenSlideImage( test, session->tileCache ));
        }
#pragma mark Adding in basic bioformats functionality
        else if (format == BIOFORMATS)
        {
          if (session->loglevel >= 2)
            *(session->logfile) << "FIF :: BioFormats image detected" << endl;
          temp = IIPImagePtr(new BioFormatsImage(test, session->tileCache));
        }
#ifdef HAVE_KAKADU
        else if( format == JPEG2000 ){
          if( session->loglevel >= 2 ) *(session->logfile) << "FIF :: JPEG2000 image detected" << endl;
          temp = IIPImagePtr(new KakaduImage( test ));
        }
    #endif
        else throw string( "Unsupported image type: " + argument );

        //==== create format specific iipimage subclass instance as pointer.

        // Open image, and add it to our cache
        temp->openImage();
        session->imageCache->insert(temp);    // insert into cache.

        if( session->loglevel >= 3 ){
          *(session->logfile) << "FIF :: Created and cached image object with key = \"" << argument << "\"" << endl;
        }
    }


    // for now, store pointer.
    // temp already points to an IIPImage instance.

    session->image = temp;


    /* Disable module loading for now!
    else{

#ifdef ENABLE_DL

      // Check our map list for the requested type
      if( moduleList.empty() ){
	throw string( "Unsupported image type: " + imtype );
      }
      else{

	map<string, string> :: iterator mod_it  = moduleList.find( imtype );

	if( mod_it == moduleList.end() ){
	  throw string( "Unsupported image type: " + imtype );
	}
	else{
	  // Construct our dynamic loading image decoder 
	  session->image = new DSOImage( test );
	  (session->image)->Load( (*mod_it).second );

	  if( session->loglevel >= 2 ){
	    *(session->logfile) << "FIF :: Image type: '" << imtype
	                        << "' requested ... using handler "
				<< (session->image)->getDescription() << endl;
	  }
	}
      }
#else
      throw string( "Unsupported image type: " + imtype );
#endif
    }
    */




    // Set the timestamp for the reply
    session->response->setLastModified( (session->image)->getTimestamp() );

    if( session->loglevel >= 2 ){
      *(session->logfile) << "FIF :: Image dimensions are " << (session->image)->getImageWidth()
			  << " x " << (session->image)->getImageHeight() << endl
			  << "FIF :: Image contains " << (session->image)->channels
			  << " channels with " << (session->image)->bpc << " bits per channel" << endl;
      tm *t = gmtime( &(session->image)->timestamp );
      char strt[64];
      strftime( strt, 64, "%a, %d %b %Y %H:%M:%S GMT", t );
      *(session->logfile) << "FIF :: Image timestamp: " << strt << endl;
    }

  }
  catch( const file_error& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    throw error;
  }


  // Check whether we have had an if_modified_since header. If so, compare to our image timestamp
  if( session->headers.find("HTTP_IF_MODIFIED_SINCE") != session->headers.end() ){

    tm mod_t;
    time_t t;

    strptime( (session->headers)["HTTP_IF_MODIFIED_SINCE"].c_str(), "%a, %d %b %Y %H:%M:%S %Z", &mod_t );

    // Use POSIX cross-platform mktime() function to generate a timestamp.
    // This needs UTC, but to avoid a slow TZ environment reset for each request, we set this once globally in Main.cc
    t = mktime(&mod_t);
    if( (session->loglevel >= 1) && (t == -1) ) *(session->logfile) << "FIF :: Error creating timestamp" << endl;

    if( (session->image)->timestamp <= t ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Unmodified content" << endl;
	*(session->logfile) << "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
      }
      throw( 304 );
    }
    else{
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Content modified" << endl;
      }
    }
  }

  // Reset our angle values
  session->view->xangle = 0;
  session->view->yangle = 90;


  if( session->loglevel >= 2 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
