#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"

#define OVER_VOLTAGE_THRESHOLD 3661 // Equivalent to ~2.95V for a 3.3V ADC reference
#define UNDER_VOLTAGE_THRESHOLD 3201 // Equivalent to ~2.58V for a 3.3V ADC reference

void ADC_Init(void);
void Timer0_Init(void);
void GPIO_Init(void);

volatile unsigned int adc_value;

int main(void)
{
    ADC_Init();     // Initialize ADC for sampling
    Timer0_Init();  // Configure Timer0 to trigger ADC at regular intervals
    GPIO_Init();    // Initialize GPIO for LED outputs

    while (1)
    {
        // Check ADC value and take actions for over/under-voltage conditions
        if (adc_value >= OVER_VOLTAGE_THRESHOLD)
            GPIO_PORTF_DATA_R = 0x02; // Turn on Green LED (PF3) for over-voltage
        else if (adc_value <= UNDER_VOLTAGE_THRESHOLD)
            GPIO_PORTF_DATA_R = 0x08; // Turn on Red LED (PF1) for under-voltage
        else
            GPIO_PORTF_DATA_R = 0x00; // Turn off LEDs for normal voltage
    }
}

// ADC initialization
void ADC_Init(void)
{
    SYSCTL_RCGCADC_R |= (1 << 0);  // Enable Clock to ADC0
    SYSCTL_RCGCGPIO_R |= (1 << 4); // Enable Clock to GPIOE


    // Configure PE3 as AIN0
    GPIO_PORTE_AFSEL_R |= (1 << 3);  // Enable alternate function
    GPIO_PORTE_DEN_R &= ~(1 << 3);   // Disable digital functionality
    GPIO_PORTE_AMSEL_R |= (1 << 3);  // Enable analog functionality

    // Configure ADC0 Sample Sequencer 3
    ADC0_ACTSS_R &= ~(1 << 3);        // Disable SS3 during configuration
    ADC0_EMUX_R = (ADC0_EMUX_R & ~0xF000) | 0x5000; // Timer trigger for SS3 (not clear)
    ADC0_SSMUX3_R = 0;                // Get input from AIN0
    ADC0_SSCTL3_R = (1 << 1) | (1 << 2); // Take one sample, set end-of-sequence flag (not clear)
    ADC0_ACTSS_R |= (1 << 3);         // Enable SS3
    ADC0_IM_R |= (1 << 3);            // Enable interrupt for SS3
    NVIC_EN0_R |= (1 << 17);      // Enable ADC0 SS3 interrupt in NVIC
}

// Timer0 initialization
void Timer0_Init(void)
{
    SYSCTL_RCGCTIMER_R |= (1 << 0);  // Enable Timer0 clock
    TIMER0_CTL_R = 0;                // Disable Timer0 for configuration
    TIMER0_CFG_R = 0x00;             // Configure as 32-bit timer
    TIMER0_TAMR_R = 0x02;            // Set periodic mode
    TIMER0_TAILR_R = 160000;         // Load value for 10ms interval (16MHz clock)
    TIMER0_CTL_R |= 0x20;            // Enable ADC trigger on timeout
    TIMER0_CTL_R |= 0x01;            // Enable Timer0
}

// GPIO initialization for LEDs
void GPIO_Init(void)
{
    SYSCTL_RCGCGPIO_R |= (1 << 5);   // Enable clock for GPIOF
    GPIO_PORTF_DIR_R |= 0x0A;        // Set PF1 (Red LED) and PF3 (Green LED) as outputs
    GPIO_PORTF_DEN_R |= 0x0A;        // Enable digital functionality on PF1 and PF3
}

// ADC0 SS3 interrupt handler
void ADC0SS3_Handler(void)
{
    adc_value = ADC0_SSFIFO3_R; // Read conversion result
    ADC0_ISC_R = (1 << 3);      // Clear interrupt flag
}
