# Makefile

# --- VARIABLES ---------------------------------------------------------------

# Répertoires
srcdir = src
incdir = inc
testdir = test

# Compilateur
CC = gcc

# Options obligatoires pour la compilation correcte
MCFLAGS = -D_XOPEN_SOURCE=500 -I$(incdir) -pthread

# Toutes les options de compilation
CFLAGS = $(MCFLAGS) -std=c11 -O2 -Wall -Wconversion -Werror -Wextra \
	-Wpedantic -Wwrite-strings -fstack-protector-all -g -fpie \
	-D_FORTIFY_SOURCE=2

# Options d'éditions des liens
LDFLAGS = -lrt -pthread -Wl,-z,relro,-z,now -pie

# Liste des objets
objects = cmdl.o cmdld.o $(srcdir)/squeue.o $(srcdir)/config.o \
	$(testdir)/test_squeue.o

# Liste des exécutables finaux
executables = cmdl cmdld
tests = $(testdir)/test_squeue
docs = README.pdf MANUAL.pdf

# --- CIBLES ------------------------------------------------------------------

default: $(executables)
test: $(tests)
doc: $(docs) # (requiert pandoc)
all: $(executables) $(tests) $(docs)
clean:
	$(RM) $(objects) $(executables) $(tests) $(docs)

# --- RÈGLES ------------------------------------------------------------------

cmdl: cmdl.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@
cmdld: cmdld.o $(srcdir)/squeue.o $(srcdir)/config.o
	$(CC) $^ $(LDFLAGS) -o $@
$(testdir)/test_squeue: $(testdir)/test_squeue.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@
$(docs):
	pandoc --pdf-engine=xelatex $^ -o $@

# Dépendances des fichiers objets (règles implicites)
cmdl.o: cmdl.c $(incdir)/common.h $(incdir)/squeue.h
cmdld.o: cmdld.c $(incdir)/common.h $(incdir)/squeue.h $(incdir)/config.h
config.o: $(srcdir)/config.c $(incdir)/config.h
squeue.o: $(srcdir)/squeue.c $(incdir)/squeue.h
test_squeue.o: $(srcdir)/squeue.c $(incdir)/squeue.h

README.pdf: README.md
MANUAL.pdf: MANUAL.md
