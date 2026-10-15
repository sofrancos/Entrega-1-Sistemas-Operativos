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
 * Copia en 'dest' el campo número 'idx' (empezando en 0) de 'linea'.
 * Respeta las comillas: una coma dentro de "..." no separa campos.
 * Devuelve 0 si el campo existe, -1 si la línea tiene menos campos.
 */
static int extraer_campo(const char *linea, int idx, char *dest, size_t tam)
{
    int actual = 0;
    const char *p = linea;

    /* Se recorre mientras haya texto y no se llegue al fin de línea ('\n' o '\r') */
    while (*p && *p != '\n' && *p != '\r') {
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
                    }
                    /* Comilla de cierre */
                    entre_comillas = 0;
                    p++;
                    continue;
                }
            } else if (*p == ',') {
                break;   /* Separador de campos */
            }

            if (actual == idx && n + 1 < tam)
                dest[n++] = *p;
            p++;
        }

        if (actual == idx) { dest[n] = '\0'; return 0; }
        if (*p == ',') p++; else break;
        actual++;
    }

    if (actual == idx) { dest[0] = '\0'; return 0; }   /* campo vacío al final */
    return -1;
}

/* ¿'texto' contiene a 'patron'? Sin distinguir mayúsculas. */
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

/* Posición de cada columna que nos interesa (-1 si no se encontró) */
typedef struct {
    int nombre, familia, pais, intervalo;
    int max_ma, min_ma, formacion, dieta;
} columnas_t;

/*
 * Lee la primera línea del CSV y averigua en qué posición está cada columna.
 * Un índice en -1 indica que esa columna no apareció en el encabezado.
 */
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
    }

    if (c->nombre < 0 || c->familia < 0 || c->pais < 0 || c->intervalo < 0 ||
        c->max_ma < 0 || c->min_ma < 0 || c->formacion < 0 || c->dieta < 0) {
        fprintf(stderr, "[csv] Al CSV le falta alguna columna necesaria\n");
        return -1;
    }
    return 0;
}

/*
 * ¿Esta línea cumple el criterio de búsqueda?
 * Usa los índices de 'c' para extraer la columna correspondiente al criterio.
 * Devuelve 1 si coincide y 0 si no.
 */
static int coincide(const char *linea, const columnas_t *c, int criterio, const char *valor)
{
    char campo[TAM_CAMPO];   /* valor de la columna que se evalúa */
    int col;                 /* índice de esa columna             */

    switch (criterio) {
    case CRITERIO_NOMBRE:   col = c->nombre;    break;
    case CRITERIO_FAMILIA:  col = c->familia;   break;
    case CRITERIO_PAIS:     col = c->pais;      break;
    case CRITERIO_FORMAGEO: col = c->formacion; break;
    case CRITERIO_DIETA:    col = c->dieta;     break;
    case CRITERIO_EDAD: {
        /* Caso especial: la edad no es una columna, sino el rango [min_ma, max_ma] */
        char smax[64], smin[64];
        if (extraer_campo(linea, c->max_ma, smax, sizeof smax) != 0 ||
            extraer_campo(linea, c->min_ma, smin, sizeof smin) != 0 ||
            smax[0] == '\0' || smin[0] == '\0')
            return 0;
        double edad = strtod(valor, NULL);
        return strtod(smin, NULL) <= edad && edad <= strtod(smax, NULL);
    }
    default:
        return 0;
    }

    if (extraer_campo(linea, col, campo, sizeof campo) != 0)
        return 0;

    if (criterio == CRITERIO_PAIS)
        return strcasecmp(campo, valor) == 0;   /* país: igualdad exacta */
    return contiene(campo, valor);              /* resto: contiene el texto */
}

/*
 * Arma en 'dest' (de tamaño 'tam') el texto que verá el usuario para un
 * registro. Los campos vacíos se reemplazan por un texto descriptivo.
 */
static void formatear(const char *linea, const columnas_t *c, char *dest, size_t tam)
{
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

    snprintf(dest, tam, "%s | %s | %s (%s-%s Ma) | %s | %s | %s",
             nombre,
             familia[0]   ? familia   : "familia desconocida",
             intervalo, smax, smin,
             pais[0]      ? pais      : "??",
             formacion[0] ? formacion : "formación desconocida",
             dieta[0]     ? dieta     : "dieta desconocida");
}

int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max, int *total)
{
    FILE *f = fopen(ruta, "r");
    if (f == NULL) {
        fprintf(stderr, "[csv] No se pudo abrir %s: %s\n", ruta, strerror(errno));
        return -1;
    }

    /* getline reserva y reutiliza este búfer; se libera una sola vez al final */
    char *linea = NULL;
    size_t cap = 0;
    columnas_t c;
    int n = 0;              /* registros guardados en 'resultados' */
    int coincidencias = 0;  /* coincidencias en todo el archivo    */

    if (getline(&linea, &cap, f) == -1 || leer_encabezado(linea, &c) == -1) {
        fprintf(stderr, "[csv] El archivo está vacío o el encabezado no es válido\n");
        free(linea);
        fclose(f);
        return -1;
    }

    /*
     * Se recorre todo el archivo: se guardan (formateadas) solo las primeras
     * 'max' coincidencias, pero se cuentan todas para informar el total real.
     */
    while (getline(&linea, &cap, f) != -1) {
        if (coincide(linea, &c, criterio, valor)) {
            if (n < max) {
                formatear(linea, &c, resultados[n], TAM_REGISTRO);
                n++;
            }
            coincidencias++;
        }
    }

    int error = ferror(f);
    free(linea);
    fclose(f);

    if (error) {
        fprintf(stderr, "[csv] Error leyendo %s\n", ruta);
        return -1;
    }
    if (total != NULL)
        *total = coincidencias;
    return n;
}
