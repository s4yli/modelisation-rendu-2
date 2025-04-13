#include "TP2Functions.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdio.h>
#include <ilcplex/cplex.h>

/* Définitions des constantes pour la génération d'instances */
#define B 1000            // Capacité maximale pour la contrainte 1
#define G 1000             // Capacité maximale pour la contrainte 2
#define N 100             // Nombre d’objets maximal
#define FILENAME_PATTERN_TP2 "tp2_examples/n=%d_b=%d_g=%d.csv"
#define FILENAME_SIZE 64

int read_TP2_instance(FILE*fin,dataSet* dsptr)
{
	int rval = 0;

	//capacites b et g
	int b;
	int g;
	//Nombre d'objets
	int n;
	rval = fscanf(fin,"%d,%d,%d\n",&n,&b,&g);
	dsptr->b = b;
	dsptr->g = g;

	dsptr->n = n;
	dsptr->c = (int*)malloc(sizeof(int)*n);
	dsptr->a = (int*)malloc(sizeof(int)*n);
	dsptr->f = (int*)malloc(sizeof(int)*n);


	int i;
	for( i = 0 ; i < n ; i++)
		rval = fscanf(fin,"%d,%d,%d\n",&(dsptr->c[i]),&(dsptr->a[i]),&(dsptr->f[i]));

	fprintf(stderr,"\nInstance file read, we have capacites [b,g] = [%d,%d] and there is %d items of values/weights:\n",
			b,g,n);
	fprintf(stderr,"i\tci\tai\tfi\n");
	fprintf(stderr,"--------------------------\n");


	for( i = 0 ; i < n ; i++)
		fprintf(stderr,"%d\t%d\t%d\t%d\n",i,dsptr->c[i],dsptr->a[i],dsptr->f[i]);
	fprintf(stderr,"\n");

	return rval;
}

