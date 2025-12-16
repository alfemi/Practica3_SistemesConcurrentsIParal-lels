/* --------------------------------------------------------------- 
Práctica 3.  
Código fuente: CalcArbolesConc.c  
Doble Grado GEIADE 
41533494W Antonio Cayuela Lopez. 
48054965F Alejandro Fernandez Mimbrera.
--------------------------------------------------------------- */ 

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "CalcArboles.h"

// constante para imprimir estadisticas cada S combinaciones evaluadas por cada hilo
#define S 1000000

static int NumHilos = 1;
static TEstadisticas *EstadisticasHilos = NULL;

// mutex para el optimo global final
static pthread_mutex_t mtx_optimo = PTHREAD_MUTEX_INITIALIZER;
static int MejorCosteGlobal = DMaximoCoste;
static int MejorNumTaladosGlobal = DMaxArboles;
static int MejorCombinacionGlobal = 0;

// variables de condicion
static pthread_mutex_t mtx_fin = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv_fin  = PTHREAD_COND_INITIALIZER;
static int HilosTerminados = 0;


static sem_t *sem_start = SEM_FAILED;
static char SEM_START_NAME[64] = {0};  

//para el optimo parcial
static sem_t *sem_parcial = SEM_FAILED;
static char SEM_PARCIAL_NAME[64] = {0};

// contador para la S
static int CombinacionesGlobales = 0;

// parcial global
static int MejorCosteParcial = DMaximoCoste;
static int MejorNumTaladosParcial = DMaxArboles;
static int MejorCombinacionParcial = 0;


int main(int argc, char *argv[])
{
    TListaArboles Optimo;

    if (argc < 3 || argc > 4) {
        printf("Error Argumentos. Usage: CalcArbolesConc <Fichero_Entrada> <num_hilos> [<Fichero_Salida>]\n");
        exit(1);
    }

    NumHilos = atoi(argv[2]);
    if (NumHilos < 1) {
        printf("Numero de hilos invalido, tiene que ser > 0\n");
        exit(1);
    }

    if (!LeerFicheroEntrada(argv[1])) {
        printf("Error lectura fichero entrada.\n");
        exit(1);
    }

    if (!CalcularCercaOptima(&Optimo)) {
        printf("Error CalcularCercaOptima.\n");
        exit(1);
    }

    printf("\n[CONCURRENTE] Calculo cerca optima %d arboles con %d hilos: Tiempo: %05.6f.\n",
           ArbolesEntrada.NumArboles, NumHilos, elapsed_sec);

    printf("\n==================== Estadísticas por hilo ====================\n");
    for (int i = 0; i < NumHilos; i++) {
        char titulo[64];
        snprintf(titulo, sizeof(titulo), "Hilo %d", i);
        PrintEstadisticas(EstadisticasHilos[i], titulo);
    }
    printf("===============================================================\n");

    PrintEstadisticas(TotalEstadisticas, "Estadisticas Globales Finales");

    printf("Solucion:\n");
    PrintResultado(Optimo);

    if (argc == 3) {
        if (!GenerarFicheroSalida(Optimo, "./Valla.res")) {
            printf("Error GenerarFicheroSalida.\n");
            exit(1);
        }
    } else {
        if (!GenerarFicheroSalida(Optimo, argv[3])) {
            printf("Error GenerarFicheroSalida.\n");
            exit(1);
        }
    }

    free(EstadisticasHilos);
    EstadisticasHilos = NULL;

    return 0;
}

