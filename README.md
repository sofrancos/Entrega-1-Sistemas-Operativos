# 🦖 Procesos, llamadas al sistema e IPC en Linux

**Universidad Nacional de Colombia**  
**Asignatura:** Sistemas Operativos

### Integrantes

- Sofía Franco Segura
- Mariana García Dussan
- Julián Foglia Wilches
- Carlos Stiven Romero Sicacha

## 📌 1. Descripción del proyecto

Esta práctica aborda el uso de **procesos, llamadas al sistema y mecanismos de comunicación entre procesos (IPC)** en Linux, mediante un programa que permite consultar ocurrencias documentadas de fósiles de dinosaurios.

El proyecto cuenta con dos implementaciones de comunicación:

| Parte | Mecanismo IPC | Directorio |
|:---:|---|---|
| A | Tuberías nombradas (*FIFO*) | `fifo/` |
| B | Memoria compartida (*Shared Memory*) | `shm/` |

Ambas versiones utilizan módulos comunes para leer el dataset, procesar las búsquedas y administrar el protocolo de comunicación.

## 🦴 2. Dataset

El programa utiliza un conjunto de datos con ocurrencias documentadas de fósiles de dinosaurios, obtenido mediante la API pública de la *Paleobiology Database* (PBDB).

**Fuente de los datos:** [Consultar dataset de Dinosauria en la PBDB](https://paleobiodb.org/data1.2/occs/list.csv?base_name=Dinosauria&show=full).

### 2.1. Cómo obtener el CSV

El archivo CSV **no se incluye en el repositorio** debido a su tamaño. Para evitar que se suba accidentalmente, la regla `data/*.csv` del archivo `.gitignore` excluye estos archivos del control de versiones.

Después de clonar el repositorio, sigue estos pasos:

**1. Descargar el dataset**

Descarga el archivo desde la carpeta compartida de Google Drive:

[📂 Descargar dataset desde Google Drive](https://drive.google.com/file/d/1Qco_Q3oWtcGwUMp9yxeFYVITsqA1Z9r5/view?usp=sharing)

**2. Guardar el archivo en la ubicación correcta**

El archivo debe quedar dentro del repositorio con el siguiente nombre y ruta:

```text
data/dataset.csv
```

Si descargaste el archivo con el nombre `pbdb_data.csv` y quedó en la carpeta Descargas, puedes copiarlo desde la raíz del repositorio con:

```bash
mkdir -p data
cp ~/Downloads/pbdb_data.csv data/dataset.csv
```

**3. Verificar que el archivo esté en su lugar**

Ejecuta los siguientes comandos desde la raíz del repositorio:

```bash
ls -lh data/dataset.csv
git check-ignore data/dataset.csv
```

El primer comando debe mostrar el archivo y su tamaño. El segundo debe devolver:

```text
data/dataset.csv
```

Esto confirma que Git está ignorando el CSV de acuerdo con la regla configurada.

> **Importante:** se  debe tener una copia local del dataset. Los programas leen el archivo directamente desde el disco, por lo que no necesitan conexión a Internet ni a GitHub durante la ejecución.

### 2.2. Columnas utilizadas

El programa identifica las columnas por su **nombre en la primera línea del CSV (encabezado)**, no por su posición. Por tanto, puede seguir funcionando aunque cambie el orden de las columnas.

| Columna | Descripción y uso |
|---|---|
| `accepted_name` | Nombre aceptado del taxón. |
| `family` | Familia a la que pertenece el taxón. |
| `cc` | Código de país de dos letras. |
| `early_interval` | Intervalo geológico que se muestra en los resultados. |
| `max_ma` | Edad máxima, expresada en millones de años. |
| `min_ma` | Edad mínima, expresada en millones de años. |
| `formation` | Formación geológica asociada al registro. |
| `diet` | Tipo de dieta registrado. |

Si falta alguna de estas columnas, la búsqueda se detiene y muestra el siguiente mensaje:

```text
[csv] Al CSV le falta alguna columna necesaria
```

---

## 📁 3. Estructura del repositorio

La organización del proyecto separa el código compartido, las dos implementaciones de IPC, los datos locales y la documentación.

```text
.
├── Makefile
├── README.md
├── comun/
│   ├── protocolo.h
│   ├── csv.h
│   ├── csv.c
│   ├── menu.h
│   └── menu.c
├── fifo/
│   ├── servidor_fifo.c
│   └── cliente_fifo.c
├── shm/
│   ├── shm_comun.h
│   ├── shm_comun.c
│   ├── servidor_shm.c
│   └── cliente_shm.c
├── data/
│   └── dataset.csv        (local, no versionado)
└── docs/
    ├── doc.md
    └── practica1.pdf
```

| Archivo o directorio | Función |
|---|---|
| `Makefile` | Compila los cuatro ejecutables y limpia binarios y recursos IPC. |
| `comun/protocolo.h` | Define las estructuras de los mensajes, las constantes del protocolo y los nombres de todos los recursos IPC. |
| `comun/csv.h` y `comun/csv.c` | Contienen las funciones para leer y buscar información en el dataset. |
| `comun/menu.h` y `comun/menu.c` | Implementan la interfaz de línea de comandos compartida por los clientes. |
| `fifo/` | Contiene el cliente y el servidor que se comunican mediante tuberías nombradas. |
| `shm/shm_comun.h` y `shm/shm_comun.c` | Definen la estructura de la región compartida y la espera con semáforos que detecta la muerte del otro proceso. |
| `shm/servidor_shm.c` y `shm/cliente_shm.c` | Contienen el cliente y el servidor que utilizan memoria compartida. |
| `data/` | Almacena localmente el dataset, que no se versiona en Git. |
| `docs/` | Contiene la documentación técnica detallada (`doc.md`) y el enunciado de la práctica. |

---

## 🔎 4. Criterios de búsqueda

Los criterios de búsqueda están definidos en `comun/protocolo.h`. El índice correspondiente se envía al servidor en el campo `criterio` de cada solicitud.

| Índice | Constante | Columna(s) | Tipo de coincidencia | Ejemplo |
|:---:|---|---|---|---|
| 1 | `CRITERIO_NOMBRE` | `accepted_name` | Contiene el texto, sin distinguir mayúsculas y minúsculas. | `rex` encuentra *Tyrannosaurus rex*. |
| 2 | `CRITERIO_FAMILIA` | `family` | Contiene el texto, sin distinguir mayúsculas y minúsculas. | `Ceratopsidae` |
| 3 | `CRITERIO_PAIS` | `cc` | Coincidencia exacta, sin distinguir mayúsculas y minúsculas. | `AR` no coincide con `ARG`. |
| 4 | `CRITERIO_EDAD` | `min_ma`, `max_ma` | El valor consultado está dentro del rango de edad registrado. | `66` |
| 5 | `CRITERIO_FORMAGEO` | `formation` | Contiene el texto, sin distinguir mayúsculas y minúsculas. | `Hell Creek` |
| 6 | `CRITERIO_DIETA` | `diet` | Contiene el texto, sin distinguir mayúsculas y minúsculas. | `herbivore` |

Para el criterio de edad, el valor consultado \(X\) debe cumplir la siguiente condición:

**Condición:** `min_ma` ≤ `X` ≤ `max_ma`

Los nombres y textos se buscan sin distinguir entre mayúsculas y minúsculas. En cambio, el criterio de país requiere una coincidencia exacta con el código de dos letras.

---

## 📄 5. Lectura del CSV

La lectura y búsqueda de los registros se implementa en `comun/csv.c`.

### 5.1. Funcionamiento

El CSV de la PBDB puede contener campos entre comillas dobles y valores que incluyen comas. Por ejemplo:

```csv
"Neuquén, Patagonia"
```

Por esta razón, no es suficiente separar cada línea usando directamente la coma como delimitador.

La función `extraer_campo` procesa la línea carácter por carácter y aplica las siguientes reglas:

- **Separación correcta:** una coma solo se interpreta como separador cuando está fuera de las comillas.
- **Comillas escapadas:** la secuencia `""` dentro de un campo se interpreta como una comilla literal.
- **Longitud máxima:** cada campo se almacena con un máximo de 255 caracteres, definido por `TAM_CAMPO` (256 bytes con el `'\0'`). Los valores más largos se recortan.
- **Límite de resultados:** cada consulta devuelve como máximo 20 registros, definido por `MAX_RESULTADOS` en `comun/protocolo.h`.

Los resultados corresponden a las primeras 20 coincidencias encontradas en el orden del archivo. La lectura continúa hasta el final para contar **todas** las coincidencias: el servidor informa el total real en el mensaje `RESP_FIN` y el cliente muestra, por ejemplo, `Se encontraron 30 resultado(s) (se muestran los primeros 20)`.

Para modificar el máximo de resultados, basta con ajustar la constante `MAX_RESULTADOS` en `comun/protocolo.h`.

### 5.2. Prueba independiente del módulo

Es posible comprobar el módulo de búsqueda sin ejecutar el cliente ni el servidor. Para ello, se puede crear un programa de prueba, por ejemplo `prueba_csv.c`, que invoque la función `csv_buscar`.

Desde la raíz del repositorio, ejecuta:

```bash
mkdir -p bin
gcc -Wall -Icomun prueba_csv.c comun/csv.c -o bin/prueba_csv
./bin/prueba_csv 1 Tyrannosaurus
```

En este ejemplo, el programa de prueba recibe `1` como criterio de búsqueda y `Tyrannosaurus` como texto consultado.

> **Nota:** `prueba_csv.c` es un archivo auxiliar que debes crear para realizar esta prueba; no forma parte necesariamente del repositorio.

El programa debe ejecutarse desde la raíz del proyecto, ya que la búsqueda utiliza la ruta relativa `data/dataset.csv`.

---

## 🔄 6. Protocolo de comunicación

El protocolo está definido en `comun/protocolo.h` y es utilizado tanto por la implementación FIFO como por la implementación de memoria compartida.

Los mensajes utilizan estructuras de **tamaño fijo**. Esto permite que el receptor conozca cuántos bytes debe leer y reduce el riesgo de interpretar mensajes incompletos o ambiguos.

### 6.1. Solicitud: cliente → servidor

La estructura `solicitud_t` contiene la información de la operación que debe realizar el servidor.

| Campo | Contenido |
|---|---|
| `tipo` | `MSG_CONSULTA` (`1`) para realizar una búsqueda o `MSG_SALIR` (`2`) para solicitar la salida. |
| `criterio` | Índice del criterio de búsqueda, entre `1` y `6`. |
| `valor` | Texto buscado, con una longitud máxima de 127 caracteres. |

### 6.2. Respuesta: servidor → cliente

La estructura `respuesta_t` contiene el tipo de respuesta, el total de coincidencias y un registro formateado o un mensaje de error.

| Campo | Contenido |
|---|---|
| `tipo` | Uno de los tipos de la tabla siguiente. |
| `total` | Solo en `RESP_FIN`: número total de coincidencias en el CSV (puede ser mayor que 20). |
| `registro` | Registro formateado (`RESP_REGISTRO`) o texto del error (`RESP_ERROR`). |

Ambas estructuras tienen tamaño fijo: `solicitud_t` ocupa 136 bytes y `respuesta_t` 1032 bytes. Los dos tamaños son menores que `PIPE_BUF` (4096 bytes), por lo que cada `write` sobre la FIFO es atómico y los mensajes no se intercalan.

| Tipo de respuesta | Valor | Significado |
|---|:---:|---|
| `RESP_REGISTRO` | `1` | Se devuelve un registro encontrado. |
| `RESP_FIN` | `2` | La consulta terminó después de devolver resultados. |
| `RESP_SIN_RESULTADOS` | `3` | La consulta no encontró coincidencias. |
| `RESP_ERROR` | `4` | Se produjo un error durante el procesamiento. |

### 6.3. Secuencia de una consulta

Una consulta válida puede producir entre cero y veinte mensajes `RESP_REGISTRO`, seguidos de **exactamente un mensaje de cierre**.

La secuencia general es:

```text
Cliente                         Servidor
   |                                |
   |------ solicitud_t ------------>|
   |                                |
   |<----- RESP_REGISTRO -----------|  (0 a 20 registros)
   |<----- RESP_REGISTRO -----------|
   |              ...               |
   |<----- Mensaje de cierre -------|
   |                                |
```

El mensaje de cierre será uno de los siguientes:

- `RESP_FIN`, si la consulta terminó después de devolver resultados.
- `RESP_SIN_RESULTADOS`, si no se encontraron coincidencias.
- `RESP_ERROR`, si ocurrió un error.

De esta manera, el cliente puede identificar cuándo termina cada consulta y distinguir entre una búsqueda sin coincidencias y una operación que produjo un error.

En la versión con memoria compartida la secuencia es la misma, pero cada respuesta (incluida la de cierre) se confirma con un ACK del cliente antes de que el servidor escriba la siguiente, porque la región tiene una sola ranura de respuesta (ver la sección 8.2).

---


## ⚙️ 7. Compilación

El proyecto está diseñado para Linux, WSL u otro entorno POSIX compatible.
Requiere:

- GCC;
- GNU Make;
- encabezados y bibliotecas POSIX;
- soporte para memoria compartida y semáforos POSIX.

Desde la raíz del repositorio, compila todo con:

```bash
make
```

También es posible compilar solamente una implementación:

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

El `Makefile` compila con `-Wall -Wextra -pthread` y enlaza con `-lrt -pthread`.
Cada ejecutable depende también de las cabeceras (`protocolo.h`, `csv.h`,
`menu.h`, `shm_comun.h`): si se modifica un parámetro del protocolo, como
`MAX_RESULTADOS`, `make` vuelve a compilar todo lo necesario.

Para eliminar los ejecutables y los recursos IPC:

```bash
make clean         # binarios + recursos IPC
make limpiar-ipc   # solo FIFO, memoria compartida y semáforos residuales
```

> Si `gcc` muestra `Error: pseudo-operador desconocido: «.base64»`, el
> compilador es más nuevo que el ensamblador instalado (binutils). Se puede
> compilar con otra versión de GCC, por ejemplo `make CC=gcc-13`.

## ▶️ 8. Ejecución

Cliente y servidor se ejecutan en terminales separadas. El servidor recibe
opcionalmente la ruta del CSV; si no se indica, usa `data/dataset.csv`
(relativa al directorio actual, por lo que conviene iniciarlo desde la raíz
del proyecto):

```bash
./bin/servidor_fifo                       # usa data/dataset.csv
./bin/servidor_fifo /ruta/a/otro.csv      # usa otro archivo
```

Si el CSV no se puede leer, el servidor lo avisa al iniciar y responde a cada
consulta con `RESP_ERROR`.

### 8.1. FIFO

Terminal 1:

```bash
./bin/servidor_fifo
```

Terminal 2:

```bash
./bin/cliente_fifo
```

El servidor crea estas tuberías nombradas:

```text
/tmp/fifo_dino_req   Cliente -> servidor
/tmp/fifo_dino_res   Servidor -> cliente
```

El cliente muestra el menú común de `comun/menu.c`, envía una consulta y
espera resultados hasta recibir `RESP_FIN`, `RESP_SIN_RESULTADOS` o
`RESP_ERROR`. El servidor atiende a **un cliente por ejecución**: cuando el
cliente sale (opción `0`) o cierra su FIFO, el servidor también termina y
elimina las FIFO.

### 8.2. Memoria compartida

Terminal 1:

```bash
./bin/servidor_shm
```

Terminal 2:

```bash
./bin/cliente_shm
```

Esta versión utiliza:

```text
/dinos_shm         Región de memoria compartida (shm_data_t)
/dinos_sem_srv     Semáforo "hay una solicitud nueva"       (inicial 0)
/dinos_sem_cli     Semáforo "hay una respuesta nueva"       (inicial 0)
/dinos_sem_ack     Semáforo "el cliente ya leyó la respuesta" (inicial 0)
/dinos_sem_mutex   Semáforo "un solo cliente a la vez"      (inicial 1)
```

En Linux aparecen en `/dev/shm` como `dinos_shm` y `sem.dinos_sem_*`.

La región compartida contiene, además de la solicitud y la respuesta, el
**estado de la comunicación** y los PID de ambos procesos:

```c
typedef struct {
    int         estado;         /* LIBRE, SOLICITUD, PROCESANDO, RESPUESTA, LEIDA, CERRADO */
    pid_t       pid_servidor;
    pid_t       pid_cliente;
    solicitud_t solicitud;
    respuesta_t respuesta;
} shm_data_t;
```

Flujo de una consulta:

1. El cliente escribe la solicitud, pone `estado = SOLICITUD` y hace `sem_post(sem_srv)`.
2. El servidor, que estaba dormido en `sem_wait(sem_srv)`, copia la solicitud y busca en el CSV.
3. Por cada respuesta, el servidor la escribe, pone `estado = RESPUESTA`, hace `sem_post(sem_cli)` y espera en `sem_wait(sem_ack)`.
4. El cliente, dormido en `sem_wait(sem_cli)`, copia la respuesta, pone `estado = LEIDA` y hace `sem_post(sem_ack)`.
5. Se repite el paso 3 hasta la respuesta de cierre (que también se confirma).

Los semáforos garantizan que el cliente nunca lea una respuesta que el servidor
no terminó de escribir (paso 3 antes del 4) y que el servidor nunca sobrescriba
la única ranura de respuesta antes de que el cliente la haya copiado (`sem_ack`).
Usar un semáforo distinto para el ACK evita confundir una confirmación con una
solicitud nueva.

Para terminar normalmente, selecciona la opción `0` del menú. El cliente envía
`MSG_SALIR` y el servidor libera la memoria compartida y los semáforos.

## 🧭 9. Funcionamiento del menú común

Los clientes FIFO y SHM utilizan las mismas funciones de `comun/menu.c`, por
lo que la interfaz y las validaciones son iguales:

```text
========== Buscador de dinosaurios ==========
  1. Buscar por nombre          (ej. Tyrannosaurus)
  2. Buscar por familia         (ej. Ceratopsidae)
  3. Buscar por país            (código de 2 letras, ej. AR)
  4. ¿Quién vivía hace X millones de años? (ej. 66)
  5. Buscar por formación geológica (ej. Hell Creek)
  6. Buscar por dieta           (ej. herbivore)
  0. Salir
```

El módulo valida que el valor no esté vacío, que el país tenga exactamente dos
letras y que la edad sea un número finito mayor o igual que cero (se rechazan
`nan`, `inf` y valores no numéricos). También permite salir mediante `Ctrl+D`.

El servidor utiliza `comun/csv.c` para leer el archivo secuencialmente. No
carga todo el dataset en memoria y devuelve como máximo 20 coincidencias por
consulta, junto con el total real.

## 🧹 10. Manejo de errores y liberación de recursos

Todas las llamadas al sistema y funciones de biblioteca relevantes (`mkfifo`,
`open`, `read`, `write`, `close`, `unlink`, `shm_open`, `ftruncate`, `mmap`,
`munmap`, `sem_open`, `sem_post`, `sem_timedwait`, `sem_close`, `sem_unlink`,
`sigaction`, `fopen`, `getline`) comprueban su valor de retorno.

### 10.1. Situaciones contempladas

| Situación | FIFO | Memoria compartida |
|---|---|---|
| El cliente se inicia sin servidor | `open` falla con `ENOENT` y el cliente lo informa. | `shm_open` sin `O_CREAT` falla con `ENOENT` y el cliente lo informa. |
| Quedaron recursos de un servidor terminado con `SIGKILL` | El cliente abre la FIFO con `O_NONBLOCK`: sin lector, `open` falla con `ENXIO` en lugar de bloquearse para siempre. | El cliente lee `pid_servidor` en la región y comprueba con `kill(pid, 0)` que el proceso exista. |
| El servidor se reinicia sobre recursos viejos | Borra las FIFO con `unlink` antes de `mkfifo`. | Hace `shm_unlink`/`sem_unlink` y crea todo con `O_CREAT \| O_EXCL`, así nunca hereda un semáforo con un valor viejo. |
| Un segundo cliente intenta conectarse | `flock(LOCK_EX \| LOCK_NB)` sobre la FIFO de solicitudes: el segundo cliente recibe `EWOULDBLOCK` y termina. | `sem_trywait(sem_mutex)`: el segundo cliente recibe `EAGAIN` y termina. |
| El cliente muere (Ctrl+C, `kill -9`) | El servidor lee EOF (`read` = 0), o `write` devuelve `EPIPE` porque `SIGPIPE` se ignora. Libera todo y termina. | El servidor espera con `sem_timedwait` (1 s) y comprueba con `kill(pid_cliente, 0)`; al detectar la muerte libera todo y termina. |
| El servidor muere | `write` del cliente devuelve `EPIPE` o `read` devuelve EOF; el cliente lo informa. | El cliente detecta la muerte con el mismo mecanismo (`sem_timedwait` + `kill`). |
| `SIGINT`/`SIGTERM` al servidor | El manejador solo activa una bandera `volatile sig_atomic_t`; `open`/`read` vuelven con `EINTR` (no se usa `SA_RESTART`) y el ciclo principal cierra descriptores y borra las FIFO. | Igual: `sem_timedwait` vuelve con `EINTR` y el servidor hace `munmap`, `close`, `sem_close`, `shm_unlink` y `sem_unlink`. |
| CSV ausente o inválido | Aviso al iniciar y `RESP_ERROR` en cada consulta. | Igual. |
| Solicitud de tipo desconocido | Se responde `RESP_ERROR` para que el cliente no quede esperando. | Igual. |

`sem_timedwait` no es espera activa: el proceso duerme en el kernel y solo se
despierta una vez por segundo para comprobar que el otro proceso siga vivo.

### 10.2. Liberar recursos IPC a mano

Si una ejecución termina con `SIGKILL` (que no se puede capturar), los recursos
quedan en el sistema. Se pueden identificar y borrar así:

```bash
ls -l /tmp/fifo_dino_* /dev/shm/ | grep dino   # identificar
make limpiar-ipc                               # liberar
# o manualmente:
rm /tmp/fifo_dino_req /tmp/fifo_dino_res
rm /dev/shm/dinos_shm /dev/shm/sem.dinos_sem_*
```

`ipcs` no muestra estos recursos: solo lista los mecanismos de System V, y aquí
se usan los de POSIX, que Linux expone como archivos en `/dev/shm`.

## 🔬 11. Observaciones con herramientas de Linux

Las siguientes salidas son reales. Se obtuvieron en Linux 7.0 con un CSV de
prueba pequeño (32 registros con las mismas columnas que el dataset de la
PBDB); la ruta del CSV se abrevió. Las llamadas al sistema son las mismas con
el dataset completo.

### 11.1. FIFO: procesos bloqueados y descriptores (`ps`, `lsof`)

Con el cliente esperando en el menú y el servidor esperando una solicitud:

```text
$ ps -o pid,ppid,stat,wchan:22,cmd -p 52177,52183
    PID    PPID STAT WCHAN                  CMD
  52177   52174 SN   anon_pipe_read         ./bin/servidor_fifo .../test.csv
  52183   52179 SN   anon_pipe_read         ./bin/cliente_fifo
```

Ambos procesos están en estado `S` (dormidos), no en `R`: no consumen CPU. El
servidor está bloqueado en `read()` sobre la FIFO de solicitudes
(`anon_pipe_read`); el cliente, en `read()` sobre su entrada estándar. Los PPID
son distintos: los procesos no están emparentados.

```text
$ lsof -p 52177            # servidor
servidor_ 52177 epsilon   3r  FIFO  259,4  0t0  6031029 /tmp/fifo_dino_req
servidor_ 52177 epsilon   4w  FIFO  259,4  0t0  6031030 /tmp/fifo_dino_res
$ lsof -p 52183            # cliente
cliente_f 52183 epsilon   3wW FIFO  259,4  0t0  6031029 /tmp/fifo_dino_req
cliente_f 52183 epsilon   4r  FIFO  259,4  0t0  6031030 /tmp/fifo_dino_res
```

El servidor tiene el descriptor 3 en lectura (`r`) sobre la FIFO de
solicitudes y el 4 en escritura (`w`) sobre la de respuestas; el cliente, al
revés. Los nodos (6031029, 6031030) coinciden: son los mismos objetos del
kernel. La `W` del cliente indica el candado de escritura de `flock`.

### 11.2. FIFO: llamadas al sistema del servidor (`strace -tt`)

Consulta por país `AR` y luego salida (fragmento):

```text
01:11:49.719776 rt_sigaction(SIGINT, {sa_handler=0x6223e00264e9, ...}, NULL, 8) = 0
01:11:49.719825 rt_sigaction(SIGPIPE, {sa_handler=SIG_IGN, ...}, NULL, 8) = 0
01:11:49.719912 mknodat(AT_FDCWD, "/tmp/fifo_dino_req", S_IFIFO|0666) = 0
01:11:49.719957 mknodat(AT_FDCWD, "/tmp/fifo_dino_res", S_IFIFO|0666) = 0
01:11:49.720123 openat(AT_FDCWD, "/tmp/fifo_dino_req", O_RDONLY) = 3
01:11:50.208637 openat(AT_FDCWD, "/tmp/fifo_dino_res", O_WRONLY) = 4
01:11:50.208680 read(3, "\1\0\0\0\3\0\0\0AR\0\0\0..."..., 136) = 136
01:11:51.708795 openat(AT_FDCWD, ".../test.csv", O_RDONLY) = 5
01:11:51.709007 read(5, "occurrence_no,accepted_name,fami"..., 4096) = 3068
01:11:51.709047 read(5, "", 4096) = 0
01:11:51.709068 close(5) = 0
01:11:51.709088 write(4, "\1\0\0\0\0\0\0\0Giganotosaurus | Carchar"..., 1032) = 1032
01:11:51.709122 write(4, "\2\0\0\0\1\0\0\0\0\0\0\0..."..., 1032) = 1032
01:11:51.709145 read(3, "\2\0\0\0\0\0\0\0..."..., 136) = 136
01:11:51.709322 close(3) = 0
01:11:51.709346 close(4) = 0
01:11:51.709369 unlink("/tmp/fifo_dino_req") = 0
01:11:51.709417 unlink("/tmp/fifo_dino_res") = 0
```

Observaciones:

- `mkfifo()` de la biblioteca se traduce en la llamada `mknodat(..., S_IFIFO)`.
- El primer `openat` empieza a las 49.72 y termina a las 50.21: estuvo
  **bloqueado ~0.5 s** hasta que el cliente abrió el otro extremo.
- El `read(3, ...)` empieza a las 50.21 y la siguiente llamada ocurre a las
  51.71: el servidor estuvo **bloqueado 1.5 s** esperando que el usuario
  escribiera la consulta.
- Se leen exactamente 136 bytes (`solicitud_t`): `\1\0\0\0` = `MSG_CONSULTA`,
  `\3\0\0\0` = `CRITERIO_PAIS`, luego `"AR"`.
- El CSV se lee por bloques de 4096 bytes con `read` (lo hace `getline`
  internamente) y se cierra al terminar cada consulta.
- Cada respuesta es un `write` de 1032 bytes: un `RESP_REGISTRO` (`\1`) y el
  `RESP_FIN` (`\2`) con `total = 1`.
- Al recibir `MSG_SALIR` (`\2`), el servidor cierra los descriptores y borra
  las FIFO.

### 11.3. Memoria compartida: recursos y espera en semáforo

```text
$ ls -l /dev/shm | grep dinos
-rw-rw-r-- 1 epsilon epsilon 1180 Oct 15 01:12 dinos_shm
-rw-rw-r-- 1 epsilon epsilon   32 Oct 15 01:12 sem.dinos_sem_ack
-rw-rw-r-- 1 epsilon epsilon   32 Oct 15 01:12 sem.dinos_sem_cli
-rw-rw-r-- 1 epsilon epsilon   32 Oct 15 01:12 sem.dinos_sem_mutex
-rw-rw-r-- 1 epsilon epsilon   32 Oct 15 01:12 sem.dinos_sem_srv

$ ps -o pid,ppid,stat,wchan:22,cmd -p 52306,52324
    PID    PPID STAT WCHAN                  CMD
  52306   52303 SN   futex_do_wait          ./bin/servidor_shm .../test.csv
  52324   52321 SN   anon_pipe_read         ./bin/cliente_shm

$ lsof -p 52306 | grep dinos
servidor_ 52306 epsilon mem    REG   0,29     1180     1122 /dev/shm/dinos_shm
servidor_ 52306 epsilon   3u   REG   0,29     1180     1122 /dev/shm/dinos_shm
```

La región mide 1180 bytes (`sizeof(shm_data_t)`) y cada semáforo 32 bytes. El
servidor está dormido (`S`) en `futex_do_wait`, es decir, en `sem_timedwait`.
`lsof` muestra la región dos veces: como descriptor abierto (`3u`) y como
memoria mapeada (`mem`).

Fragmento de `strace -tt` del servidor:

```text
01:12:00.240985 newfstatat(AT_FDCWD, "/dev/shm/sem.BbOGsb", ...) = -1 ENOENT
01:12:00.241008 openat(AT_FDCWD, "/dev/shm/sem.BbOGsb", O_RDWR|O_CREAT|O_EXCL|..., 0666) = 3
01:12:00.241084 link("/dev/shm/sem.BbOGsb", "/dev/shm/sem.dinos_sem_srv") = 0
01:12:00.241139 unlink("/dev/shm/sem.BbOGsb") = 0
...
01:12:00.241695 openat(AT_FDCWD, "/dev/shm/dinos_shm", O_RDWR|O_CREAT|O_EXCL|..., 0666) = 3
01:12:00.241718 ftruncate(3, 1180) = 0
01:12:00.241738 mmap(NULL, 1180, PROT_READ|PROT_WRITE, MAP_SHARED, 3, 0) = 0x78cbb4d77000
01:12:00.241807 futex(0x78cbb4d7b000, FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME, 0, {...}) = -1 ETIMEDOUT
01:12:01.242238 kill(52324, 0) = 0
01:12:01.242419 futex(0x78cbb4d7b000, FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME, 0, {...}) = -1 ETIMEDOUT
01:12:02.242701 kill(52324, 0) = 0
01:12:02.242885 futex(0x78cbb4d7b000, FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME, 0, {...}) = 0
01:12:02.243784 futex(0x78cbb4d7a000, FUTEX_WAKE, 1) = 1
...
01:12:02.244363 unlink("/dev/shm/dinos_shm") = 0
01:12:02.244400 unlink("/dev/shm/sem.dinos_sem_srv") = 0
```

Observaciones:

- No existen llamadas al sistema llamadas `shm_open`, `sem_open`, `sem_wait`
  ni `sem_post`: son funciones de glibc.
  - `shm_open` es un `openat` sobre `/dev/shm/<nombre>`.
  - `sem_open` con `O_CREAT` crea un archivo temporal, lo enlaza con `link`
    al nombre definitivo y borra el temporal.
  - `shm_unlink` y `sem_unlink` son `unlink`.
  - `sem_wait`/`sem_timedwait` solo llaman al kernel (`futex WAIT`) cuando el
    semáforo vale 0 y hay que dormir; `sem_post` solo llama a `futex WAKE` si
    hay alguien esperando. Si no hay que dormir ni despertar a nadie, la
    operación es atómica en espacio de usuario y **no aparece en `strace`**.
- El servidor duerme en `futex WAIT` con un límite de 1 s; al expirar
  (`ETIMEDOUT`) comprueba con `kill(pid_cliente, 0)` que el cliente siga vivo
  y vuelve a dormir. A las 02.24 el cliente publica la solicitud y el `futex`
  devuelve 0.
- Los datos de la consulta y de las respuestas **no aparecen en `strace`**:
  a diferencia de la FIFO (donde se ven en `read`/`write`), en memoria
  compartida se copian con instrucciones normales de memoria, sin pasar por
  el kernel.

### 11.4. Comandos útiles

```bash
ps -o pid,ppid,stat,wchan:22,cmd -C servidor_fifo,cliente_fifo
pstree -p $(pgrep -x servidor_fifo)
cat /proc/<PID>/status | grep -E "State|PPid|VmRSS"
ls -l /proc/<PID>/fd
lsof -p <PID>
strace -tt -o servidor.trace ./bin/servidor_fifo
ls -l /tmp/fifo_dino_* /dev/shm | grep dino
```

## 🤖 12. Uso de herramientas de IA

Se utilizó Claude Code (Anthropic) como apoyo para revisar el código frente al
enunciado, detectar errores y proponer correcciones. A partir de esa revisión
se añadieron: el manejo de señales y de `SIGPIPE` en FIFO, la detección del
servidor ausente o caído, el campo de estado y los semáforos de ACK y
exclusión mutua en memoria compartida, las dependencias de cabeceras del
`Makefile` y la corrección de esta documentación. Todos los integrantes
revisaron estos cambios y deben poder explicarlos y modificarlos.

---

Para conocer el funcionamiento detallado de cada archivo, el protocolo
completo y el flujo interno de ambas implementaciones, consulta
[`docs/doc.md`](docs/doc.md).
