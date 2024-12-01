#include <stdlib.h>
#include <stdio.h>
#include <pic18f57q43.h>
#include <xc.h>
#include <string.h>
#include "cabecera.h"
#include "DHT11.h"
#include "LIB_UART.h"
#include "LCD.h"

#define _XTAL_FREQ 4000000UL
#define BUFFER_SIZE 256

//Generamos caracteres para realizar el logo de nuestro proyecto
unsigned char logo1[] = {0x0, 0x0, 0x18, 0xC, 0x0, 0x1, 0x3, 0x2};
unsigned char logo2[] = {0x4, 0x4, 0x0, 0x0, 0x1F, 0x0, 0x0, 0x0};
unsigned char logo3[] = {0x0, 0x0, 0x6, 0xC, 0x0, 0x8, 0x18, 0x8};
unsigned char logo4[] = {0x2, 0x12, 0x1B, 0x9, 0xC, 0x6, 0x3, 0x1};
unsigned char logo5[] = {0x1, 0x6, 0xC, 0x19, 0x12, 0x1C, 0x9, 0x1F};
unsigned char logo6[] = {0x1B, 0xC, 0x14, 0x4, 0xC, 0x18, 0x10, 0x0};

short dht_ok; // Flag de verificacion del bit de paridad
float temperatura; // Almacena la temperatura
float humedad; // Almacena la humedad

//variables que nos permitirán almacenar los datos para mostrar en lcd
char buffer[20];
char TT[20];

//variables para los sensores de humedad de suelo
int valor = 0;
int sensor1 = 0;
int sensor2 = 0;

//funciones
void configuro();
void LCD_init();

int convierte_sensor_der(float humedad1);
int convierte_sensor_izq(float humedad2);
void captura_ADC();
void irrigacion_derecha();
void irrigacion_izquierda();
unsigned int readHCSR04(void);
unsigned int calculateDistance(unsigned int timeValue);


//UART CONFIGURACION SIM900
void enviarDataSensores(void);
void obtenerEstadoBoton(void);
void SIM900POSTconfig(void);
void HTTP_GET();
void procesar_respuesta();
void ejecutar_proceso_http();
void enviar_comando(const char* comando);

void __interrupt(irq(IRQ_INT0)) INT0_ISR(void);
void __interrupt(irq(IRQ_U1RX)) U1RX_ISR(void);

char buffer_sim[BUFFER_SIZE];
volatile int buffer_index = 0;
volatile int respuesta = 0;

