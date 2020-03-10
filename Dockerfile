FROM ubuntu:latest

RUN apt-get update
RUN apt-get install -y gcc g++ make cmake git
RUN apt-get install -y python3-dev python3-venv python3-pip
RUN apt-get install -y libopenmpi-dev libhdf5-openmpi-dev
RUN apt-get install -y freeglut3-dev

ENV HDF5_DIR /usr/lib/x86_64-linux-gnu/hdf5/openmpi
WORKDIR /code/fastpli

CMD git clean -d -f -x && \
   make BUILD=debug install && \
   make test && \
   make examples/requirements && \
   make docs && \
   for f in examples/*.py; do echo "$f"; env/bin/python3 $f; done
