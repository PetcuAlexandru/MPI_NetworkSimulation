#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
#include <time.h>

#define SONDAJ     0
#define ECOU 	     1
#define TERMINAT 	 2
#define CONFIRMARE 3

typedef struct ruta{
		int dest;
		int nextHop;
	}RUTA;

typedef struct mesaj{
		int sursa;
		int dest;
		char msg[256];
	}MESAJ;
	
MESAJ mesaje[256];	
	
/* Fiecare nod isi citeste vecinii */	
void citireVecini(char file[],int **topologie, int rank){
		char fline[256];
		char *next;
		int vecin, i;
		
		FILE *in = fopen(file, "r");
		for(i = 0; i <= rank; i++)
			fgets(fline, 256, in);
		
		next = strtok(fline, " :\n");
		while(next != NULL){
				vecin = atoi(next);
				if(vecin != rank){
					topologie[rank][vecin] = 1;
				}
				next = strtok(NULL, " :\n");
		}
		fclose(in);
}

int distante[256], vizitat[256];
void setVizitat(){
	int i;
	for(i = 0; i < 256; i++){
		vizitat[i] = 0;
	}
}

int eVizitat(int nod){
	return vizitat[nod];
}

/* Calcularea tabelei de rutare */
int nextHop = -1;
int next = -1;
int sursa;
void calculRute(int **topologie, int source, int dest, int np){
	int i;
	distante[dest]++;
	
	if(topologie[source][dest] > 0 ){
		if(nextHop == -1)
			nextHop = dest;
		next = nextHop;	
		return;
	}		
	else{
		vizitat[source] = 1;
		for(i = 0; i < np; i++)
			if(topologie[source][i] == 1 && !eVizitat(i)){
				if(topologie[sursa][i])
					nextHop = i;
				vizitat[i] = 1;
				calculRute(topologie, i, dest, np);
			}
	}
}

int citireMesaje(char file[]){
	int i, nrMsj;
	char fline[256], next[10];
	
	
	FILE *in = fopen(file, "r");
	fscanf(in, "%d", &nrMsj);
	
	for(i = 0; i < nrMsj; i++){
		fscanf(in,"%s", next);
		mesaje[i].sursa = atoi(next);
		fscanf(in,"%s", next);
		if(next[0] == 'B')
			mesaje[i].dest = mesaje[i].sursa + 1;
		else	
			mesaje[i].dest = atoi(next);
		fgets(mesaje[i].msg, 256, in);
		mesaje[i].msg[strlen(mesaje[i].msg) - 1] = 0;
	}
	
	fclose(in);
	
	return nrMsj;
}


