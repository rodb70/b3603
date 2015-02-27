/* Copyright (C) 2015 Baruch Even
 *
 * This file is part of the B3603 alternative firmware.
 *
 *  B3603 alternative firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  B3603 alternative firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with B3603 alternative firmware.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "outputs.h"

#include "stm8s.h"

#define PWM_VAL 0x2000
#define PWM_HIGH (PWM_VAL >> 8)
#define PWM_LOW (PWM_VAL & 0xFF)

void pwm_init(void)
{
	/* Timer 1 Channel 1 for Iout control */
	TIM1_CR1 = 0x10; // Down direction
	TIM1_ARRH = PWM_HIGH; // Reload counter = 16384
	TIM1_ARRL = PWM_LOW;
	TIM1_PSCRH = 0; // Prescaler 0 means division by 1
	TIM1_PSCRL = 0;
	TIM1_RCR = 0; // Continuous

	TIM1_CCMR1 = 0x70;    //  Set up to use PWM mode 2.
	TIM1_CCER1 = 0x03;    //  Output is enabled for channel 1, active low
	TIM1_CCR1H = 0x00;      //  Start with the PWM signal off
	TIM1_CCR1L = 0x00;

	TIM1_BKR = 0x80;       //  Enable the main output.

	/* Timer 2 Channel 1 for Vout control */
	TIM2_ARRH = PWM_HIGH; // Reload counter = 16384
	TIM2_ARRL = PWM_LOW;
	TIM2_PSCR = 0; // Prescaler 0 means division by 1
	TIM2_CR1 = 0x00;

	TIM2_CCMR1 = 0x70;    //  Set up to use PWM mode 2.
	TIM2_CCER1 = 0x03;    //  Output is enabled for channel 1, active low
	TIM2_CCR1H = 0x00;      //  Start with the PWM signal off
	TIM2_CCR1L = 0x00;

	// Timers are still off, will be turned on when output is turned on
}

void cvcc_led_cc(void)
{
	PA_ODR |= (1<<3);
	PA_DDR |= (1<<3);
}

void cvcc_led_cv(void)
{
	PA_ODR &= ~(1<<3);
	PA_DDR |= (1<<3);
}

void cvcc_led_off(void)
{
	PA_DDR &= ~(1<<3);
}

uint8_t output_state(void)
{
	return (PB_ODR & (1<<4)) ? 0 : 1;
}

uint16_t pwm_from_set(uint16_t set, calibrate_t *cal)
{
	uint32_t tmp;

	// x*a
	tmp = set;
	tmp *= cal->a;
	tmp >>= 10;

	// x*a + b
	tmp += cal->b;

	// (x*a + b) * PWM_VAL
	tmp *= PWM_VAL;

	// round((x*a + b) * PWM_VAL)
	tmp >>= 10;

	// (x*a + b)/3.3v * PWM_VAL
	tmp *= 310; // (1/3.3v) = (303<<10)/1000
	tmp >>= 10;

	return (uint16_t)tmp;
}

void control_voltage(cfg_output_t *cfg, cfg_system_t *sys)
{
	uint16_t ctr = pwm_from_set(cfg->vset, &sys->vout_pwm);

	TIM2_CCR1H = ctr >> 8;
	TIM2_CCR1L = ctr & 0xFF;
	TIM2_CR1 |= 0x01; // Enable timer
}

void control_current(cfg_output_t *cfg, cfg_system_t *sys)
{
	uint16_t ctr = pwm_from_set(cfg->cset, &sys->cout_pwm);

	TIM1_CCR1H = ctr >> 8;
	TIM1_CCR1L = ctr & 0xFF;
	TIM1_CR1 |= 0x01; // Enable timer
}

void output_commit(cfg_output_t *cfg, cfg_system_t *sys)
{
	if (cfg->output) {
		control_voltage(cfg, sys);
		control_current(cfg, sys);
	}

	if (cfg->output != output_state()) {
		// Startup and shutdown orders need to be in reverse order
		if (cfg->output) {
			// We turned on the PWMs above already
			PB_ODR &= ~(1<<4);

			// Set the CV/CC to some value, it will get reset to actual on next polling cycle
			cvcc_led_cv();
		} else {
			// Set Output Enable OFF
			PB_ODR |= (1<<4);

			// Turn off PWM for Iout
			TIM1_CCR1H = 0;
			TIM1_CCR1L = 0;
			TIM1_CR1 &= 0xFE; // Disable timer

			// Turn off PWM for Vout
			TIM2_CCR1H = 0;
			TIM2_CCR1L = 0;
			TIM2_CR1 &= 0xFE; // Disable timer

			// Turn off CV/CC led
			cvcc_led_off();
		}
	}
}

void output_check_state(cfg_output_t *cfg, uint8_t state_constant_current)
{
	if (cfg->output) {
		if (state_constant_current)
			cvcc_led_cc();
		else
			cvcc_led_cv();
	}
}