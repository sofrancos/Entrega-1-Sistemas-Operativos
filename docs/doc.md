# Documentación del proyecto

## 1. Descripción general

Este proyecto implementa un buscador de dinosaurios utilizando un modelo
cliente-servidor en C sobre Linux. El cliente recibe una consulta del usuario
desde la terminal y el servidor busca los registros correspondientes en un
archivo CSV.

La misma funcionalidad está implementada con dos mecanismos de comunicación
entre procesos (IPC):

1. **FIFO (Named Pipes):** los procesos intercambian estructuras mediante dos
   tuberías nombradas.
2. **Memoria compartida (SHM):** ambos procesos acceden a una región común de
   memoria y coordinan cada operación con semáforos POSIX.

El cliente y el servidor son ejecutables independientes y pueden iniciarse
desde terminales diferentes. El cliente presenta el menú y muestra resultados;
el servidor lee el CSV y ejecuta las búsquedas.

```text
Usuario -> Cliente -> solicitud -> Servidor -> data/dataset.csv
           Cliente <- respuestas <- Servidor
```

---

## 2. Estructura del proyecto

```text
.
├── Makefile
├── README.md
├── comun/
│   ├── csv.c
│   ├── csv.h
│   ├── menu.c
│   ├── menu.h
│   └── protocolo.h
├── data/
│   └── dataset.csv        (local, no versionado)
├── docs/
│   ├── doc.md
│   └── practica1.pdf
├── fifo/
│   ├── cliente_fifo.c
│   └── servidor_fifo.c
└── shm/
    ├── shm_comun.c
    ├── shm_comun.h
    ├── cliente_shm.c
    └── servidor_shm.c
```

Los servidores aceptan la ruta del CSV como primer argumento. Sin argumento
usan la ruta relativa `data/dataset.csv`, por lo que en ese caso deben
ejecutarse desde la raíz del proyecto.

### 2.1. Archivos comunes

#### `comun/protocolo.h`

Define el contrato que comparten clientes y servidores: las estructuras de los
mensajes, sus constantes y los nombres de **todos** los recursos IPC, para que
ambos lados usen siempre los mismos.

```c
typedef struct {
    int tipo;
    int criterio;
    char valor[TAM_VALORUSUARIO];
} solicitud_t;              /* 136 bytes */

typedef struct {
    int tipo;
    int total;              /* en RESP_FIN: coincidencias totales */
    char registro[TAM_REGISTRO];
} respuesta_t;              /* 1032 bytes */
```

Constantes principales:

| Constante | Valor | Función |
|---|---:|---|
| `TAM_VALORUSUARIO` | 128 | Máximo del texto de búsqueda |
| `TAM_REGISTRO` | 1024 | Máximo de un resultado formateado |
| `MAX_RESULTADOS` | 20 | Máximo de resultados enviados por consulta |
| `MSG_CONSULTA` | 1 | Solicitud de búsqueda |
| `MSG_SALIR` | 2 | Solicitud de terminación |
| `RESP_REGISTRO` | 1 | Respuesta con un resultado |
| `RESP_FIN` | 2 | Fin del envío de resultados (lleva `total`) |
| `RESP_SIN_RESULTADOS` | 3 | No hubo coincidencias |
| `RESP_ERROR` | 4 | Error del servidor |

Recursos IPC:

| Constante | Nombre | Uso |
|---|---|---|
| `FIFO_SOLICITUD` | `/tmp/fifo_dino_req` | FIFO cliente -> servidor |
| `FIFO_RESPUESTA` | `/tmp/fifo_dino_res` | FIFO servidor -> cliente |
| `SHM_NOMBRE` | `/dinos_shm` | Región de memoria compartida |
| `SEM_SOLICITUD` | `/dinos_sem_srv` | Hay una solicitud nueva |
| `SEM_RESPUESTA` | `/dinos_sem_cli` | Hay una respuesta nueva |
| `SEM_ACK` | `/dinos_sem_ack` | El cliente leyó la respuesta |
| `SEM_MUTEX` | `/dinos_sem_mutex` | Un solo cliente a la vez |

