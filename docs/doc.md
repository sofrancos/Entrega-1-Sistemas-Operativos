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
│   └── dataset.csv
├── docs/
│   ├── doc.md
│   ├── observaciones.md
│   └── pr_ctica_1_so.md
├── fifo/
│   ├── cliente_fifo.c
│   └── servidor_fifo.c
└── shm/
    ├── cliente_shm.c
    └── servidor_shm.c
```

El servidor utiliza la ruta relativa `data/dataset.csv`, por lo que debe
ejecutarse desde la raíz del proyecto. En el estado actual del repositorio
debe colocarse allí el archivo CSV antes de hacer consultas.

### 2.1. Archivos comunes

#### `comun/protocolo.h`

Define el contrato que comparten clientes y servidores:

```c
typedef struct {
    int tipo;
    int criterio;
    char valor[TAM_VALORUSUARIO];
} solicitud_t;

typedef struct {
    int tipo;
    char registro[TAM_REGISTRO];
} respuesta_t;
```

Constantes principales:

| Constante | Valor | Función |
|---|---:|---|
| `TAM_VALORUSUARIO` | 128 | Máximo del texto de búsqueda |
| `TAM_REGISTRO` | 1024 | Máximo de un resultado formateado |
| `MAX_RESULTADOS` | 20 | Máximo de resultados por consulta |
| `MSG_CONSULTA` | 1 | Solicitud de búsqueda |
| `MSG_SALIR` | 2 | Solicitud de terminación |
| `RESP_REGISTRO` | 1 | Respuesta con un resultado |
| `RESP_FIN` | 2 | Fin del envío de resultados |
| `RESP_SIN_RESULTADOS` | 3 | No hubo coincidencias |
| `RESP_ERROR` | 4 | Error del servidor |

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
- `menu_mostrar_cierre`: imprime el total, la ausencia de resultados o un
  error.

El menú permite buscar por nombre, familia, país, edad, formación geológica o
dieta. La opción `0` termina la sesión. También se interpreta `Ctrl+D` como
salida.

Validaciones:

- la opción debe estar entre `0` y `6`;
- el valor no puede estar vacío;
- el código de país debe tener exactamente dos caracteres;
- la edad debe ser un número mayor o igual que cero;
- las líneas demasiado largas se descartan y se solicitan de nuevo.

#### `comun/csv.c` y `comun/csv.h`

La función principal es:

```c
int csv_buscar(const char *ruta, int criterio, const char *valor,
               char resultados[][TAM_REGISTRO], int max);
```

Su funcionamiento es:

1. abrir el archivo;
2. leer el encabezado;
3. localizar las columnas necesarias por nombre;
4. recorrer el archivo línea por línea;
5. comprobar cada coincidencia;
6. formatear los resultados;
7. guardar como máximo `max` registros;
8. liberar la memoria de `getline` y cerrar el archivo.

El archivo debe contener estas columnas:

```text
accepted_name, family, cc, early_interval, max_ma, min_ma, formation, diet
```

La búsqueda es secuencial y no carga todo el dataset en memoria. Nombre,
familia, formación y dieta usan coincidencia parcial sin distinguir
mayúsculas. País utiliza igualdad exacta sin distinguir mayúsculas. Edad
comprueba que el valor esté dentro del rango geológico.

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
make
```

También pueden compilarse por separado:

```bash
make fifo
make shm
```

Los ejecutables se generan en `bin/`:

```text
bin/cliente_fifo
bin/servidor_fifo
bin/cliente_shm
bin/servidor_shm
```

El `Makefile` utiliza `-Wall -Wextra -Icomun -pthread` y enlaza con
`-lrt -pthread`.

Para eliminar ejecutables y recursos temporales:

```bash
make clean
```

---

## 4. Implementación FIFO

### 4.1. Recursos

Se utilizan dos FIFO:

```text
/tmp/fifo_dino_req   Cliente -> servidor
/tmp/fifo_dino_res   Servidor -> cliente
```

El servidor las crea con `mkfifo` y las elimina al terminar. Aunque
`protocolo.h` conserva nombres genéricos de FIFO, los programas FIFO utilizan
las constantes locales anteriores.

### 4.2. Servidor FIFO

`fifo/servidor_fifo.c`:

