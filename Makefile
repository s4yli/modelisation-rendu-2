SYSTEM     = x86-64_linux
LIBFORMAT  = static_pic
CPLEXDIR=/opt/ibm/ILOG/CPLEX_Studio1210/cplex

CPLEXBINDIR   = $(CPLEXDIR)/bin/$(BINDIST)
CPLEXLIBDIR   = $(CPLEXDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
CPLEXINCDIR   = $(CPLEXDIR)/include

# Nom de l'exécutable
EXE = TP2

# Dossiers pour organiser les fichiers générés
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# Rechercher automatiquement tous les fichiers .c sauf TP1.c
SRC = $(filter-out TP1.c, $(wildcard *.c))
OBJS = $(SRC:%.c=$(OBJ_DIR)/%.o)

# Ajout librairies CPLEX
ADDLIBS = -L$(CPLEXLIBDIR) -lilocplex -lcplex -m64 -lm -lpthread -ldl

# Ajouter le fichier avec `main` manuellement
MAIN_OBJ = $(OBJ_DIR)/TP2.o

# Ajouter les flags de compilation
DEFINES= -DSYS_UNIX=1 

INCL = -I$(CPLEXINCDIR) -g $(DEFINES)

# C Compiler command
CC = gcc

# C Compiler options
CFLAGS = -m64 -fPIC -fexceptions -O3 -pipe -DNDEBUG -Wall

# Compilation principale
all: $(BIN_DIR)/$(EXE)

$(BIN_DIR)/$(EXE): $(MAIN_OBJ) $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(ADDLIBS) -lm

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCL) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
