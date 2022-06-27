# guard
# Author: Florian Büstgens
# 2022
# Please see LICENSE and README.md

TARGET = guard
VERSION = 0.1
WORKDIR != pwd
CC=clang
SRC != find . -name *.c
OBJ != find . -name *.c | sed -e 's/\.c/\.o/g' | sed 's|^./src/||'

WARN= -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations -Wformat-security -Wformat -Werror=format-security -Wstack-protector
SEC= -march=native -fstack-protector-all --param ssp-buffer-size=4 -fpie -ftrapv -D_FORTIFY_SOURCE=2
CFLAGS= ${SEC} ${WARN} -std=c99 -pedantic -O2 -I/usr/local/include 
LDFLAGS= -L/usr/local/lib -lX11 -Wl,-z,relro,-z,now -pie -pthread

.PHONY: all
all: ${OBJ}
	@${CC} ${OBJ} -o ${TARGET} ${LDFLAGS}

${OBJ}: ${SRC}
	@${CC} -c $(CFLAGS) $<

.PHONY: clean
clean:
	@rm -rf ${OBJ} ${TARGET}
