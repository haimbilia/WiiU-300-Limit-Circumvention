FROM ghcr.io/wiiu-env/devkitppc:20260225@sha256:472118aa3d2a1ebb62691e9ae18a21d4d8f14238d688d679c30f5973dc9d9f35

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418@sha256:91fe2c0b1fce10cb3744c670e57b161d4dd97f8c72cae7c5f9fb0ee2c69811ad /artifacts $DEVKITPRO

COPY upstream/libfunctionpatcher /tmp/libfunctionpatcher
RUN make -C /tmp/libfunctionpatcher install && rm -rf /tmp/libfunctionpatcher

WORKDIR /project
