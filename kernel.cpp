__asm("jmp kmain");     // Эта инструкция обязательно должна быть первой, т.к. этот код компилируется в бинарный, 
                        //и загрузчик передает управление по адресу первой инструкции бинарного образа ядра ОС.
#define VIDEO_BUF_PTR (0xb8000)
#define GDT_CS (0x8)    // Селектор секции кода, установленный загрузчиком ОС
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
#define PIC1_PORT (0x20)
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) 
#define VIDEO_HEIGHT (25) 


int line = 0;
int column = 0;

struct idt_entry    // Structure describe data about interrupt's handler 
{
    unsigned short base_lo; // Младшие биты адреса обработчика
    unsigned short segm_sel; // Селектор сегмента кода
    unsigned char always0; // Этот байт всегда 0
    unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
    unsigned short base_hi; // Старшие биты адреса обработчика
}   __attribute__((packed)); // Выравнивание запрещено

struct idt_ptr      // Struct, whose address is passed as an argument lidt command
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)); // alignment banned
struct idt_entry g_idt[256]; // Real table IDT
struct idt_ptr g_idtp; // Table descriptor for command lidt

char scancodes_table[] = {
    0, 
    0,  // ESC
    '1','2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    8,  // BACKSPACE
    '\t',  // TAB
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    ' ',// ENTER
    0,  // CTRL
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '<','>','+',
    0,  // left SHIFT
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,  // right SHIFT
    '*',// NUMPAD *
    0,  // ALT
    ' ', // SPACE
    0, //CAPSLOCK
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //F1 - F10
    0, //NUMLOCK
    0, //SCROLLOCK
    0, //HOME
    0, 
    0, //PAGE UP
    '-', //NUMPAD
    0, 0,
    0, //(r)
    '+', //NUMPAD
    0, //END
    0, 
    0, //PAGE DOWN
    0, //INS
    0, //DEL
    0, //SYS RQ
    0, 
    0, 0, //F11-F12
    0,
    0, 0, 0, //F13-F15
    0, 0, 0, 0, 0, 0, 0, 0, 0, //F16-F24
    0, 0, 0, 0, 0, 0, 0, 0,
};

// Functions for KERNEL
void out_str(int color, const char* ptr, unsigned int strnum);
void cursor_moveto(unsigned int strnum, unsigned int pos);
void keyb_process_keys(void);
void keyb_handler(void);
void keyb_init(void);
static inline void outw (unsigned short port, unsigned char data);
static inline void outb (unsigned short port, unsigned char data);
static inline unsigned char inb (unsigned short port);
static inline  unsigned int rdtsc(void); //for getting ticks 
void intr_disable(void);
void intr_enable(void);
void intr_start(void);
void intr_init(void);
void putchar(char c, int row, int col);
void on_key(unsigned int scan_code);
void command_handler(void);

//Functions for the VARIANT
void help(void);
void info(void);
void shutdown(void);
char* itoa(int i, char b[]);
bool strcmp(const char* str);
int bcd_to_decimal(unsigned char x);
char high_time(unsigned int time);   // for prtinting each number as single char
char low_time(unsigned int time);    // for prtinting each number as single char
unsigned int get_time(unsigned char mode);
void curtime(void);
unsigned char* get_old_time(char* address);
void loadtime(void);
void uptime(void);
void cls(void);
void ticks(void);


void default_intr_handler(void) // Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону
{
    asm("pusha");
// ... (реализация обработки)
    asm("popa; leave; iret");
}
typedef void (*intr_handler)();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
    unsigned int hndlr_addr = (unsigned int) hndlr;
    g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
    g_idt[num].segm_sel = segm_sel;
    g_idt[num].always0 = 0;
    g_idt[num].flags = flags;
    g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

void intr_init(void)    // Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
{
    int i;
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    for(i = 0; i < idt_count; i++)
        intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR,
    default_intr_handler);      // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start(void)
{
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    g_idtp.base = (unsigned int) (&g_idt[0]);
    g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
    asm("lidt %0" : : "m" (g_idtp) );
}

void intr_enable(void)
{
    asm("sti");
}

void intr_disable(void)
{
    asm("cli");
}