int solve_1DKP(dataSet* dsptr)
{
    int rval = 0;
    IP_problem* ip_prob_ptr = &(dsptr->master);
    
    // Initialisation CPLEX
    ip_prob_ptr->env = NULL;
    ip_prob_ptr->lp = NULL;
    ip_prob_ptr->env = CPXopenCPLEX(&rval);
    if(rval) fprintf(stderr,"ERROR WHILE CALLING CPXopenCPLEX\n");
    if (ip_prob_ptr->env == NULL) {
        char errmsg[1024];
        CPXgeterrorstring(ip_prob_ptr->env, rval, errmsg);
        fprintf(stderr, "%s", errmsg);
        exit(0);
    }

    // Création du problème
    ip_prob_ptr->lp = CPXcreateprob(ip_prob_ptr->env, &rval, "1DKP");
    if(rval) fprintf(stderr,"ERROR WHILE CALLING CPXcreateprob\n");

    // Paramètres CPLEX
    CPXsetintparam(ip_prob_ptr->env, CPX_PARAM_DATACHECK, CPX_ON);
    CPXsetintparam(ip_prob_ptr->env, CPX_PARAM_SCRIND, CPX_ON);

    // Récupération des données
    int n = dsptr->n;
    int* a = dsptr->a;
    int* c = dsptr->c;
    int b = dsptr->b;

    // Allocation mémoire pour les variables
    ip_prob_ptr->nv = n;
    ip_prob_ptr->x = (double*)malloc(sizeof(double)*n);
    ip_prob_ptr->cost = (double*)malloc(sizeof(double)*n);
    ip_prob_ptr->c_type = (char*)malloc(sizeof(char)*n);
    ip_prob_ptr->up_bound = (double*)malloc(sizeof(double)*n);
    ip_prob_ptr->low_bound = (double*)malloc(sizeof(double)*n);
    ip_prob_ptr->var_name = (char**)malloc(sizeof(char*)*n);

    // Indices des variables
    int* id_x_j = (int*)malloc(sizeof(int)*n);
    
    // Définition des variables binaires
    for(int j = 0; j < n; j++) {
        id_x_j[j] = j;
        ip_prob_ptr->cost[j] = c[j];
        ip_prob_ptr->c_type[j] = 'B';
        ip_prob_ptr->up_bound[j] = 1.0;
        ip_prob_ptr->low_bound[j] = 0.0;
        ip_prob_ptr->var_name[j] = (char*)malloc(1024);
        snprintf(ip_prob_ptr->var_name[j], 1024, "x_%d", j);
    }

    // Ajout des colonnes
    rval = CPXnewcols(ip_prob_ptr->env, ip_prob_ptr->lp, 
                     n, 
                     ip_prob_ptr->cost, 
                     ip_prob_ptr->low_bound, 
                     ip_prob_ptr->up_bound, 
                     ip_prob_ptr->c_type, 
                     ip_prob_ptr->var_name);
    if(rval) fprintf(stderr,"CPXnewcols error: %d\n", rval);

    // Préparation de la contrainte
    ip_prob_ptr->rhs = (double*)malloc(sizeof(double));
    ip_prob_ptr->sense = (char*)malloc(sizeof(char));
    ip_prob_ptr->rmatbeg = (int*)malloc(sizeof(int));
    ip_prob_ptr->rmatind = (int*)malloc(sizeof(int)*n);
    ip_prob_ptr->rmatval = (double*)malloc(sizeof(double)*n);
    ip_prob_ptr->const_name = (char**)malloc(sizeof(char*));
    ip_prob_ptr->const_name[0] = (char*)malloc(1024);

    // Contrainte unique
    ip_prob_ptr->rmatbeg[0] = 0;
    ip_prob_ptr->rhs[0] = b;
    ip_prob_ptr->sense[0] = 'L';
    snprintf(ip_prob_ptr->const_name[0], 1024, "capacity_constraint");

    for(int j = 0; j < n; j++) {
        ip_prob_ptr->rmatind[j] = id_x_j[j];
        ip_prob_ptr->rmatval[j] = a[j];
    }

    // Ajout de la contrainte
    rval = CPXaddrows(ip_prob_ptr->env, ip_prob_ptr->lp,
                     0,                   // Nombre de nouvelles colonnes
                     1,                   // Nombre de nouvelles contraintes
                     n,                   // Nombre de coefficients non-nuls
                     ip_prob_ptr->rhs,    // RHS
                     ip_prob_ptr->sense,  // Sens
                     ip_prob_ptr->rmatbeg,// Début des coefficients
                     ip_prob_ptr->rmatind,// Indices
                     ip_prob_ptr->rmatval,// Valeurs
                     NULL,               // Nouveaux noms de colonnes
                     ip_prob_ptr->const_name);
    if(rval) fprintf(stderr,"CPXaddrows error: %d\n", rval);

    // Passage en maximisation
    CPXchgobjsen(ip_prob_ptr->env, ip_prob_ptr->lp, CPX_MAX);

    // Résolution
    rval = CPXmipopt(ip_prob_ptr->env, ip_prob_ptr->lp);
    if(rval) fprintf(stderr,"CPXmipopt error: %d\n", rval);

    // Récupération solution
    double objval;
    CPXgetobjval(ip_prob_ptr->env, ip_prob_ptr->lp, &objval);
    CPXgetx(ip_prob_ptr->env, ip_prob_ptr->lp, ip_prob_ptr->x, 0, n-1);

    // Affichage résultats
    printf("\nSolution optimale (valeur: %.2f):\n", objval);
    for(int j = 0; j < n; j++) {
        if(ip_prob_ptr->x[j] > 0.9) {
            printf("Objet %d sélectionné (poids=%d, valeur=%d)\n", 
                  j, a[j], c[j]);
        }
    }

    // Nettoyage mémoire
    free(id_x_j);
    return rval;
}

