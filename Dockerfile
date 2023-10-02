FROM archlinux:latest as build

WORKDIR /app

RUN pacman -Sy --noconfirm glibc make gcc postgresql-libs

COPY facil.io/ ./facil.io/
COPY src/ ./src/
COPY models/ ./models/
COPY main.c .
COPY Makefile .

RUN make release

EXPOSE 5000

ENTRYPOINT [ "/app/webserver" ]