Los criterios disponibles son:

| Valor | Criterio | Campo o condición |
|---:|---|---|
| 1 | Nombre | `accepted_name` |
| 2 | Familia | `family` |
| 3 | País | `cc` |
| 4 | Edad | Rango `min_ma`–`max_ma` |
| 5 | Formación geológica | `formation` |
| 6 | Dieta | `diet` |

#### `comun/menu.c` y `comun/menu.h`

Contienen el único menú utilizado por las dos versiones del cliente. Sus
funciones públicas son:

- `menu_leer_solicitud`: muestra opciones, lee y valida la entrada y construye
  una `solicitud_t`;
- `menu_mostrar_registro`: imprime un resultado;
- `menu_mostrar_cierre(tipo, detalle, mostrados, total)`: imprime el total
  real de coincidencias (y cuántas se mostraron si son menos), la ausencia de
  resultados o un error.

El menú permite buscar por nombre, familia, país, edad, formación geológica o
dieta. La opción `0` termina la sesión. También se interpreta `Ctrl+D` como
salida.

Validaciones:

- la opción debe estar entre `0` y `6`;
- el valor no puede estar vacío;
- el código de país debe tener exactamente dos letras;
- la edad debe ser un número finito mayor o igual que cero (se rechazan
  `nan`, `inf` y texto no numérico);
- las líneas demasiado largas se descartan y se solicitan de nuevo.

#### `comun/csv.c` y `comun/csv.h`

La función principal es:

```c
int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max, int *total);
```

Su funcionamiento es:

1. abrir el archivo;
2. leer el encabezado;
3. localizar las columnas necesarias por nombre;
4. recorrer el archivo línea por línea con `getline`;
5. comprobar cada coincidencia;
6. formatear y guardar las primeras `max` coincidencias;
7. contar todas las coincidencias en `*total`;
8. liberar la memoria de `getline` y cerrar el archivo.

Devuelve cuántos registros guardó, o `-1` si hubo un error.

El archivo debe contener estas columnas:

```text
accepted_name, family, cc, early_interval, max_ma, min_ma, formation, diet
```

La búsqueda es secuencial y no carga todo el dataset en memoria: en cada
momento solo hay una línea en el búfer de `getline` y como máximo
`MAX_RESULTADOS` registros formateados. Nombre, familia, formación y dieta usan
coincidencia parcial sin distinguir mayúsculas. País utiliza igualdad exacta
sin distinguir mayúsculas. Edad comprueba que el valor esté dentro del rango
geológico.

Cada registro se muestra con el formato:

```text
nombre | familia | intervalo (max_ma-min_ma Ma) | país | formación | dieta
```

---

## 3. Compilación

El proyecto requiere Linux, WSL u otro entorno POSIX compatible, además de:

- GCC;
- GNU Make;
- encabezados y bibliotecas POSIX;
- soporte para memoria compartida y semáforos POSIX.

Desde la raíz del repositorio:

```bash
make            # todo
make fifo       # solo la versión FIFO
make shm        # solo la versión SHM
```

Los ejecutables se generan en `bin/`. El `Makefile` utiliza
`-Wall -Wextra -Icomun -pthread` y enlaza con `-lrt -pthread`. Cada ejecutable
depende también de las cabeceras, así que modificar `protocolo.h` provoca la
recompilación.

```bash
make limpiar-ipc   # borra FIFO, memoria compartida y semáforos residuales
make clean         # lo anterior y además bin/
```

---

## 4. Implementación FIFO

### 4.1. Recursos

Se utilizan dos FIFO:

```text
/tmp/fifo_dino_req   Cliente -> servidor
/tmp/fifo_dino_res   Servidor -> cliente
```

El servidor las borra si quedaron de una ejecución anterior, las crea con
`mkfifo` y las elimina al terminar (normalmente, por señal o por error).

### 4.2. Servidor FIFO

`fifo/servidor_fifo.c`:

