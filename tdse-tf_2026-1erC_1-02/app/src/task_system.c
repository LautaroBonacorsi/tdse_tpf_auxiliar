/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include <stdio.h>
/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"

#include "task_display_attribute.h"
#include "task_display_interface.h"
#include "task_system_attribute.h"
#include "task_system_interface.h"

/********************** macros and definitions *******************************/
#define DEL_SYS_MIN			0ul
#define DISPLAY_COLS		16u

/* Modes to excite Task System */
typedef enum task_system_mode {
	NORMAL,
	SETUP,
	MODE_QTY
} task_system_mode_t;

#define SYSTEM_DTA_QTY		MODE_QTY

/********************** internal data declaration ****************************/
task_system_dta_t task_system_dta_list[SYSTEM_DTA_QTY];

/*
 * Variables del modelo de Itemis.
 *
 * selectedSensor:
 * 0 = oximetro
 * 1 = pulsometro
 *
 * paramUmbral:
 * 0 = umbral minimo
 * 1 = umbral maximo
 * 2 = alarma
 */
static int32_t selectedSensor = 0;
static int32_t paramUmbral = 0;

static int32_t ouMax = 10;
static int32_t ouMin = 0;
static bool oAlarma = false;

static int32_t puMax = 10;
static int32_t puMin = 0;
static bool pAlarma = false;

static int32_t auxMax = 10;
static int32_t auxMin = 0;
static bool auxAlarma = false;

static char display_line_1[DISPLAY_COLS + 1u];
static char display_line_2[DISPLAY_COLS + 1u];

/********************** internal functions declaration ***********************/
static void task_system_normal_statechart(void);
static void task_system_setup_statechart(void);

static void task_system_set_mode(task_system_mode_t task_system_mode);
static void task_system_set_setup_state(task_system_st_t state);

static void task_system_show_normal(void);
static void task_system_show_setup_state(void);

static void task_system_write_display(const char *line_1,
									  const char *line_2);

/********************** internal data definition *****************************/
const char *p_task_system 		= "Task System (System Statechart)";
const char *p_task_system_ 		= "Non-Blocking Code";
const char *p_task_system__ 	= "(Update by Time Code, period = 1mS)";

/********************** external data declaration ****************************/
task_system_mode_t g_task_system_mode;

/********************** external functions definition ************************/
void task_system_init(void *parameters)
{
	uint32_t index;
	task_system_dta_t *p_task_system_dta;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu",
				GET_NAME(task_system_init),
				HAL_GetTick());

	LOGGER_INFO("   %s is a %s",
				GET_NAME(task_system),
				p_task_system);

	LOGGER_INFO("   %s is a %s",
				GET_NAME(task_system),
				p_task_system_);

	LOGGER_INFO("   %s is a %s",
				GET_NAME(task_system),
				p_task_system__);

	init_event_task_system();

	for (index = 0; index < SYSTEM_DTA_QTY; index++)
	{
		p_task_system_dta = &task_system_dta_list[index];

		p_task_system_dta->tick = DEL_SYS_MIN;
		p_task_system_dta->state = ST_SYS_NORMAL;
		p_task_system_dta->event = EV_SYS_IDLE;
		p_task_system_dta->flag = false;
	}

	task_system_set_mode(NORMAL);
	task_system_show_normal();
}


void task_system_update(void *parameters)
{
	switch (g_task_system_mode)
	{
		case NORMAL:

			task_system_normal_statechart();

			break;

		case SETUP:

			task_system_setup_statechart();

			break;

		default:

			task_system_set_mode(NORMAL);
			task_system_show_normal();

			break;
	}
}


static void task_system_normal_statechart(void)
{
	task_system_dta_t *p_task_system_dta;

	p_task_system_dta = &task_system_dta_list[NORMAL];

	if (false == any_event_task_system())
	{
		return;
	}

	p_task_system_dta->event = get_event_task_system();
	p_task_system_dta->flag = true;

	/*
	 * En modo NORMAL, el botón MENU ingresa
	 * al menú de configuración. (mapeado a EV_SYS_ESCAPE)
	 */
	if (EV_SYS_ESCAPE == p_task_system_dta->event)
	{
		task_system_set_setup_state(ST_SYS_MENU1_SENSOR);
		task_system_set_mode(SETUP);
	}

	p_task_system_dta->flag = false;
}


