FROM camicroscope/image-decoders:latest

### update
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get -q update
RUN apt-get -q -y upgrade
RUN apt-get -q -y dist-upgrade
RUN apt-get clean
RUN apt-get -q update

RUN apt-get -q -y install git autoconf automake make libtool pkg-config apache2 libapache2-mod-fcgid libfcgi0ldbl g++ libmemcached-dev libjpeg-turbo8-dev
RUN a2enmod rewrite
RUN a2enmod fcgid

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

## Print BioFormats errors, etc. to Docker console (stderr)
#RUN ln -sf /proc/self/fd/1 /var/log/apache2/error.log

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

# CMD service apache2 start && while true; do sleep 1000; done
CMD apachectl -D FOREGROUND
