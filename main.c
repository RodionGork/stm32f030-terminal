#include "stm32f030.h"

#define SYS_CLK 8000000
#define KBD_BUF_SIZE 16
#define DISP_H 4
#define DISP_W 20

#define LCD_RS_LINE 2
#define LCD_E_LINE 3
#define LCD_DAT_LINE 4

char hex[] = "0123456789ABCDEF";

unsigned char disp[DISP_W * DISP_H];

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

unsigned char buf[KBD_BUF_SIZE];
char bhead, btail;
char cx, cy;
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
    REG_L(SYSCFG_BASE, SYSCFG_EXTICR1) = (5 << 0);
    REG_L(EXTI_BASE, EXTI_IMR) |= (1 << 0);
    REG_L(EXTI_BASE, EXTI_FTSR) |= (1 << 0);
    irqEnable(5);
    enableInterrupts();
}

void exti01Handler(void) {
    unsigned char c;
    REG_L(EXTI_BASE, EXTI_PR) |= 0x3;
    if (REG_L(GPIOF_BASE, GPIO_IDR) & 2) {
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
                    buf[bhead] = c;
                    c = (bhead + 1) % KBD_BUF_SIZE;
                    if (c != btail) {
                        bhead = c;
                    }
                }
            }
            release = 0;
            extKey = 0;
        }
    }
}

void setupPorts() {
    char i;
    REG_L(RCC_BASE, RCC_AHBENR) |= (1 << 17) | (1 << 22); //port A, F

    REG_L(GPIOA_BASE, GPIO_MODER) = (1 << (14 * 2));
    
    REG_L(GPIOA_BASE, GPIO_MODER) |= (1 << (LCD_RS_LINE * 2));
    REG_L(GPIOA_BASE, GPIO_MODER) |= (1 << (LCD_E_LINE * 2));
    for (i = 0; i < 4; i++) {
        REG_L(GPIOA_BASE, GPIO_MODER) |= (1 << ((LCD_DAT_LINE + i) * 2));
    }
}

void lcdE(char state) {
    REG_L(GPIOA_BASE, GPIO_BSRR) |= 1 << (LCD_E_LINE + (state ? 0 : 16));
}

void lcdRS(char state) {
    REG_L(GPIOA_BASE, GPIO_BSRR) |= 1 << (LCD_RS_LINE + (state ? 0 : 16));
}

void lcdDelay(int delay) {
    delay *= 100;
    while (delay-- > 0) {
        __asm("nop");
    }
}

void lcdHalf(char half) {
    char i;
    lcdDelay(1);
    lcdE(1);
    for (i = 0; i < 4; i++) {
        REG_L(GPIOA_BASE, GPIO_BSRR) |= 1 << (LCD_DAT_LINE + i + ((half & 1) ? 0 : 16));
        half >>= 1;
    }
    lcdDelay(1);
    lcdE(0);
    lcdDelay(1);
}

void lcdWrite(char rs, unsigned char value) {
    lcdRS(rs);
    lcdHalf(value >> 4);
    lcdHalf(value & 0xF);
}

void lcdInit(void) {
    int i;
    lcdE(0);
    lcdRS(0);
    lcdDelay(300);
    for (int i = 0; i < 3; i++) {
        lcdHalf(3);
        lcdDelay(60);
    }
    lcdHalf(2);
    lcdWrite(0, 0x28);
    lcdWrite(0, 0x08);
    lcdWrite(0, 0x01);
    lcdDelay(20);
    lcdWrite(0, 0x06);
    lcdWrite(0, 0x0F);
    cx = 0;
    cy = 0;
    for (i = 0; i < sizeof(disp); i++) {
        disp[i] = ' ';
    }
}

void adjustCursor(unsigned char col, unsigned char row) {
    cx = col;
    cy = row;
    lcdWrite(0, 0x80 + col + (row & 1) * 0x40 + (row & 2) * 10);
}

void scrollLine(void) {
    int i;
    for (i = 0; i < DISP_W * (DISP_H - 1); i++) {
        disp[i] = disp[i + DISP_W];
    }
    for (; i < DISP_W * DISP_H; i++) {
        disp[i] = ' ';
    }
    for (cy = 0; cy < DISP_H; cy++) {
        adjustCursor(0, cy);
        for (cx = 0; cx < DISP_W; cx++) {
            lcdWrite(1, disp[cy * DISP_W + cx]);
        }
    }
    adjustCursor(0, DISP_H - 1);
}

void newLine(void) {
    if (cy >= DISP_H - 1) {
        scrollLine();
    } else {
        adjustCursor(0, cy + 1);
    }
}

void print(unsigned char c) {
    if (c >= ' ') {
        disp[cy * DISP_W + cx] = c;
        lcdWrite(1, c);
        cx += 1;
        if (cx >= DISP_W) {
            newLine();
        }
    }
    if (c == '\b' && cx > 0) {
        adjustCursor(cx - 1, cy);
        lcdWrite(1, ' ');
        adjustCursor(cx, cy);
    } else if (c == '\r') {
        newLine();
    }
}

int main(void) {
    lcdDelay(1000);
    setupPorts();
    
    uartEnable(SYS_CLK / 9600);
    
    setupExtInterrupt();
    
    curCnt = 0;
    bhead = 0;
    btail = 0;
    extKey = 0;
    release = 0;
    shift = 0;
    lcdDelay(1000);
    lcdInit();
    while(1) {
        if (btail != bhead) {
            send(buf[btail]);
            print(buf[btail]);
            btail = (btail + 1) % KBD_BUF_SIZE;
        }
    }    
}

