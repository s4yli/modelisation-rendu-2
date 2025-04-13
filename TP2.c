#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "TP2Functions.h"
#include <ilcplex/cplex.h>

int main(int argc, char **argv)
{
    int rval = 0;
    
    // Nom du fichier d'instance par défaut
    char instance_file[1024];
    snprintf(instance_file, 1024, "%s", "instance.csv");
    
    // Traitement des options en ligne de commande
    char c;
    while ((c = getopt(argc, argv, "F:h")) != EOF) {
        switch(c) {
            case 'F':
                snprintf(instance_file, 1024, "%s", optarg);
                break;
            case 'h':
                fprintf(stderr, "Usage: ./TP2 [options]\n");
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "\t-F Instance file name to load (default: %s).\n", instance_file);
                exit(0);
                break;
            default:
                exit(0);
        }
    }
    
    // 1. Générer les fichiers d'instances pour 1DKP et 2DKP
    generateFiles_TP2();
    fprintf(stderr, "Instances générées.\n");

    // 2. Lecture d'une instance et résolution (par exemple avec solve_1DKP)
    dataSet data;
    FILE* fin = fopen(instance_file, "r");
    if (!fin) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier d'instance %s\n", instance_file);
        exit(EXIT_FAILURE);
    }
    read_TP2_instance(fin, &data);
    fclose(fin);

    // Option 1 : Mesurer le temps d'exécution sur l'instance chargée
    long long duration = measure_execution_time_1DKP(instance_file, &data);
    printf("Résolution de 1DKP sur %s :\n\tTemps d'exécution : %lld microsecondes\n\tValeur de l'objectif : %.2f\n",
            instance_file, duration, data.master.objval);

    // Libération de la mémoire associée à l'instance résolue
    free(data.c);
    free(data.a);
    free(data.f);
    CPXfreeprob(data.master.env, &data.master.lp);
    CPXcloseCPLEX(&data.master.env);
    
    // 3. Lancer la fonction de mesure de performance sur une suite d'instances
    // Celle-ci réalise les deux tests suivants dans une même fonction :
    //      - n fixé (n = N) et b variant
    //      - b fixé (b = B) et n variant
    // Les résultats sont enregistrés dans un fichier CSV.
    measure_1DKP_performance();
    fprintf(stderr, "Tests de performance réalisés, consultez le fichier results_1DKP.csv\n");

    return rval;
}
