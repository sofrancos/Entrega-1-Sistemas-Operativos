#ifndef CSV_H
#define CSV_H

#include "protocolo.h"

/*
 * Busca en el CSV los registros que cumplen el criterio.
 * Lee el archivo línea por línea, sin cargarlo completo en memoria.
 *
 *   ruta        : ruta del archivo CSV
 *   criterio    : CRITERIO_NOMBRE, CRITERIO_FAMILIA, CRITERIO_PAIS, CRITERIO_EDAD,
 *                 CRITERIO_FORMAGEO o CRITERIO_DIETA
 *   valor       : lo que escribió el usuario
 *   resultados  : arreglo donde se guardan los registros encontrados
 *   max         : cuántos resultados caben como máximo
 *   total       : si no es NULL, recibe cuántas coincidencias hay en todo el
 *                 archivo (puede ser mayor que max)
 *
 * Devuelve cuántos registros guardó en 'resultados' (0 si ninguno),
 * o -1 si hubo un error.
 */
int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max, int *total);

#endif
