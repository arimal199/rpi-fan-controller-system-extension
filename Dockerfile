FROM alpine:latest AS builder

RUN apk add --no-cache build-base musl-dev libgpiod-dev linux-headers

COPY pkg/src/fan-controller.c /tmp/
RUN cd /tmp && \
  cc fan-controller.c -static -Os -o fan-controller

FROM scratch
WORKDIR /rootfs/usr/local/bin

COPY fan-controller.service /rootfs/etc/systemd/system/
COPY ./pkg/manifest.yaml /
COPY --from=builder /tmp/fan-controller fan-controller