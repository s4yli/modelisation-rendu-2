#include "TP2Functions.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdio.h>
#include <ilcplex/cplex.h>

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

