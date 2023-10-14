FROM ubuntu:23.04

RUN apt-get update
RUN apt install libssl-dev -y \
    cmake \ 
    gcc \
    g++ \
    lsb-release
    git 

RUN sh -c 'echo "deb https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add -
RUN apt-get update
RUN apt-get install postgresql -y

#COPY below files.

libpq-fe.h
pgtypes_date.h
pgtypes.h
pgtypes_timestamp.h
pgtypes_interval.h
ecpg_config.h