1. instala manejadores para `SIGINT` y `SIGTERM`;
2. ignora `SIGPIPE`;
3. elimina FIFO antiguas;
4. crea las dos FIFO;
5. espera al cliente en `open`;
6. recibe una `solicitud_t`;
7. ejecuta `csv_buscar` para `MSG_CONSULTA`;
8. envía las respuestas;
9. responde a `MSG_SALIR`;
10. cierra descriptores y elimina las FIFO.

Las funciones `leer_todo` y `escribir_todo` repiten `read` o `write` hasta
procesar todos los bytes de una estructura. Esto evita asumir que una llamada
al sistema siempre transfiere el mensaje completo.

### 4.3. Cliente FIFO

`fifo/cliente_fifo.c` abre las dos FIFO, obtiene las solicitudes mediante el
menú común y envía estructuras binarias de tamaño fijo. Después lee respuestas
hasta recibir:

- `RESP_FIN`;
- `RESP_SIN_RESULTADOS`;
- `RESP_ERROR`.

Para una consulta con coincidencias, el protocolo es:

```text
RESP_REGISTRO
RESP_REGISTRO
...
RESP_FIN
```

Las operaciones FIFO pueden bloquearse: `open` espera al otro extremo,
`read` espera datos y `write` puede esperar si el búfer está lleno. Este
bloqueo permite sincronizar ambos procesos sin espera activa.

---

## 5. Implementación con memoria compartida

### 5.1. Recursos

La versión SHM utiliza:

```text
Memoria compartida: /dinos_shm
Semáforo del servidor: /dinos_sem_srv
Semáforo del cliente: /dinos_sem_cli
```

La región compartida tiene una solicitud y una respuesta:

```c
typedef struct {
    solicitud_t solicitud;
    respuesta_t respuesta;
} shm_data_t;
```

Memoria compartida no proporciona sincronización por sí sola. Los semáforos
evitan que un proceso lea mientras el otro todavía está escribiendo.

### 5.2. Significado de los semáforos

| Semáforo | Quién lo publica | Significado |
|---|---|---|
| `sem_srv` | Cliente | Hay una solicitud nueva o el cliente confirmó la lectura de un registro |
| `sem_cli` | Servidor | Hay una respuesta nueva disponible |

Ambos se inicializan en cero.

### 5.3. Flujo SHM

El cliente:

1. obtiene una solicitud con `menu_leer_solicitud`;
2. la copia a `shm_ptr->solicitud`;
3. ejecuta `sem_post(sem_srv)`;
4. espera con `sem_wait(sem_cli)`;
5. lee `shm_ptr->respuesta`;
6. muestra el resultado;
7. después de un `RESP_REGISTRO`, ejecuta `sem_post(sem_srv)` como ACK;
8. continúa hasta recibir una respuesta final.

El servidor:

1. espera `sem_wait(sem_srv)`;
2. lee la solicitud compartida;
3. ejecuta `csv_buscar`;
4. escribe una respuesta;
5. despierta al cliente con `sem_post(sem_cli)`;
6. después de cada registro espera el ACK del cliente;
7. escribe `RESP_FIN` al terminar.

El ACK es necesario porque la región tiene una sola ranura de respuesta. Sin
él, el servidor podría sobrescribir el registro antes de que el cliente lo
leyera.

Cuando el usuario selecciona `0`, el cliente escribe `MSG_SALIR` y despierta
al servidor. El servidor libera el mapeo con `munmap`, cierra el descriptor,
ejecuta `shm_unlink`, cierra los semáforos y ejecuta `sem_unlink`.

---

## 6. Diferencias entre FIFO y SHM

| Aspecto | FIFO | SHM |
|---|---|---|
| Transporte | `read` y `write` sobre tuberías nombradas | Acceso directo a estructuras compartidas |
| Sincronización | Bloqueo natural de las operaciones de FIFO | `sem_wait` y `sem_post` explícitos |
| Recursos | Dos FIFO y descriptores | SHM, mapeo y dos semáforos |
| Riesgo principal | Lectura parcial o cierre del otro extremo | Condición de carrera y sobrescritura |
| Limpieza | `unlink` | `shm_unlink` y `sem_unlink` |

