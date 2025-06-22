FROM alpine:3.14

RUN apk update && \
    apk add gcc make build-base && \
    apk add build-base linux-headers && \
    apk add openrc && \
    apk add jansson-dev

COPY . /opt

WORKDIR /opt
RUN make

CMD ["/bin/sh"]