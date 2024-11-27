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
volatile int j;

// Function prototypes
void UART5_Init(void);
void UART5_SendString(const char *str);
void UART5_SendNumber(uint32_t num);
void Timer0_Handler(void);
void ADC0Seq3_Handler(void);

void UART5_Init(void) {
    // Enable UART5 and GPIOE clocks
    SYSCTL_RCGCUART_R |= 0x00000020; // Enable clock for UART5
    SYSCTL_RCGCGPIO_R |= 0x00000010; // Enable clock for GPIOE

    // Configure PE4 (RX) and PE5 (TX) for UART functionality
    GPIO_PORTE_AFSEL_R |= 0x30;       // Enable alternate function for PE4, PE5
    GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R & 0xFF00FFFF) + 0x00110000; // Configure UART pins
    GPIO_PORTE_DEN_R |= 0x30;         // Digital enable for PE4, PE5

    // Configure UART5 for 9600 baud rate
    UART5_CTL_R &= ~0x0001;           // Disable UART5 during configuration
    UART5_IBRD_R = 104;               // Integer part of baud rate divisor
    UART5_FBRD_R = 11;                // Fractional part of baud rate divisor
    UART5_LCRH_R = 0x0060;            // 8-bit word length, no parity, 1 stop bit
    UART5_CTL_R = 0x0301;             // Enable UART5, TXE, RXE
}

void UART5_SendString(const char *str) {
    while (*str) {
        while ((UART5_FR_R & 0x0020) != 0); // Wait until TX FIFO is not full
        UART5_DR_R = *str++;
    }
    // Add carriage return and newline
    while ((UART5_FR_R & 0x0020) != 0);
    UART5_DR_R = '\r';
    while ((UART5_FR_R & 0x0020) != 0);
    UART5_DR_R = '\n';
}

void UART5_SendNumber(uint32_t num) {
    char buffer[12];  // Buffer to store the number as a string
    int index = 0;

    // Handle zero case
    if (num == 0) {
        buffer[index++] = '0';
    } else {
        // Convert number to string in reverse order
        while (num > 0) {
            buffer[index++] = (num % 10) + '0';
            num /= 10;
        }
    }

    // Reverse the string
    int start = 0;
    int end = index - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }

    // Null-terminate the string
    buffer[index] = '\0';

    // Send the string
    UART5_SendString(buffer);
}

void Timer0_Handler(void) {
    TIMER0_ICR_R = 0x01; // Clear the timer interrupt flag
}

void ADC0Seq3_Handler(void) {
    ADC0_ISC_R = 0x08;              // Clear the interrupt flag
    grid_voltage = ADC0_SSFIFO3_R;  // Read the conversion result

    // Determine the voltage status and control LEDs
    if (grid_voltage > OVER_VOLTAGE_THRESHOLD) {
        GPIO_PORTF_DATA_R |= 0x08;  // Green LED on
        GPIO_PORTF_DATA_R &= ~0x06; // Other LEDs off
        UART5_SendString("Overvoltage Detected: ");
    } else if (grid_voltage < UNDER_VOLTAGE_THRESHOLD) {
        GPIO_PORTF_DATA_R |= 0x02;  // Red LED on
        GPIO_PORTF_DATA_R &= ~0x0C; // Other LEDs off
        UART5_SendString("Undervoltage Detected: ");
    } else {
        GPIO_PORTF_DATA_R &= ~0x0A; // Turn off red and green LEDs
        GPIO_PORTF_DATA_R |= 0x04;  // Blue LED on for normal voltage
        UART5_SendString("Voltage Normal: ");
    }

    // Send the grid voltage value over UART
    UART5_SendNumber(grid_voltage);

    // Small delay (simulates processing time)
    for (j = 0; j < 200000; j++);
}

void main(void) {
    // Initialize clocks and peripherals
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOF | SYSCTL_RCGCGPIO_GPIOE;
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_TIMER0;
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_ADC0;

    // Allow time for the peripherals to be ready
    volatile int delay = SYSCTL_RCGCGPIO_R;

    // Configure GPIOF for LEDs
    GPIO_PORTF_DIR_R |= 0x0E;  // Pins 1, 2, 3 as outputs
    GPIO_PORTF_DEN_R |= 0x0E;  // Enable digital functionality

    // Configure GPIOE for ADC input (PE3)
    GPIO_PORTE_AFSEL_R |= 0x08; // Enable alternate function for PE3
    GPIO_PORTE_DEN_R &= ~0x08;  // Disable digital functionality for PE3
    GPIO_PORTE_AMSEL_R |= 0x08; // Enable analog functionality for PE3

    // Configure Timer0 for periodic mode
    TIMER0_CTL_R = 0;
    TIMER0_CFG_R = 0x00000000;
    TIMER0_TAMR_R = 0x02;
    TIMER0_TAILR_R = 160000;    // Load value for 10ms interval (16MHz clock)
    TIMER0_CTL_R |= 0x20;       // Enable ADC trigger on timeout
    TIMER0_CTL_R |= 0x01;       // Enable Timer0
    TIMER0_IMR_R |= 0x01;

    // Configure ADC0
    ADC0_ACTSS_R &= ~0x08;
    ADC0_EMUX_R = (ADC0_EMUX_R & 0xFFFF0FFF) | 0x5000;
    ADC0_SSMUX3_R = 0;
    ADC0_SSCTL3_R = 0x06;
    ADC0_IM_R |= 0x08;
    ADC0_ACTSS_R |= 0x08;

    // Enable ADC0 and Timer0 interrupts in NVIC
    NVIC_EN0_R |= 1 << 17; // ADC0SS3
    NVIC_EN0_R |= 1 << 19; // Timer0A

    // Initialize UART5
    UART5_Init();

    // Enable global interrupts
    __asm(" CPSIE I");

    while (1) {
        // Main loop
    }
}