La interfaz del usuario y la lógica de búsqueda son las mismas. Solo cambia
la forma en que cliente y servidor intercambian las estructuras.

---

## 7. Ejecución

El servidor debe iniciarse desde la raíz del proyecto.

### FIFO

Terminal 1:

```bash
./bin/servidor_fifo
```

Terminal 2:

```bash
./bin/cliente_fifo
```

### SHM

Terminal 1:

```bash
./bin/servidor_shm
```

Terminal 2:

```bash
./bin/cliente_shm
```

El orden recomendado es iniciar primero el servidor y después el cliente.
Para salir normalmente se selecciona `0`.

---

## 8. Manejo de errores

Se comprueban errores de:

- creación y apertura de FIFO;
- lectura y escritura;
- apertura, dimensionamiento y mapeo de SHM;
- apertura de semáforos;
- apertura y lectura del CSV;
- cierre inesperado del otro proceso;
- entradas inválidas;
- tipos de respuesta desconocidos.

El servidor FIFO utiliza una bandera modificada por los manejadores de
`SIGINT` y `SIGTERM`; el manejador solo marca la terminación y el ciclo
principal realiza la limpieza. `SIGPIPE` se ignora para tratar un cliente
cerrado como un error de escritura.

Una terminación forzada puede dejar recursos IPC con nombre. En Linux se
pueden revisar los objetos SHM con `ls -l /dev/shm` y eliminar los recursos
residuales con las herramientas apropiadas. `make clean` elimina los
recursos temporales esperados por este proyecto.

---

## 9. Observación con herramientas de Linux

Para observar los procesos:

```bash
ps -ef | grep servidor
ps -ef | grep cliente
pstree -p
```

Para observar descriptores y FIFO:

```bash
lsof -p <PID>
ls -l /tmp/fifo_dino_req /tmp/fifo_dino_res
```

Para observar llamadas al sistema:

```bash
strace -f -o servidor.trace ./bin/servidor_fifo
strace -f -o cliente.trace ./bin/cliente_fifo
```

En los rastreos FIFO pueden aparecer `mkfifo`, `openat`, `read`, `write`,
`close` y `unlink`. En SHM también pueden observarse `shm_open`, `ftruncate`,
`mmap`, `munmap`, `sem_open`, `sem_wait`, `sem_post`, `sem_close` y
`sem_unlink`.

Mientras el servidor espera una solicitud, `read` o `sem_wait` lo mantienen
bloqueado sin espera activa. Esta es una de las diferencias observables entre
un proceso esperando correctamente y uno que consulta continuamente el estado
de una variable.

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
3. el servidor abre `data/dataset.csv`;
4. localiza `accepted_name`;
5. recorre el archivo línea por línea;
6. formatea hasta 20 coincidencias;
7. envía cada resultado;
8. envía `RESP_FIN`;
9. el cliente muestra los resultados y vuelve al menú.

Si no hay coincidencias, se envía `RESP_SIN_RESULTADOS`. Si el CSV no existe,
su encabezado es inválido o ocurre un error de lectura, se envía
`RESP_ERROR`.

---

## 11. Límites y consideraciones

- Se muestran como máximo 20 resultados por consulta.
- La búsqueda vuelve a leer el CSV en cada consulta.
- La ruta del archivo es relativa: `data/dataset.csv`.
- El encabezado debe contener las columnas esperadas.
- El proyecto requiere un entorno POSIX; la compilación completa no está
  soportada por un MinGW que no incluya `sys/mman.h` y los demás encabezados
  POSIX necesarios.
- Cliente y servidor deben usar la misma versión de `protocolo.h`.
- SHM utiliza una sola ranura de respuesta, por lo que el ACK del cliente es
  indispensable.

## 12. Resumen

`menu.c` maneja la interacción; `protocolo.h` define los mensajes; `csv.c`
realiza la búsqueda secuencial; y las carpetas `fifo/` y `shm/` contienen las
dos implementaciones de IPC. FIFO usa tuberías nombradas y bloqueos de
lectura/escritura. SHM comparte estructuras directamente y usa semáforos para
evitar condiciones de carrera. Ambas versiones ejecutan las mismas consultas,
usan el mismo dataset y devuelven el mismo tipo de respuestas.