static inline unsigned char inb (unsigned short port) // Чтение из порта
{
    unsigned char data;
    asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outb (unsigned short port, unsigned char data) // Запись
{
    asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline void outw (unsigned short port, unsigned char data)
{
    asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void keyb_process_keys(void)
{
    // Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
    if (inb(0x64) & 0x01)
    {
        unsigned char scan_code;
        unsigned char state;
        scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
        if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
            on_key(scan_code); 
    }
}

void keyb_handler(void)
{
    asm("pusha");
    // Обработка поступивших данных
    keyb_process_keys();
    // Отправка контроллеру 8259 нотификации о том, что прерывание обработано
    outb(PIC1_PORT, 0x20);
    asm("popa; leave; iret");
}

void keyb_init(void)
{
    // Регистрация обработчика прерывания
    intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
    // Разрешение только прерываний клавиатуры от контроллера 8259
    outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура).
    // Разрешены будут только прерывания, чьи биты установлены в 0
}

void on_key(unsigned int scan_code)
{
    if(scan_code == 28)   //Enter
    {
        // executing command handler
        command_handler();
        out_str(0x07, "", line);
        out_str(0x07, "$", ++line);
        cursor_moveto(line, 1); // to put ahead "$"
        return;
    }
    if(scan_code == 14)    //Backspace
    {
        if(column <= 1)
            return;
        unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
        video_buf += (VIDEO_WIDTH * line + (column - 1)) * 2;
        video_buf[0] = '\0';
        cursor_moveto(line, column - 1);
        return;
    }
    if(scan_code > 127)
        return;
    if(scan_code == 16)   
    {
        putchar('q', line, column);
        return;
    }
    if(scan_code == 17)   
    {
        putchar('w', line, column);
        return;
    }
    if(scan_code == 18)   
    {
        putchar('e', line, column);
        return;
    }
    if(scan_code == 19)   
    {
        putchar('r', line, column);
        return;
    }
    if(scan_code == 20)   
    {
        putchar('t', line, column);
        return;
    }
    if(scan_code == 21)   
    {
        putchar('y', line, column);
        return;
    }
    if(scan_code == 22)   
    {
        putchar('u', line, column);
        return;
    }
    if(scan_code == 23)   
    {
        putchar('i', line, column);
        return;
    }
    putchar(scancodes_table[scan_code], line, column);
}

void putchar(char c, int row, int col)
{
    unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
    video_buf += (VIDEO_WIDTH*row + col)*2;
    video_buf[0] = (unsigned char) c;
    video_buf[1] = 0x07;
    cursor_moveto(line, ++column);
}

void command_handler(void)
{
    if (strcmp("loadtime"))
    {
        loadtime();
        cursor_moveto(++line, 2);
        return;
    }
    else if (strcmp("curtime"))
    {
        curtime();
        return;
    }
    else if (strcmp("ticks"))
    {
        ticks();
        return;
    }
    else if (strcmp("clear"))
    {
        cls();
        cursor_moveto(++line, 2);
        return;
    }
    else if (strcmp("help"))
    {
        help();
        return;
    }
    else if (strcmp("info"))
    {
        info();
        return;
    }
    else if (strcmp("uptime"))
    {
        uptime();
        return;
    }
    else if (strcmp("shutdown"))
    {
        shutdown();
        return;
    }
    out_str(0x07, "Enter correct command", line++);   
}

// Function moves cursor to a strnum (0 - topmost position) line in position pos on this line (leftmost position)
void cursor_moveto(unsigned int strnum, unsigned int pos)   
{
    if(strnum >= 24)
    {
        cls();

        strnum = 0;
        out_str(0x07, "$", strnum);
    }
    if(pos > 42)
    {
        unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
        video_buf += (VIDEO_WIDTH * line + column) * 2;
        video_buf[0] = '\0';
        pos--;
    }
    line = strnum;
    column = pos;
    unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
    outb(CURSOR_PORT, 0x0F);
    outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
    outb(CURSOR_PORT, 0x0E);
    outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
}

void out_str(int color, const char* ptr, unsigned int strnum)
{
    unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
    video_buf += VIDEO_WIDTH*2 * strnum;
    while (*ptr)
    {
        video_buf[0] = (unsigned char) *ptr; // Symbol (code)
        video_buf[1] = color; // Color of symbol and background
        video_buf += 2;
        ptr++;
        column++;
    }
}

void help(void)
{
    char* hello = "Hello, user!";
    char* warning1 = "In order to call any function enter the function name and press ENTER";
    char* warning2 = "Supported commands:";
    char* loadtime = "loadtime -- show time, when Bootloader started(Greenwich Mean Time)";
    char* curtime = "curtime -- show the current time(Greenwich Mean Time)";
    char* ticks = "ticks -- show the number of ticks since the OS start";
    char* clear = "clear -- clear the screen";
    char* help = "help -- show all supported commands";
    char* info = "info -- show information about author and development tools";
    char* uptime = "uptime -- show OS runtime";
    out_str(0x07, "", line++);
    out_str(0x07, hello, line++);
    out_str(0x07, warning1, line++);
    out_str(0x07, warning2, line++);
    out_str(0x07, loadtime, line++);
    out_str(0x07, curtime, line++);
    out_str(0x07, ticks, line++);
    out_str(0x07, clear, line++);
    out_str(0x07, help, line++);
    out_str(0x07, info, line++);
    out_str(0x07, uptime, line++);
}

void info(void)
{
    char* name = "Info OS: beta ver. 0.01. Developer: Aleksandrov Pavel, 23558/4, SPBPU, 2018";
    char* translator = "Bootloader Translator: GNU assembler, Syntax: Intel";
    char* compiler = "Kernel compiler: gcc";
    out_str(0x07, "", line++);
    out_str(0x07, name, line++);
    out_str(0x07, translator, line++);
    out_str(0x07, compiler, line++);
}

void shutdown(void) // hex-codes from wiki.osdev.org
{
    outb(0xf4, 0x00);
}

bool strcmp(const char* str)
{
    unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
    video_buf += (VIDEO_WIDTH * line + 1) * 2;
    for(int i = 0; str[i] != '\0'; i++)
    {
        if(str[i] != *(video_buf + i * 2))
            return 0;
    }
    return 1;
}

char* itoa(int i, char b[])
{
    char const digit[] = "0123456789";
    char* p = b;
    int shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

int bcd_to_decimal(unsigned char x) 
{
    return x - 6 * (x >> 4);
}

char high_time(unsigned int time)
{
    unsigned int high = time / 10;
    char c_high  = high + '0';
    return c_high;
}

char low_time(unsigned int time)
{
    unsigned int low = time % 10;
    char c_low  = low + '0';
    
    return c_low;
}

unsigned int get_time(unsigned char mode)
{
    outb(0x70, mode);
    unsigned int answer = bcd_to_decimal(inb(0x71));
    return answer;
}

void curtime(void)
{
    char* now_time = "Current time:";
    line++;
    out_str(0x07, "", line++);
    out_str(0x07, now_time, line);
    putchar(high_time(get_time(4)), line, column);
    putchar(low_time(get_time(4)), line, column);
    putchar(':', line, column);
    putchar(high_time(get_time(2)), line, column);
    putchar(low_time(get_time(2)), line, column);
    putchar(':', line, column);
    putchar(high_time(get_time(0)), line, column);
    putchar(low_time(get_time(0)), line, column);
}

unsigned int get_old_time(unsigned char* address)
{
    unsigned char *memory = (unsigned char*) address;
    unsigned int answer = bcd_to_decimal(memory[0]);
    return answer;
}

void loadtime(void)
{
    char* now_time = "Load time:";
    unsigned int time = get_old_time((unsigned char*)(0x00000500));
    line++;
    out_str(0x07, "", line++);
    out_str(0x07, now_time, line);
    putchar(high_time(time), line, column);
    putchar(low_time(time), line, column);
    putchar(':', line, column);
    time = get_old_time((unsigned char*)(0x00000520));
    putchar(high_time(time), line, column);
    putchar(low_time(time), line, column);
    putchar(':', line, column);
    time = get_old_time((unsigned char*)(0x00000540));
    putchar(high_time(time), line, column);
    putchar(low_time(time), line, column);
}

void uptime(void)
{
    char* uptime = "Uptime time:";
    unsigned int time = get_old_time((unsigned char*)(0x00000500));
    line++;
    out_str(0x07, "", line++);
    out_str(0x07, uptime, line);
    putchar(high_time(get_time(4) - time), line, column);
    putchar(low_time(get_time(4) - time), line, column);
    putchar(':', line, column);
    time = get_old_time((unsigned char*)(0x00000520));
    putchar(high_time(get_time(2) - time), line, column);
    putchar(low_time(get_time(2) - time), line, column);
    putchar(':', line, column);
    time = get_old_time((unsigned char*)(0x00000540));
    putchar(high_time(get_time(0) - time), line, column);
    putchar(low_time(get_time(0) - time), line, column);
    line++;
}

void cls(void) // clear the screen 
{
    unsigned char *video_buf = (unsigned char*) VIDEO_BUF_PTR;
    for (int i = 0; i < VIDEO_WIDTH*VIDEO_HEIGHT; i++) 
    {
        *(video_buf + i*2) = '\0';   // each element takes 2 bytes (1 - for ASCII code byte, 2 -  for attribute byte - color)
        //*(video_buf + i*2 + 1) = 0x00;  // change background color
    }
    
}

static inline  unsigned int rdtsc(void)
{
   unsigned int result;
   asm volatile ( "rdtsc" : "=A"(result) ); 
   return result;
}

void ticks(void)
{
    char* result = itoa(rdtsc(), result);
    out_str(0x07, "", line++);
    out_str(0x07, "Number of ticks since start(appxromaximately)", line ++);
    out_str(0x07, result, line++);
}

extern "C" int kmain()
{
    const char* hello = "Welcome to InfoOS (gcc edition)!";
    cls();
    out_str(0x07, hello, line++);
    cursor_moveto(1, 0);
    out_str(0x07, "$", line++);
    cursor_moveto(1, 1);
    intr_disable();
    intr_init();
    keyb_init();
    intr_start();
    intr_enable();
    while(1)
    {
        asm("hlt");
    }
    return 0;
}
