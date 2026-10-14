#ifndef PROTOCOLO_H
#define PROTOCOLO_H

/* Nombres de los recursos IPC */
#define FIFO_SOLICITUD "/tmp/dinos_solicitud"   /* cliente -> servidor */
#define FIFO_RESPUESTA "/tmp/dinos_respuesta"   /* servidor -> cliente */

/* Tamanos maximos */
#define TAM_VALORUSUARIO      128    /* texto que escribe el usuario */
#define TAM_REGISTRO   1024   /* un registro del CSV          */
#define MAX_RESULTADOS 20     /* resultados por consulta      */

/* Tipos de solicitud */
#define MSG_CONSULTA 1
#define MSG_SALIR    2

/* Criterios de busqueda */
#define CRITERIO_NOMBRE  1
#define CRITERIO_FAMILIA 2
#define CRITERIO_PAIS    3
#define CRITERIO_EDAD    4
#define CRITERIO_FORMAGEO   5
#define CRITERIO_PHYLUM  6
#define CRITERIO_DIETA   7

/* Tipos de respuesta */
#define RESP_REGISTRO       1   /* trae un registro          */
#define RESP_FIN            2   /* no hay más registros      */
#define RESP_SIN_RESULTADOS 3   /* la búsqueda no encontró nada */
#define RESP_ERROR          4   /* algo falló en el servidor */

/* Mensaje cliente -> servidor */
typedef struct {
    int  tipo;               /* MSG_CONSULTA o MSG_SALIR */
    int  criterio;           /* CRITERIO_...             */
    char valor[TAM_VALORUSUARIO];   /* lo que se busca          */
} solicitud_t;

/* Mensaje servidor -> cliente */
typedef struct {
    int  tipo;                    /* RESP_...                    */
    char registro[TAM_REGISTRO];  /* registro o mensaje de error */
} respuesta_t;

#endif