1. instala manejadores para `SIGINT` y `SIGTERM` (sin `SA_RESTART`);
2. ignora `SIGPIPE`;
3. avisa si el CSV no se puede leer;
4. elimina FIFO antiguas;
5. crea las dos FIFO;
6. espera al cliente en `open` (bloqueante);
7. recibe una `solicitud_t`;
8. ejecuta `csv_buscar` para `MSG_CONSULTA`, o responde `RESP_ERROR` si el tipo es desconocido;
9. envía las respuestas, comprobando cada `write`;
10. termina al recibir `MSG_SALIR`, al leer EOF, ante un error o al recibir una señal;
11. cierra descriptores y elimina las FIFO.

El manejador de señales solo pone en 1 una variable `volatile sig_atomic_t`.
Como no se usa `SA_RESTART`, un `open` o `read` bloqueado vuelve con `EINTR`,
y el ciclo principal ve la bandera y hace la limpieza. Esto evita llamar a
funciones no seguras dentro del manejador.

Al ignorar `SIGPIPE`, si el cliente muere mientras el servidor escribe, `write`
devuelve `-1` con `errno = EPIPE` en lugar de que el kernel mate al servidor
(lo que dejaría las FIFO sin borrar).

Las funciones `leer_todo` y `escribir_todo` repiten `read` o `write` hasta
procesar todos los bytes de una estructura y reintentan ante `EINTR`. Esto
evita asumir que una llamada al sistema siempre transfiere el mensaje
completo. Además, como los mensajes miden menos que `PIPE_BUF` (4096 bytes),
cada `write` es atómico.

### 4.3. Cliente FIFO

`fifo/cliente_fifo.c`:

1. ignora `SIGPIPE`;
2. abre la FIFO de solicitudes con `O_WRONLY | O_NONBLOCK`:
   - `ENOENT`: la FIFO no existe, el servidor no se ha iniciado;
   - `ENXIO`: la FIFO existe pero nadie la tiene abierta para lectura (por
     ejemplo, quedó de un servidor terminado con `SIGKILL`). Sin
     `O_NONBLOCK`, el cliente quedaría bloqueado para siempre;
3. vuelve a poner el descriptor en modo bloqueante con `fcntl`;
4. toma un candado `flock(LOCK_EX | LOCK_NB)` sobre la FIFO; si otro cliente
   ya lo tiene, termina con un mensaje. El kernel libera el candado cuando el
   proceso cierra el descriptor o muere;
5. abre la FIFO de respuestas (bloqueante);
6. obtiene solicitudes del menú común y las envía;
7. lee respuestas hasta recibir `RESP_FIN`, `RESP_SIN_RESULTADOS` o
   `RESP_ERROR`.

Para una consulta con coincidencias, el protocolo es:

```text
RESP_REGISTRO
RESP_REGISTRO
...
RESP_FIN (total = coincidencias en todo el CSV)
```

### 4.4. Bloqueos en FIFO

| Operación | Se bloquea mientras... |
|---|---|
| `open(O_RDONLY)` | no haya ningún proceso con el extremo de escritura abierto |
| `open(O_WRONLY)` | no haya ningún lector (con `O_NONBLOCK` falla con `ENXIO`) |
| `read` | la FIFO esté vacía y haya algún escritor; si no queda ninguno devuelve 0 (EOF) |
| `write` | la FIFO esté llena (64 KiB); si no queda ningún lector devuelve `EPIPE` |

Este bloqueo sincroniza ambos procesos sin espera activa.

---

## 5. Implementación con memoria compartida

### 5.1. Recursos

| Recurso | Nombre | Valor inicial |
|---|---|---:|
| Memoria compartida | `/dinos_shm` | — |
| Semáforo de solicitud | `/dinos_sem_srv` | 0 |
| Semáforo de respuesta | `/dinos_sem_cli` | 0 |
| Semáforo de ACK | `/dinos_sem_ack` | 0 |
| Semáforo de exclusión | `/dinos_sem_mutex` | 1 |