bool LeerFicheroEntrada(char *PathFicIn)
{
	FILE *FicIn;
	int a;

	FicIn=fopen(PathFicIn,"r");
	if (FicIn==NULL)
	{
		perror("Lectura Fichero entrada.");
		return false;
	}
	printf("Datos Entrada:\n");

	// Leemos el nmero de arboles del bosque de entrada.
	if (fscanf(FicIn, "%d", &(ArbolesEntrada.NumArboles))<1)
	{
		perror("Lectura arboles entrada");
		return false;
	}
	printf("\tArboles: %d.\n",ArbolesEntrada.NumArboles);

	// Leer atributos arboles.
	for(a=0;a<ArbolesEntrada.NumArboles;a++)
	{
		ArbolesEntrada.Arboles[a].IdArbol=a+1;
		// Leer x, y, Coste, Longitud.
		if (fscanf(FicIn, "%d %d %d %d",&(ArbolesEntrada.Arboles[a].Coord.x), &(ArbolesEntrada.Arboles[a].Coord.y), &(ArbolesEntrada.Arboles[a].Valor), &(ArbolesEntrada.Arboles[a].Longitud))<4)
		{
			perror("Lectura datos arbol.");
			return false;
		}
		printf("\tArbol %d-> (%d,%d) Coste:%d, Longitud:%d.\n",a+1,ArbolesEntrada.Arboles[a].Coord.x, ArbolesEntrada.Arboles[a].Coord.y, ArbolesEntrada.Arboles[a].Valor, ArbolesEntrada.Arboles[a].Longitud);
	}

	return true;
}



bool GenerarFicheroSalida(TListaArboles Optimo, char *PathFicOut)
{
	FILE *FicOut;

	FicOut=fopen(PathFicOut,"w+");
	if (FicOut==NULL)
	{
		perror("Escritura fichero salida.");
		return false;
	}

	return GenerarFicheroSalidaFile(Optimo, FicOut);
}


void PrintResultado(TListaArboles Optimo)
{
    GenerarFicheroSalidaFile(Optimo, stdout);
}


bool GenerarFicheroSalidaFile(TListaArboles Optimo, FILE *FicOut)
{
	int a;

	// Escribir arboles a talartalado.
	// Escribimos nmero de arboles a talar.
	if (fprintf(FicOut, "Cortar %d arbol/es: ", Optimo.NumArboles)<1)
	{
		perror("Escribir numero de arboles a talar");
		return false;
	}

	for(a=0;a<Optimo.NumArboles;a++)
	{
		// Escribir nmero arbol.
		if (fprintf(FicOut, "%d ",ArbolesEntrada.Arboles[Optimo.Arboles[a]].IdArbol)<1)
		{
			perror("Escritura numero arbol.");
			return false;
		}
	}

	// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nMadera Sobrante: %4.2f (%4.2f)", Optimo.MaderaSobrante,  Optimo.LongitudCerca)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

	// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nValor arboles cortados: %4.2f.", Optimo.CosteArbolesCortados)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

	// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nValor arboles restantes: %4.2f\n", Optimo.CosteArbolesRestantes)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

	return true;
}



void OrdenarArboles(void)
{
  int a,b;
  
	for(a=0; a<(ArbolesEntrada.NumArboles-1); a++)
	{
		for(b=a+1; b<ArbolesEntrada.NumArboles; b++)
		{
			if ( ArbolesEntrada.Arboles[b].Coord.x < ArbolesEntrada.Arboles[a].Coord.x ||
				 (ArbolesEntrada.Arboles[b].Coord.x == ArbolesEntrada.Arboles[a].Coord.x && ArbolesEntrada.Arboles[b].Coord.y < ArbolesEntrada.Arboles[a].Coord.y) )
			{
				TArbol aux;

				// aux=a
				aux.Coord.x = ArbolesEntrada.Arboles[a].Coord.x;
				aux.Coord.y = ArbolesEntrada.Arboles[a].Coord.y;
				aux.IdArbol = ArbolesEntrada.Arboles[a].IdArbol;
				aux.Valor = ArbolesEntrada.Arboles[a].Valor;
				aux.Longitud = ArbolesEntrada.Arboles[a].Longitud;

				// a=b
				ArbolesEntrada.Arboles[a].Coord.x = ArbolesEntrada.Arboles[b].Coord.x;
				ArbolesEntrada.Arboles[a].Coord.y = ArbolesEntrada.Arboles[b].Coord.y;
				ArbolesEntrada.Arboles[a].IdArbol = ArbolesEntrada.Arboles[b].IdArbol;
				ArbolesEntrada.Arboles[a].Valor = ArbolesEntrada.Arboles[b].Valor;
				ArbolesEntrada.Arboles[a].Longitud = ArbolesEntrada.Arboles[b].Longitud;

				// b=aux
				ArbolesEntrada.Arboles[b].Coord.x = aux.Coord.x;
				ArbolesEntrada.Arboles[b].Coord.y = aux.Coord.y;
				ArbolesEntrada.Arboles[b].IdArbol = aux.IdArbol;
				ArbolesEntrada.Arboles[b].Valor = aux.Valor;
				ArbolesEntrada.Arboles[b].Longitud = aux.Longitud;
			}
		}
	}
}