int solve_2DKP(dataSet* dsptr)
{
	int rval = 0;

	IP_problem* ip_prob_ptr = &(dsptr->master);
	ip_prob_ptr->env = NULL;
	ip_prob_ptr->lp = NULL;
	ip_prob_ptr->env = CPXopenCPLEX (&rval);
	if(rval) fprintf(stderr,"ERROR WHILE CALLING CPXopenCPLEX\n");
	if ( ip_prob_ptr->env == NULL ) 
	{
		char  errmsg[1024];
		fprintf (stderr, "Could not open CPLEX environment.\n");
		CPXgeterrorstring (ip_prob_ptr->env, rval, errmsg);
		fprintf (stderr, "%s", errmsg);
		exit(0);	
	}

	//We create the MIP problem
	ip_prob_ptr->lp = CPXcreateprob (ip_prob_ptr->env, &rval, "TP2");
	if(rval) fprintf(stderr,"ERROR WHILE CALLING CPXcreateprob\n");

	rval = CPXsetintparam (ip_prob_ptr->env, CPX_PARAM_DATACHECK, CPX_ON); 
	rval = CPXsetintparam (ip_prob_ptr->env, CPX_PARAM_SCRIND, CPX_ON);

	int n = dsptr->n;
	int nv = n;
	int* a = dsptr->a;
	int* f = dsptr->f;
	int* c = dsptr->c;
	int b = dsptr->b;
	int g = dsptr->g;

	//We fill our arrays
	//Memory
	ip_prob_ptr->nv = nv;
        ip_prob_ptr->x = (double*)malloc(sizeof(double)*nv);
        ip_prob_ptr->cost = (double*)malloc(sizeof(double)*nv);
        ip_prob_ptr->c_type = (char*)malloc(sizeof(char)*nv);
        ip_prob_ptr->up_bound = (double*)malloc(sizeof(double)*nv);
        ip_prob_ptr->low_bound = (double*)malloc(sizeof(double)*nv);
	ip_prob_ptr->var_name = (char**)malloc(sizeof(char*)*nv);

	int j,id = 0;
	//Structures keeping the index of each variable
	int*id_x_j = (int*)malloc(sizeof(int)*n);

	//variables xi definition
	for( j = 0 ; j < n ; j++)
	{
		//We keep the id
		id_x_j[j] = id;

		//We generate the variable attributes
		ip_prob_ptr->x[id] = 0;
		ip_prob_ptr->cost[id] = c[j];
		ip_prob_ptr->c_type[id] = 'B';
		ip_prob_ptr->up_bound[id] = 1;
		ip_prob_ptr->low_bound[id] = 0;
		ip_prob_ptr->var_name[id] = (char*)malloc(sizeof(char)*1024);
	        snprintf(       ip_prob_ptr->var_name[id],
        	                1024,
                	        "x_j%d",
                        	j);
		id++;
	}


	rval = CPXnewcols( ip_prob_ptr->env, ip_prob_ptr->lp, 
			nv, 
			ip_prob_ptr->cost, 
			ip_prob_ptr->low_bound,
			ip_prob_ptr->up_bound,
			ip_prob_ptr->c_type,
			ip_prob_ptr->var_name);
	if(rval)
		fprintf(stderr,"CPXnewcols returned errcode %d\n",rval);



	//Constraints part
        ip_prob_ptr->rhs = (double*)malloc(sizeof(double));
        ip_prob_ptr->sense = (char*)malloc(sizeof(char));
        ip_prob_ptr->rmatbeg = (int*)malloc(sizeof(int));
	ip_prob_ptr->nz = n;


        ip_prob_ptr->rmatind = (int*)malloc(sizeof(int)*nv);
        ip_prob_ptr->rmatval = (double*)malloc(sizeof(double)*nv);
	ip_prob_ptr->const_name = (char**)malloc(sizeof(char*));
	ip_prob_ptr->const_name[0] = (char*)malloc(sizeof(char)*1024);

	//We fill what we can 
	ip_prob_ptr->rmatbeg[0] = 0;

	//We generate and add each constraint to the model

	//capacity constraint #1
	ip_prob_ptr->rhs[0] = b;
	ip_prob_ptr->sense[0] = 'L';
	//Constraint name
	snprintf(       ip_prob_ptr->const_name[0],
			1024,
			"capacity_1"
			);
	id=0;
	//variables x_j coefficients
	for( j = 0 ; j < n ; j++)
	{
		ip_prob_ptr->rmatind[id] = id_x_j[j];
		ip_prob_ptr->rmatval[id] =  a[j];
		id++;
	}
	rval = CPXaddrows( ip_prob_ptr->env, ip_prob_ptr->lp, 
			0,//No new column
			1,//One new row
			n,//Number of nonzero coefficients
			ip_prob_ptr->rhs, 
			ip_prob_ptr->sense, 
			ip_prob_ptr->rmatbeg, 
			ip_prob_ptr->rmatind, 
			ip_prob_ptr->rmatval,
			NULL,//No new column
			ip_prob_ptr->const_name );
	if(rval)
		fprintf(stderr,"CPXaddrows returned errcode %d\n",rval);

	//capacity constraint 2#
	ip_prob_ptr->rhs[0] = g;
	ip_prob_ptr->sense[0] = 'L';
	//Constraint name
	snprintf(       ip_prob_ptr->const_name[0],
			1024,
			"capacity_2"
			);
	id=0;
	//variables x_j coefficients
	for( j = 0 ; j < n ; j++)
	{
		ip_prob_ptr->rmatind[id] = id_x_j[j];
		ip_prob_ptr->rmatval[id] =  f[j];
		id++;
	}
	rval = CPXaddrows( ip_prob_ptr->env, ip_prob_ptr->lp, 
			0,//No new column
			1,//One new row
			n,//Number of nonzero coefficients
			ip_prob_ptr->rhs, 
			ip_prob_ptr->sense, 
			ip_prob_ptr->rmatbeg, 
			ip_prob_ptr->rmatind, 
			ip_prob_ptr->rmatval,
			NULL,//No new column
			ip_prob_ptr->const_name );
	if(rval)
		fprintf(stderr,"CPXaddrows returned errcode %d\n",rval);

	//We switch to maximization
	rval = CPXchgobjsen( ip_prob_ptr->env, ip_prob_ptr->lp, CPX_MAX );


	//We write the problem for debugging purposes, can be commented afterwards
	rval = CPXwriteprob (ip_prob_ptr->env, ip_prob_ptr->lp, "multiDKnapsack.lp", NULL);
	if(rval)
		fprintf(stderr,"CPXwriteprob returned errcode %d\n",rval);

	//We solve the model
	rval = CPXmipopt (ip_prob_ptr->env, ip_prob_ptr->lp);
	if(rval)
		fprintf(stderr,"CPXmipopt returned errcode %d\n",rval);

	rval = CPXsolwrite( ip_prob_ptr->env, ip_prob_ptr->lp, "multiDKnapsack.sol" );
	if(rval)
		fprintf(stderr,"CPXsolwrite returned errcode %d\n",rval);

	//We get the objective value
	rval = CPXgetobjval( ip_prob_ptr->env, ip_prob_ptr->lp, &(ip_prob_ptr->objval) );
	if(rval)
		fprintf(stderr,"CPXgetobjval returned errcode %d\n",rval);

	//We get the best solution found 
	rval = CPXgetobjval( ip_prob_ptr->env, ip_prob_ptr->lp, &(ip_prob_ptr->objval) );
	rval = CPXgetx( ip_prob_ptr->env, ip_prob_ptr->lp, ip_prob_ptr->x, 0, nv-1 );
	if(rval)
		fprintf(stderr,"CPXgetx returned errcode %d\n",rval);


	/**************** FILL HERE ***************/



	return rval;
}