La estructura de la región está en `shm/shm_comun.h`:

```c
typedef struct {
    int         estado;         /* ESTADO_...                                */
    pid_t       pid_servidor;   /* lo escribe el servidor al crear la región */
    pid_t       pid_cliente;    /* lo escribe el cliente al conectarse       */
    solicitud_t solicitud;
    respuesta_t respuesta;      /* una sola ranura: se usa con ACK           */
} shm_data_t;
```

Valores de `estado`:

| Estado | Lo escribe | Significado |
|---|---|---|
| `ESTADO_LIBRE` | servidor | No hay operación en curso |
| `ESTADO_SOLICITUD` | cliente | Hay una solicitud escrita |
| `ESTADO_PROCESANDO` | servidor | Se está buscando en el CSV |
| `ESTADO_RESPUESTA` | servidor | Hay una respuesta escrita |
| `ESTADO_LEIDA` | cliente | El cliente copió la respuesta |
| `ESTADO_CERRADO` | servidor | El servidor terminó |

La memoria compartida no proporciona sincronización por sí sola: el estado
sirve para observar la comunicación y detectar un servidor cerrado, pero
**quien ordena los accesos son los semáforos**. Ningún proceso consulta el
estado en un ciclo (eso sería espera activa).

### 5.2. Significado de los semáforos

| Semáforo | Quién hace `post` | Quién hace `wait` | Significado |
|---|---|---|---|
| `sem_srv` | Cliente | Servidor | Hay una solicitud nueva |
| `sem_cli` | Servidor | Cliente | Hay una respuesta nueva |
| `sem_ack` | Cliente | Servidor | El cliente ya copió la respuesta |
| `sem_mutex` | Cliente (al salir) | Cliente (`sem_trywait` al entrar) | Solo un cliente conectado |

El ACK usa un semáforo propio: si compartiera `sem_srv` con las solicitudes,
una solicitud de otro proceso podría confundirse con una confirmación.

### 5.3. Flujo SHM

El servidor al iniciar:

1. instala manejadores de `SIGINT`/`SIGTERM`;
2. hace `shm_unlink` y `sem_unlink` de restos anteriores;
3. crea los cuatro semáforos con `O_CREAT | O_EXCL` (nunca hereda un valor viejo);
4. crea la región con `shm_open(O_CREAT | O_EXCL)`, `ftruncate` y `mmap`;
5. la pone en cero y escribe `pid_servidor`.

Los semáforos se crean antes que la región porque el cliente usa la existencia
de la región para saber que el servidor está listo.

El cliente al iniciar:

1. abre la región **sin** `O_CREAT` (si no existe, el servidor no está activo);
2. comprueba con `fstat` que ya tenga su tamaño (si no, el acceso daría `SIGBUS`);
3. la mapea con `mmap`;
4. comprueba con `kill(pid_servidor, 0)` que el servidor exista (la región
   puede ser un resto de un servidor terminado con `SIGKILL`);
5. abre los semáforos sin `O_CREAT`;
6. hace `sem_trywait(sem_mutex)`; si falla con `EAGAIN`, ya hay otro cliente;
7. escribe `pid_cliente`.

Cada consulta:

1. el cliente escribe la solicitud, pone `ESTADO_SOLICITUD` y hace `sem_post(sem_srv)`;
2. el servidor sale de `sem_wait(sem_srv)`, copia la solicitud y ejecuta `csv_buscar`;
3. por cada respuesta, el servidor la escribe, pone `ESTADO_RESPUESTA`, hace
   `sem_post(sem_cli)` y espera `sem_wait(sem_ack)`;
4. el cliente sale de `sem_wait(sem_cli)`, copia la respuesta, pone
   `ESTADO_LEIDA` y hace `sem_post(sem_ack)`;
5. se repite hasta la respuesta de cierre, que también se confirma.

Cuando el usuario selecciona `0`, el cliente escribe `MSG_SALIR`, despierta al
servidor, devuelve el mutex y cierra sus referencias (`munmap`, `close`,
`sem_close`). El servidor marca `ESTADO_CERRADO` y hace `munmap`, `close`,
`sem_close`, `shm_unlink` y `sem_unlink`.