int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles)
{
	int arbol=0;

	CombinacionArboles->NumArboles=0;
	CombinacionArboles->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==0)
		{
			CombinacionArboles->Arboles[CombinacionArboles->NumArboles]=arbol;
			CombinacionArboles->NumArboles++;
			CombinacionArboles->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArboles->NumArboles;
}


int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados)
{
	int arbol=0;

	CombinacionArbolesTalados->NumArboles=0;
	CombinacionArbolesTalados->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==1)
		{
			CombinacionArbolesTalados->Arboles[CombinacionArbolesTalados->NumArboles]=arbol;
			CombinacionArbolesTalados->NumArboles++;
			CombinacionArbolesTalados->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArbolesTalados->NumArboles;
}


void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas)
{
	int c, arbol;

	for (c=0;c<CombinacionArboles.NumArboles;c++)
	{
        arbol=CombinacionArboles.Arboles[c];
		Coordenadas[c].x = ArbolesEntrada.Arboles[arbol].Coord.x;
		Coordenadas[c].y = ArbolesEntrada.Arboles[arbol].Coord.y;
	}
}


float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca)
{
	int x;
	float coste=0;
	
	for (x=0;x<(SizeCerca-1);x++)
	{
		coste+= CalcularDistancia(CoordenadasCerca[x].x, CoordenadasCerca[x].y, CoordenadasCerca[x+1].x, CoordenadasCerca[x+1].y);
	}
	
	return coste;
}


float CalcularDistancia(int x1, int y1, int x2, int y2)
{
	return(sqrt(pow((double)abs(x2-x1),2.0)+pow((double)abs(y2-y1),2.0)));
}


int CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles)
{
	int a;
	int LongitudTotal=0;
	
	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		LongitudTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Longitud;
	}
	
	return(LongitudTotal);
}


int CalcularCosteCombinacion(TListaArboles CombinacionArboles)
{
	int a;
	int CosteTotal=0;
	
	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		CosteTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Valor;
	}
	
	return(CosteTotal);
}


void MostrarBosque(void)
{
    int a;

    for (a=0;a<ArbolesEntrada.NumArboles;a++)
        PrintArbol(ArbolesEntrada.Arboles[a]);
}

void PrintArbol(TArbol arbol)
{
    printf("Arbol Id:%d --> x:%d,y:%d longitud:%d valor:%d.\n",arbol.IdArbol, arbol.Coord.x, arbol.Coord.y, arbol.Longitud, arbol.Valor);
}


void MostrarArboles(TListaArboles CombinacionArboles)
{
	int a;

	for (a=0;a<CombinacionArboles.NumArboles;a++)
		printf("%d ",ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].IdArbol);

  for (;a<ArbolesEntrada.NumArboles;a++)
    printf("  ");  
}

void ResetEstadidisticas(PtrEstadisticas std)
{
	memset(std, 0, sizeof(TEstadisticas));
	std->MejorCombinacionCoste = DMaximoCoste;
	std->MejorCombinacionArboles = DMaxArboles;
}

void PrintEstadisticas(TEstadisticas estadisticas, char *tipo)
{
	printf("\n++ %s +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n",tipo);
	printf("++ Comb Evaluadas: %d \tComb Validas: %d \tComb Invalidas:    %d\n",
						estadisticas.CombinacionesEvaluadas, estadisticas.CombinacionesValidas, estadisticas.CombinacionesNoValidas);
	printf("++ Mejor Coste:    %d \t\tPeor Coste:   %d  \tMejor num arboles: %d \tPeor num arboles: %d\n",
					estadisticas.MejorCombinacionCoste, estadisticas.PeorCombinacionCoste,
					estadisticas.MejorCombinacionArboles, estadisticas.PeorCombinacionArboles);
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

}


