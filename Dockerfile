FROM archlinux:latest as build

WORKDIR /src

RUN pacman -Syu --noconfirm && pacman -S --noconfirm base-devel gcc-libs gcc postgresql-libs

COPY . .

RUN ["make", "release"]

EXPOSE 5000

ENTRYPOINT [ "webserver" ]