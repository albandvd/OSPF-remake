FROM alpine:3.14

RUN apk update && \
    apk add gcc make build-base && \
    apk add jansson-dev

COPY entrypoint.sh /opt
COPY src /opt/ospf
RUN dos2unix /opt/entrypoint.sh
RUN chmod +x /opt/entrypoint.sh

WORKDIR /opt/ospf
RUN make

ENTRYPOINT ["/opt/entrypoint.sh"]