### 5.4. Condición de carrera que se evita

Sin semáforos, si el servidor empezara a leer `solicitud.valor` mientras el
cliente todavía lo copia, buscaría un texto a medio escribir. Del mismo modo,
el cliente podría imprimir un `respuesta.registro` mezcla del registro
anterior y del nuevo. Y como hay una sola ranura de respuesta, el servidor
podría sobrescribir el registro *n* con el *n+1* antes de que el cliente lo
leyera, y el cliente perdería resultados. Los pares `post`/`wait` (`sem_cli`
antes de leer, `sem_ack` antes de sobrescribir) imponen el orden; además,
`sem_post`/`sem_wait` actúan como barreras de memoria, así que lo escrito
antes del `post` es visible después del `wait`.

### 5.5. Espera sin bloqueo eterno

`esperar_semaforo` (en `shm/shm_comun.c`) usa `sem_timedwait` con un límite de
`INTERVALO_VIDA` (1 s). Si el semáforo llega, devuelve `ESPERA_OK`. Si expira
el tiempo, comprueba con `kill(pid_otro, 0)` si el otro proceso existe
(`ESRCH` = no existe): si murió, devuelve `ESPERA_OTRO_MURIO`; si no, vuelve a
dormir. Si llega una señal de terminación, devuelve `ESPERA_SENAL`.

El proceso pasa casi todo el tiempo dormido en el kernel (`futex`), así que no
es espera activa: se despierta una vez por segundo para hacer una sola
comprobación. Gracias a esto, ni el servidor ni el cliente quedan bloqueados
para siempre si el otro termina de forma inesperada.

---

## 6. Diferencias entre FIFO y SHM

| Aspecto | FIFO | SHM |
|---|---|---|
| Transporte | `read` y `write`: los datos se copian al kernel y de vuelta | Acceso directo a la región: los datos no pasan por el kernel |
| Sincronización | Implícita: bloqueo de `open`/`read`/`write` | Explícita: `sem_wait` y `sem_post` |
| Recursos | Dos FIFO y sus descriptores | Una región, su mapeo y cuatro semáforos |
| Detección de que el otro murió | EOF en `read`, `EPIPE` en `write` | `sem_timedwait` + `kill(pid, 0)` |
| Un solo cliente | `flock` sobre la FIFO | `sem_trywait` sobre `sem_mutex` |
| Riesgo principal | Lectura parcial o cierre del otro extremo | Condición de carrera y sobrescritura |
| Limpieza | `unlink` | `munmap`, `shm_unlink` y `sem_unlink` |
| Visible en `strace` | Contenido de cada mensaje en `read`/`write` | Solo `futex` cuando hay que dormir o despertar |

La interfaz del usuario y la lógica de búsqueda son las mismas. Solo cambia
la forma en que cliente y servidor intercambian las estructuras.

---

## 7. Ejecución

```bash
# FIFO
./bin/servidor_fifo [ruta.csv]     # terminal 1
./bin/cliente_fifo                 # terminal 2

# SHM
./bin/servidor_shm [ruta.csv]      # terminal 1
./bin/cliente_shm                  # terminal 2
```

Primero se inicia el servidor; si el cliente se inicia antes, informa que el
servidor no está activo y termina. Para salir normalmente se selecciona `0`.
En ambas versiones el servidor atiende a un cliente y termina cuando ese
cliente sale.

---

## 8. Manejo de errores

Se comprueba el valor de retorno de:

- creación, apertura, lectura, escritura, cierre y borrado de FIFO;
- `shm_open`, `ftruncate`, `mmap`, `munmap`, `fstat`;
- `sem_open`, `sem_post`, `sem_timedwait`, `sem_trywait`, `sem_close`, `sem_unlink`;
- `sigaction`, `fcntl`, `flock`;
- apertura y lectura del CSV.

Situaciones manejadas:

