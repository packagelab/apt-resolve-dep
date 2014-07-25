FROM debian:squeeze
MAINTAINER Alan Grosskurth <code@alan.grosskurth.ca>

RUN \
  apt-get update && \
  env DEBIAN_FRONTEND=noninteractive \
    apt-get -y install --no-install-recommends build-essential zlib1g-dev

ADD . /app
WORKDIR /app

RUN make

USER nobody
ENTRYPOINT ["./apt-resolve-dep"]
CMD []
