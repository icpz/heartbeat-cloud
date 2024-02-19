
FROM alpine:latest AS builder

RUN apk add --no-cache build-base git cmake grpc-dev protobuf-dev && \
    mkdir /work && cd /work && git clone --recursive https://github.com/icpz/heartbeat-cloud

RUN cd /work && mkdir heartbeat-cloud/build && cd heartbeat-cloud/build && cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j4 && mkdir /work/bin && cp src/heartbeat-client src/heartbeat-server /work/bin/


FROM alpine:latest

RUN apk add --no-cache grpc-cpp libprotobuf tzdata bash curl

COPY --from=builder /work/bin /work/
CMD [ "/work/heartbeat-server" ]

