#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "TP2Functions.h"
#include <ilcplex/cplex.h>

int main(int argc, char **argv) {
    /* 1. Génération des fichiers d’instances */
    generateFiles_TP2();
    printf("Instances TP2 générées.\n");
    
    /* 2. Mesure des performances de solve_1DKP sur les instances générées */
    measure_1DKP_performance_TP2();
    
    /* Vous pouvez également ajouter ici d’autres appels, par exemple pour
       solver solve_2DKP ou d’autres mesures de performances. */
    
    return 0;
}
