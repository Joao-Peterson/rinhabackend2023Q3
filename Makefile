# Commands:
# ---------------------------------------------------------------
# 	build 		: build program in debug mode
# 	release 	: build program in release mode
# 	clear 		: clear all build files and binaries
# 	test 		: build test binary
# 	mem  		: runs valgrind for mem leak checks on the program 
# 	profile  	: runs valgrind for mem profiling on the program 
# 	image  		: builds docker image for the program
# 	up  		: runs docker compose up for the app, nginx and postgres
# 	down  		: runs docker compose down
# 	gatling  	: runs gatling with predefined test on the docker compose stack

# Options -------------------------------------------------------

CC=gcc
C_FLAGS=-Wall -Wpedantic
C_FLAGS_RELEASE=-O3
C_FLAGS_DEBUG=-g
# C_FLAGS_DEBUG+=-D DEBUG
I_FLAGS=-Isrc -Imodels
I_FLAGS+=-Ifacil.io
LD_FLAGS=
LD_FLAGS+=-lpthread
LD_FLAGS+=-lm
LD_FLAGS+=-lpq

BINARY=webserver

SOURCES=src/db.c
SOURCES+=src/utils.c
SOURCES+=src/string+.c
SOURCES+=facil.io/fiobj_ary.c
SOURCES+=facil.io/fiobj_data.c
SOURCES+=facil.io/fiobject.c
SOURCES+=facil.io/fiobj_hash.c
SOURCES+=facil.io/fiobj_json.c
SOURCES+=facil.io/fiobj_mustache.c
SOURCES+=facil.io/fiobj_numbers.c
SOURCES+=facil.io/fiobj_str.c
SOURCES+=facil.io/fio.c
SOURCES+=facil.io/fio_cli.c
SOURCES+=facil.io/fio_siphash.c
SOURCES+=facil.io/fio_tls_missing.c
SOURCES+=facil.io/fio_tls_openssl.c
SOURCES+=facil.io/http1.c
SOURCES+=facil.io/http.c
SOURCES+=facil.io/http_internal.c
SOURCES+=facil.io/redis_engine.c
SOURCES+=facil.io/websockets.c

BUILD_DIR=build
DIST_DIR=dist

GATLING_VERSION=3.9.5
GATLING_TOOL=gatling/gatling/bin/gatling.sh

# DON'T EDIT -----------------------------------------------------

OBJS:=$(subst .c,.o,$(SOURCES))

.PHONY : build clear build_dir dist_dir dist gatling

# debug build
build : C_FLAGS += $(C_FLAGS_DEBUG)
build : build_dir $(BINARY)

# release build
release : C_FLAGS += $(C_FLAGS_RELEASE)
release : build_dir $(BINARY)

# test binary build
test : C_FLAGS += $(C_FLAGS_DEBUG)
test : dbtest.o $(OBJS)
	$(CC) $(LD_FLAGS) $^ -o $(notdir $@)

# C binary build rule
$(BINARY) : main.o $(OBJS)
	$(CC) $(LD_FLAGS) $^ -o $(notdir $@)

# C objects build rule
%.o : %.c
	$(CC) $(C_FLAGS) $(I_FLAGS) -c $^ -o $@

# make output dir
build_dir : 
	@mkdir -p $(BUILD_DIR)

# clear build files
clear : 
	@rm -vrdf $(BUILD_DIR)
	@rm -vrdf $(DIST_DIR)
	@rm -vf $(BINARY)
	@rm -vf *.exe
	@rm -vf */*.o
	@rm -vf *.o

### Valgrind stuff (memory leak cheker and profiler)

# check for leaks
mem : build
	@sudo docker-compose down
	@sudo docker-compose up db -d
	@sleep 5
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(BINARY)
# valgrind --tool=callgrind $(TEST_EXE)

# profile memory consuption
profile : build
	valgrind --tool=massif --time-unit=B ./$(BINARY)

### Docker stuff

# create image
image : clear
	sudo docker build -t rinhabackend2023q3capi .

# compose up
up : image
	sudo docker-compose up -d

# compose down
down :
	sudo docker-compose down

### Gatling stuff

# install gatling tool
$(GATLING_TOOL) :
	@echo "Downloading Gatling $(GATLING_VERSION)"
	@rm -rd gatling/gatling
	@curl -fsSL "https://repo1.maven.org/maven2/io/gatling/highcharts/gatling-charts-highcharts-bundle/$(GATLING_VERSION)/gatling-charts-highcharts-bundle-$(GATLING_VERSION)-bundle.zip" > ./gatling/gatling.zip
	@unzip gatling/gatling.zip -d gatling
	@mv gatling/gatling-charts-highcharts-bundle-$(GATLING_VERSION) gatling/gatling
	@rm gatling/gatling.zip
	@echo "Gatling installed!"

gatling : $(GATLING_TOOL) down up
	sh $(GATLING_TOOL) \
	-rm local \
	-s RinhaBackendSimulation \
	-rd "Simulação RinhaBackend2023Q3 - C API" \
	-rf ../results \
	-sf ../simulations \
	-rsf ../resources
	@sleep 5
	@curl -fsSL "http://localhost:9999/contagem-pessoas" > count.txt

gatling-slim : $(GATLING_TOOL) down up
	sh $(GATLING_TOOL) \
	-rm local \
	-s RinhaBackendSlimSimulation \
	-rd "Simulação Slim RinhaBackend2023Q3 - C API" \
	-rf ../results \
	-sf ../simulations \
	-rsf ../resources
	@sleep 5
	@curl -fsSL "http://localhost:9999/contagem-pessoas" > count.txt

gatling-test : $(GATLING_TOOL)
	sh $(GATLING_TOOL) \
	-rm local \
	-s TestSimulation \
	-rd "Simulação Teste RinhaBackend2023Q3 - C API" \
	-rf ../results \
	-sf ../simulations \
	-rsf ../resources
	@sleep 5
	@curl -fsSL "http://localhost:9999/contagem-pessoas" > count.txt

gatling-local : build
	sh $(GATLING_TOOL) \
	-rm local \
	-s LocalTestSimulation \
	-rd "Simulação Teste (Local) RinhaBackend2023Q3 - C API" \
	-rf ../results \
	-sf ../simulations \
	-rsf ../resources
	@sleep 5
	@curl -fsSL "http://localhost:5000/contagem-pessoas" > count.txt