static void task_system_setup_statechart(void)
{
	task_system_dta_t *p_task_system_dta;

	p_task_system_dta = &task_system_dta_list[SETUP];

	if (false == any_event_task_system())
	{
		return;
	}

	p_task_system_dta->event = get_event_task_system();
	p_task_system_dta->flag = true;

	switch (p_task_system_dta->state)
	{
		/*
		 * Menu 1:
		 * Selección entre oxímetro y pulsómetro.
		 */
		case ST_SYS_MENU1_SENSOR:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				selectedSensor =
					(0 == selectedSensor) ? 1 : 0;

				task_system_show_setup_state();
			}
			else if (EV_SYS_ENTER ==
					 p_task_system_dta->event)
			{
				task_system_set_setup_state(
					ST_SYS_MENU2_PARAMETER);
			}
			else if (EV_SYS_ESCAPE ==
					 p_task_system_dta->event)
			{
				/*
				 * Esta transición no aparece en el modelo,
				 * pero es necesaria para salir de SETUP.
				 */

				task_system_set_mode(NORMAL);
				task_system_show_normal();
			}

			break;

		/*
		 * Menu 2:
		 * Selección entre mínimo, máximo y alarma.
		 */
		case ST_SYS_MENU2_PARAMETER:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				paramUmbral = (paramUmbral + 1) % 3;

				task_system_show_setup_state();
			}
			else if (EV_SYS_ESCAPE ==
					 p_task_system_dta->event)
			{
				task_system_set_setup_state(
					ST_SYS_MENU1_SENSOR);
			}
			else if (EV_SYS_ENTER ==
					 p_task_system_dta->event)
			{
				/*
				 * Implementación del pseudoestado Choice
				 * del modelo de Itemis.
				 */
				if (0 == paramUmbral)
				{
					task_system_set_setup_state(
						ST_SYS_MENU3_MINIMUM);
				}
				else if (1 == paramUmbral)
				{
					task_system_set_setup_state(
						ST_SYS_MENU3_MAXIMUM);
				}
				else
				{
					task_system_set_setup_state(
						ST_SYS_MENU3_ALARM);
				}
			}

			break;

		/*
		 * Menu 3:
		 * Edición del umbral mínimo.
		 */
		case ST_SYS_MENU3_MINIMUM:

			if (EV_SYS_NEXT ==
				p_task_system_dta->event)
			{
				auxMin++;
				task_system_show_setup_state();
			}
			else if (EV_SYS_DOWN ==
					 p_task_system_dta->event)
			{
				auxMin--;
				task_system_show_setup_state();
			}
			else if (EV_SYS_ESCAPE ==
					 p_task_system_dta->event)
			{
				/*
				 * Escape cancela la edición porque
				 * auxMin todavía no fue copiado.
				 */
				task_system_set_setup_state(
					ST_SYS_MENU2_PARAMETER);
			}
			else if (EV_SYS_ENTER ==
					 p_task_system_dta->event)
			{
				if ((0 == selectedSensor) &&
					(auxMin < ouMax))
				{
					ouMin = auxMin;

					task_system_set_setup_state(
						ST_SYS_MENU2_PARAMETER);
				}
				else if ((1 == selectedSensor) &&
						 (auxMin < puMax))
				{
					puMin = auxMin;

					task_system_set_setup_state(
						ST_SYS_MENU2_PARAMETER);
				}
			}

			break;

		/*
		 * Menu 3:
		 * Edición del umbral máximo.
		 */
		case ST_SYS_MENU3_MAXIMUM:

			if (EV_SYS_NEXT ==
				p_task_system_dta->event)
			{
				auxMax++;
				task_system_show_setup_state();
			}
			else if (EV_SYS_DOWN ==
					 p_task_system_dta->event)
			{
				auxMax--;
				task_system_show_setup_state();
			}
			else if (EV_SYS_ESCAPE ==
					 p_task_system_dta->event)
			{
				task_system_set_setup_state(
					ST_SYS_MENU2_PARAMETER);
			}
			else if (EV_SYS_ENTER ==
					 p_task_system_dta->event)
			{
				/*
				 * En el statechart aparece auxMin.
				 * Acá corresponde comparar auxMax.
				 */
				if ((0 == selectedSensor) &&
					(auxMax > ouMin))
				{
					ouMax = auxMax;

					task_system_set_setup_state(
						ST_SYS_MENU2_PARAMETER);
				}
				else if ((1 == selectedSensor) &&
						 (auxMax > puMin))
				{
					puMax = auxMax;

					task_system_set_setup_state(
						ST_SYS_MENU2_PARAMETER);
				}
			}

			break;

		/*
		 * Menu 3:
		 * Activación o desactivación de alarma.
		 */
		case ST_SYS_MENU3_ALARM:

			if (EV_SYS_NEXT ==
				p_task_system_dta->event)
			{
				auxAlarma = !auxAlarma;

				task_system_show_setup_state();
			}
			else if (EV_SYS_ESCAPE ==
					 p_task_system_dta->event)
			{
				task_system_set_setup_state(
					ST_SYS_MENU2_PARAMETER);
			}
			else if (EV_SYS_ENTER ==
					 p_task_system_dta->event)
			{
				if (0 == selectedSensor)
				{
					oAlarma = auxAlarma;
				}
				else
				{
					pAlarma = auxAlarma;
				}

				task_system_set_setup_state(
					ST_SYS_MENU2_PARAMETER);
			}

			break;

		default:

			task_system_set_setup_state(
				ST_SYS_MENU1_SENSOR);

			break;
	}

	/*
	 * Cada evento se procesa una sola vez.
	 * EV_SYS_IDLE, generado al soltar un botón,
	 * simplemente se descarta.
	 */
	p_task_system_dta->flag = false;
}


