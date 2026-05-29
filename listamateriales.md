- Dip Switch (geriatrico, adulto, pediatrico, [personalizado - patente pendiente]). Carga valores predeterminados de umbral con alarma activada.

- 3 botones (A, B, C). Los botones van a ser para moverse en un menú de diferentes niveles (TP3):
		- Nivel Main: Elegir entre oxímetro o pulsómetro.
		- Nivel #1: Elegir entre umbral mínimo y máxima.
		- Nivel #2: Elegir entre cambiar valores y activar/desactivar la alarma.

- Sensor Analógico (MAX30102). El sensor MAX30102 se gestionará a través de su pin de interrupción (INT), avisando al microcontrolador cuándo hay un nuevo dato en el FIFO del sensor.

- Leds y buzzer (son para alerta de umbral acompañados de notificación en display). Para cada umbral se cumple un patrón distinto. (Implementación final: incorporar distintos colores).

- Módulo HM-10: Se conecta por I2C.
- Memoria EEPROM (I2C)
- Display LCD 16x2 (I2C <- a verificar)
