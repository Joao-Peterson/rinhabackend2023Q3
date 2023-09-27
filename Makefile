# Commands:
# ---------------------------------------------------------------
# 	build 		: build lib objects and test file for testing 
# 	release 	: build lib objects, archive and organize the lib files for use in the 'dist/' folder
# 	dist 		: dist just organizes the lib files for use in the 'dist/' folder
# 	clear 		: clear compiled executables
# 	install  	: installs binaries, includes and libs to the specified "INSTALL_" path variables
# 	label  		: update the author, year and version using sed in the specified files 
# 	mem  		: runs valgrind on the test binary 
# 	doc  		: runs doxygen and generate output on /docs folder 

# Options -------------------------------------------------------

CC=gcc
C_FLAGS=-Wall -Wpedantic
C_FLAGS_RELEASE=-O2
C_FLAGS_DEBUG=-g
# C_FLAGS_DEBUG+=-D DEBUG
I_FLAGS=-Isrc
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

# DON'T EDIT -----------------------------------------------------

OBJS:=$(subst .c,.o,$(SOURCES))

.PHONY : build clear build_dir dist_dir dist

build : C_FLAGS += $(C_FLAGS_DEBUG)
build : build_dir $(BINARY)

release : C_FLAGS += $(C_FLAGS_RELEASE)
release : build_dir $(BINARY)

test : C_FLAGS += $(C_FLAGS_DEBUG)
test : dbtest.o $(OBJS)
	$(CC) $(LD_FLAGS) $^ -o $(notdir $@)

$(BINARY) : main.o $(OBJS)
	$(CC) $(LD_FLAGS) $^ -o $(notdir $@)

%.o : %.c
	$(CC) $(C_FLAGS) $(I_FLAGS) -c $^ -o $@

build_dir : 
	@mkdir -p $(BUILD_DIR)

clear : 
	@rm -vrdf $(BUILD_DIR)
	@rm -vrdf $(DIST_DIR)
	@rm -vf $(BINARY)
	@rm -vf *.exe
	@rm -vf */*.o
	@rm -vf *.o

mem : $(BINARY)
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes ./$<
# valgrind --tool=callgrind $(TEST_EXE)

profile : $(BINARY)
	valgrind --tool=massif --time-unit=B ./$<

image :
	sudo docker build -t petersonsheff/rinhabackend2023q3capi .

up : image
	sudo docker-compose up -d

down :
	sudo docker-compose down

gatling : down up

