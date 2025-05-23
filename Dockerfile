FROM alpine:3.14

RUN apk update && \
    apk add gcc make

COPY entrypoint.sh /opt
RUN chmod +x /opt/entrypoint.sh

