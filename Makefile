# Makefile
# --------

# Compilateur
CC = gcc

# Répertoires contenant les sources et les en-têtes
srcdir = src
incdir = inc

# Options obligatoires pour la compilation correcte
MCFLAGS = -D_XOPEN_SOURCE=500 -I$(incdir) -pthread

# Toutes les options de compilation
CFLAGS = $(MCFLAGS) -std=c11 -O2 -Wall -Wconversion -Werror -Wextra -Wpedantic \
	-Wwrite-strings -fstack-protector-all -g -D_FORTIFY_SOURCE=2 -DUSE_SYSLOG

# Options d'éditions des liens
LDFLAGS = -lrt -pthread

# Liste des objets
objects = cmdl.o cmdld.o $(srcdir)/logger.o $(srcdir)/squeue.o 

# Liste des exécutables finaux
executables = cmdl cmdld 

# Cible par défaut (tous les exécutables)
all: $(executables)
	@echo "hello"

# Nettoyage des fichiers créés
clean:
	$(RM) $(objects) $(executables)

# Règles pour les deux exécutables
cmdl: cmdl.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@
cmdld: cmdld.o $(srcdir)/logger.o $(srcdir)/squeue.o
	$(CC) $^ $(LDFLAGS) -o $@

# Dépendances des fichiers objets (règles implicites)
cmdl.o: cmdl.c $(incdir)/common.h $(incdir)/squeue.h
cmdld.o: cmdld.c $(incdir)/common.h $(incdir)/logger.h $(incdir)/squeue.h
logger.o: $(srcdir)/logger.c $(incdir)/common.h $(incdir)/logger.h
squeue.o: $(srcdir)/squeue.c $(incdir)/common.h $(incdir)/squeue.h
