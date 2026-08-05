# Guía de Aprendizaje de Código

Este documento servirá como un registro detallado y estructurado de todo lo que vayamos aprendiendo sobre el código. Lo actualizaremos a medida que avancemos.

## Índice
1. [Introducción al Proyecto](#1-introducción-al-proyecto)
2. [Estructura del Proyecto](#2-estructura-del-proyecto)
3. [Conceptos Básicos y Módulos](#3-conceptos-básicos-y-módulos)

---

## 1. Introducción al Proyecto
Estamos analizando el proyecto **`tdse-tf_2026-1erC_1-02`**.
Por lo que hemos observado inicialmente, se trata de un sistema embebido programado en C, diseñado para un microcontrolador de la familia **STM32** (específicamente un STM32F103, como lo indica el archivo `.ld` de la memoria flash).

El sistema parece ser un dispositivo médico o de monitoreo de salud, ya que cuenta con algoritmos para medición de SpO2 (oxígeno en sangre) y utiliza un sensor MAX30102. Además, funciona sobre una arquitectura **"Bare Metal - Event-Triggered Systems (ETS)"**, lo que significa que no usa un sistema operativo complejo (como Linux o FreeRTOS), sino un planificador de tareas propio basado en eventos o tiempo.

## 2. Estructura del Proyecto
El proyecto sigue una estructura típica generada por las herramientas de STMicroelectronics (como STM32CubeIDE o STM32CubeMX):
- **`Core/`**: Contiene el código autogenerado para inicializar el hardware (relojes, pines, periféricos). Aquí se encuentra el punto de entrada principal del programa en C (`main.c`).
- **`app/`**: Contiene la lógica de la aplicación propiamente dicha, separada del hardware. Está dividida en tareas (sensor, sistema, pantalla, actuador, telemetría) y drivers específicos.
- **`Drivers/`**: Bibliotecas y archivos de bajo nivel proporcionados por el fabricante (HAL - Hardware Abstraction Layer) para interactuar con los periféricos del microcontrolador.

## 3. Conceptos Básicos y Módulos

### El Archivo `main.c` (Punto de Entrada)
Todo programa en C comienza su ejecución en la función `main()`. En los proyectos de STM32, este archivo suele ser generado automáticamente por una herramienta llamada **STM32CubeMX**, y tiene características especiales que debemos conocer:

- **Bloques `USER CODE BEGIN / END`**: Verás que el archivo está lleno de comentarios como `/* USER CODE BEGIN Includes */`. Es una regla de oro escribir nuestro propio código **solo** dentro de estos bloques. Si re-generamos el código con la herramienta del fabricante, cualquier código fuera de estos bloques se borrará.
- **Inicialización del Hardware (`HAL_Init`, `SystemClock_Config`, `MX_...`)**: Antes de que nuestra aplicación haga algo, el `main()` se encarga de configurar el "Hardware Abstraction Layer" (HAL), ajustar los relojes del microcontrolador (para que corra a la velocidad correcta) e inicializar los pines (GPIO) y periféricos (I2C, UART, Timers).
- **El Bucle Infinito (`while(1)`)**: En sistemas embebidos, el programa nunca "termina". Siempre hay un bucle infinito que mantiene el sistema vivo. 
  - En nuestro proyecto, dentro del `while(1)`, se llama a **`app_update()`**. Esto es lo que ejecuta nuestras tareas.
  - Justo después hay una instrucción llamada **`__WFI()`** (Wait For Interrupt). Esto pone a "dormir" al procesador para ahorrar muchísima energía, hasta que ocurra algún evento (como un "tic" de un reloj interno o presionar un botón).

### Profundizando en `__WFI()` (Wait For Interrupt)
En sistemas "Bare Metal" (sin sistema operativo), el microcontrolador puede gastar mucha batería si se queda dentro del bucle `while(1)` dando vueltas sin parar cuando no hay nada útil que hacer.
- **¿Qué hace?**: `__WFI()` detiene el "cerebro" del procesador. El código pausa su ejecución en esa línea exacta y entra en un estado de bajo consumo.
- **¿Cuándo entra en acción?**: En cada vuelta del `while(1)`, después de ejecutar `app_update()` (nuestras tareas principales), el sistema llega a `__WFI()` y se duerme inmediatamente.
- **¿Cuándo sale de acción (despierta)?**: Solo despierta cuando ocurre una **Interrupción**. En este proyecto hay principalmente dos interrupciones que lo despiertan:
  1. **El SysTick (El reloj del sistema)**: Un temporizador interno configurado para hacer un "Tic" (interrupción) cada 1 milisegundo. Esto significa que, como máximo, el procesador duerme menos de 1 milisegundo antes de despertar, actualizar sus contadores de tiempo, y dar otra vuelta al `while(1)`.
  2. **Interrupciones Externas (Botones/Sensores)**: Si alguien presiona un botón físico o el sensor I2C envía nuevos datos, se genera una interrupción, el procesador despierta, atiende esa interrupción (ej: lee el botón) y continúa dando otra vuelta al bucle principal.

#### Modos de Bajo Consumo (STM32)
`__WFI()` (Modo *Sleep*) no es el único, pero suele ser el más práctico para sistemas que deben reaccionar rápido. STM32 tiene 3 modos principales:
1. **Sleep Mode (`__WFI()`)**: Apaga solo el procesador (el cerebro). Los periféricos (timers, I2C) siguen encendidos. Despierta rapidísimo sin perder datos. Es ideal si tienes que chequear cosas cada 1 milisegundo.
2. **Stop Mode**: Apaga el procesador y casi todos los relojes internos. Ahorra muchísima más energía, pero tarda un poco más en despertar y solo puede ser despertado por eventos externos muy específicos.
3. **Standby Mode**: Apaga prácticamente todo (incluido el regulador de voltaje). Es el que menos consume, pero al despertar, el microcontrolador se reinicia desde cero, perdiendo el estado en el que estaba.

#### Optimización en `main.c`
Al ser un archivo generado por **STM32CubeMX**, casi no se optimiza "a mano" su estructura, ya que al cambiar configuraciones visualmente, el programa lo reescribe. La regla general es confiar en su inicialización y enfocar la optimización en la lógica de la aplicación (`app.c`).

## 4. El Planificador de Tareas (`app.c`)
Este archivo es el verdadero "corazón" de nuestra aplicación (nuestro código, no el generado por ST). Actúa como un pequeño sistema operativo (un *Scheduler* o Planificador cooperativo).

### La Lista de Tareas y su Estructura
Para que el planificador pueda ejecutar las tareas, el desarrollador creó un sistema muy inteligente utilizando tres piezas de código:

1. **`task_cfg_t` (El Molde de la Tarea)**:
   Es un `struct` (una estructura de datos personalizada). Define qué necesita tener una tarea para existir en este programa. Por dentro contiene "Punteros a Funciones": 
   - Un puntero a una función de inicialización (`task_init`).
   - Un puntero a una función de actualización (`task_update`).
   - Un puntero a parámetros extra (`parameters`).
   Esto obliga a que cualquier tarea que inventemos en el futuro cumpla con estas reglas estrictas.

2. **`task_cfg_list[]` (La Lista de Ejecución)**:
   Es un "arreglo" (array) construido con los moldes de `task_cfg_t`. Aquí es donde se registran las tareas reales del sistema:
   ```c
   const task_cfg_t task_cfg_list[] = {
       {task_sensor_init, task_sensor_update, NULL},
       {task_system_init, task_system_update, NULL},
       /* ... */
   };
   ```

   **¿Por qué no ponemos los drivers (EEPROM, MAX30102) en esta lista?**
   Por una regla de **Arquitectura de Software** (Separación en Capas):
   - **Los Drivers** (como `max30102` o `eeprom`) son la capa inferior. Hablan directamente con el metal (el hardware).
   - **Las Tareas** (como `task_sensor`) son la capa superior (Aplicación). Las tareas *utilizan* a los drivers.
   
   Si pusiéramos los drivers en la lista de tareas, mezclaríamos capas. Además, necesitamos garantizar que el hardware (el driver) esté 100% encendido y configurado *antes* de que la lista de tareas empiece a ejecutarse. Por eso, en `app_init()`, primero se inician manualmente los drivers, y luego se recorre la lista de tareas.

3. **`TASK_QTY` (La Cantidad de Tareas)**:
   Es una Macro que calcula automáticamente cuántas tareas hay en la lista utilizando una fórmula clásica de C: `#define TASK_QTY (sizeof(task_cfg_list)/sizeof(task_cfg_t))`
   - `sizeof(task_cfg_list)` te da el peso total de toda la lista en bytes.
   - `sizeof(task_cfg_t)` te da el peso de una sola tarea.
   Al dividir el peso total entre el peso de uno solo, la computadora calcula exactamente cuántas tareas hay (en este caso 5). ¡Así, si mañana agregas una tarea nueva a la lista, el código se ajusta solo sin tener que cambiar números a mano!

### Función `app_init()` (Desglose línea por línea)
Esta función se llama una sola vez desde el `main.c` antes de entrar al bucle infinito. Su trabajo es preparar todo el terreno.

1. **`uint32_t index;`**: Crea una variable para usar como contador en el bucle `for` de más abajo.
2. **`LOGGER_INFO(...)`**: Son funciones de impresión (similares a `printf`). Envían texto por la consola (UART) para que el desarrollador vea en su pantalla que la aplicación arrancó correctamente y qué versión es.
3. **`cycle_counter_init();`**: Inicializa un contador de ciclos del procesador. Es una herramienta de hardware que sirve para medir con extrema precisión (nanosegundos) cuánto tarda el código en ejecutarse. Esto se usará luego para calcular el WCET (Worst-Case Execution Time).
4. **`eeprom_init(&hi2c1);`**: Inicializa la memoria EEPROM. Nota cómo le pasa `&hi2c1`. Le está pasando la "dirección de memoria" (`&`) de nuestro famoso hardware I2C para que la EEPROM sepa por dónde comunicarse.
5. **`max30102_init(&hi2c1);`**: Igual que el anterior, pero inicializa el sensor médico de oxígeno. También usa el mismo cableado físico (`hi2c1`).
6. **`algorithm_init();`**: Inicializa las variables matemáticas del algoritmo que calculará el oxígeno en sangre (SpO2) a partir de los datos brutos del sensor.
7. **El Bucle `for`**:
   ```c
   for (index = 0; TASK_QTY > index; index++) {
       if (NULL != task_cfg_list[index].task_init) {
           (*task_cfg_list[index].task_init)(task_cfg_list[index].parameters);
       }
   }
   ```
   - **`for (...)`**: Recorre la lista de tareas desde la 0 hasta la última (`TASK_QTY`).
   - **`if (NULL != ...)`**: Es una medida de seguridad. Pregunta: *"¿Este trabajador realmente tiene una función de inicio? ¿O está vacía (NULL)?"*.
   - **`(*task...)(...)`**: Si existe, ¡la ejecuta! Usa el puntero a función para arrancar esa tarea específica, y le pasa los parámetros (el comodín `void *`) correspondientes.

### Función `app_update()` (Desglose línea por línea)
Esta función corre en un ciclo sin fin dentro de `main.c`. Su trabajo es ejecutar todas las tareas, pero **solo si** ha pasado el tiempo necesario (1 milisegundo).

```c
void app_update(void) {
	uint32_t index;
	bool b_time_update_required = false;
```
1. **Variables locales**: Crea `index` para el bucle y `b_time_update_required` (una bandera que nos dirá si debemos ejecutar o no las tareas).

```c
	/* Protect shared resource */
	__asm("CPSID i");	/* disable interrupts */
	if (0 < g_app_tick_cnt) {
		g_app_tick_cnt--;
		b_time_update_required = true;
	}
	__asm("CPSIE i");	/* enable interrupts */
```
2. **La Sección Crítica**:
   - `__asm("CPSID i");`: Apaga las interrupciones del microcontrolador.
   - Revisa la variable `g_app_tick_cnt`. Esta variable es aumentada por el SysTick (el reloj de hardware) en otro archivo.
   - Si la variable es mayor a 0 (significa que el reloj hizo "Tic"), la disminuye (consumimos el Tic) y enciende nuestra bandera `b_time_update_required = true`.
   - `__asm("CPSIE i");`: Vuelve a encender las interrupciones. Esto previene que el reloj cambie la variable exactamente en el mismo instante en que nosotros la estábamos leyendo.

```c
	/* Check if it's time to run tasks */
	while (b_time_update_required) {
		g_app_cnt++; // Aumenta el contador de ejecuciones de la app.
		cycle_counter_reset(); // Pone el cronómetro de precisión (DWT) a cero.
```
3. **Inicio de ejecución**: Si la bandera es verdadera, entra en este `while`. Reinicia el cronómetro de nanosegundos (que aprendimos en `dwt.h`) para empezar a medir el tiempo.

```c
		/* Update EEPROM y MAX30102 driver state machines */
		eeprom_update();
		max30102_update();
```
4. **Actualización de Drivers**: Antes que las tareas, deja que el hardware haga lo suyo. Por ejemplo, `max30102_update()` revisará si el sensor tiene datos nuevos de oxígeno listos para ser leídos.

```c
		for (index = 0; TASK_QTY > index; index++) {
			if (NULL != task_cfg_list[index].task_update) {
				(*task_cfg_list[index].task_update)(task_cfg_list[index].parameters);
			}
		}
```
5. **Ejecución de las Tareas**: Recorre nuestra famosa lista `task_cfg_list`. Revisa si existe la función `update` y, usando el puntero a función, ejecuta el ciclo de trabajo de cada una de las 5 tareas (Sensor, Pantalla, etc.).

```c
		/* Measure WCET */
		uint32_t cycles = cycle_counter_get();
		if (g_wcet < cycles) {
			g_wcet = cycles;
		}
```
6. **Medición de Tiempo Peor (WCET)**:
   - Lee el cronómetro exacto (`cycle_counter_get()`).
   - Revisa si este ciclo tardó MÁS tiempo que el récord máximo guardado (`g_wcet`). Si es así, actualiza el récord. Esto es vital para saber si nuestras tareas se están tomando demasiado tiempo y podrían saturar el procesador.

```c
		/* Protect shared resource */
		__asm("CPSID i");
		if (g_app_tick_cnt > 0) {
			g_app_tick_cnt--;
			b_time_update_required = true;
		} else {
			b_time_update_required = false;
		}
		__asm("CPSIE i");
	}
}
```
7. **Re-verificación del tiempo**: Antes de salir del `while`, apaga las interrupciones otra vez y se pregunta: *"¿Mientras ejecutaba todas esas tareas, el reloj volvió a hacer Tic?"*. Si la respuesta es sí, no sale del bucle, vuelve a ejecutarlas de nuevo para recuperar el tiempo perdido. Si no, pone la bandera en `false`, el bucle termina, y la función finaliza (permitiendo que el `main.c` llegue a su `__WFI()` y se duerma).

### Demultiplexación (HAL Callbacks)
Al final del archivo hay funciones que empiezan con `HAL_..._Callback`. Cuando el hardware termina de recibir o enviar un dato por I2C o UART, llama a estas funciones automáticamente. `app.c` actúa como un "director de tráfico", reenviando el aviso al archivo correcto (por ejemplo, al driver de la EEPROM o a la tarea de Telemetría).

## 6. Herramientas de Medición (`dwt.h`)
El archivo `dwt.h` (Data Watchpoint and Trace) es una maravilla oculta de los procesadores ARM Cortex. 

### ¿Qué es el DWT?
El DWT es un hardware interno del procesador diseñado originalmente para depurar código. Sin embargo, tiene un registro especial de 32 bits llamado **CYCCNT** (Cycle Count) que simplemente **cuenta cada "tic" del reloj del procesador**. 
Si el procesador corre a 72 MHz, este contador suma 1 cada 13 nanosegundos, lo que lo convierte en el cronómetro más preciso de todo el sistema, ¡y lo mejor es que no gasta ninguno de los Timers estándar!

### Las Funciones de `dwt.h`
En este archivo vemos varias funciones pequeñas:
1. `cycle_counter_init()`: Activa el hardware DWT, pone el contador a cero y lo enciende.
2. `cycle_counter_reset()`: Pone el contador a 0 de golpe.
3. `cycle_counter_enable()` / `disable()`: Pausa o reanuda el cronómetro.
4. `cycle_counter_get()`: Te devuelve el número bruto de ciclos de reloj transcurridos.
5. `cycle_counter_get_time_us()`: Toma los ciclos y, sabiendo a qué velocidad va el sistema (`SystemCoreClock`), hace la división matemática para devolverte el tiempo real en **Microsegundos (us)**.

### El truco mágico: `static inline`
Si miras el código, todas las funciones dicen `static inline void ... __attribute__((always_inline))`.
- **¿Qué es `inline`? (La Analogía del Libro)**: 
  - **Función Normal**: Imagina que estás leyendo una novela. Cada vez que el protagonista "Prepara un café", el libro te dice: *"Ve a la página 300, lee los pasos para hacer café, y luego regresa aquí"*. Ese proceso de ir a otra página y volver (en el procesador: saltar a otra parte de la memoria) **toma tiempo**.
  - **Función `inline`**: La palabra `inline` le da una orden estricta al compilador: *"No me mandes a otra página"*. Durante la compilación, la computadora agarra las líneas de código de esa función y las **copia y pega** literalmente en el lugar exacto donde la llamaste. El libro queda un poquito más largo, pero lo lees rapidísimo de corrido sin perder tiempo pasando páginas. 
  - Como estas funciones en `dwt.h` se usan para medir microsegundos, ahorrarse el tiempo de "pasar la página" es obligatorio para no arruinar la medición.

### La palabra clave `extern`
En C, cuando ves algo como `extern I2C_HandleTypeDef hi2c1;`, significa lo siguiente:
- **`extern`**: Le dice al compilador "Oye, esta variable existe y tiene memoria asignada, pero **NO** fue creada en este archivo. Fue creada en otro lugar (en este caso, en `main.c`). Simplemente confía en mí y déjame usarla".
- **`I2C_HandleTypeDef`**: Es un "Handle" (un controlador o manejador). Es una estructura de datos muy grande proporcionada por ST que contiene toda la configuración, estado y punteros de memoria para que el protocolo I2C funcione.
- **`hi2c1`**: Es el nombre de la variable (Handle del I2C número 1).

**¿Por qué se usa?** Porque el `main.c` es el dueño de la variable física (allí se inicializa el hardware), pero `app.c` necesita usar esa misma variable para poder enviarle comandos al sensor (que está conectado por I2C). Con `extern`, ambos archivos comparten la **misma** variable en lugar de crear copias distintas.

### ¿Por qué `extern` y no `#include`?
Es un error clásico de C intentar compartir variables usando `#include`. 
- **`#include` copia y pega**: Si creas la variable `int mi_numero;` en un archivo `archivo.h` y luego haces `#include "archivo.h"` en tres archivos `.c` distintos, el compilador literalmente va a crear **3 variables diferentes** llamadas `mi_numero`. Al intentar unir el programa, el sistema colapsará con un error de *"Múltiples definiciones"* porque no sabe cuál de las 3 es la real.
- **La regla de oro en C**: 
  - Usamos **`#include`** para compartir "Los Planos" o las reglas (tipos de datos, definiciones de funciones `void funcion();`).
  - Usamos **`extern`** para compartir "El Objeto Físico" (la variable real que ocupa espacio en la memoria RAM).

### Archivos `.h` vs Archivos `.c`
Para mantener el orden, C separa el código en dos tipos de archivos que trabajan en pareja (por ejemplo, `app.h` y `app.c`):
- **Archivos `.h` (Headers / Cabeceras)**: Son **El Menú del Restaurante**. Aquí solo pones lo que quieres que otros archivos vean. Escribes los *prototipos* de las funciones (ej. `void app_init(void);`), declaras estructuras de datos y constantes. Responden a la pregunta: *"¿Qué puede hacer este módulo?"*. **Nunca** llevan lógica ni ocupan memoria.
- **Archivos `.c` (Source / Código Fuente)**: Son **La Cocina**. Aquí va la lógica real, los algoritmos, los bucles `while`, los `if`, y la creación de variables en memoria. Aquí escribes exactamente *cómo* funciona `app_init()`. Responden a la pregunta: *"¿Cómo lo hace?"*. Es el código privado que compila en instrucciones para el microcontrolador.

### Punteros a Funciones y `void *`
En el archivo `app.c` vimos la estructura `task_cfg_t` definida así:
```c
typedef struct {
	void (*task_init)(void *);
	void (*task_update)(void *);
	void *parameters;
} task_cfg_t;
```
La sintaxis de los punteros en C asusta al principio, pero tiene una estructura lógica:
- **El tipo de retorno `void`**: La primera palabra (`void`) indica qué devuelve la función al terminar. `void` significa "vacío", es decir, la función hace su trabajo pero no devuelve ningún resultado matemático (no devuelve un `int` ni un `float`).
- **El nombre del puntero `(*task_init)`**: Los paréntesis con el asterisco `(* )` le gritan al compilador: *"¡Ojo! Esto no es una función normal, esto es una VARIABLE que guarda la dirección de memoria de una función"*.
- **El parámetro `(void *)`**: El último paréntesis indica qué necesita recibir la función para trabajar. Aquí recibe un **Puntero a void (`void *`)**. 
  - ¿Qué es un `void *`? Es el "comodín" de C. Es un puntero que puede apuntar a *cualquier cosa* (a un número entero, a una estructura gigante, o a nada). Se usa mucho cuando programas un sistema genérico (como este planificador) y no sabes de antemano qué tipo de parámetros va a necesitar cada tarea. Si la tarea del sensor necesita recibir la velocidad del reloj, y la tarea de pantalla necesita recibir el brillo, ambas pueden pasarse usando el comodín `void *`, y por dentro la tarea lo convertirá a lo que realmente necesita.

## 7. `LOGGER_INFO` vs `printf`
Es muy común ver en proyectos profesionales que no se usa la clásica función `printf("Hola");` de C, sino herramientas personalizadas como `LOGGER_INFO("Hola");`.

### ¿Por qué no usar `printf` directamente?
1. **Falta de control global**: Si tienes 500 `printf` en tu código y tu jefe te dice *"El producto ya está listo para venderse, quítale todos los mensajes de la consola para que vaya más rápido"*, tendrías que borrar 500 líneas a mano.
2. **Formato repetitivo**: Con `printf`, siempre tienes que acordarte de poner el salto de línea `\n` o prefijos como `[INFO]` o `[ERROR]`.
3. **Peligro con interrupciones**: Si un `printf` está a medio camino de enviar "Hola", y justo ocurre una interrupción que intenta hacer otro `printf("Mundo")`, la pantalla terminará mostrando basura como "HoMundola".

### ¿Qué hace `LOGGER_INFO`?
Si abrimos el archivo `logger.h`, descubrimos que `LOGGER_INFO` no es una función real, sino una **Macro** (una regla de reemplazo de texto inteligente) que soluciona todo lo anterior:
- **Apagado Global**: Tiene un interruptor maestro (`#define LOGGER_CONFIG_ENABLE 1`). Si lo cambias a `0`, el compilador borra mágicamente todos los `LOGGER_INFO` de tu código y el procesador no pierde tiempo ejecutándolos.
- **Sección Crítica Segura**: Antes de empezar a imprimir, la macro ejecuta `__asm("CPSID i")` (apaga interrupciones temporariamente) y cuando termina de imprimir las vuelve a encender. Así se asegura de que nadie interrumpa el mensaje a la mitad.

## 8. El Reloj de la Aplicación (`app_it.c`)
Este archivo es muy corto, pero vital. Contiene las "Rutinas de Servicio de Interrupción" (ISRs) específicas de nuestra aplicación.

### `HAL_SYSTICK_Callback()`
El único código que tiene adentro es este:
```c
volatile uint32_t g_app_tick_cnt = 0;

void HAL_SYSTICK_Callback(void) {
	g_app_tick_cnt++;
}
```
¿Recuerdas que dijimos que el SysTick interrumpe al procesador cada 1 milisegundo? Cuando ocurre esa interrupción, el hardware viaja por varios archivos hasta que finalmente aterriza aquí, ejecuta esta función, aumenta la variable en 1 y vuelve a dormirse.
Esta es exactamente la misma variable `g_app_tick_cnt` que `app_update()` (en `app.c`) revisa constantemente para saber si ya pasó el tiempo y debe ejecutar las tareas. ¡Es el marcapasos del sistema!

## 9. La Tarea del Sensor (`task_sensor.c`)
¡Atención aquí! Uno pensaría que `task_sensor` se encarga de leer el sensor médico de oxígeno (MAX30102), pero en este proyecto tiene un rol distinto: **Leer los botones físicos del usuario**.

El MAX30102 ya es leído por su propio driver en `app_update()`. La `task_sensor` actúa como una "Máquina de Estados" para leer los pulsadores (MENU, UP, DOWN, ACK) sin que el sistema rebote o se equivoque.

### La Lista de Sensores (Botones)
Al principio del archivo vemos:
```c
const task_sensor_cfg_t task_sensor_cfg_list[] = {
	{ID_BTN_MENU, BTN_MENU_PORT, BTN_MENU_PIN, BTN_PRESSED, DEL_BTN_MAX, EV_SYS_IDLE, EV_SYS_ESCAPE},
	{ID_BTN_UP,   BTN_UP_PORT,   BTN_UP_PIN,   BTN_PRESSED, DEL_BTN_MAX, EV_SYS_IDLE, EV_SYS_NEXT},
	// ...
};
```
Esta es otra lista (array) donde el programador registró qué pines del microcontrolador están conectados a qué botones, cuánto tiempo de "antirrebote" necesitan (`DEL_BTN_MAX`), y lo más importante: **Qué evento generan**. Por ejemplo, si presionas el botón UP, la tarea generará un evento llamado `EV_SYS_NEXT`.

**¿Para qué generar eventos?** Porque las tareas no actúan solas. `task_sensor` lee el botón, genera el evento `EV_SYS_NEXT` y se lo envía a `task_system.c` (el cerebro de la interfaz) para que decida qué hacer con ese evento (por ejemplo, cambiar la pantalla del menú).

### El Archivo de Atributos (`task_sensor_attribute.h`)
Antes de ver la lógica, es importante entender el archivo `task_sensor_attribute.h`. En este proyecto, cada tarea tiene un archivo `_attribute.h` asociado. Es como "El Diccionario" de la tarea. Aquí se definen todas las palabras (Estados, Eventos y Estructuras) que la tarea va a usar.

En este archivo encontramos varios `enum` (enumeraciones). Un `enum` en C es simplemente una forma de darle nombres legibles a los números enteros. Por ejemplo:

1. **Los Identificadores (`task_sensor_id_t`)**:
```c
typedef enum task_sensor_id{
	ID_BTN_MENU, // Por debajo, C dice que esto vale 0
	ID_BTN_UP,   // Vale 1
	ID_BTN_DOWN, // Vale 2
	ID_BTN_ACK   // Vale 3
} task_sensor_id_t;
```

2. **Los Estados del Antirrebote (`task_sensor_st_t`)**:
Esto es vital para entender la máquina de estados. Tiene exactamente 4 estados clásicos para leer un botón físico de forma segura:
```c
typedef enum task_sensor_st{
	ST_BTN_UP,       // El botón está suelto.
	ST_BTN_FALLING,  // Alguien acaba de tocar el botón (pero podría ser ruido eléctrico).
	ST_BTN_DOWN,     // El botón está firmemente presionado (pasó el tiempo de antirrebote).
	ST_BTN_RISING    // El botón se acaba de soltar (esperando que termine el ruido).
} task_sensor_st_t;
```

3. **Los Datos Dinámicos (`task_sensor_dta_t`)**:
```c
typedef struct {
	task_sensor_st_t state; // Estado actual (UP, DOWN, etc.)
	task_sensor_ev_t event; // Evento actual
	uint32_t tick;          // Cronómetro individual para el antirrebote
} task_sensor_dta_t;
```
A diferencia de las configuraciones, esta estructura guarda la "Memoria a Corto Plazo" (RAM) de cada botón. Por ejemplo, el `tick` se usa para contar cuántos milisegundos han pasado desde que el botón cambió a `ST_BTN_FALLING`.

### La Estructura `task_sensor_cfg_t`
Si abrimos el archivo `task_sensor_attribute.h`, veremos cómo está diseñado el molde de esta lista de botones:
```c
typedef struct {
	task_sensor_id_t	identifier;   // ID del botón (ej. MENÚ)
	GPIO_TypeDef *		gpio_port;    // El puerto físico (ej. GPIOC)
	uint16_t			pin;          // El pin físico (ej. PIN 13)
	GPIO_PinState		pressed;      // ¿Es presionado cuando da voltaje Alto o Bajo?
	uint32_t			tick_max;     // Milisegundos para evitar el "Rebote" mecánico
	task_system_ev_t	signal_up;    // Qué evento enviar cuando el botón se suelta
	task_system_ev_t	signal_down;  // Qué evento enviar cuando el botón se presiona
} task_sensor_cfg_t;
```
Esta estructura guarda las **Reglas Fijas** (Configuración) de cada botón. Como nunca cambia durante la ejecución, se guarda con la palabra `const`, lo que hace que viva en la memoria Flash (la misma donde se guarda tu programa) y no gaste memoria RAM.

### Función `task_sensor_init()` (Desglose línea por línea)
Esta función es llamada por el planificador (`app_init`) una sola vez al arrancar.

```c
void task_sensor_init(void *parameters) {
	uint32_t index;
	task_sensor_dta_t *p_task_sensor_dta;
	task_sensor_st_t state;
	task_sensor_ev_t event;
```
1. **Preparativos**: Crea un contador (`index`), un puntero a los datos de la tarea (`p_task_sensor_dta`) y variables para guardar el estado y el evento inicial.

```c
	for (index = 0; SENSOR_DTA_QTY > index; index++) {
```
2. **El Bucle**: Hace un recorrido por todos los botones que tenemos en nuestro sistema (los 4 botones).

```c
		/* Update Task Sensor Data Pointer */
		p_task_sensor_dta = &task_sensor_dta_list[index];
```
3. **Puntero a los Datos**: A diferencia de `cfg_list` (que son las reglas fijas en Flash), aquí agarra la lista `dta_list` (Data). Esta lista vive en la RAM y guarda el estado actual de cada botón. Toma el puntero al botón actual.

```c
		/* Init & Print out: Index & Task execution FSM */
		state = ST_BTN_UP;
		p_task_sensor_dta->state = state;

		event = EV_BTN_UP;
		p_task_sensor_dta->event = event;
```
4. **Estado Inicial**: Para cada botón, asume que nadie lo está presionando al encender el equipo. Así que guarda en la memoria (RAM) que el estado de ese botón es `ST_BTN_UP` (Suelto) y que su último evento conocido fue `EV_BTN_UP`.

### Función `task_sensor_update()` (Desglose línea por línea)
Esta es la función que el planificador central (`app.c`) ejecuta cada 1 milisegundo exacto.

```c
void task_sensor_update(void *parameters) {
	uint32_t index;

	/* 1. Update buttons state machines */
	for (index = 0; SENSOR_DTA_QTY > index; index++) {
		/* Run Task Statechart */
		task_sensor_statechart(index);
	}
```
1. **Recorrer los Botones**: Usa un bucle `for` para recorrer los 4 botones del sistema. Para cada botón, llama a `task_sensor_statechart(index)`. Esto significa que la máquina de estados de cada botón avanza 1 paso (o descuenta 1 milisegundo de su cronómetro interno) en cada vuelta del sistema. ¡Así es como el "antirrebote" sabe medir el tiempo sin usar `delay()`!

```c
	/* 2. Update DIP Switch state */
	uint8_t new_dip = 0;
	if (DIP_ON == HAL_GPIO_ReadPin(DIP1_PORT, DIP1_PIN)) { new_dip |= 0x01; } // Sube el bit 0
	if (DIP_ON == HAL_GPIO_ReadPin(DIP2_PORT, DIP2_PIN)) { new_dip |= 0x02; } // Sube el bit 1
	if (DIP_ON == HAL_GPIO_ReadPin(DIP3_PORT, DIP3_PIN)) { new_dip |= 0x04; } // Sube el bit 2
	if (DIP_ON == HAL_GPIO_ReadPin(DIP4_PORT, DIP4_PIN)) { new_dip |= 0x08; } // Sube el bit 3
```
2. **Lectura del DIP Switch**: En la placa de desarrollo hay un "DIP Switch" (una cajita roja con 4 micro-interruptores). En lugar de crear una máquina de estados compleja para estos, simplemente lee los 4 pines de una sola pasada.
   - Crea una variable `new_dip` en cero (`0000 0000` en binario).
   - Lee el Pin 1. Si está encendido, usa la operación matemática "OR Bit a bit" (`|= 0x01`) para forzar que el primer bit sea un 1 (`0000 0001`).
   - Hace lo mismo con el resto, sumando bits (`0x02` es `0000 0010`, `0x04` es `0000 0100`, etc.).
   - Al final, `new_dip` es un número único del 0 al 15 que representa la combinación exacta de los 4 interruptores.

```c
	if (new_dip != dip_switch_val) {
		dip_switch_val = new_dip;
		LOGGER_INFO("DIP Switch modificado: 0x%02X", dip_switch_val);
	}
}
```
3. **Registro de Cambios**: Compara el valor nuevo (`new_dip`) con el valor histórico que teníamos guardado (`dip_switch_val`). 
   - Si son diferentes (el usuario movió una perilla del DIP Switch), guarda el nuevo valor histórico e imprime un aviso en consola. Si no cambió nada, no hace absolutamente nada para ahorrar tiempo de procesador.

### Función `task_sensor_statechart()` (Desglose línea por línea)
Esta función es llamada una vez por cada botón (4 veces en total) cada milisegundo por `task_sensor_update()`. Aquí es donde ocurre la lectura física del pin y la máquina de estados.

```c
void task_sensor_statechart(uint32_t index) {
	const task_sensor_cfg_t *p_task_sensor_cfg;
	task_sensor_dta_t *p_task_sensor_dta;

	/* Update Task Sensor Configuration & Data Pointer */
	p_task_sensor_cfg = &task_sensor_cfg_list[index];
	p_task_sensor_dta = &task_sensor_dta_list[index];
```
1. **Preparar Punteros**: Primero agarra las reglas del botón (`cfg`) y la memoria actual del botón (`dta`) para no tener que escribir los arreglos enteros a cada rato.

```c
	if (p_task_sensor_cfg->pressed == HAL_GPIO_ReadPin(p_task_sensor_cfg->gpio_port, p_task_sensor_cfg->pin)) {
		p_task_sensor_dta->event = EV_BTN_DOWN;
	} else {
		p_task_sensor_dta->event = EV_BTN_UP;
	}
```
2. **Lectura Física Directa (Polling)**: Llama al hardware de STM32 (`HAL_GPIO_ReadPin`) para preguntar: *"¿Qué voltaje hay en el pin físico de este botón ahora mismo?"*.
   - Si el voltaje coincide con la regla de presionado (ej. voltaje ALTO), actualiza el evento en la RAM a `EV_BTN_DOWN` (Alguien lo está tocando).
   - Si no, lo actualiza a `EV_BTN_UP` (Nadie lo toca).
   **¡Ojo!** Aún no envía el evento al sistema, porque esto podría ser solo ruido. Pasa a la Máquina de Estados para evaluarlo.

```c
	switch (p_task_sensor_dta->state) {
		case ST_BTN_UP:
			if (EV_BTN_DOWN == p_task_sensor_dta->event) {
				p_task_sensor_dta->tick = p_task_sensor_cfg->tick_max;
				p_task_sensor_dta->state = ST_BTN_FALLING;
			}
			break;
```
3. **Estado 1 (`ST_BTN_UP`)**: Si el botón venía estando suelto, solo nos importa si la lectura física detectó un toque (`EV_BTN_DOWN`).
   - Si alguien lo tocó, carga el cronómetro (`tick`) con el máximo de tiempo de antirrebote (ej. 50 ms).
   - Cambia el estado a "Cayendo" (`ST_BTN_FALLING`).

```c
		case ST_BTN_FALLING:
			if (p_task_sensor_dta->tick > 0) {
				p_task_sensor_dta->tick--;
			}
			else if (DEL_BTN_MIN == p_task_sensor_dta->tick) {
				if (EV_BTN_DOWN == p_task_sensor_dta->event) {
					put_event_task_system(p_task_sensor_cfg->signal_down);
					p_task_sensor_dta->state = ST_BTN_DOWN;
				}
				else if (EV_BTN_UP == p_task_sensor_dta->event) {
					p_task_sensor_dta->state = ST_BTN_UP;
				}
			}
			break;
```
4. **Estado 2 (`ST_BTN_FALLING`)**: Estamos en periodo de sospecha. Alguien tocó el botón, pero no sabemos si fue a propósito o un ruido.
   - `if (tick > 0) tick--;`: Va restando 1 al cronómetro en cada vuelta (cada milisegundo) hasta llegar a cero. ¡Esto es genial porque no traba al procesador!
   - `else if (tick == 0)`: ¡El tiempo se acabó! Es hora de juzgar qué pasó.
   - Si la lectura sigue siendo "Presionado" (`EV_BTN_DOWN`), entonces fue un toque real. Envía el evento al cerebro (`put_event_task_system`) y pasa al estado confirmado `ST_BTN_DOWN`.
   - Si la lectura volvió a ser "Suelto" (`EV_BTN_UP`), fue solo ruido eléctrico (rebote). Vuelve al estado `ST_BTN_UP` y lo ignora por completo.

```c
		case ST_BTN_DOWN:
			if (EV_BTN_UP == p_task_sensor_dta->event) {
				p_task_sensor_dta->tick = p_task_sensor_cfg->tick_max;
				p_task_sensor_dta->state = ST_BTN_RISING;
			}
			break;
```
5. **Estado 3 (`ST_BTN_DOWN`)**: El botón ya está presionado. Solo está esperando a que el usuario lo suelte. Si detecta que se soltó (`EV_BTN_UP`), recarga el cronómetro y pasa al estado de sospecha de subida (`ST_BTN_RISING`).

```c
		case ST_BTN_RISING:
        	// Es exactamente igual a FALLING, cuenta hasta 0.
            // Si al llegar a 0 sigue suelto, envía el evento 'signal_up' al cerebro
            // y vuelve a ST_BTN_UP.
            break;
```
6. **Estado 4 (`ST_BTN_RISING`)**: Espera a que termine el rebote mecánico de soltar el botón. Cuando el cronómetro llega a cero y confirma que sigue suelto, avisa al sistema y vuelve al estado inicial.

## 10. El Cerebro Central (`task_system.c`)
Si `task_sensor` son los ojos y dedos de la aplicación, `task_system` es el Cerebro. Es la tarea más grande de todas porque aquí reside toda la "Lógica de Negocio" (Business Logic) y la interfaz de usuario (UI).

### ¿Cómo funciona?
`task_system` se queda escuchando pasivamente hasta que alguien le envía un **Evento**. 
1. **Eventos de Botones**: Si `task_sensor` le envía un `EV_SYS_NEXT` o `EV_SYS_ENTER`.
2. **Eventos Médicos**: Si el sensor MAX30102 terminó de calcular la sangre, le envía un evento `EV_SYS_SPO2_DATA`.

Cuando llega un evento, `task_system` mira en qué **Modo** está actualmente el equipo:

#### Modo Normal (`task_system_normal_statechart`)
Este es el modo en el que el paciente está siendo medido. 
- Si recibe datos de sangre (`EV_SYS_SPO2_DATA`), agarra esos números, formatea un texto bonito y se los envía a `task_display` para que los dibuje en la pantalla LCD.
- Inmediatamente después, revisa matemáticamente si esos números superaron los límites de las alarmas médicas (`g_ou_max`, `g_pu_min`, etc.). Si es así, le grita a `task_actuator` que encienda la alarma crítica.
- Si en este modo recibe el evento del botón menú (`EV_SYS_ESCAPE`), cambia el estado del equipo entero a Modo Configuración (`SETUP`).

#### Modo Configuración (`task_system_setup_statechart`)
Este modo es la clásica "Navegación de Menús" de las pantallas LCD antiguas. Es una gran máquina de estados con sub-menús:
- `ST_SYS_MENU1_SENSOR`: Te pregunta si quieres configurar el Oxímetro o el Pulsómetro.
- `ST_SYS_MENU2_PARAMETER`: Te pregunta si quieres cambiar el Máximo, Mínimo o encender la Alarma.
- `ST_SYS_MENU3_MINIMUM`: Te permite usar los botones `UP` y `DOWN` para aumentar o disminuir una variable temporal (`g_aux_min`). Al apretar Enter, guarda este nuevo límite médico y vuelve atrás.

Lo brillante de esta arquitectura es que **ninguna de las otras tareas sabe que existen menús**. `task_display` solo sabe escribir letras. `task_sensor` solo sabe apretar botones. Es `task_system` quien recibe "Botón Arriba" y decide que eso significa "Súbele 1 punto al límite de oxígeno" porque sabe que estamos en el `MENU3`.

### Los Buffers de Texto y la Memoria Privada (`static`)
Dentro del archivo, cerca del principio, encontramos estas dos variables:
```c
static char display_line_1[DISPLAY_COLS + 1u];
static char display_line_2[DISPLAY_COLS + 1u];
```
Estas son las variables que el Cerebro usa como "Borrador" antes de mandarle el texto a la pantalla LCD. Analicemos cada parte:
1. **`static`**: Cuando usas `static` en una variable global, le estás diciendo al compilador: *"Haz que esta variable sea privada. Ningún otro archivo `.c` puede verla ni modificarla"*. Es una excelente medida de seguridad. ¡Ni siquiera usando `extern` podrían acceder a ella!
2. **`char [...]`**: Es un Arreglo de Caracteres (lo que en otros lenguajes se llama "String" o Cadena de texto).
3. **`DISPLAY_COLS`**: Es una constante definida más arriba que vale `16`. Esto es porque la pantalla LCD física tiene 16 columnas y 2 filas (16x2).
4. **`+ 1u` (El Terminador Nulo)**: Este es un detalle CRÍTICO en el lenguaje C. Si la pantalla tiene 16 letras, ¿por qué creamos un arreglo de 17 espacios? 
   En C, los textos no saben su propio tamaño. La única forma en que una función sabe dónde termina una palabra es porque C inserta automáticamente un carácter invisible llamado **Terminador Nulo (`\0`)** al final. 
   - Las primeras 16 casillas guardan las letras visibles (ej: `"Hola Mundo      "`).
   - La casilla 17 guarda el `\0`.
   Si nos olvidáramos de sumar ese `+1`, la función de impresión seguiría leyendo la memoria a ciegas, imprimiendo basura en la pantalla hasta que el programa explote.

   > [!NOTE]
   > **¿El terminador nulo aplica para cualquier arreglo?**
   > ¡NO! El terminador nulo (`\0`) es una regla exclusiva para arreglos de **caracteres (Strings)**. C asume que cualquier arreglo de tipo `char` que quieras imprimir terminará en `\0`. 
   > Para arreglos de números (como `int` o `float`), no existe un "número nulo". Si tienes un arreglo de enteros y quieres imprimirlo o leerlo, estás obligado a usar un bucle `for` y decirle a C exactamente cuántos elementos tiene el arreglo. Si no le pasas el tamaño, C no tiene forma mágica de saber dónde terminan los números.

### El Misterio de `#pragma pack(push, 1)` y el Guardado en Memoria
Dentro de `task_system.c`, encontramos una estructura peculiar para guardar los ajustes médicos (los umbrales de alarma):
```c
#pragma pack(push, 1)
typedef struct {
	uint32_t magic_word;
	int32_t g_ou_max;
	int32_t g_ou_min;
	int32_t g_pu_max;
	int32_t g_pu_min;
	uint8_t b_o_alarma;
	uint8_t b_p_alarma;
} sys_config_t;
#pragma pack(pop)
```
Esta estructura está diseñada específicamente para ser guardada en la **Memoria EEPROM** (la memoria que no se borra cuando apagas el equipo).

#### 1. ¿Qué es `#pragma pack(push, 1)`? (Alineación de Memoria)
Por defecto, los procesadores de 32 bits (como el STM32) leen la memoria mucho más rápido si las variables están agrupadas en bloques de 4 bytes (32 bits).
Si declaras una variable de 8 bits (`uint8_t`) seguida de una de 32 bits (`int32_t`), el compilador, en secreto, **añadirá 3 bytes "vacíos" (basura) en el medio** para "alinear" la siguiente variable a un múltiplo de 4. Esto se conoce como *Memory Padding*.

El problema es que cuando vas a enviar esta estructura por un cable hacia la memoria externa EEPROM, ¡no quieres enviar basura! Quieres enviar exactamente el tamaño real de los datos.
- `#pragma pack(push, 1)`: Le da una orden estricta al compilador: *"Obliga a que estas variables se guarden una pegada a la otra en la memoria RAM, con una alineación de 1 byte. Cero espacios vacíos."*
- `#pragma pack(pop)`: Le dice al compilador: *"Listo, ya puedes volver a tu comportamiento normal para el resto del código"*.

#### 2. ¿Qué es la `magic_word`?
Es un truco muy común en sistemas integrados. Imagina que enciendes un equipo recién salido de fábrica. Su memoria EEPROM estará completamente vacía o llena de pura basura (`0xFFFFFFFF`). 
El programa usa la `magic_word` (Palabra Mágica) como un código secreto (por ejemplo, el número `0xAABBCCDD`).
- Cuando el equipo arranca, lee la estructura entera de la EEPROM.
- Luego revisa: `¿magic_word == 0xAABBCCDD?`
- **Si es correcto**: Significa que la memoria tiene configuraciones válidas guardadas por un médico. Carga los valores.
- **Si no es correcto (o es basura)**: Significa que la memoria está en blanco. Entonces ignora lo que leyó y carga las "Configuraciones por Defecto de Fábrica" (ej. Oxígeno al 95%).

## 11. El Cartero (`task_system_interface.c`)
Vimos que `task_sensor` envía eventos y `task_system` los recibe. ¿Pero cómo viaja el mensaje entre los dos archivos? A través de la **Interfaz**.

Casi todas las tareas de este proyecto tienen un archivo `_interface.c`. Este archivo actúa como el "Buzón de Correos" exclusivo de esa tarea.

Si abrimos `task_system_interface.c`, veremos que implementa una de las estructuras de datos más famosas e importantes en la informática: una **Cola Circular** (Ring Buffer).

### La Cola Circular
```c
#define QUEUE_LENGTH	(16ul)

typedef struct {
	uint32_t			head;  // La Cabeza (donde se insertan mensajes nuevos)
	uint32_t			tail;  // La Cola (por donde se leen los mensajes antiguos)
	uint32_t			count; // Cuántos mensajes hay sin leer
	task_system_ev_t	queue[QUEUE_LENGTH]; // El arreglo de 16 casillas
} event_task_system_queue_t;
```

- **Inicializar (`init_event_task_system`)**: Se llama una sola vez al encender el equipo. Pone la Cabeza y la Cola en la posición `0`, y recorre las 16 casillas llenándolas con el valor `EMPTY` (255) para limpiar cualquier "basura" que haya quedado en la memoria RAM al encender el chip.

### 1. Insertar Evento (`put_event_task_system`)
Esta es la función que usan tareas como `task_sensor` para "depositar una carta en el buzón".
```c
void put_event_task_system(task_system_ev_t event) {
	event_task_system_queue.count++;
	event_task_system_queue.queue[event_task_system_queue.head++] = event;

	if (QUEUE_LENGTH == event_task_system_queue.head)
		event_task_system_queue.head = 0;
}
```
- **`count++`**: Le suma 1 al contador estadístico de mensajes totales.
- **`...queue[...head++] = event;`**: Guarda el evento en la casilla del arreglo a la que apunta la cabeza (`head`), e inmediatamente después, le suma +1 a la cabeza para dejarla apuntando a la próxima casilla libre.
- **`if (QUEUE_LENGTH == ...head)`**: Si de tanto sumar, la cabeza llegó a valer 16 (el límite físico de la RAM), la fuerza a volver a `0`. ¡Esto es lo que convierte a la cola en "circular"!

### 2. Leer Evento (`get_event_task_system`)
El cerebro (`task_system`) llama a esta función cuando tiene tiempo de procesar mensajes viejos.
```c
task_system_ev_t get_event_task_system(void) {
	task_system_ev_t event;
	event_task_system_queue.count--;
	event = event_task_system_queue.queue[event_task_system_queue.tail];
	event_task_system_queue.queue[event_task_system_queue.tail++] = EMPTY;

	if (QUEUE_LENGTH == event_task_system_queue.tail)
		event_task_system_queue.tail = 0;
	return event;
}
```
- **`count--`**: Resta 1 al contador estadístico, ya que estamos sacando una carta.
- **`event = ...queue[...tail];`**: Copia el evento desde la posición actual de la cola (`tail`) hacia una variable temporal.
- **`...queue[...tail++] = EMPTY;`**: Borra el evento del buzón sobreescribiéndolo con `EMPTY` (255) y avanza la cola (`tail++`) hacia adelante.
- **El `if` circular**: Al igual que en la cabeza, si la cola llega a 16, vuelve a `0`. Finalmente devuelve el evento que leyó.

### 3. Revisar el Buzón (`any_event_task_system`)
Antes de intentar leer una carta, el cerebro debe asegurarse de que el buzón no esté vacío.
```c
bool any_event_task_system(void) {
  return (event_task_system_queue.head != event_task_system_queue.tail);
}
```
Es la función más rápida de todas. Simplemente se pregunta: *"¿La Cabeza y la Cola están en posiciones distintas?"*. 
- Si la Cabeza avanzó y se alejó de la Cola, significa que alguien introdujo cartas que aún no han sido leídas por la Cola (devuelve Verdadero). 
- Si están en la misma posición, el buzón está vacío (devuelve Falso).

### ¿Por qué usar una Cola Circular?
Imagina que el usuario es muy rápido y presiona el botón "Arriba" y luego "Abajo" en el mismo milisegundo. 
Si el sistema solo tuviera una variable global llamada `ultimo_evento`, el evento "Abajo" sobreescribiría al evento "Arriba" antes de que el Cerebro pudiera leerlo. 
Gracias a la Cola de 16 casillas, los eventos se forman en fila ordenadamente. El Cerebro puede leerlos uno por uno sin perder ninguno, garantizando que la interfaz nunca se sienta "trabada" o ignore los clics del médico.