void main(void) {
    configuro();
    LCD_init(); //Iniciamos nuestro LCD
    U1_INIT(BAUD_9600);
    
    //DESCOMENTAR LINEA DE CODIGO SI SE PRENDE POR PRIMERA VEZ EL SIM O SI SE LE FUE LA SEÑAL
    //SIM900POSTconfig();
    
    
    //Generamos nuestro logo mediante caracteres
    GENERACARACTER(logo1, 0);
    GENERACARACTER(logo2, 1);
    GENERACARACTER(logo3, 2);
    GENERACARACTER(logo4, 3);
    GENERACARACTER(logo5, 4);
    GENERACARACTER(logo6, 5);

    while (1) {

        //SIM900 HTTP POST
        enviarDataSensores();
        __delay_ms(5000); // Esperar 5 segundos antes de la próxima lectura y envío
        //SIM900 HTTP GET
        ejecutar_proceso_http();
        __delay_ms(2000);


        POS_CURSOR(1, 0);
        ESCRIBE_MENSAJE("TRABAJO FINAL", 13);
        __delay_ms(1500);

        BORRAR_LCD();

        POS_CURSOR(1, 0);
        ENVIA_CHAR(0x00);
        ENVIA_CHAR(0x01);
        ENVIA_CHAR(0x02);

        POS_CURSOR(2, 0);
        ENVIA_CHAR(0x03);
        ENVIA_CHAR(0x04);
        ENVIA_CHAR(0x05);

        POS_CURSOR(1, 3);
        ESCRIBE_MENSAJE("  SunLeaf", 9);
        POS_CURSOR(2, 3);
        ESCRIBE_MENSAJE("  Irrigation", 12);
        __delay_ms(2500);

        BORRAR_LCD();

        POS_CURSOR(1, 0);
        ESCRIBE_MENSAJE("Distancia:", 10);

        //Llamamos a la función del sensor ultrasónico para calcular distancia
        //a partir de los valores que toma nuestro sensor 
        LCD_ESCRIBE_VAR_INT(calculateDistance(readHCSR04()), 3);
        POS_CURSOR(1, 14);
        ESCRIBE_MENSAJE("cm", 2);
        __delay_ms(200); // Retardo de 100 ms entre mediciones

        //Utilizamos condiciones para saber qué tan lleno está el tanque
        if (calculateDistance(readHCSR04()) < 15) {
            __delay_ms(200);
            POS_CURSOR(2, 2);
            ESCRIBE_MENSAJE("TANQUE LLENO", 12);
        } else if (calculateDistance(readHCSR04()) < 40) {
            __delay_ms(200);
            POS_CURSOR(2, 2);
            ESCRIBE_MENSAJE("TANQUE MEDIO", 12);
        } else {
            __delay_ms(200);
            POS_CURSOR(2, 2);
            ESCRIBE_MENSAJE2("TANQUE BAJO ");
        }

        __delay_ms(2500);

        BORRAR_LCD();

        //Llamamos a la función que nos dará los valores de temperatura y
        //humedad utilizando nuestro librería
        dht_ok = DHT11_Read_Data(&temperatura, &humedad);
        if (dht_ok == 1) {
            sprintf(buffer, "H:%f%%", humedad);
            POS_CURSOR(2, 0);
            ESCRIBE_MENSAJE(buffer, 6);
            ENVIA_CHAR('%');
            sprintf(buffer, "T:%fC", temperatura);
            POS_CURSOR(2, 8);
            ESCRIBE_MENSAJE(buffer, 6);
            ENVIA_CHAR(0xDF); //Enviamos el carácter °
            ENVIA_CHAR('C');
        }
        __delay_ms(2500);

        BORRAR_LCD();

        //Ahora pasaremos a la conversión A/D para nuestros sensores
        //de humedad de suelo

        //Ubicamos nuestro puerto (RA1)
        ADPCH = 0x01;

        //Llamamos a nuestra función que tomará los datos
        captura_ADC();
        valor = ADRESH; //Utilizamos 8 bits y almacenamos en "valor"

        //Llamamos a la función para convertir nuestro valor en porcentaje de
        //humedad que presenta nuestro sensor
        sensor1 = convierte_sensor_der(valor);

        POS_CURSOR(1, 0);
        ESCRIBE_MENSAJE("Humedad 1: ", 11);
        sprintf(TT, "%d  ", sensor1);
        ESCRIBE_MENSAJE(TT, 3);
        ENVIA_CHAR('%');

        //Repetimos lo mismo para el segundo sensor        
        ADPCH = 0x02; //Cambiamos el puerto a RA2
        captura_ADC();
        valor = ADRESH;
        sensor2 = convierte_sensor_izq(valor);
        POS_CURSOR(2, 0);
        ESCRIBE_MENSAJE("Humedad 2: ", 11);
        sprintf(TT, "%d  ", sensor2);
        ESCRIBE_MENSAJE(TT, 3);
        ENVIA_CHAR('%');

        __delay_ms(2500);

        BORRAR_LCD();

        //a partir de aquí pondremos las condiciones
        if (calculateDistance(readHCSR04()) < 40) {
            if (sensor1 < 50 && sensor2 < 50) {
                POS_CURSOR(1, 0);
                ESCRIBE_MENSAJE("Poca humedad", 12);
                POS_CURSOR(2, 0);
                ESCRIBE_MENSAJE("Ambos sectores", 14);
                __delay_ms(2500);
                BORRAR_LCD();
                irrigacion_derecha();
                irrigacion_izquierda();

            } else if (sensor1 < 50) {
                POS_CURSOR(1, 0);
                ESCRIBE_MENSAJE("Poca humedad", 12);
                POS_CURSOR(2, 0);
                ESCRIBE_MENSAJE("Sector der.", 11);
                __delay_ms(2500);
                BORRAR_LCD();
                irrigacion_derecha();

            } else if (sensor2 < 50) {
                POS_CURSOR(1, 0);
                ESCRIBE_MENSAJE("Poca humedad", 12);
                POS_CURSOR(2, 0);
                ESCRIBE_MENSAJE("Sector izq.", 11);
                __delay_ms(2500);
                BORRAR_LCD();
                irrigacion_izquierda();

            } else {
                POS_CURSOR(1, 0);
                ESCRIBE_MENSAJE("Humedad", 7);
                POS_CURSOR(2, 0);
                ESCRIBE_MENSAJE("Adecuada", 8);
                __delay_ms(2500);
                BORRAR_LCD();

            }
        } else {
            POS_CURSOR(1, 0);
            ESCRIBE_MENSAJE("Falta agua", 10);
            POS_CURSOR(2, 0);
            ESCRIBE_MENSAJE("Llene el tanque", 15);
            __delay_ms(2500);


        }



        BORRAR_LCD();


    }
}

