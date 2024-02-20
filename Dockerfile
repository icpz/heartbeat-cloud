
FROM alpine:latest AS builder

RUN apk add --no-cache build-base cmake grpc-dev protobuf-dev

COPY . /heartbeat-cloud

RUN cmake -S/heartbeat-cloud -B/heartbeat-cloud/build -DCMAKE_BUILD_TYPE=Release && \
    cd /heartbeat-cloud/build && make -j4 && mkdir /work && \
    cp src/heartbeat-client src/heartbeat-server /work/


FROM alpine:latest

RUN apk add --no-cache grpc-cpp libprotobuf tzdata bash curl

COPY --from=builder /work /work/
CMD [ "/work/heartbeat-server" ]

