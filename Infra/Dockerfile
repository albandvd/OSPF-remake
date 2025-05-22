FROM alpine:3.14

RUN apk update && \
    apk add gcc make

COPY ../src /opt

COPY entrypoint.sh /opt/entrypoint.sh
RUN chmod +x /opt/entrypoint.sh

ENTRYPOINT ["/opt/entrypoint.sh"]