void configuro() {
    //Configuramos la frecuencia de nuestro PIC
    OSCCON1 = 0x60; //HFINTOSC seleccionado             
    OSCFRQ = 0x02; //Oscilador interno a 4MHz           
    OSCEN = 0x40; //HFINTOSC habilitado

    //Configuramos el puerto RD para el LCD

    TRISD = 0x00; //Puertos RD como salida
    ANSELD = 0x00; //Puertos RD como digital    


    //Configuración Interrupciones para UART
    OSCCON2 = 0x02;
    TRISAbits.TRISA0 = 0;
    ANSELAbits.ANSELA0 = 0;
    LATAbits.LATA0 = 0;
    TRISFbits.TRISF3 = 0;
    ANSELFbits.ANSELF3 = 0;
    LATFbits.LATF3 = 0;
    TRISBbits.TRISB0 = 1;
    ANSELBbits.ANSELB0 = 0;
    WPUBbits.WPUB0 = 1;
    TRISFbits.TRISF0 = 0;
    ANSELFbits.ANSELF0 = 0;
    TRISFbits.TRISF1 = 1;
    ANSELFbits.ANSELF1 = 0;
    PIE4bits.U1RXIE = 1;
    INTCON0bits.GIE = 1;
    INTCON0bits.INT0EDG = 0;
    PIE1bits.INT0IE = 1;


    //  Configuración de las bombas y los relés para electroválvulas

    //Señal parte derecha
    TRISFbits.TRISF6 = 0; //RF6 como salida
    LATFbits.LATF6 = 0; //RF6 empieza en 0
    ANSELFbits.ANSELF6 = 0; //RF6 como digital

    //Señal parte izquierda
    TRISFbits.TRISF7 = 0; //RF7 como salida
    LATFbits.LATF7 = 0; //RF7 empieza en 0
    ANSELFbits.ANSELF7 = 0; //RF7 como digital


    //  Configuración de los sensores de humedad de suelo
    TRISAbits.TRISA1 = 1; //RA1 como entrada
    ANSELAbits.ANSELA1 = 1; //RA1 como analógico

    TRISAbits.TRISA2 = 1; //RA2 como entrada
    ANSELAbits.ANSELA2 = 1; //RA2 como analógico

    ADCON0bits.ADFM = 0; //Just: izquierda
    ADCON0bits.CS = 1; //Fuente de reloj: ADCRC
    ADPCH = 0x01; //Puerto RA1
    ADPCH = 0x02; //Puerto RA2
    ADCLK = 0x00; //Divisor de reloj Fosc/2
    ADCON0bits.ADON = 1; // Habilitamos el ADC: ON
    ADCON2 = 0x62; //CRS=6, modo: AVG
    ADRPT = 64; //Repeticiones: para cumplir 2^CRS

    // Configuración de pines para HC-SR04
    TRISCbits.TRISC0 = 1; // RC0 como entrada (ECHO)
    TRISCbits.TRISC1 = 0; // RC1 como salida (TRIGGER)
    ANSELCbits.ANSELC0 = 0; // RC0 como digital
    ANSELCbits.ANSELC1 = 0; // RC1 como digital

    // Configuración de Timer1
    T1CLK = 0x01; // Fuente de reloj FOSC/4 para Timer1
    T1CON = 0x31; // Timer1 ON, prescaler 1:8, modo 16-bit


}

