#include "stm32f030.h"

#define SYS_CLK 8000000

char hex[] = "0123456789ABCDEF";

char scans[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 'q', '1', 0, 0, 0, 'z', 's', 'a', 'w', '2', 0, 
    0, 'c', 'x', 'd', 'e', '4', '3', 0, 0, ' ', 'v', 'f', 't', 'r', '5', 0, 
    0, 'n', 'b', 'h', 'g', 'y', '6', 0, 0, 0, 'm', 'j', 'u', '7', '8', 0, 
    0, ',', 'k', 'i', 'o', '0', '9', 0, 0, '.', '/', 'l', ';', 'p', '-', 0, 
    0, 0, '\'', 0, '[', '=', 0, 0, 0, 0, '\r', ']', 0, '\\', 0, 0, 
    0, 0, 0, 0, 0, 0, '\b', 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

char scans2[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 'Q', '!', 0, 0, 0, 'Z', 'S', 'A', 'W', '@', 0, 
    0, 'C', 'X', 'D', 'E', '$', '#', 0, 0, ' ', 'V', 'F', 'T', 'R', '%', 0, 
    0, 'N', 'B', 'H', 'G', 'Y', '^', 0, 0, 0, 'M', 'J', 'U', '&', '*', 0, 
    0, '<', 'K', 'I', 'O', ')', '(', 0, 0, '>', '?', 'L', ':', 'P', '_', 0, 
    0, 0, '"', 0, '{', '+', 0, 0, 0, 0, '\r', '}', 0, '|', 0, 0, 
    0, 0, 0, 0, 0, 0, '\b', 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

unsigned char buf[16];
char bp;
char extKey, release, shift;
int cur;
char curCnt;

int intDiv(int a, int b) {
	int res = 0;
	int power = 1;
	while (a - b >= b) {
		b <<= 1;
		power <<= 1;
	}
	while (power > 0) {
		if (a - b >= 0) {
			a -= b;
			res += power;
		}
		b >>= 1;
		power >>= 1;
	}
	return res;
}

void uartEnable(int divisor) {
    REG_L(GPIOA_BASE, GPIO_MODER) &= ~(3 << (9 * 2));
    REG_L(GPIOA_BASE, GPIO_MODER) |= (2 << (9 * 2));
    REG_L(GPIOA_BASE, GPIO_AFRH) &= ~(0xF << ((9 - 8) * 4));
    REG_L(GPIOA_BASE, GPIO_AFRH) |= (1 << ((9 - 8) * 4));
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 14);
    REG_L(USART_BASE, USART_BRR) = divisor;
    REG_L(USART_BASE, USART_CR1) |= 1;
    REG_L(USART_BASE, USART_CR1) |= (1 << 3);
    
}

void send(int c) {
    REG_L(USART_BASE, USART_TDR) = c;
    while ((REG_L(USART_BASE, USART_ISR) & (1 << 6)) == 0);
}

void sends(char* s) {
    while (*s) {
        send(*(s++));
    }
}

void sendHex(int x, int d) {
    while (d-- > 0) {
        send(hex[(x >> (d * 4)) & 0xF]);
    }
}

void sendDec(int x) {
    static char s[10];
    int i, x1;
    i = 0;
    while (x > 0) {
        x1 = intDiv(x, 10);
        s[i++] = x - x1 * 10;
        x = x1;
    }
    if (i == 0) {
        s[i++] = 0;
    }
    while (i > 0) {
        send('0' + s[--i]);
    }
}

void irqEnable(unsigned char n) {
    REG_L(NVIC_BASE, NVIC_ISER + (n / 32) * 4) |= (1 << (n % 32));
}

void enableInterrupts(void) {
    __asm("cpsie i");
}

void setupExtInterrupt(void) {
    REG_L(RCC_BASE, RCC_AHB2ENR) |= (1 << 0); // syscfg en
    REG_L(SYSCFG_BASE, SYSCFG_EXTICR1) = (5 << 4);
    REG_L(EXTI_BASE, EXTI_IMR) |= (1 << 1);
    REG_L(EXTI_BASE, EXTI_FTSR) |= (1 << 1);
    irqEnable(5);
    enableInterrupts();
}

void exti01Handler(void) {
    unsigned char c;
    REG_L(EXTI_BASE, EXTI_PR) |= 0x3;
    if (REG_L(GPIOF_BASE, GPIO_IDR) & 1) {
        cur |= (1 << 10);
    }
    cur >>= 1;
    if (++curCnt >= 11) {
        curCnt = 0;
        c = (unsigned char) cur;
        if (c == 0xF0) {
            release = 1;
        } else if (c > 0xE0) {
            extKey = 1;
        } else {
            if ((c == 0x12 || c == 0x59) && !extKey) {
                shift = !release;
            } else if (c < 0x70) {
                if (!shift) {
                    c = scans[c];
                } else {
                    c = scans2[c];
                }
                if (c && !extKey && !release) {
                    buf[bp++] = c;
                }
            }
            release = 0;
            extKey = 0;
        }
    }
}

int main() {
    int n;
    REG_L(RCC_BASE, RCC_AHBENR) |= (1 << 17) | (1 << 22); //port A, F

    REG_L(GPIOA_BASE, GPIO_MODER) |= 1;
    
    
    uartEnable(SYS_CLK / 9600);
    
    setupExtInterrupt();
    
    curCnt = 0;
    bp = 0;
    extKey = 0;
    release = 0;
    shift = 0;
    while(1) {
        REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 0);
        n=25000; while(--n);
        REG_L(GPIOA_BASE, GPIO_BSRR) |= (1 << 16);
        n=100000; while(--n);
        if (bp > 0) {
            for (n = 0; n < bp; n++) {
                send(buf[n]);
                if (buf[n] == '\b') {
                    sends(" \b");
                } else if (buf[n] == '\r') {
                    send('\n');
                }
            }
            bp = 0;
        }
    }    
}

