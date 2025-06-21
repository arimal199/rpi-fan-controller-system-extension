FROM alpine:latest AS builder

RUN apk add --no-cache gcc musl-dev linux-headers

COPY fan.c /tmp/
RUN cd /tmp && cc fan_controller.c -static -Os -o fan_controller

FROM scratch

COPY --from=builder /tmp/fan_controller /fan_controller

ENTRYPOINT ["/fan_controller"]