/* ====================================================================
   Fonction : get_time_us()
   Renvoie le temps actuel en microsecondes.
   ==================================================================== */
long long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000000LL) + tv.tv_usec;
}

/* ====================================================================
   Génération d’instances pour TP2
   Le format attendu dans le fichier :
       Ligne 1 : n,b,g
       Ensuite n lignes : c,a,f
   ==================================================================== */
void ecrireDansCSV_TP2(const char* filename, int b, int g, int n) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erreur d'ouverture du fichier %s pour écriture\n", filename);
        exit(EXIT_FAILURE);
    }
    /* Génération des données avec graine fixe pour reproductibilité */
    srand(42);
    /* Écrire la première ligne */
    fprintf(f, "%d,%d,%d\n", n, b, g);
    for (int i = 0; i < n; i++) {
        int c = rand() % 100 + 1; // Valeur entre 1 et 100
        int a = rand() % 100 + 1; // Poids entre 1 et 100
        int f_val = rand() % 100 + 1; // Frais entre 1 et 100
        fprintf(f, "%d,%d,%d\n", c, a, f_val);
    }
    fclose(f);
}

/* Fonction qui génère deux séries d'instances pour TP2 :
   - Expérience 1 : n fixé (N) et b (et g) variant.
   - Expérience 2 : b et g fixés (B et G) et n variant.
 */