// evaluacion local que hace cada hilo
static int EvaluarCombinacionLocal(int Combinacion, TEstadisticas *est, int *outNumTalados)
{
    TVectorCoordenadas CoordArboles, CercaArboles;
    TListaArboles CombinacionArboles, CombinacionArbolesTalados;
    int NumArboles, NumArbolesTalados, PuntosCerca, CosteCombinacion;
    float LongitudCerca, MaderaArbolesTalados;

    // Convertimos la combinacin al vector de arboles no talados.
    NumArboles = ConvertirCombinacionToArboles(Combinacion, &CombinacionArboles);

    // Obtener el vector de coordenadas de arboles no talados.
    ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);
	
    // Calcular la cerca
    PuntosCerca = chainHull_2D(CoordArboles, NumArboles, CercaArboles);
    
    // Evaluar si obtenemos suficientes �boles para construir la cerca 
    LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);

    // Evaluar la madera obtenida mediante los arboles talados.
    NumArbolesTalados = ConvertirCombinacionToArbolesTalados(Combinacion, &CombinacionArbolesTalados);
    if (outNumTalados) *outNumTalados = NumArbolesTalados;

    // Evaluar el coste de los arboles talados.
    MaderaArbolesTalados = (float)CalcularMaderaArbolesTalados(CombinacionArbolesTalados);

    if (LongitudCerca > MaderaArbolesTalados) {
        est->CombinacionesNoValidas++;
        return DMaximoCoste;
    }

    CosteCombinacion = CalcularCosteCombinacion(CombinacionArbolesTalados);

    // Actualizar estadisticas locales del hilo
    est->CombinacionesValidas++;
    if (est->MejorCombinacionCoste > CosteCombinacion) est->MejorCombinacionCoste = CosteCombinacion;
    if (est->PeorCombinacionCoste < CosteCombinacion)  est->PeorCombinacionCoste  = CosteCombinacion;
    if (est->MejorCombinacionArboles > NumArbolesTalados) est->MejorCombinacionArboles = NumArbolesTalados;
    if (est->PeorCombinacionArboles < NumArbolesTalados)  est->PeorCombinacionArboles  = NumArbolesTalados;

    return CosteCombinacion;
}


// resultado optimo global
static void ActualizarOptimoGlobal(int comb, int coste, int numTalados)
{
    pthread_mutex_lock(&mtx_optimo);

    if (MejorCombinacionGlobal == 0 ||
        coste < MejorCosteGlobal ||
        (coste == MejorCosteGlobal && numTalados < MejorNumTaladosGlobal) ||
        (coste == MejorCosteGlobal && numTalados == MejorNumTaladosGlobal && comb < MejorCombinacionGlobal))
    {
        MejorCosteGlobal = coste;
        MejorNumTaladosGlobal = numTalados;
        MejorCombinacionGlobal = comb;
    }

    pthread_mutex_unlock(&mtx_optimo);
}


// hilos
typedef struct {
    int id;
    int inicio;   // inicio rango combinaciones a evaluar
    int fin;      // fin rango combinaciones a evaluar
} TArgsHilo;


static void NotificarFin(void)
{
    pthread_mutex_lock(&mtx_fin);
    HilosTerminados++;
    pthread_cond_signal(&cv_fin);
    pthread_mutex_unlock(&mtx_fin);
}

