## Process this file with automake to produce Makefile.in

noinst_PROGRAMS =	iipsrv.fcgi


INCLUDES =		@INCLUDES@ @LIBFCGI_INCLUDES@ @JPEG_INCLUDES@ @TIFF_INCLUDES@

# -Wl,-rpath,$(JAVA_HOME)/lib/server
LIBS =			@LIBS@ @LIBFCGI_LIBS@ @DL_LIBS@ @JPEG_LIBS@ @TIFF_LIBS@ -lm -lopenslide -lopenjp2 -ljvm -L$(JAVA_HOME)/lib/server
AM_LDFLAGS =		@LIBFCGI_LDFLAGS@ -rpath $(JAVA_HOME)/lib/server
AM_CPPFLAGS = -I/usr/local/include/openslide -DBFBRIDGE_INLINE

AM_CFLAGS = -DBFBRIDGE_INLINE

# jni-md.h should also be included hence the platform paths, see link in https://stackoverflow.com/a/37029528
# https://github.com/openjdk/jdk/blob/6e3cc131daa9f3b883164333bdaad7aa3a6ca018/src/jdk.hotspot.agent/share/classes/sun/jvm/hotspot/utilities/PlatformInfo.java#L32
AM_CPPFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I$(JAVA_HOME)/include/darwin -I$(JAVA_HOME)/include/win32 -I$(JAVA_HOME)/include/bsd

iipsrv_fcgi_LDADD = Main.o

if ENABLE_KAKADU
iipsrv_fcgi_LDADD += KakaduImage.o
endif

if ENABLE_PNG
#iipsrv_fcgi_LDADD += PNGCompressor.o PTL.o
endif

if ENABLE_MODULES
iipsrv_fcgi_LDADD += DSOImage.o
endif

EXTRA_iipsrv_fcgi_SOURCES = DSOImage.h DSOImage.cc KakaduImage.h KakaduImage.cc PNGCompressor.h PNGCompressor.cc PTL.cc Main.cc

iipsrv_fcgi_SOURCES = \
			IIPImage.h \
			IIPImage.cc \
			TPTImage.h \
			TPTImage.cc \
			OpenSlideImage.h \
			OpenSlideImage.cc \
			BioFormatsImage.h \
			BioFormatsImage.cc \
			BioFormatsInstance.h \
			BioFormatsInstance.cc \
			BioFormatsThread.h \
			BioFormatsThread.cc \
			JPEGCompressor.h \
			JPEGCompressor.cc \
			RawTile.h \
			Timer.h \
			Cache.h \
			TileManager.h \
			TileManager.cc \
			Tokenizer.h \
			IIPResponse.h \
			IIPResponse.cc \
			BioFormatsManager.h \
			BioFormatsManager.cc \
			View.h \
			View.cc \
			Transforms.h \
			Transforms.cc \
			Environment.h \
			URL.h \
			Writer.h \
			Task.h \
			Task.cc \
			OBJ.cc \
			FIF.cc \
			JTL.cc \
			TIL.cc \
			ICC.cc \
			CVT.cc \
			Zoomify.cc \
			DeepZoom.cc \
			SPECTRA.cc \
			PFL.cc \
			IIIF.cc \
			Watermark.h \
			Watermark.cc \
			Memcached.h