void generateFiles_TP2() {
    char filename[FILENAME_SIZE];
    printf("Génération des instances TP2...\n");
    
    // Expérience 1 : n constant, b et g variant de 0 à B par pas de 20.
    for (int b_val = 0; b_val <= B; b_val += 20) {
        snprintf(filename, FILENAME_SIZE, FILENAME_PATTERN_TP2, N, b_val, b_val);
        ecrireDansCSV_TP2(filename, b_val, b_val, N);
    }
    
    // Expérience 2 : b et g fixés et n variant de 0 à N par pas de 2.
    for (int n_val = 0; n_val <= N; n_val += 2) {
        snprintf(filename, FILENAME_SIZE, FILENAME_PATTERN_TP2, n_val, B, G);
        ecrireDansCSV_TP2(filename, B, G, n_val);
    }
}

/* ====================================================================
   Mesure du temps d'exécution de solve_1DKP()
   Cette fonction lit une instance depuis le fichier donné, puis
   exécute solve_1DKP() sur l'instance en mesurant le temps.
   ==================================================================== */
long long measure_execution_time_1DKP(const char* instance_filename, dataSet* ds_ptr) {
    FILE* fin = fopen(instance_filename, "r");
    if (!fin) {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier d'instance %s\n", instance_filename);
        return -1;
    }
    if (read_TP2_instance(fin, ds_ptr) != 0) {
        fclose(fin);
        return -1;
    }
    fclose(fin);
    long long start = get_time_us();
    int rval = solve_1DKP(ds_ptr);
    long long end = get_time_us();
    if (rval != 0) {
        fprintf(stderr, "Erreur lors de la résolution par solve_1DKP sur l'instance %s\n", instance_filename);
        return -1;
    }
    return end - start;
}

/* ====================================================================
   Mesure globale et écriture des résultats de solve_1DKP dans un CSV.
   Deux séries de tests :
     - Expérience 1 : n fixé (N) et b variant (et g variant de même manière).
     - Expérience 2 : b fixé (B, G) et n variant.
   ==================================================================== */
void measure_1DKP_performance_TP2() {
    char csv_filename[] = "results_TP2_1DKP.csv";
    FILE* fp = fopen(csv_filename, "w");
    if (!fp) {
        fprintf(stderr, "Erreur à l'ouverture du fichier %s pour écriture\n", csv_filename);
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "Test,n,b,g,temps_us,objectif\n");
    
    dataSet ds;
    char instance_file[FILENAME_SIZE];
    long long elapsed;
    
    // Expérience 1 : n fixe à N, b et g variant de 0 à B (pas 20)
    for (int b_val = 0; b_val <= B; b_val += 20) {
        snprintf(instance_file, FILENAME_SIZE, FILENAME_PATTERN_TP2, N, b_val, b_val);
        elapsed = measure_execution_time_1DKP(instance_file, &ds);
        if (elapsed >= 0) {
            fprintf(fp, "n_fixé,%d,%d,%d,%lld,%.2f\n", N, b_val, b_val, elapsed, ds.master.objval);
        }
        /* Libération des tableaux alloués dans ds et fermeture de l'environnement CPLEX */
        free(ds.c);
        free(ds.a);
        free(ds.f);
        CPXfreeprob(ds.master.env, &ds.master.lp);
        CPXcloseCPLEX(&ds.master.env);
    }
    
    // Expérience 2 : b et g fixés à B et G, n variant de 0 à N (pas 2)
    for (int n_val = 0; n_val <= N; n_val += 2) {
        snprintf(instance_file, FILENAME_SIZE, FILENAME_PATTERN_TP2, n_val, B, G);
        elapsed = measure_execution_time_1DKP(instance_file, &ds);
        if (elapsed >= 0) {
            fprintf(fp, "b_fixé,%d,%d,%d,%lld,%.2f\n", n_val, B, G, elapsed, ds.master.objval);
        }
        free(ds.c);
        free(ds.a);
        free(ds.f);
        CPXfreeprob(ds.master.env, &ds.master.lp);
        CPXcloseCPLEX(&ds.master.env);
    }
    
    fclose(fp);
    printf("Résultats des performances de solve_1DKP écrits dans %s\n", csv_filename);
}