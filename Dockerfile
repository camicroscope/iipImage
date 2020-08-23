FROM ubuntu:bionic

### update
RUN apt-get -q update
RUN apt-get -q -y upgrade
RUN apt-get -q -y dist-upgrade
RUN apt-get clean
RUN apt-get -q update

RUN apt-get -q -y install  openssh-server git autoconf automake make libtool pkg-config cmake apache2 libapache2-mod-fcgid libfcgi0ldbl zlib1g-dev libpng-dev libjpeg-dev libtiff5-dev libgdk-pixbuf2.0-dev libxml2-dev libsqlite3-dev libcairo2-dev libglib2.0-dev g++ libmemcached-dev libjpeg-turbo8-dev
RUN a2enmod rewrite
RUN a2enmod fcgid

RUN mkdir /root/src
COPY . /root/src
WORKDIR /root/src

## replace apache's default fcgi config with ours.
RUN rm /etc/apache2/mods-enabled/fcgid.conf
COPY ./fcgid.conf /etc/apache2/mods-enabled/fcgid.conf

## enable proxy
RUN ln -s /etc/apache2/mods-available/proxy_http.load /etc/apache2/mods-enabled/proxy_http.load
RUN ln -s /etc/apache2/mods-available/proxy.load /etc/apache2/mods-enabled/proxy.load
RUN ln -s /etc/apache2/mods-available/proxy.conf /etc/apache2/mods-enabled/proxy.conf

## Add configuration file
COPY apache2.conf /etc/apache2/apache2.conf
COPY ports.conf /etc/apache2/ports.conf

WORKDIR /root/src

### openjpeg version in ubuntu 14.04 is 1.3, too old and does not have openslide required chroma subsampled images support.  download 2.1.0 from source and build
RUN git clone https://github.com/uclouvain/openjpeg.git --branch=v2.3.0
RUN mkdir /root/src/openjpeg/build
WORKDIR /root/src/openjpeg/build
RUN cmake -DBUILD_JPIP=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_CODEC=ON -DBUILD_PKGCONFIG_FILES=ON ../
RUN make
RUN make install

### Openslide
WORKDIR /root/src
## get my fork from openslide source cdoe
RUN git clone https://github.com/openslide/openslide.git

## build openslide
WORKDIR /root/src/openslide
RUN git checkout tags/v3.4.1
RUN autoreconf -i
#RUN ./configure --enable-static --enable-shared=no
# may need to set OPENJPEG_CFLAGS='-I/usr/local/include' and OPENJPEG_LIBS='-L/usr/local/lib -lopenjp2'
# and the corresponding TIFF flags and libs to where bigtiff lib is installed.
RUN ./configure
RUN make
RUN make install

###  iipsrv
WORKDIR /root/src/iipsrv
RUN ./autogen.sh
#RUN ./configure --enable-static --enable-shared=no
RUN ./configure
RUN make
## create a directory for iipsrv's fcgi binary
RUN mkdir -p /var/www/localhost/fcgi-bin/
RUN cp /root/src/iipsrv/src/iipsrv.fcgi /var/www/localhost/fcgi-bin/

#COPY apache2-iipsrv-fcgid.conf /root/src/iip-openslide-docker/apache2-iipsrv-fcgid.conf

RUN chgrp -R 0 /var && \
    chmod -R g+rwX /var 

RUN export APACHE_LOCK_DIR=/var/log 

CMD service apache2 start && while true; do sleep 1000; done
# CMD apachectl -D FOREGROUND
