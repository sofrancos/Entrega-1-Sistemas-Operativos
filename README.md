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
├── comun/
│   ├── protocolo.h
│   ├── csv.h
│   ├── csv.c
│   ├── menu.h
│   └── menu.c
│
├── fifo/
│   └── Cliente y servidor de la Parte A
│
├── shm/
│   └── Cliente y servidor de la Parte B
│
├── data/
│   └── dataset.csv
│
├── docs/
│   └── Notas y observaciones del grupo
│
└── .gitignore
```

> La estructura muestra la organización general del proyecto. Los nombres y archivos específicos de los directorios `fifo/` y `shm/` se omiten aquí para destacar su función.

| Archivo o directorio | Función |
|---|---|
| `comun/protocolo.h` | Define las estructuras de los mensajes y las constantes del protocolo. |
| `comun/csv.h` y `comun/csv.c` | Contienen las funciones para leer y buscar información en el dataset. |
| `comun/menu.h` y `comun/menu.c` | Implementan la interfaz de línea de comandos compartida por los clientes. |
| `fifo/` | Contiene el cliente y el servidor que se comunican mediante tuberías nombradas. |
| `shm/` | Contiene el cliente y el servidor que utilizan memoria compartida. |
| `data/` | Almacena localmente el dataset, que no se versiona en Git. |
| `docs/` | Guarda notas y observaciones del grupo relacionadas con las herramientas de Linux. |

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
- **Longitud máxima:** cada campo se almacena con un máximo de 128 caracteres, definido por `TAM_CAMPO`. Los valores más largos se recortan.
- **Límite de resultados:** cada consulta devuelve como máximo 20 registros, definido por `MAX_RESULTADOS` en `comun/protocolo.h`.

Los resultados corresponden a las primeras 20 coincidencias encontradas en el orden del archivo. Cuando se alcanza ese límite, la lectura se detiene sin recorrer el resto del dataset.

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

La estructura `respuesta_t` contiene el estado de la respuesta y un registro formateado o un mensaje de error.

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

---

## ⚙️ 7. Compilación

El proyecto está diseñado para Linux, WSL u otro entorno POSIX compatible.
Requiere:

- GCC;
- GNU Make;
- encabezados y bibliotecas POSIX;
- soporte para memoria compartida y semáforos POSIX.

El dataset debe estar ubicado en `data/dataset.csv` antes de ejecutar el
servidor. Desde la raíz del repositorio, compila todo con:

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

El `Makefile` compila con `-Wall -Wextra -pthread` y enlaza con `-lrt
-pthread`. Para eliminar los ejecutables y recursos temporales:

```bash
make clean
```

> La versión SHM utiliza encabezados POSIX como `sys/mman.h`; por eso no se
> garantiza la compilación completa en un MinGW de Windows sin soporte POSIX.

## ▶️ 8. Ejecución

El servidor debe iniciarse desde la raíz del proyecto porque busca el dataset
con la ruta relativa `data/dataset.csv`. Cliente y servidor deben ejecutarse
en terminales separadas.

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
`RESP_ERROR`.

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
/dinos_shm       Región de memoria compartida
/dinos_sem_srv   Semáforo para solicitudes y confirmaciones
/dinos_sem_cli   Semáforo para respuestas
```

El cliente escribe la solicitud en la memoria compartida y publica
`/dinos_sem_srv`. El servidor lee la solicitud, escribe una respuesta y
publica `/dinos_sem_cli`. Cuando recibe un registro, el cliente lo muestra y
publica de nuevo `/dinos_sem_srv` como confirmación; así el servidor no
sobrescribe la respuesta antes de que sea leída.

Para terminar normalmente, selecciona la opción `0` del menú. El cliente
envía `MSG_SALIR` y el servidor libera la memoria compartida y los semáforos.

## 🧭 9. Funcionamiento del menú común

Los clientes FIFO y SHM utilizan las mismas funciones de `comun/menu.c`, por
lo que la interfaz y las validaciones son iguales:

```text
1. Buscar por nombre
2. Buscar por familia
3. Buscar por país
4. Buscar por edad en millones de años
5. Buscar por formación geológica
6. Buscar por dieta
0. Salir
```

El módulo valida que el valor no esté vacío, que el país tenga dos caracteres
y que la edad sea un número no negativo. También permite salir mediante
`Ctrl+D`.

El servidor utiliza `comun/csv.c` para leer el archivo secuencialmente. No
carga todo el dataset en memoria y devuelve como máximo 20 coincidencias por
consulta.

## 🧹 10. Manejo de errores y observación del sistema

El programa informa errores al crear o abrir recursos IPC, al leer o escribir
mensajes y al abrir o procesar el CSV. Si el otro proceso termina, el proceso
restante detecta el cierre de la comunicación y finaliza o informa la pérdida
de conexión.

Durante la ejecución se pueden observar los procesos y recursos con:

```bash
ps -ef | grep servidor
ps -ef | grep cliente
pstree -p
lsof -p <PID>
ls -l /tmp/fifo_dino_req /tmp/fifo_dino_res
ls -l /dev/shm
```

También se pueden observar las llamadas al sistema con:

```bash
strace -f -o servidor.trace ./bin/servidor_fifo
strace -f -o cliente.trace ./bin/cliente_fifo
```

En FIFO se pueden identificar llamadas como `mkfifo`, `openat`, `read`,
`write`, `close` y `unlink`. En SHM aparecen además operaciones como
`shm_open`, `ftruncate`, `mmap`, `sem_wait` y `sem_post`.

Para conocer el funcionamiento detallado de cada archivo, el protocolo
completo y el flujo interno de ambas implementaciones, consulta
[`docs/doc.md`](docs/doc.md).
