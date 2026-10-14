#ifndef CSV_H
#define CSV_H

#include "protocolo.h"

/*
 * Busca en el CSV los registros que cumplen el criterio.
 * Lee el archivo sin cargarlo completo en memoria.
 *
 *   ruta        : ruta del archivo CSV
 *   criterio    : CRITERIO_NOMBRE, CRITERIO_FAMILIA, CRITERIO_PAIS o CRITERIO_EDAD, CRITERIO_FORMAGEO,	o CRITERIO_DIETA
 *   valor       : lo que escribio³ el usuario
 *   resultados  : arreglo donde se guardan los registros encontrados
 *   max         : cuantos resultados caben como maximo
 *
 * Devuelve cuantos registros encontro (0 si ninguno), o -1 si hubo un error.
 */
int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max);

#endif