static void task_system_set_setup_state(task_system_st_t state)
{
	task_system_dta_t *p_task_system_dta;

	p_task_system_dta = &task_system_dta_list[SETUP];

	p_task_system_dta->state = state;

	/*
	 * Acciones de entrada de los estados Menu 3.
	 * Copian el valor real a una variable auxiliar.
	 */
	switch (state)
	{
		case ST_SYS_MENU3_MINIMUM:

			auxMin = (0 == selectedSensor) ?
					 ouMin :
					 puMin;

			break;

		case ST_SYS_MENU3_MAXIMUM:

			auxMax = (0 == selectedSensor) ?
					 ouMax :
					 puMax;

			break;

		case ST_SYS_MENU3_ALARM:

			auxAlarma = (0 == selectedSensor) ?
						oAlarma :
						pAlarma;

			break;

		default:

			break;
	}

	task_system_show_setup_state();
}


static void task_system_show_normal(void)
{
	task_system_write_display(
		"task_system_mode",
		" NORMAL");
}


static void task_system_show_setup_state(void)
{
	task_system_st_t state;

	state = task_system_dta_list[SETUP].state;

	switch (state)
	{
		case ST_SYS_MENU1_SENSOR:

			task_system_write_display(
				"SELECT SENSOR",
				(0 == selectedSensor) ?
				"> OXIMETER" :
				"> PULSE METER");

			break;

		case ST_SYS_MENU2_PARAMETER:

			if (0 == paramUmbral)
			{
				task_system_write_display(
					(0 == selectedSensor) ?
					"OXIMETER SETUP" :
					"PULSE SETUP",
					"> MINIMUM");
			}
			else if (1 == paramUmbral)
			{
				task_system_write_display(
					(0 == selectedSensor) ?
					"OXIMETER SETUP" :
					"PULSE SETUP",
					"> MAXIMUM");
			}
			else
			{
				task_system_write_display(
					(0 == selectedSensor) ?
					"OXIMETER SETUP" :
					"PULSE SETUP",
					"> ALARM");
			}

			break;

		case ST_SYS_MENU3_MINIMUM:

			snprintf(
				display_line_1,
				sizeof(display_line_1),
				"%-16.16s",
				"MINIMUM VALUE");

			snprintf(
				display_line_2,
				sizeof(display_line_2),
				"VALUE: %-9ld",
				(long)auxMin);

			put_event_task_display(
				0,
				0,
				display_line_1);

			put_event_task_display(
				0,
				1,
				display_line_2);

			break;

		case ST_SYS_MENU3_MAXIMUM:

			snprintf(
				display_line_1,
				sizeof(display_line_1),
				"%-16.16s",
				"MAXIMUM VALUE");

			snprintf(
				display_line_2,
				sizeof(display_line_2),
				"VALUE: %-9ld",
				(long)auxMax);

			put_event_task_display(
				0,
				0,
				display_line_1);

			put_event_task_display(
				0,
				1,
				display_line_2);

			break;

		case ST_SYS_MENU3_ALARM:

			task_system_write_display(
				"ALARM STATE",
				auxAlarma ?
				"> ON" :
				"> OFF");

			break;

		default:

			task_system_write_display(
				"SETUP MENU",
				"INVALID STATE");

			break;
	}
}


static void task_system_write_display(const char *line_1,
									  const char *line_2)
{
	/*
	 * %-16.16s completa cada línea con espacios
	 * y limita el texto a las 16 columnas del LCD.
	 */
	snprintf(
		display_line_1,
		sizeof(display_line_1),
		"%-16.16s",
		line_1);

	snprintf(
		display_line_2,
		sizeof(display_line_2),
		"%-16.16s",
		line_2);

	put_event_task_display(
		0,
		0,
		display_line_1);

	put_event_task_display(
		0,
		1,
		display_line_2);
}


static void task_system_set_mode(
	task_system_mode_t task_system_mode)
{
	g_task_system_mode = task_system_mode;
}
/********************** end of file ******************************************/
