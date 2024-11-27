#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"

#define SYSCTL_RCGC2_GPIOF      0x00000020  // GPIO F Clock Gating Control
#define SYSCTL_RCGCTIMER_TIMER0 0x00000001  // Timer 0 Clock Gating Control
#define SYSCTL_RCGCADC_ADC0     0x00000001  // ADC 0 Clock Gating Control
#define SYSCTL_RCGCGPIO_GPIOE   0x00000010  // GPIO E Clock Gating Control

#define OVER_VOLTAGE_THRESHOLD 3661 // Equivalent to ~2.95V for a 3.3V ADC reference
#define UNDER_VOLTAGE_THRESHOLD 3201 // Equivalent to ~2.58V for a 3.3V ADC reference

volatile uint32_t grid_voltage;
volatile uint32_t i = 0;
volatile uint32_t datos[10];
volatile int j;

//void GPIOPortE_Handler(void);

void Timer0_Handler(void) {
    // Clear the timer interrupt flag
    TIMER0_ICR_R = 0x01;
}

void main(void) {
    // 1. Enable clocks for GPIOF, GPIOE, TIMER0, and ADC0
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOF | SYSCTL_RCGCGPIO_GPIOE;
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_TIMER0;
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_ADC0;

    // Allow time for the peripherals to be ready
    volatile int delay = SYSCTL_RCGCGPIO_R;

    // 2. Configure GPIOF for LEDs
    GPIO_PORTF_DIR_R |= 0x0E;  // Pins 1, 2, 3 as outputs
    GPIO_PORTF_DEN_R |= 0x0E;  // Enable digital functionality

    // 3. Configure GPIOE for ADC input (PE3)
    GPIO_PORTE_AFSEL_R |= 0x08; // Enable alternate function for PE3
    GPIO_PORTE_DEN_R &= ~0x08;  // Disable digital functionality for PE3
    GPIO_PORTE_AMSEL_R |= 0x08; // Enable analog functionality for PE3

    // 4. Configure Timer0 for periodic mode
    TIMER0_CTL_R = 0;                  // Disable Timer0 during configuration
    TIMER0_CFG_R = 0x00000000;         // Configure for 32-bit timer mode
    TIMER0_TAMR_R = 0x02;              // Configure for periodic mode
    TIMER0_TAILR_R = 160000;           // Load value
    TIMER0_CTL_R |= 0x20;              // Enable ADC trigger on timeout
    TIMER0_CTL_R |= 0x01;              // Enable Timer0
    TIMER0_IMR_R |= 0x01;              // Enable Timer0 timeout interrupt

    // 5. Configure ADC0
    ADC0_ACTSS_R &= ~0x08;             // Disable sample sequencer 3 during config
    ADC0_EMUX_R = (ADC0_EMUX_R & 0xFFFF0FFF) | 0x5000; // Timer triggers sequencer 3
    ADC0_SSMUX3_R = 0;                 // Select channel 0 (AIN0, PE3)
    ADC0_SSCTL3_R = 0x06;              // Single-ended, interrupt enable, end of sequence
    ADC0_IM_R |= 0x08;                 // Enable SS3 interrupt
    ADC0_ACTSS_R |= 0x08;              // Enable sample sequencer 3

    // 6. Enable ADC0 interrupts in NVIC
    NVIC_EN0_R |= 1 << 17;             // Enable interrupt 17 (ADC0SS3)
    NVIC_EN0_R |= 1 << 19;             // Enable interrupt 19 (Timer0A)

    // 7. Enable global interrupts
    __asm(" CPSIE I");                 // Assembly instruction to enable interrupts

    while (1) {
        // Main loop does nothing; everything is handled by the ADC interrupt
    }
}

// ADC0 Sequencer 3 Interrupt Handler
void ADC0Seq3_Handler(void) {
    ADC0_ISC_R = 0x08;                 // Clear the interrupt flag
    grid_voltage = ADC0_SSFIFO3_R;          // Read the conversion result

    if (grid_voltage > OVER_VOLTAGE_THRESHOLD) {
        // Overvoltage detected, blink green LED
        GPIO_PORTF_DATA_R |= 0x08;     // Turn on green LED
        GPIO_PORTF_DATA_R &= ~0x06;    // Turn off other LEDs
    } else if (grid_voltage < UNDER_VOLTAGE_THRESHOLD) {
        // Undervoltage detected, blink red LED
        GPIO_PORTF_DATA_R |= 0x02;     // Turn on red LED
        GPIO_PORTF_DATA_R &= ~0x0C;    // Turn off other LEDs
    } else {
        // Normal voltage, turn off red and green LEDs
        GPIO_PORTF_DATA_R &= ~0x0A;    // Turn off red and green LEDs
        GPIO_PORTF_DATA_R |= 0x04;     // Turn on blue LED to indicate normal
    }

    // Small delay (simulates processing time)
    for (j = 0; j < 200000; j++);
}
