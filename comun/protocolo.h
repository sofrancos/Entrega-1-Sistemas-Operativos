#ifndef PROTOCOLO_H
#define PROTOCOLO_H

/* Ruta del CSV si el servidor no recibe otra como argumento */
#define RUTA_CSV_DEFECTO "data/dataset.csv"

/* Nombres de los recursos IPC de la versión FIFO */
#define FIFO_SOLICITUD "/tmp/fifo_dino_req"   /* cliente -> servidor */
#define FIFO_RESPUESTA "/tmp/fifo_dino_res"   /* servidor -> cliente */

/* Nombres de los recursos IPC POSIX de la versión con memoria compartida */
#define SHM_NOMBRE       "/dinos_shm"        /* región compartida                 */
#define SEM_SOLICITUD    "/dinos_sem_srv"    /* cliente -> servidor: hay solicitud */
#define SEM_RESPUESTA    "/dinos_sem_cli"    /* servidor -> cliente: hay respuesta */
#define SEM_ACK          "/dinos_sem_ack"    /* cliente -> servidor: respuesta leída */
#define SEM_MUTEX        "/dinos_sem_mutex"  /* un solo cliente a la vez           */

/* Tamaños máximos */
#define TAM_VALORUSUARIO 128    /* texto que escribe el usuario */
#define TAM_REGISTRO     1024   /* un registro del CSV          */
#define MAX_RESULTADOS   20     /* resultados por consulta      */

/* Tipos de solicitud */
#define MSG_CONSULTA 1
#define MSG_SALIR    2

/* Criterios de búsqueda */
#define CRITERIO_NOMBRE   1
#define CRITERIO_FAMILIA  2
#define CRITERIO_PAIS     3
#define CRITERIO_EDAD     4
#define CRITERIO_FORMAGEO 5
#define CRITERIO_DIETA    6

/* Tipos de respuesta */
#define RESP_REGISTRO       1   /* trae un registro               */
#define RESP_FIN            2   /* no hay más registros           */
#define RESP_SIN_RESULTADOS 3   /* la búsqueda no encontró nada   */
#define RESP_ERROR          4   /* algo falló en el servidor      */

/* Mensaje cliente -> servidor */
typedef struct {
    int  tipo;                      /* MSG_CONSULTA o MSG_SALIR */
    int  criterio;                  /* CRITERIO_...             */
    char valor[TAM_VALORUSUARIO];   /* lo que se busca          */
} solicitud_t;

/* Mensaje servidor -> cliente */
typedef struct {
    int  tipo;                    /* RESP_...                                     */
    int  total;                   /* en RESP_FIN: coincidencias totales del CSV   */
    char registro[TAM_REGISTRO];  /* registro o mensaje de error                  */
} respuesta_t;

#endif
