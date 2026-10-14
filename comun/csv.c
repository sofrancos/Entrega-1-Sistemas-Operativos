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

/* ¿'texto' contiene a 'patron'? Sin distinguir mayusculas. */
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
