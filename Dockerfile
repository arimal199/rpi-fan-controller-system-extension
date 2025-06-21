FROM alpine:latest AS builder

RUN apk add --no-cache gcc musl-dev linux-headers

COPY fan.c /tmp/
RUN cd /tmp && cc fan-controller.c -static -Os -o fan-controller

FROM scratch

COPY --from=builder /tmp/fan-controller /fan-controller

ENTRYPOINT ["/fan-controller"]
