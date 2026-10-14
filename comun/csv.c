#include "csv.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Nombres de las columnas en el CSV de la PBDB */
#define COL_NOMBRE    "accepted_name"
#define COL_FAMILIA   "family"
#define COL_PAIS      "cc"
#define COL_INTERVALO "early_interval"
#define COL_MAX_MA    "max_ma"
#define COL_MIN_MA    "min_ma"
#define COL_FORMACION "formation"
#define COL_DIETA     "diet"

#define TAM_CAMPO 256

/*
 * Copia en 'dest' el campo numero 'idx' (empezando en 0) de 'linea'.
 * Respeta las comillas: una coma dentro de "..." no separa campos.
 * Devuelve 0 si el campo existe, -1 si la linea tiene menos campos.
 */
static int extraer_campo(const char *linea, int idx, char *dest, size_t tam)
{
    int actual = 0;
    const char *p = linea;

    while (*p && *p != '\n' && *p != '\r') {//se revisa que haya texto y que no sea un codon de terminacion o de retorno
        int entre_comillas = 0;
        size_t n = 0;

        if (*p == '"') { entre_comillas = 1; p++; }

		while (*p && *p != '\n' && *p != '\r') {
		    if (entre_comillas) {
		        if (*p == '"') {
		            if (p[1] == '"') {
		                /* Dos comillas representan una comilla literal */
		                if (actual == idx && n + 1 < tam)
		                    dest[n++] = '"';

		                p += 2;
		                continue;
		            } else {
		                /* Comilla de cierre */
		                entre_comillas = 0;
		                p++;
		                continue;
		            }
		        }
		    } else if (*p == ',') {
		        break; /* Separador de campos */
		    }

		    if (actual == idx && n + 1 < tam)
		        dest[n++] = *p;

		    p++;
		}
        if (actual == idx) { dest[n] = '\0'; return 0; }
        if (*p == ',') p++; else break;
        actual++;
    }

    if (actual == idx) { dest[0] = '\0'; return 0; }   /* campo vacio al final */
    return -1;
}

/* ¬ø'texto' contiene a 'patron'? Sin distinguir mayusculas. */
static int contiene(const char *texto, const char *patron)
{
    size_t lp = strlen(patron);
    if (lp == 0) return 0;

    for (; *texto; texto++) {
        size_t i = 0;
        while (i < lp && texto[i] &&
               tolower((unsigned char)texto[i]) == tolower((unsigned char)patron[i]))
            i++;
        if (i == lp) return 1;
    }
    return 0;
}



/* Posicion de cada columna que nos interesa (-1 si no se encontro) */
typedef struct {
    int nombre, familia, pais, intervalo;
    int max_ma, min_ma, formacion, dieta;
} columnas_t;

/* Lee la primera linea del CSV y averigua en que© posicion esta° cada columna */
static int leer_encabezado(const char *linea, columnas_t *c)
{
    char campo[TAM_CAMPO];

    c->nombre = c->familia = c->pais = c->intervalo = -1;
    c->max_ma = c->min_ma = c->formacion = c->dieta = -1;

    for (int i = 0; extraer_campo(linea, i, campo, sizeof campo) == 0; i++) {
        if      (strcmp(campo, COL_NOMBRE)    == 0) c->nombre    = i;
        else if (strcmp(campo, COL_FAMILIA)   == 0) c->familia   = i;
        else if (strcmp(campo, COL_PAIS)      == 0) c->pais      = i;
        else if (strcmp(campo, COL_INTERVALO) == 0) c->intervalo = i;
        else if (strcmp(campo, COL_MAX_MA)    == 0) c->max_ma    = i;
        else if (strcmp(campo, COL_MIN_MA)    == 0) c->min_ma    = i;
        else if (strcmp(campo, COL_FORMACION) == 0) c->formacion = i;
        else if (strcmp(campo, COL_DIETA)     == 0) c->dieta     = i;
    }//funcionan como indicadores de que no se han encontrado las columnas cuando tienen c=-1, se les asigna un indice cuando se encuentran

    if (c->nombre < 0 || c->familia < 0 || c->pais < 0 || c->intervalo < 0 ||
        c->max_ma < 0 || c->min_ma < 0 || c->formacion < 0 || c->dieta < 0) {
        fprintf(stderr, "[csv] Al CSV le falta alguna columna necesaria\n");//error por si no se encuentra la columna
        return -1;
    }
    return 0;
}



