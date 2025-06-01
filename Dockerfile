FROM alpine:3.14

RUN apk update && \
    apk add gcc make build-base

COPY entrypoint.sh /opt
COPY src /opt/ospf
RUN ls -l /opt/ospf
RUN chmod +x /opt/entrypoint.sh

WORKDIR /opt/ospf
RUN make

#RUN /opt/entrypoint.sh