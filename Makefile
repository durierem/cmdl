# Makefile
# --------

# Répertoires contenant les sources et les en-têtes
srcdir = src
incdir = inc

# Compilateur
CC = gcc

# Options obligatoires pour la compilation correcte
CXXFLAGS = -D_XOPEN_SOURCE=500 -I$(incdir) -pthread

# Options de compilation supplémentaires
CFLAGS = -std=c11 -O2 -Wall -Wconversion -Werror -Wextra -Wpedantic \
	-Wwrite-strings -fstack-protector-all -g -D_FORTIFY_SOURCE=2

# Options d'éditions des liens
LDFLAGS = -lrt -pthread

# Liste des objets
objects = $(srcdir)/squeue.o cmdld.o cmdl.o

# Cible par défaut
targets = cmdl cmdld 

all: $(targets)

clean:
	$(RM) $(objects) $(targets)

$(targets): $(objects)
	$(CC) $(objects) $(CXXFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ 

# Dépendances des fichiers objets
squeue.o: $(srcdir)/squeue.c $(incdir)/squeue.h $(incdir)/config.h
cmdld.o: cmdld.c $(incdir)/squeue.h $(incdir)/config.h
cmdl.o: cmdl.c $(incdir)/squeue.h $(incdir)/config.h