int main(int argc, char*argv[]){
	
	int i, j;
	int rank, np, tag = 11;
	
	MPI_Status status;
	MPI_Request req;
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &np);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if(rank == 0){ // Procesul Master-------------------------------------------/
		
		int h;	
		/* Crearea si alocarea de memorie pentru matricea de adiacenta */
		int **topologie;
		int *line;
	
		line = (int *)malloc(np * np * sizeof(int));
		topologie = (int**)malloc(np * sizeof(int *));
		for(h = 0; h < np; h++)
				topologie[h] = &(line[np * h]);
		
		for(i = 0; i < np; i++)
				for(j = 0; j < np; j++)
						topologie[i][j] = 0;
				
		/* Citirea datelor din fisierul de intrare */
		citireVecini(argv[1], topologie, rank);
		
		/* Trimite mesaj de tip SONDAJ la nodurile copii */
		for(i = 1; i < np; i++)
			if(topologie[rank][i] != 0){
				MPI_Send(&rank, 1, MPI_INT, i, SONDAJ, MPI_COMM_WORLD);
			}
		
		/* Crearea matricii in care primesc datele de la nodurile copii */
		int **topologieCopil;
		int *linie;
				
		linie = (int *)malloc(np * np * sizeof(int));
		topologieCopil = (int**)malloc(np * sizeof(int *));
		for(h = 0; h < np; h++)
				topologieCopil[h] = &(linie[np * h]);
			
						
		/* Primeste ECOUri de la nodurile copil */
		for(h = 1; h < np; h++)
			if(topologie[rank][h] != 0){
				MPI_Recv(&(topologieCopil[0][0]), np * np, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				if(status.MPI_TAG == ECOU){
					for(i = 0; i < np; i++)
						for(j = 0; j < np; j++)
							topologie[i][j] = topologie[i][j] | topologieCopil[i][j];
				}
				else{ //e tot sondaj
					int sursa = status.MPI_SOURCE;
					MPI_Send(&(topologie[0][0]), np * np, MPI_INT, sursa, ECOU, MPI_COMM_WORLD);
					h--;
				}
			}
		
		
		/* Trimite la copii topologia completa */
		for(i = 0; i < np; i++)
			if(topologie[rank][i] == 1){
				MPI_Send(&(topologie[0][0]), np * np, MPI_INT, i, SONDAJ, MPI_COMM_WORLD);
			}
		
		/* Determinarea tabelei de rutare */
		RUTA rute[np];
		setVizitat();
		
		for(i = 0; i < np; i++)
			if(rank != i){
					sursa = rank;
					calculRute(topologie, rank, i, np);
					if(nextHop == -1 || next == -1){
						rute[i].dest = i;
						for(h = 0; h < np; h++)
							if(topologie[rank][h] == 3)
								rute[i].nextHop = h;
					}
					else{
						rute[i].dest = i;
						rute[i].nextHop = next;
					}
						
					nextHop = -1;			
					next = -1;	
					setVizitat();
			}
			
		printf("\n");
		printf("Tabela nod: %d\n", rank);
		for(i = 0; i < np; i++){
			if(i != rank)
				printf("\t\tDestinatie: %d Next hop: %d\n", i, rute[i].nextHop);
		}		
		printf("\n");
			
		/* Citirea fisierului cu mesaje */
		char terminat[1256] = "terminat";
		char mesajPrimit[1256], mesajTrimis[1256];
		int indexMesaje = 0;
		int numarMesaje = citireMesaje(argv[2]);
		int oki = numarMesaje;
		
		/* Trimite mesaje la destinatie */
		for(i = 0; i < numarMesaje; i++)
			if(mesaje[i].sursa == rank){
				strcpy(mesajTrimis, mesaje[i].msg);
				if(rute[mesaje[i].dest].nextHop >= 0 && rute[mesaje[i].dest].nextHop < 12){
					printf("Am trimis mesajul: %s cu sursa %d , destinatie %d, nextHop %d", 
								mesajTrimis, rank, mesaje[i].dest, rute[mesaje[i].dest].nextHop);				
					MPI_Send(mesajTrimis, 256, MPI_CHAR, rute[mesaje[i].dest].nextHop, mesaje[i].dest + 1000, MPI_COMM_WORLD);
				}
			}
			
		/* Si-a trimis toate mesajele, trimite mesaj de terminare la toate nodurile */
		
		while(oki){
				MPI_Recv(mesajPrimit, 1256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				int rtag = status.MPI_TAG;
				if(rtag > 1000){
					if(status.MPI_TAG - 1000 == rank && strcmp(mesajPrimit,"terminat") != 0){
						printf("Mesajul: %s a ajuns la destinatie %d\n", mesajPrimit, rank);
						indexMesaje++;
						for(i = 0; i < np; i++)
							if(rank != i)
								MPI_Send(terminat, 256, MPI_CHAR, rute[i].nextHop, i + 1000, MPI_COMM_WORLD);
					}
					else if(status.MPI_TAG - 1000 != rank && strcmp(mesajPrimit,"terminat") != 0){
						printf("Mesajul: %s cu destinatia %d a trecut prin nodul %d \n",
										mesajPrimit, status.MPI_TAG - 1000, rank);
						MPI_Send(mesajPrimit, 256, MPI_CHAR, rute[status.MPI_TAG - 1000].nextHop, status.MPI_TAG, MPI_COMM_WORLD);				
					}
					else if(status.MPI_TAG - 1000 != rank && strcmp(mesajPrimit,"terminat") == 0){
						MPI_Send(mesajPrimit, 256, MPI_CHAR, rute[status.MPI_TAG - 1000].nextHop, status.MPI_TAG, MPI_COMM_WORLD);				
					}
					else if(status.MPI_TAG - 1000 == rank && strcmp(mesajPrimit,"terminat") == 0){
						indexMesaje++;	
						oki--;
					}
					if(indexMesaje >= numarMesaje)
						break;
			 }
		}
	}
	else{// Celelalte procese--------------------------------------------------/			
			
		/* Crearea si alocarea de memorie pentru matricea de adiacenta */
		int h;
		int **topologie;
		int *line;
	
		line = (int *)malloc(np * np * sizeof(int));
		topologie = (int**)malloc(np * sizeof(int *));
		for(h = 0; h < np; h++)
				topologie[h] = &(line[np * h]);
			
		for(i = 0; i < np; i++)
				for(j = 0; j < np; j++)
						topologie[i][j] = 0;
							
		/* Citirea datelor din fisierul de intrare */
		citireVecini(argv[1], topologie, rank);
						
	
		/* Primeste mesaj de tip sondaj de la nodul parinte */
		int parent;
		MPI_Recv(&parent, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		topologie[rank][parent] = 3;
		
		/* Trimite mesaj de tip SONDAJ la nodurile copii */
		int mesaj = 0;
		for(i = 0; i < np; i++)
			if(topologie[rank][i] == 1){
				MPI_Send(&rank, 1, MPI_INT, i, SONDAJ, MPI_COMM_WORLD);
			}
				
		/* Crearea matricii in care primesc datele de la nodurile copii */
		int **topologieCopil;
		int *linie;
				
		linie = (int *)malloc(np * np * sizeof(int));
		topologieCopil = (int**)malloc(np * sizeof(int *));
		for(h = 0; h < np; h++)
				topologieCopil[h] = &(linie[np * h]);
						
		/* Primeste ECOUri de la nodurile copil */
		for(h = 1; h < np; h++)
			if(topologie[rank][h] == 1){		
				MPI_Recv(&(topologieCopil[0][0]), np * np, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				if(status.MPI_TAG == ECOU){
					for(i = 0; i < np; i++)
						for(j = 0; j < np; j++)
							topologie[i][j] = topologie[i][j] | topologieCopil[i][j];
				}
				else{ //e tot sondaj
					int sursa = status.MPI_SOURCE;
					MPI_Send(&(topologie[0][0]), np * np, MPI_INT, sursa, ECOU, MPI_COMM_WORLD);
					h--;
				}
			}
			
		/* Trimite la parinte topologia determinata de acest nod */
		for(i = 0; i < np; i++)
			if(topologie[rank][i] == 3)
				MPI_Send(&(topologie[0][0]), np * np, MPI_INT, i, ECOU, MPI_COMM_WORLD);	
				
		/* Primeste de la nodul parinte topologia completa si o da mai departe */
		MPI_Recv(&(topologie[0][0]), np * np, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		
		for(i = 0; i < np; i++)
			if(topologie[rank][i] == 1){
				MPI_Send(&(topologie[0][0]), np * np, MPI_INT, i, SONDAJ, MPI_COMM_WORLD);
			}
		
		for(i = 0; i < 256; i++){
			distante[i] = 0;
		}
		/* Determinarea tabelei de rutare */
		RUTA rute[np];
		setVizitat();
		
		for(i = 0; i < np; i++)
			if(rank != i){
					sursa = rank;
					calculRute(topologie, rank, i, np);
					if(nextHop == -1 || next == -1){
						rute[i].dest = i;
						for(h = 0; h < np; h++)
							if(topologie[rank][h] == 3)
								rute[i].nextHop = h;
					}
					else{
						rute[i].dest = i;
						rute[i].nextHop = next;
					}
						
					nextHop = -1;			
					next = -1;	
					setVizitat();
			}
		printf("\n");
		printf("Tabela nod: %d\n", rank);
		for(i = 0; i < np; i++){
			if(i != rank)
				printf("\t\tDestinatie: %d Next hop: %d\n", i, rute[i].nextHop);
		}		
		printf("\n");
		
		/* Citirea fisierului cu mesaje */
		char terminat[1256] = "terminat";
		char mesajPrimit[1256], mesajTrimis[1256];
		int indexMesaje = 0;
		int numarMesaje = citireMesaje(argv[2]);
		int oki = numarMesaje;
		
		/* Trimite mesaje la destinatie */
		for(i = 0; i < numarMesaje; i++)
			if(mesaje[i].sursa == rank){
				strcpy(mesajTrimis, mesaje[i].msg);
				if(rute[mesaje[i].dest].nextHop >= 0 && rute[mesaje[i].dest].nextHop < 12){
					printf("Am trimis mesajul: %s cu sursa %d , destinatie %d, nextHop %d \n", 
								mesajTrimis, rank, mesaje[i].dest, rute[mesaje[i].dest].nextHop);				
					MPI_Send(mesajTrimis, 256, MPI_CHAR, rute[mesaje[i].dest].nextHop, mesaje[i].dest + 1000, MPI_COMM_WORLD);
				}
			}
		
		while(oki){
				MPI_Recv(mesajPrimit, 1256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				int rtag = status.MPI_TAG;
				if(rtag > 1000){
					if(status.MPI_TAG - 1000 == rank && strcmp(mesajPrimit,"terminat") != 0){
						printf("Mesajul: %s a ajuns la destinatie %d\n", mesajPrimit, rank);
						indexMesaje++;
						for(i = 0; i < np; i++)
							if(rank != i){
									MPI_Send(terminat, 256, MPI_CHAR, rute[i].nextHop, i + 1000, MPI_COMM_WORLD);
							}
					}
					else if(status.MPI_TAG - 1000 != rank && strcmp(mesajPrimit,"terminat") != 0){
						printf("Mesajul: %s cu destinatia %d a trecut prin nodul %d \n",
										mesajPrimit, status.MPI_TAG - 1000, rank);
						MPI_Send(mesajPrimit, 256, MPI_CHAR, rute[status.MPI_TAG - 1000].nextHop, status.MPI_TAG, MPI_COMM_WORLD);				
					}
					else if(status.MPI_TAG - 1000 != rank && strcmp(mesajPrimit,"terminat") == 0){
						MPI_Send(mesajPrimit, 256, MPI_CHAR, rute[status.MPI_TAG - 1000].nextHop, status.MPI_TAG, MPI_COMM_WORLD);
					}
					else if(status.MPI_TAG - 1000 == rank && strcmp(mesajPrimit,"terminat") == 0){
						indexMesaje++;	
						oki--;
					}
					if(indexMesaje >= numarMesaje)
						break;
			 }
		}
	}
	
	MPI_Finalize();
	return 0;
}

