FROM archlinux:latest as build

WORKDIR /app

RUN pacman -Sy --noconfirm glibc make gcc postgresql-libs

COPY . .

RUN make release

EXPOSE 5000

ENTRYPOINT [ "/app/webserver" ]