# Guía de Verificación: Tiempos, CPU y Consumo (STM32 Bare-Metal)

Esta guía detalla los pasos para verificar empíricamente los parámetros críticos de tiempo real y consumo de energía de tu Monitor Multiparamétrico basado en un esquema **Super-Loop con base de tiempo de 1mS**.

---

## 1. Medición y Análisis de Tiempos de Ejecución de Tareas (WCET)

Dado que **ya tienes implementado el uso del DWT (Data Watchpoint and Trace)** en el código, este es el flujo de trabajo ideal:

1. **Lectura de Ciclos por Tarea:**
   - Envuelve cada tarea (ej. lectura del MAX30102, actualización de LCD, envío Bluetooth) con lecturas de `DWT->CYCCNT` antes y después de su ejecución.
   - Resta los valores para obtener la cantidad de ciclos de reloj de cada tarea.
   - Divide la cantidad de ciclos por la frecuencia de reloj del procesador (ej. si usas 72 MHz, divides por 72.000.000) para obtener el **WCET en segundos** para esa tarea específica.

2. **Cálculo del WCET Total:**
   - Suma manualmente los WCET individuales de las tareas. Ese será tu WCET total para un ciclo completo del Super-Loop en el peor de los casos.

---

## 2. Cálculo del Factor de Uso (U) de la CPU

El factor de uso $U$ indica qué porcentaje del tiempo disponible está el procesador haciendo trabajo útil.
Con los WCET de cada tarea ya calculados con el DWT, el cálculo a mano de $U$ es muy preciso:

$$U = \frac{\text{Suma manual de los WCET de las tareas ejecutadas en el ciclo}}{\text{Base de tiempo (1 ms)}}$$

### Perfilado de la CPU en diferentes Modos Operativos
El abordaje que propones es excelente. Dado que tu sistema puede transicionar por distintos estados, $U$ no será estático. Deberías calcular y documentar $U$ para cada "Modo", por ejemplo:
- **Modo Máxima Carga:** Muestreo del sensor + Telemetría Bluetooth activada + LCD actualizando.
- **Modo Parcial:** Muestreo de sensor + Telemetría Bluetooth desactivada.
- **Modo Reposo/Alarma:** Sin lectura de sensor, pero con alarmas visuales/sonoras.

Esto te permitirá demostrar formalmente que aún en el "Modo Máxima Carga", el procesador cumple con el deadline y mantiene $U \le 1$.

---

## 3. Medición y Análisis de Consumo (3.3V y 5V)

### Medición del Baseline de Bajo Consumo (Low-Power)
Tu planteo de **comentar la llamada principal de la aplicación (`app_update()`)** para que el ciclo de ejecución cíclico solo corra la instrucción `__WFI()` (Wait For Interrupt) es **la forma perfecta** de aislar la corriente mínima (baseline).

1. Comenta `app_update()` en el Super-Loop.
2. Abre la línea de alimentación de 3.3V (y luego la de 5V) y coloca el multímetro (miliamperímetro) en serie.
3. El valor medido será el **consumo estático base** de tu sistema durmiendo.

### Medición de Consumo Dinámico (Modo Normal)
1. Descomenta `app_update()` para que el equipo corra en su modo de máxima carga.
2. Vuelve a medir con el multímetro para obtener la corriente media de operación.
3. **Picos (Opcional):** Si quieres ir más allá de la media del multímetro, inserta una resistencia Shunt (ej. $1 \Omega$) en serie con la línea de alimentación, conecta el osciloscopio en paralelo a la resistencia, y mide la caída de tensión máxima ($\Delta V$) para ver los picos de corriente (ej. el pico al momento en que el módulo Bluetooth transmite).

---

## 4. Verificación del Ejecutor Cíclico (1 vuelta < 1mS)

Para certificar visualmente que el sistema cumple con el requisito estricto de tiempo real sin usar DWT, puedes utilizar el osciloscopio:

1. **Test de Toggle en Osciloscopio:**
   Agrega una línea para invertir (toggle) un pin justo antes de evaluar el flag del SysTick.
   ```c
   while(1) {
       if(flag_1ms) {
           HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_2); // Toggle en cada ciclo validado
           flag_1ms = 0;
           app_update(); // Tus tareas
           __WFI(); // O reposo
       }
   }
   ```
   - En el osciloscopio conectado a `PA2`, deberías ver una **onda cuadrada perfecta de exactamente 500 Hz**. 
   - Un período de onda completa dura 2ms (1ms en estado ALTO + 1ms en estado BAJO).
   - **Criterio de Falla:** Si ves "jitter" (fluctuaciones en el ancho), desfases, o la frecuencia baja de 500 Hz, significa que la suma de tus WCET superó 1ms. El sistema perdió la base de tiempo.
