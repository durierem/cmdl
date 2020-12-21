# Makefile
# --------

# Compilateur
CC = gcc

# Répertoires 
srcdir = src
incdir = inc
testdir = test

# Options obligatoires pour la compilation correcte
MCFLAGS = -D_XOPEN_SOURCE=500 -I$(incdir) -pthread

# Toutes les options de compilation
CFLAGS = $(MCFLAGS) -std=c11 -O2 -Wall -Wconversion -Werror -Wextra \
	-Wpedantic -Wwrite-strings -fstack-protector-all -g -fpie \
	-D_FORTIFY_SOURCE=2 -DUSE_SYSLOG

# Options d'éditions des liens
LDFLAGS = -lrt -pthread -Wl,-z,relro,-z,now -pie

# Liste des objets
objects = cmdl.o cmdld.o $(srcdir)/squeue.o $(testdir)/test_squeue.o

# Liste des exécutables finaux
executables = cmdl cmdld
tests = $(testdir)/test_squeue

# Cible par défaut
all: $(executables)

# Programme(s) de test
test: $(tests)

cmdl: cmdl.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@
cmdld: cmdld.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@
$(testdir)/test_squeue: $(testdir)/test_squeue.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@

# Nettoyage des fichiers créés par la compilation
clean:
	$(RM) $(objects) $(executables) $(tests)

# Dépendances des fichiers objets (règles implicites)
cmdl.o: cmdl.c $(incdir)/common.h $(incdir)/squeue.h
cmdld.o: cmdld.c $(incdir)/common.h $(incdir)/squeue.h
squeue.o: $(srcdir)/squeue.c $(incdir)/common.h $(incdir)/squeue.h
test_squeue.o: $(srcdir)/squeue.c $(incdir)/squeue.h
