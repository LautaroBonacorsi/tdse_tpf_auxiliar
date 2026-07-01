# Guía de Armado y Conexión - Proyecto TDSE

Esta guía detalla cómo conectar el hardware necesario a la placa **Nucleo-F103RB** para testear el firmware del proyecto. Se describen los componentes, los pines a utilizar, los valores de los componentes pasivos (resistencias, capacitores) y la justificación de cada conexión según el código provisto.

---

## 1. Consideraciones Iniciales (Alimentación)
Todos los periféricos externos y circuitos descritos a continuación operan con niveles lógicos de **3.3V**, provistos directamente por los pines de la placa Nucleo (pines marcados como `3V3` y `GND`).
*   **Precaución:** No utilizar la salida de 5V para alimentar sensores lógicos de 3.3V a menos que el módulo cuente con un regulador interno, para no dañar los pines GPIO de la STM32.

---

## 2. Sensor MAX30102 (Oxímetro y Frecuencia Cardíaca)
El módulo MAX30102 se comunica vía I2C y utiliza un pin de interrupción para notificar cuando los datos están listos.

*   **Conexiones:**
    *   **VIN / VCC:** Pin `3V3` de la Nucleo.
    *   **GND:** Pin `GND` de la Nucleo.
    *   **SCL:** Pin `PB6` (I2C1_SCL).
    *   **SDA:** Pin `PB7` (I2C1_SDA).
    *   **INT:** Pin `PB10` (MAX_INT_PIN).
*   **Componentes Adicionales:** 
    *   **Resistores Pull-up I2C:** Las líneas I2C (SDA y SCL) requieren resistencias *pull-up* hacia 3.3V. **Valor recomendado:** `4.7 kΩ`. *Nota: Muchos módulos MAX30102 ya incluyen estas resistencias en su placa (PCB). Si tu módulo las tiene, no es necesario agregarlas externamente.*
*   **Por qué:** El código configura el periférico `I2C1` en los pines `PB6` y `PB7`. El pin `PB10` se usa para leer las interrupciones hardware que genera el sensor (ej. "Dato listo" o "Dedo detectado"), evitando así consultar continuamente (polling) al sensor y ahorrando CPU.

---

## 3. Display LCD (Comunicación I2C)
El proyecto incluye soporte para una pantalla LCD (usualmente con módulo adaptador I2C PCF8574) que comparte el mismo bus I2C que el sensor MAX30102.

*   **Conexiones mediante Adaptador de Niveles Lógicos (Módulo 8 canales CJMCU/TXB0108):**
    Como la Nucleo opera a 3.3V y los displays LCD típicos requieren 5V para encender correctamente e iluminar los caracteres, es ideal usar tu adaptador de niveles lógicos bidireccional.
    *   **Lado de Bajo Voltaje (VA - 3.3V hacia Nucleo):**
        *   `VA` -> Pin `3V3` de la Nucleo.
        *   `GND` -> Pin `GND` de la Nucleo.
        *   `OE` (Output Enable) -> Pin `3V3` de la Nucleo (¡Importante! Debe estar a 3.3V para habilitar el chip).
        *   `A1` -> Pin `PB6` de la Nucleo (I2C1_SCL).
        *   `A2` -> Pin `PB7` de la Nucleo (I2C1_SDA).
    *   **Lado de Alto Voltaje (VB - 5V hacia LCD):**
        *   `VB` -> Pin `5V` de la Nucleo.
        *   `GND` -> `GND` del módulo LCD (y asegurar que los GNDs se unan).
        *   `B1` -> `SCL` del módulo LCD.
        *   `B2` -> `SDA` del módulo LCD.
        *   Conectar también el `VCC` del módulo LCD a la línea de `5V` (misma que va a `VB`).
*   **Por qué:** El driver (`driver_lcd_i2c.c`) reutiliza el periférico `I2C1` (`hi2c1`) para comunicarse. El adaptador de niveles protege el bus de 3.3V de la Nucleo y garantiza que el LCD reciba señales claras de 5V. En este módulo en particular, el pin `OE` activa los canales de traducción internos, por lo que dejarlo desconectado impediría que las señales pasen de un lado a otro.

---

## 4. Botones de Control (Pulsadores)
El sistema cuenta con 4 botones (`MENU`, `UP`, `DOWN`, `ACK`). En el código se define el estado de presionado como `BTN_PRESSED` = `GPIO_PIN_SET` (alto, 3.3V), por lo tanto, deben conectarse utilizando una configuración **Pull-Down**.

*   **Conexiones de Pines:**
    *   **BTN_MENU:** Pin `PC0`
    *   **BTN_UP:** Pin `PC1`
    *   **BTN_DOWN:** Pin `PC2`
    *   **BTN_ACK:** Pin `PC3`
*   **Esquema de conexión por cada botón:**
    1.  Conectar una terminal del pulsador a **3.3V**.
    2.  Conectar la otra terminal del pulsador al **Pin GPIO** correspondiente (ej. PC0).
    3.  Desde esa misma terminal (la que va al pin GPIO), conectar una resistencia a **GND**.
*   **Componentes Adicionales:**
    *   **Resistencias Pull-Down:** `10 kΩ` (una por botón, total 4).
    *   *(Opcional)* **Capacitor de Debouncing:** `100 nF` (0.1 µF) en paralelo con la resistencia a GND para ayudar a filtrar rebotes de hardware, aunque el código probablemente ya implemente *debouncing* por software (se observa un manejo de retardos y estados `ST_BTN_FALLING` en la máquina de estados).