void LCD_init() {
    __delay_ms(29);
    LCD_CONFIG();
    BORRAR_LCD();
    CURSOR_HOME();
    CURSOR_ONOFF(OFF);
}

void captura_ADC() {
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO == 1);
}

int convierte_sensor_der(float humedad1) {
    int porcentaje = 0;
    porcentaje = (-1.4925 * humedad1 + 183.58);
    return porcentaje;
}

int convierte_sensor_izq(float humedad2) {
    int porcentaje = 0;
    porcentaje = (-1.3889 * humedad2 + 176.39);
    return porcentaje;
}

unsigned int readHCSR04(void) {
    unsigned int count;

    TMR1H = 0; // Reinicia Timer1
    TMR1L = 0;
    LATCbits.LATC1 = 1; // Activa el pulso de TRIGGER
    __delay_us(10); // Duración del pulso de 10 microsegundos
    LATCbits.LATC1 = 0; // Desactiva el pulso de TRIGGER

    while (PORTCbits.RC0 == 0); // Espera a que ECHO se ponga en alto
    T1CONbits.TMR1ON = 1; // Inicia Timer1
    while (PORTCbits.RC0 == 1); // Espera a que ECHO se ponga en bajo
    T1CONbits.TMR1ON = 0; // Detiene Timer1

    count = (TMR1H << 8) | TMR1L; // Calcula el valor del contador de Timer1
    return count; // Devuelve el tiempo medido
}

unsigned int calculateDistance(unsigned int timeValue) {
    float tiempo = (float) timeValue;
    unsigned int distancia;
    tiempo = tiempo / 5.8; // Convierte el tiempo a distancia (velocidad del sonido en cm/us)
    distancia = (unsigned int) tiempo; // Convierte a entero
    return distancia;
}

void irrigacion_derecha() {
    //mensaje
    POS_CURSOR(1, 0);
    ESCRIBE_MENSAJE("Activando", 9);
    POS_CURSOR(2, 0);
    ESCRIBE_MENSAJE("Riego der.", 10);
    //Se prende la bomba de agua y electrovalvula derecha
    LATFbits.LATF6 = 1;
    __delay_ms(5000);
    __delay_ms(5000);
    LATFbits.LATF6 = 0;

}

void irrigacion_izquierda() {
    //mensaje
    POS_CURSOR(1, 0);
    ESCRIBE_MENSAJE("Activando", 9);
    POS_CURSOR(2, 0);
    ESCRIBE_MENSAJE("Riego izq.", 10);
    //Se prende la bomba de agua y electrovalvula izquierda
    LATFbits.LATF7 = 1;
    __delay_ms(5000);
    __delay_ms(5000);
    LATFbits.LATF7 = 0;
}

//SIM900 FUNCIONES

void SIM900POSTconfig(void) {
    U1_STRING_SEND("AT\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+SAPBR=3,1,\"APN\",\"www\"\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+SAPBR=1,1\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+SAPBR=1,1\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+SAPBR=2,1\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+HTTPINIT\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+HTTPPARA=\"CID\",1\r\n");
    __delay_ms(3000);
}