static void *HiloEvaluador(void *ptr)
{
    TArgsHilo *a = (TArgsHilo *)ptr;
    TEstadisticas *st = &EstadisticasHilos[a->id];

    // arrancar
    if (sem_start != SEM_FAILED) {
        while (sem_wait(sem_start) == -1 && errno == EINTR) { // volver a intentar 
        }
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ResetEstadidisticas(st);

    int mejorLocalCoste = DMaximoCoste;
    int mejorLocalNumTalados = DMaxArboles;
    int mejorLocalComb = 0;

    // Para sumar al global solo el bloque nuevo
    int last_eval = 0;
    int last_val  = 0;
    int last_inv  = 0;

    for (int comb = a->inicio; comb < a->fin; comb++) {

        int numTalados = 0;
        int coste = EvaluarCombinacionLocal(comb, st, &numTalados);
        st->CombinacionesEvaluadas++;

        // mejor comb local
        if (mejorLocalComb == 0 ||
            coste < mejorLocalCoste ||
            (coste == mejorLocalCoste && numTalados < mejorLocalNumTalados) ||
            (coste == mejorLocalCoste && numTalados == mejorLocalNumTalados && comb < mejorLocalComb))
        {
            mejorLocalCoste = coste;
            mejorLocalNumTalados = numTalados;
            mejorLocalComb = comb;
        }

        // Cada S combinaciones actualizar el global parcial y hacer print
        if ((st->CombinacionesEvaluadas % S) == 0) {

            // semaforo para el resultado parcial 
            while (sem_wait(sem_parcial) == -1 && errno == EINTR) { // volver a intentar 
                }

            // contador global
            CombinacionesGlobales += S;

            // sumar a TotalEstadisticas 
            int now_eval = st->CombinacionesEvaluadas;
            int now_val  = st->CombinacionesValidas;
            int now_inv  = st->CombinacionesNoValidas;

            TotalEstadisticas.CombinacionesEvaluadas += (now_eval - last_eval);
            TotalEstadisticas.CombinacionesValidas   += (now_val  - last_val);
            TotalEstadisticas.CombinacionesNoValidas += (now_inv  - last_inv);

            // mejor/peor global 
            if (TotalEstadisticas.MejorCombinacionCoste > st->MejorCombinacionCoste)
                TotalEstadisticas.MejorCombinacionCoste = st->MejorCombinacionCoste;
            if (TotalEstadisticas.PeorCombinacionCoste < st->PeorCombinacionCoste)
                TotalEstadisticas.PeorCombinacionCoste = st->PeorCombinacionCoste;

            if (TotalEstadisticas.MejorCombinacionArboles > st->MejorCombinacionArboles)
                TotalEstadisticas.MejorCombinacionArboles = st->MejorCombinacionArboles;
            if (TotalEstadisticas.PeorCombinacionArboles < st->PeorCombinacionArboles)
                TotalEstadisticas.PeorCombinacionArboles = st->PeorCombinacionArboles;

            last_eval = now_eval;
            last_val  = now_val;
            last_inv  = now_inv;

            // actualizar optimo parcial global con mejor local del hilo
            if (mejorLocalComb != 0 &&
                (MejorCombinacionParcial == 0 ||
                 mejorLocalCoste < MejorCosteParcial ||
                 (mejorLocalCoste == MejorCosteParcial && mejorLocalNumTalados < MejorNumTaladosParcial) ||
                 (mejorLocalCoste == MejorCosteParcial &&
                  mejorLocalNumTalados == MejorNumTaladosParcial &&
                  mejorLocalComb < MejorCombinacionParcial)))
            {
                MejorCosteParcial = mejorLocalCoste;
                MejorNumTaladosParcial = mejorLocalNumTalados;
                MejorCombinacionParcial = mejorLocalComb;
            }

            // imprimir
            TListaArboles OptimoParcial;
            ConvertirCombinacionToArbolesTalados(MejorCombinacionParcial, &OptimoParcial);

            printf("\t[%d] OptimoParcial %d-> Coste %d, %d Arboles talados:",
                   CombinacionesGlobales,
                   MejorCombinacionParcial,
                   MejorCosteParcial,
                   OptimoParcial.NumArboles);

            MostrarArboles(OptimoParcial);
            PrintEstadisticas(TotalEstadisticas, "Estadisticas Globales");

            sem_post(sem_parcial);
        }

        if ((st->CombinacionesEvaluadas % 4096) == 0) {
            pthread_testcancel();
        }
    }

    // optimo final global
    if (mejorLocalComb != 0 && mejorLocalCoste < DMaximoCoste) {
        ActualizarOptimoGlobal(mejorLocalComb, mejorLocalCoste, mejorLocalNumTalados);
    }

    NotificarFin();
    return NULL;
}


// combinar todas las estadisticas de todos los hilos
static void FusionarEstadisticasGlobales(void)
{
    ResetEstadidisticas(&TotalEstadisticas);

    for (int i = 0; i < NumHilos; i++) {
        TEstadisticas *s = &EstadisticasHilos[i];

        TotalEstadisticas.CombinacionesEvaluadas += s->CombinacionesEvaluadas;
        TotalEstadisticas.CombinacionesValidas   += s->CombinacionesValidas;
        TotalEstadisticas.CombinacionesNoValidas += s->CombinacionesNoValidas;

        if (TotalEstadisticas.MejorCombinacionCoste > s->MejorCombinacionCoste)
            TotalEstadisticas.MejorCombinacionCoste = s->MejorCombinacionCoste;
        if (TotalEstadisticas.PeorCombinacionCoste < s->PeorCombinacionCoste)
            TotalEstadisticas.PeorCombinacionCoste = s->PeorCombinacionCoste;

        if (TotalEstadisticas.MejorCombinacionArboles > s->MejorCombinacionArboles)
            TotalEstadisticas.MejorCombinacionArboles = s->MejorCombinacionArboles;
        if (TotalEstadisticas.PeorCombinacionArboles < s->PeorCombinacionArboles)
            TotalEstadisticas.PeorCombinacionArboles = s->PeorCombinacionArboles;
    }
}


// control de errores
static void CancelarHilos(pthread_t *th, int creados)
{
    for (int i = 0; i < creados; i++) {
        pthread_cancel(th[i]);
    }
}


// calculo optimo concurrente
bool CalcularCercaOptima(PtrListaArboles Optimo)
{
    struct timespec start, finish;

    int MaxCombinaciones = (int)pow(2.0, ArbolesEntrada.NumArboles) - 1;
    if (MaxCombinaciones <= 0) return false;

    OrdenarArboles();
    MostrarBosque();

    if (NumHilos > MaxCombinaciones) NumHilos = MaxCombinaciones;
    if (NumHilos < 1) NumHilos = 1;

    // reset globales
    MejorCosteGlobal = DMaximoCoste;
    MejorNumTaladosGlobal = DMaxArboles;
    MejorCombinacionGlobal = 0;

    MejorCosteParcial = DMaximoCoste;
    MejorNumTaladosParcial = DMaxArboles;
    MejorCombinacionParcial = 0;

    CombinacionesGlobales = 0;
    ResetEstadidisticas(&TotalEstadisticas);

    // semaforos
    snprintf(SEM_START_NAME, sizeof(SEM_START_NAME), "/ca_start_%d", getpid());
    snprintf(SEM_PARCIAL_NAME, sizeof(SEM_PARCIAL_NAME), "/ca_parcial_%d", getpid());

    // semaforo para los parciales
    sem_unlink(SEM_PARCIAL_NAME);
    sem_parcial = sem_open(SEM_PARCIAL_NAME, O_CREAT | O_EXCL, 0600, 1);
    if (sem_parcial == SEM_FAILED) {
        perror("sem_open (parcial)");
        return false;
    }

    pthread_mutex_lock(&mtx_fin);
    HilosTerminados = 0;
    pthread_mutex_unlock(&mtx_fin);

    EstadisticasHilos = (TEstadisticas *)malloc(sizeof(TEstadisticas) * NumHilos);
    if (!EstadisticasHilos) {
        fprintf(stderr, "Error reservando memoria para EstadisticasHilos.\n");
        sem_close(sem_parcial);
        sem_unlink(SEM_PARCIAL_NAME);
        sem_parcial = SEM_FAILED;
        return false;
    }

    pthread_t *th = (pthread_t *)malloc(sizeof(pthread_t) * NumHilos);
    TArgsHilo *args = (TArgsHilo *)malloc(sizeof(TArgsHilo) * NumHilos);
    if (!th || !args) {
        fprintf(stderr, "Error reservando memoria para hilos/args.\n");
        free(EstadisticasHilos);
        EstadisticasHilos = NULL;

        sem_close(sem_parcial);
        sem_unlink(SEM_PARCIAL_NAME);
        sem_parcial = SEM_FAILED;

        return false;
    }

    
    sem_unlink(SEM_START_NAME);
    sem_start = sem_open(SEM_START_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (sem_start == SEM_FAILED) {
        perror("sem_open (start)");

        free(th);
        free(args);
        free(EstadisticasHilos);
        EstadisticasHilos = NULL;

        sem_close(sem_parcial);
        sem_unlink(SEM_PARCIAL_NAME);
        sem_parcial = SEM_FAILED;

        return false;
    }

    int total = MaxCombinaciones;
    int base  = total / NumHilos;
    int extra = total % NumHilos;

    int actual = 1;
    int creados = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NumHilos; i++) {
        int tam = base + (i < extra ? 1 : 0);

        args[i].id = i;
        args[i].inicio = actual;
        args[i].fin = actual + tam;
        actual += tam;

        int rc = pthread_create(&th[i], NULL, HiloEvaluador, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create fallo (%d): %s\n", rc, strerror(rc));
            CancelarHilos(th, creados);

            // Liberar barrera a los ya creados 
            for (int k = 0; k < creados; k++) sem_post(sem_start);

            pthread_mutex_lock(&mtx_fin);
            while (HilosTerminados < creados) {
                pthread_cond_wait(&cv_fin, &mtx_fin);
            }
            pthread_mutex_unlock(&mtx_fin);

            sem_close(sem_start);
            sem_unlink(SEM_START_NAME);
            sem_start = SEM_FAILED;

            sem_close(sem_parcial);
            sem_unlink(SEM_PARCIAL_NAME);
            sem_parcial = SEM_FAILED;

            free(th);
            free(args);
            free(EstadisticasHilos);
            EstadisticasHilos = NULL;

            return false;
        }

        pthread_detach(th[i]);
        creados++;
    }

    // "soltar barrera": arrancan todos los hilos
    for (int i = 0; i < NumHilos; i++) sem_post(sem_start);

    // espera pasiva fin
    pthread_mutex_lock(&mtx_fin);
    while (HilosTerminados < NumHilos) {
        pthread_cond_wait(&cv_fin, &mtx_fin);
    }
    pthread_mutex_unlock(&mtx_fin);

    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed_sec  = (finish.tv_sec - start.tv_sec);
    elapsed_sec += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;


    sem_close(sem_start);
    sem_unlink(SEM_START_NAME);
    sem_start = SEM_FAILED;

    sem_close(sem_parcial);
    sem_unlink(SEM_PARCIAL_NAME);
    sem_parcial = SEM_FAILED;

    // estadisticas finales exactas 
    FusionarEstadisticasGlobales();

    //optimo final
    Optimo->NumArboles = 0;
    Optimo->Coste = MejorCosteGlobal;

    if (MejorCombinacionGlobal != 0 && MejorCosteGlobal < DMaximoCoste) {
        TListaArboles CombinacionArboles;
        TVectorCoordenadas CoordArboles, CercaArboles;
        int NumArboles, PuntosCerca;
        float MaderaArbolesTalados;

        ConvertirCombinacionToArbolesTalados(MejorCombinacionGlobal, Optimo);

        NumArboles = ConvertirCombinacionToArboles(MejorCombinacionGlobal, &CombinacionArboles);
        ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);
        PuntosCerca = chainHull_2D(CoordArboles, NumArboles, CercaArboles);

        Optimo->LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);
        MaderaArbolesTalados = (float)CalcularMaderaArbolesTalados(*Optimo);
        Optimo->MaderaSobrante = MaderaArbolesTalados - Optimo->LongitudCerca;

        Optimo->CosteArbolesCortados = MejorCosteGlobal;
        Optimo->CosteArbolesRestantes = CalcularCosteCombinacion(CombinacionArboles);
    }

    free(th);
    free(args);

    return true;
}