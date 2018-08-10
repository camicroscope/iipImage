FROM node:8
RUN mkdir /root/src
COPY . /root/src

WORKDIR /images
VOLUME ["/images"]

WORKDIR /root/src



# iip
RUN apt-get update
RUN apt-get -q -y upgrade
RUN apt-get -q -y install git autoconf automake make libtool pkg-config cmake
RUN git submodule init
RUN git submodule sync --recursive
RUN git submodule update
RUN cd iipsrv
RUN apt-get install -q -y libtiff5-dev zlib1g-dev libjpeg-dev libopenjpeg-dev openslide-tools

RUN apt-get -q -y install g++ libmemcached-dev

RUN apt-get -q -y install apache2 libapache2-mod-fcgid

WORKDIR /root/src/iipsrv

RUN bash autogen.sh
RUN ./configure
RUN make

RUN mkdir -p /var/www/localhost/fcgi-bin/
RUN cp ../fcgid.conf /etc/apache2/mods-enabled/fcgid.conf
RUN cp src/iipsrv.fcgi /var/www/localhost/fcgi-bin/

WORKDIR /root/src
RUN echo "ServerName localhost" >> /etc/apache2/apache2.conf

# our auth server
WORKDIR /root/src
RUN npm install
RUN npm install -g nodemon
EXPOSE 4010
CMD apachectl -k start && nodemon auth.js -w ./