*   **Por qué:** Al presionar el botón, el pin se conecta a 3.3V, leyendo un nivel alto (`1` lógico). Al soltarlo, la resistencia de pull-down asegura que el pin lea `0V` (`0` lógico), evitando que quede "flotando" y genere lecturas falsas por ruido electromagnético.

---

## 5. DIP Switch (Configuraciones)
El DIP switch de 4 posiciones se utiliza para establecer opciones de configuración. Funciona con la misma lógica que los botones (`DIP_ON` = `GPIO_PIN_SET`).

*   **Conexiones de Pines:**
    *   **DIP1:** Pin `PA0`
    *   **DIP2:** Pin `PA1`
    *   **DIP3:** Pin `PA4`
    *   **DIP4:** Pin `PB0`
*   **Esquema de conexión por cada switch:**
    1.  Lado de entrada del switch a **3.3V**.
    2.  Lado de salida del switch al **Pin GPIO** y a una resistencia conectada a **GND** (configuración Pull-Down).
*   **Componentes Adicionales:**
    *   **Resistencias Pull-Down:** `10 kΩ` (una por cada línea del DIP switch, total 4).
*   **Por qué:** Igual que los botones, para asegurar un valor lógico de `0` estable cuando el interruptor está apagado, y leer un `1` cuando está encendido.

---

## 6. LEDs Indicadores
Se definen dos LEDs indicadores externos: Amarillo y Rojo.

*   **Conexiones de Pines:**
    *   **LED_YELLOW:** Pin `PB1`
    *   **LED_RED:** Pin `PB2`
*   **Esquema de conexión:**
    1.  Pin GPIO a la resistencia limitadora.
    2.  Resistencia limitadora al ánodo (pata larga) del LED.
    3.  Cátodo (pata corta) del LED a **GND**.
*   **Componentes Adicionales:**
    *   **Resistencias Limitadoras de Corriente:** `220 Ω` a `330 Ω`.
*   **Por qué:** El pin GPIO entrega 3.3V (`LED_ON = GPIO_PIN_SET`). El LED necesita una resistencia en serie para limitar la corriente (usualmente a ~10mA) y evitar que se queme el LED o se dañe el pin del microcontrolador.

---

## 7. Salida PWM - Zumbador (Buzzer) o LED Extra
El código inicializa el temporizador `TIM3` con salida PWM en el canal 1.
*   **Pin de Conexión:** Pin `PA6` (TIM3_CH1).
*   **Uso probable:** Un buzzer pasivo para generar tonos/alarmas sonoras, o para atenuación de un LED.
*   **Esquema de conexión (Buzzer Pasivo):** Conectar el terminal positivo del buzzer pasivo a `PA6`, y el terminal negativo a `GND`. Si el buzzer requiere más corriente de la que puede entregar el pin de la placa, se deberá usar un transistor NPN (ej. 2N2222) como interruptor, más una resistencia de `1 kΩ` en la base.

---

## 8. Comunicación Serial UART (Opcional)
El código inicializa dos interfaces UART:
1.  **USART2 (Pines PA2/PA3):** Estos pines están físicamente conectados al módulo ST-LINK en la misma placa Nucleo. Proporcionan el "Virtual COM Port". No necesitas conectar nada externamente; simplemente usa un cable USB y un software de terminal (Putty, TeraTerm) a 115200 baudios (usualmente) para ver los logs del sistema (`LOGGER_INFO`).
2.  **USART1 (Pines PA9/PA10):** Estos pines (PA9=TX, PA10=RX) están disponibles para conectarse a un periférico externo serial, como un módulo Bluetooth HC-05 u otro dispositivo.
    *   *Nota de cruce:* Si conectas un módulo, recuerda que el `PA9 (TX)` de la Nucleo va al `RX` del módulo, y el `PA10 (RX)` de la Nucleo va al `TX` del módulo.

---

## Resumen de Pines a la Placa (Cheat Sheet)

| Componente | Pin STM32 | Función | Tipo de Señal / Extra |
| :--- | :--- | :--- | :--- |
| MAX30102 SCL | **PB6** | I2C1_SCL | Pull-up (4.7kΩ) si no lo trae el módulo |
| MAX30102 SDA | **PB7** | I2C1_SDA | Pull-up (4.7kΩ) si no lo trae el módulo |
| LCD I2C SCL | **PB6** | I2C1_SCL | Vía adaptador de niveles lógicos a 5V |
| LCD I2C SDA | **PB7** | I2C1_SDA | Vía adaptador de niveles lógicos a 5V |
| MAX30102 INT | **PB10** | Interrupción | - |
| Botón MENU | **PC0** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| Botón UP | **PC1** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| Botón DOWN | **PC2** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| Botón ACK | **PC3** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| DIP 1 | **PA0** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| DIP 2 | **PA1** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| DIP 3 | **PA4** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| DIP 4 | **PB0** | Entrada GPIO | Activo en Alto (Pull-Down de 10kΩ) |
| LED Amarillo | **PB1** | Salida GPIO | Resistencia en serie 220Ω |
| LED Rojo | **PB2** | Salida GPIO | Resistencia en serie 220Ω |
| PWM (Buzzer) | **PA6** | TIM3_CH1 | Señal PWM |
