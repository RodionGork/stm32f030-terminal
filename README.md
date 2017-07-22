# stm32f030-terminal

UART terminal with PS/2 keyboard and 2004A LCD

###Wiring

For TSSOP-20 package

- pin 16 (VDD) to +3.3
- pin 5  (VDDA) to +3.3
- pin 15 (GND) to GND
- pin 1  (BOOT0) to +3.3
- pin 4  (RESET) to GND, temporarily
- pin 17  (USART1_TX) to RX of the FTDI-cable
- pin 18  (USART1_RX) to TX of the FTDI-cable

- pin 6 (PA0) LED
- pin 2 (PF0) ps/2 - data
- pin 3 (PF1) ps/2 - clock