void enviarDataSensores() {


    unsigned int distancia;
    char postData[200];

    // Lectura de datos del sensor DHT11
    if (!DHT11_Read_Data(&temperatura, &humedad)) {
        U1_STRING_SEND("Error al leer datos del sensor DHT11\r\n");
        return;
    }

    // Lectura y conversión del sensor de humedad de suelo derecho
    ADPCH = 0x01; // Seleccionar canal AN1 (RA1)
    captura_ADC();
    valor = ADRESH; // Leemos el valor de ADC
    sensor1 = convierte_sensor_der(valor);

    // Lectura y conversión del sensor de humedad de suelo izquierdo
    ADPCH = 0x02; // Seleccionar canal AN2 (RA2)
    captura_ADC();
    valor = ADRESH; // Leemos el valor de ADC
    sensor2 = convierte_sensor_izq(valor);

    // Lectura y cálculo de distancia del sensor ultrasónico
    distancia = calculateDistance(readHCSR04());

    // Formatear datos en JSON
    sprintf(postData, "{\"temperatura\": %.1f, \"humedad\": %.1f, \"humedad_izq\": %d, \"humedad_der\": %d, \"distancia\": %d}",
            temperatura, humedad, sensor1, sensor2, distancia);

    U1_STRING_SEND("AT+HTTPPARA=\"URL\",\"http://a524-38-25-22-102.ngrok-free.app/data\"\r\n");
    __delay_ms(3000);
    U1_STRING_SEND("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    __delay_ms(3000);

    // Establecer el tamaño de los datos a enviar
    char cmd[30];
    sprintf(cmd, "AT+HTTPDATA=%d,10000\r\n", strlen(postData));
    U1_STRING_SEND(cmd);
    __delay_ms(3000); // Esperar un momento antes de enviar los datos

    // Enviar los datos
    U1_STRING_SEND(postData);
    __delay_ms(5000);

    // Ejecutar la acción HTTP POST
    U1_STRING_SEND("AT+HTTPACTION=1\r\n");
    __delay_ms(5000);
    U1_STRING_SEND("AT+HTTPREAD\r\n");
    __delay_ms(3000);
}

void __interrupt(irq(IRQ_INT0)) INT0_ISR(void) {
    PIR1bits.INT0IF = 0;
}

void __interrupt(irq(IRQ_U1RX)) U1RX_ISR(void) {
    PIR4bits.U1RXIF = 0;
    char received = U1RXB; // Acceder al buffer de recepción
    if (buffer_index < BUFFER_SIZE - 1) {
        buffer_sim[buffer_index++] = received;

        if (received == '\n') {
            if (strstr(buffer_sim, "OK\r\n")) {// Asume que la respuesta termina con una nueva línea
                buffer_sim[buffer_index] = '\0'; // Termina el string
                respuesta = 1;
                buffer_index = 0; // Reinicia el índice del buffer para la próxima respuesta
            }
        }
    }
}

void enviar_comando(const char* comando) {
    U1_STRING_SEND(comando);
    __delay_ms(3000);
}

void HTTP_GET() {
    memset(buffer_sim, 0, BUFFER_SIZE); // Limpia el buffer antes de la solicitud
    buffer_index = 0; // Reinicia el índice del buffer
    respuesta = 0; // Resetea la bandera de respuesta recibida

    enviar_comando("ATE0\r\n");
    enviar_comando("AT+HTTPPARA=\"CID\",1\r\n");
    enviar_comando("AT+HTTPPARA=\"URL\",\"http://a524-38-25-22-102.ngrok-free.app/led\"\r\n");
    enviar_comando("AT+HTTPACTION=0\r\n");

    // Esperar la respuesta del comando AT+HTTPACTION=0
    while (!respuesta);
    respuesta = 0;

    enviar_comando("AT+HTTPREAD\r\n");
    while (!respuesta);
}

void procesar_respuesta() {
    // Depuración: Imprime la respuesta completa
    U1_STRING_SEND("Respuesta recibida: ");
    U1_STRING_SEND(buffer_sim);
    U1_NEWLINE();

    // Busca "Estado del sistema: 1" en la respuesta
    if (strstr(buffer_sim, "Estado del sistema: 1") != NULL) {
        LATAbits.LATA0 = 1;
    } else {
        LATAbits.LATA0 = 0; // Apaga el LED
    }
}

void ejecutar_proceso_http() {
    HTTP_GET();
    if (respuesta) {
        procesar_respuesta();
        respuesta = 0;
        memset(buffer_sim, 0, BUFFER_SIZE); // Limpia el buffer después de procesar
    }
}