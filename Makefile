# Source, Executable, Includes, Library Defines
TOPDIR = $(shell pwd)
BDIR   = $(TOPDIR)/build
OBJDIR = $(BDIR)/obj
IDIR   = $(TOPDIR)/include
INCL   = $(IDIR)/cfp.h
SRC    = ./src/lz4_decompress_size.c ./src/cfp.c
OBJ   = $(SRC:%.c=$(OBJDIR)/%.o)
LIBS   = -llz4
EXE    = $(BDIR)/lz4_decompress_size

#VALGRIND=valgrind --leak-check=full -s
VALGRIND=

# Compiler, Linker Defines
CC      = /usr/bin/gcc
CFLAGS  = -std=c99 -pedantic -D_FORTIFY_SOURCE=2 -Wall -Wextra -Werror -O3
LIBPATH = -L.
LDFLAGS = -o $(EXE) $(LIBPATH)
CFDEBUG = -std=c99 -pedantic -D_FORTIFY_SOURCE=0 -Wall -Wextra -Werror -g -DDEBUG -O0
RM      = /bin/rm -f


# Compiling and assembling object files
$(OBJDIR)/%.o: %.c
	mkdir -p '$(@D)'
	$(CC) -o $@ -c $(CFLAGS) -I$(IDIR) $<

# Linking
$(EXE): $(OBJ)
	@mkdir -p $(BDIR)
	$(CC) $(LDFLAGS) -I$(IDIR) $(OBJ) $(LIBS)

# Object dependencies
$(OBJ): $(INCL)

.PHONY: clean

TEST_FLAGS = r c l
check:
	@mkdir -p ./output
	@for flag in $(TEST_FLAGS); do \
		$(VALGRIND) $(EXE) $$flag ./input/wal.sgml.lz4 > ./output/wal.sgml || exit;	\
		diff ./output/wal.sgml ./expected/wal.sgml || exit;	\
	done

clean:
	$(RM) $(OBJ) $(EXE) ./output/wal.sgml