- servidor ausente al iniciar el cliente;
- recursos residuales de una ejecución terminada con `SIGKILL`;
- segundo cliente simultáneo;
- muerte del cliente o del servidor en cualquier momento;
- `SIGINT`/`SIGTERM` al servidor (libera todos los recursos);
- entradas inválidas;
- tipos de solicitud o respuesta desconocidos;
- CSV inexistente o con encabezado inválido.

`SIGKILL` no se puede capturar, así que puede dejar recursos IPC con nombre.
Los siguientes inicios del servidor los eliminan automáticamente, y también
se pueden borrar con `make limpiar-ipc`:

```bash
ls -l /tmp/fifo_dino_* /dev/shm | grep dino
make limpiar-ipc
```

`ipcs` no los muestra porque solo lista recursos de System V; los recursos
POSIX que usa este proyecto aparecen como archivos en `/dev/shm`.

---

## 9. Observación con herramientas de Linux

Las observaciones concretas (con salidas reales de `ps`, `lsof` y `strace`)
están en la sección 11 del `README.md`. Resumen de lo que se ve en `strace`:

| Función de C | Llamada al sistema observada |
|---|---|
| `mkfifo` | `mknodat(..., S_IFIFO\|0666)` |
| `open` | `openat` |
| `shm_open` | `openat("/dev/shm/dinos_shm", ...)` |
| `sem_open` con `O_CREAT` | `openat` de un archivo temporal + `link` + `unlink` |
| `shm_unlink`, `sem_unlink` | `unlink` |
| `sem_wait`, `sem_timedwait` | `futex(FUTEX_WAIT_BITSET)`, solo si hay que dormir |
| `sem_post` | `futex(FUTEX_WAKE)`, solo si hay alguien esperando |
| `kill(pid, 0)` | `kill(pid, 0)` |

Mientras el servidor espera una solicitud, `ps -o stat,wchan` muestra estado
`S` y `anon_pipe_read` (FIFO) o `futex_do_wait` (SHM): está bloqueado sin
espera activa.

---

## 10. Flujo completo de una consulta

Para una búsqueda por nombre con el valor `Tyrannosaurus`:

1. el menú crea:

   ```text
   tipo = MSG_CONSULTA
   criterio = CRITERIO_NOMBRE
   valor = "Tyrannosaurus"
   ```

2. el cliente envía o publica la solicitud;
3. el servidor abre el CSV;
4. localiza `accepted_name`;
5. recorre el archivo completo línea por línea;
6. formatea hasta 20 coincidencias y cuenta todas;
7. envía cada resultado;
8. envía `RESP_FIN` con el total;
9. el cliente muestra los resultados y el total y vuelve al menú.

Si no hay coincidencias, se envía `RESP_SIN_RESULTADOS`. Si el CSV no existe,
su encabezado es inválido o ocurre un error de lectura, se envía
`RESP_ERROR`.

---

## 11. Límites y consideraciones

- Se muestran como máximo 20 resultados por consulta, pero se informa el total.
- La búsqueda vuelve a leer el CSV completo en cada consulta.
- El encabezado debe contener las columnas esperadas.
- Cada servidor atiende a un cliente por ejecución.
- El proyecto requiere un entorno POSIX.
- Cliente y servidor deben compilarse con la misma versión de `protocolo.h`.
- SHM utiliza una sola ranura de respuesta, por lo que el ACK del cliente es
  indispensable.

## 12. Resumen

`menu.c` maneja la interacción; `protocolo.h` define los mensajes y los
nombres de los recursos; `csv.c` realiza la búsqueda secuencial; y las
carpetas `fifo/` y `shm/` contienen las dos implementaciones de IPC. FIFO usa
tuberías nombradas y los bloqueos de `open`/`read`/`write`. SHM comparte
estructuras directamente y usa semáforos para evitar condiciones de carrera.
Ambas versiones ejecutan las mismas consultas, usan el mismo dataset,
devuelven el mismo tipo de respuestas y detectan la terminación inesperada
del otro proceso.