/* ¬øEsta linea cumple el criterio de busqueda? */
static int coincide(const char *linea, const columnas_t *c, int criterio, const char *valor)
//se compara la linea con el valor segun un criterio usando la estructura con los indices de las columnas
{
    char campo[TAM_CAMPO];//almacena temporalmente el valor de la columna que se evalua
    int col;//indice de la columna que se evalua

    switch (criterio) {//se escoge el indice c segun la busqueda del usuario
    case CRITERIO_NOMBRE:   col = c->nombre;    break;
    case CRITERIO_FAMILIA:  col = c->familia;   break;
    case CRITERIO_PAIS:     col = c->pais;      break;
    case CRITERIO_FORMAGEO: col = c->formacion; break;
    case CRITERIO_DIETA:    col = c->dieta;     break;
    case CRITERIO_EDAD: {//caso especial al trabajarse como numero dentro de un intervalo en vez de ser una columna propia
        char smax[64], smin[64];
        if (extraer_campo(linea, c->max_ma, smax, sizeof smax) != 0 ||
            extraer_campo(linea, c->min_ma, smin, sizeof smin) != 0 ||
            smax[0] == '\0' || smin[0] == '\0')
            return 0;//se convierten los valores de extraer campo a double y se evalua el rango para determinar la edad
        double edad = strtod(valor, NULL);
        return strtod(smin, NULL) <= edad && edad <= strtod(smax, NULL);
    }
    default:
        return 0;
    }

    if (extraer_campo(linea, col, campo, sizeof campo) != 0)
        return 0;//verifica si se logro

    if (criterio == CRITERIO_PAIS)
        return strcasecmp(campo, valor) == 0;   /* pais: igualdad exacta */
    return contiene(campo, valor);              /* resto: contiene el texto */
}//coincide retorna 1 si se encontro y 0 si nada cumplio con el criterio

/* Arma el texto que vera° el usuario para un registro */
static void formatear(const char *linea, const columnas_t *c, char *dest, size_t tam)
{//se escribe una cadena de tamano tam del csv en formato legible para el usuario en dest
    char nombre[TAM_CAMPO], familia[TAM_CAMPO], intervalo[TAM_CAMPO];
    char formacion[TAM_CAMPO], dieta[TAM_CAMPO];
    char pais[16], smax[32], smin[32];

    extraer_campo(linea, c->nombre,    nombre,    sizeof nombre);
    extraer_campo(linea, c->familia,   familia,   sizeof familia);
    extraer_campo(linea, c->intervalo, intervalo, sizeof intervalo);
    extraer_campo(linea, c->pais,      pais,      sizeof pais);
    extraer_campo(linea, c->max_ma,    smax,      sizeof smax);
    extraer_campo(linea, c->min_ma,    smin,      sizeof smin);
    extraer_campo(linea, c->formacion, formacion, sizeof formacion);
    extraer_campo(linea, c->dieta,     dieta,     sizeof dieta);
//se recuperan los campos de cada columna con los indices calculados y se  envian a los arreglos
    snprintf(dest, tam, "%s | %s | %s (%s-%s Ma) | %s | %s | %s",
             nombre,
             familia[0]   ? familia   : "familia desconocida",
             intervalo, smax, smin,
             pais[0]      ? pais      : "??",
             formacion[0] ? formacion : "formaci√≥n desconocida",
             dieta[0]     ? dieta     : "dieta desconocida");
}//se actualiza el arreglo de arreglos incluyendo el caso de valores nulos tambien




int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max)
{//funcion principal del codigo, recibe la ruta al csv, el criterio, el valor y retorna lo encontrado en matrix resultados de tamano max
    FILE *f = fopen(ruta, "r");
    if (f == NULL) {
        fprintf(stderr, "[csv] No se pudo abrir %s: %s\n", ruta, strerror(errno));
        return -1;
    }//error al abrir el archivo

    char *linea = NULL;   /* getline reserva y reutiliza esta memoria, puntero apunta al espacio de memoria de cada linea leida de capacidad cap */
    size_t cap = 0;
    columnas_t c;
    int n = 0;//numero de registros encontrados

    if (getline(&linea, &cap, f) == -1 || leer_encabezado(linea, &c) == -1) {
        fprintf(stderr, "[csv] El archivo esta° vacio o el encabezado no es valido\n");
        free(linea);
        fclose(f);
        return -1;
    }//se obtienen las columnas principales del archivo
    while (n < max && getline(&linea, &cap, f) != -1) {
        if (coincide(linea, &c, criterio, valor)) {
            formatear(linea, &c, resultados[n], TAM_REGISTRO);
            n++;
        }
    }//lee lineas y las almacena mientras haya capacidad y coincidan con el criterio, las formatea legibles y las almacena en resultados

    int error = ferror(f);
    free(linea);
    fclose(f);

    if (error) {
        fprintf(stderr, "[csv] Error leyendo %s\n", ruta);
        return -1;
    }//cierra archivos y la linea administrada por getline
    return n;//retorna espacios escritos
}
