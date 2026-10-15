#ifndef MENU_H
#define MENU_H

#include "protocolo.h"

/*
 * Muestra el menu, lee la opcion y el valor que escribe el usuario
 * y llena 'sol' con la solicitud lista para enviar al servidor.
 * Si el usuario elige salir (o presiona Ctrl+D), sol->tipo queda en MSG_SALIR.
 */
void menu_leer_solicitud(solicitud_t *sol);

/* Muestra un registro encontrado. */
void menu_mostrar_registro(const char *registro);

/*
 * Muestra el mensaje final de una consulta según el tipo de respuesta:
 * sin resultados, error del servidor, o el total encontrado.
 *   mostrados : registros que el cliente alcanzó a mostrar
 *   total     : coincidencias totales que informó el servidor en RESP_FIN
 */
void menu_mostrar_cierre(int tipo_respuesta, const char *detalle, int mostrados, int total);

#endif
