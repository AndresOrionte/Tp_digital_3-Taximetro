/*
	 ===============================================================================
Name        : Taximetro.c
Author      : Grupo 1
Description : Un taximetro hecho para el integrador de Digitales 3
===============================================================================
*/

#include "LPC17xx.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_systick.h"


/*---------------------------------------------------Configs----------------------------------------------------------------------------------------*/
void confGPIO(void); // Prototipo de la funcion de conf. de puertos
void configADC(void);
void config_timer(void);
void confExtInt(void); //Prototipo de la funcion de conf. de interrupciones externas
void config_timer_1(void);
void confUART(void);
void confDMA(void);
/*---------------------------------------------------main----------------------------------------------------------------------------------------*/
void rutina_1(void); // poner un nombre mas representativo
void rutina_2(void);
void rutina_3(void);
void bucle(void);
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------Utilidades----------------------------------------------------------------------------------------*/
void actualizar_estado(void);
void actualizar_mensaje(void);
uint8_t obtener_teclaMatricial(void);
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------DEFINES----------------------------------------------------------------------------------------*/
#define De_mS_uS(num) (num*100000)
#define De_S_uS(num) (num*1000000)

#define OUTPUT 1
#define INPUT 0
#define ENTRADA_TECLADO 0XF
#define SALIDA_TECLADO (0XF << 4)

#define CANAL_0 0
#define LEDS_ROJO (1 << 4)
#define LEDS_VERDE (1 << 5)
#define BUZZER (1 << 6)

#define TIMEMUESTREO 1
#define F_MUESTREO 192307
#define CLKPWR_PCLKSEL_CCLK_DIV_8 3

#define EDGE_RISING 0
#define LONGITUD 16
#define LONGITUD_MENSAJE 10

#define VELOCIDAD_MAX 40
#define RESOLUCION 4096
/* Estados del taximetro*/
#define LIBRE 1
#define OCUPADO 2
#define PARADO  3

#define DMA_SIZE 7

#define BAJADA_DE_BANDERA 90
#define VALOR_FICHA 9
#define DISTANCIA_FICHA 200
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------Variables Globales-------------------------------------------------------------------------*/
GPDMA_LLI_Type DMA_LLI_Struct;

uint8_t static parpadeo=1;

uint8_t static LED_ON_OFF=1;
uint8_t static modo = LIBRE;
uint16_t static tarifa = 0;
uint16_t static distancia = 0;
uint8_t static car_state = 0;
volatile uint16_t ADC0Value = 0;

uint8_t const teclado_matricial[LONGITUD] = {	1,2,3,0xA, \
	4,5,6,0XB, \
		7,8,9,0xC, \
		0XE,0,0XE,0XD
};
uint8_t mensaje[LONGITUD_MENSAJE] = {"\rM   $0000"};

/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------Programa-------------------------------------------------------------------------*/
int main(void)
{
	//El nombre de la funcion es descriptivo para sus funcionalidades
	confGPIO();

	//Perifericos pedidos
	confExtInt();
	confUART();
	confDMA();
	configADC();

	config_timer();
	config_timer_1();
	bucle();

	return 0;
}
/*
 * A medida que el modo cambia por teclado, cambia el estado del taximetro
 */
void bucle(void)
{
	while(1)
	{
		switch (modo)
		{
			case LIBRE:
				rutina_1(); //Rutina de modo 1
				break;

			case OCUPADO:
				rutina_2(); //Rutina de modo 2
				break;

			case PARADO:
				rutina_3(); //Rutina de modo 3
				break;
		}

	}
}
/*
 * Cambio la luz, apago los timers y reinicio valores
 */
void rutina_1(void)
{
	actualizar_estado();

	TIM_Cmd(LPC_TIM0, DISABLE);
	TIM_Cmd(LPC_TIM1, DISABLE);

	tarifa = 0;
	distancia=0;

	actualizar_mensaje();
	while (modo == LIBRE){}

	tarifa=BAJADA_DE_BANDERA; //
	return;
}
/* 
 * Cambio las luces, prendo los timers y arranco a verificar cuando aumento la tarifa
 * mientras actualizo el mensaje enviado
 */
void rutina_2(void)
{
	actualizar_estado();

	TIM_ResetCounter(LPC_TIM0);
	TIM_Cmd(LPC_TIM0, ENABLE);

	actualizar_mensaje();
	while (modo == OCUPADO)
	{
		if (distancia > DISTANCIA_FICHA)
		{
			tarifa += VALOR_FICHA;
			distancia = 0;
			actualizar_mensaje();
		}
	}
	return;
}

/*
 * Apaga los timers pero no reinicia los valores para poder reanudarlos o 
 * terminalos
 */

void rutina_3(void)
{
	actualizar_estado();

	TIM_Cmd(LPC_TIM0, DISABLE);
	TIM_Cmd(LPC_TIM1, DISABLE);

	actualizar_mensaje();

//Systick para hacer parpadear el led de PARADO

	SYSTICK_Cmd(1);
	SYSTICK_IntCmd(1);
	SYSTICK_InternalInit(150);

	while (modo == PARADO){}

	SYSTICK_Cmd(DISABLE);
	SYSTICK_IntCmd(DISABLE);
	return;
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------Configs----------------------------------------------------------------------------------------*/

void confGPIO(void)
{
	uint8_t i;
	PINSEL_CFG_Type PinCfg;

	PinCfg.Funcnum = PINSEL_FUNC_0;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLDOWN;
	PinCfg.Portnum = PINSEL_PORT_2;

	/* entradas */

	for (i = 0; i< 4; i++)
	{
		PinCfg.Pinnum = i;
		PINSEL_ConfigPin(&PinCfg); //configura los pines p2.0 a p2.3 como gpio, con pull-down y modo normal
	}
	GPIO_SetDir(PINSEL_PORT_2, ENTRADA_TECLADO, INPUT); // configura los pines p2.0 a p2.3 como entradas

	/* salidas */

	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	for (i = 4; i< 8; i++)
	{
		PinCfg.Pinnum = i;
		PINSEL_ConfigPin(&PinCfg); //configura los pines p2.4 a p2.7 como gpio, con pull-up y modo normal
	}

	GPIO_SetDir(PINSEL_PORT_2, SALIDA_TECLADO, OUTPUT); // configura los pines p2.4 a p2.7 como salids

	/* LEDS */
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	PinCfg.Portnum = PINSEL_PORT_0;
	PinCfg.Pinnum = 4;

	PINSEL_ConfigPin(&PinCfg);

	PinCfg.Pinnum = 5;
	PINSEL_ConfigPin(&PinCfg);

	PinCfg.Pinnum = 6;
	PINSEL_ConfigPin(&PinCfg);

	GPIO_SetDir(PINSEL_PORT_0, (LEDS_ROJO | LEDS_VERDE | BUZZER), OUTPUT);
	FIO_ByteSetValue(2, 0, SALIDA_TECLADO);

	GPIO_ClearValue(PINSEL_PORT_0, (BUZZER));

	return;
}

void configADC(void)
{
	PINSEL_CFG_Type conf_pin;
	conf_pin.Funcnum = PINSEL_FUNC_1;            //PIN EN MODO AD0.0
	conf_pin.Portnum = PINSEL_PORT_0;            //PUERTO 0
	conf_pin.Pinnum = 23;                        //PIN 23
	conf_pin.Pinmode = PINSEL_PINMODE_TRISTATE;    //Ni pull-up ni down
	PINSEL_ConfigPin(&conf_pin);

	CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_ADC, CLKPWR_PCLKSEL_CCLK_DIV_8);
	///CONFIGURACION ADC///
	/*
	 * Nuestro CCLK es de 100MHz y configuramos el divisor de periferico a 8
	 * CCLK/divP = 100MHz/8 = 12,5MHz 
	 * A todo esto, al utilizar el modo controlado necesito 65 ciclos de reloj
	 * para establecer la muestra.
	 * 12,5MHz/65 = 195312 Hz, siendo la frecuencia de trabajo maxima configurable
	 */

	ADC_Init(LPC_ADC, F_MUESTREO); //ENCIENDO ADC
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0,ENABLE); //POR CANAL 0
	ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01);    //START CON MATCH, DEBE MUESTREAR CADA 1 SEG
	ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_RISING); //Arranca ṕr Mat0.1 en alto
	ADC_BurstCmd(LPC_ADC, DISABLE);//Modo controlado
	ADC_IntConfig(LPC_ADC, ADC_ADGINTEN, SET);//Activo las interrupciones

	ADC_GlobalGetStatus(LPC_ADC, 1); //Bajo la bandera
	NVIC_EnableIRQ(ADC_IRQn);
	void SysTick_Handler(void) {
		if(parpadeo){
			parpadeo=0;
			GPIO_ClearValue(PINSEL_PORT_0, (LEDS_ROJO));
		}
		else
		{
			parpadeo=1;
			GPIO_SetValue(PINSEL_PORT_0, (LEDS_ROJO));
		}
		SYSTICK_ClearCounterFlag();
		return;
	}
	return;
}

void config_timer(void)
{
	TIM_TIMERCFG_Type    struct_config;
	TIM_MATCHCFG_Type    struct_match;

	/*
	 * configuro el timer para que aumente cada 1 ms
	 */

	struct_config.PrescaleOption    =    TIM_PRESCALE_USVAL;
	struct_config.PrescaleValue     =    De_mS_uS(1);

	struct_match.MatchChannel       =    1;
	struct_match.IntOnMatch         =    DISABLE; //deshabilitamos las interrupciones por timer
	struct_match.ResetOnMatch       =    ENABLE;//resetea el contador del timer cuando se produce un match
	struct_match.StopOnMatch        =    DISABLE; //no detiene el contador del timer cuando se produce un match
	struct_match.ExtMatchOutputType =    TIM_EXTMATCH_TOGGLE;

	/*
	 * Si yo necesito muestrear cada 1 hz, la frecuencia de toogle debe ser del
	 * doble, en decir la mitad de tiempo, 5mS
	 */

	struct_match.MatchValue         =    5;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &struct_config); //se prende el timer0, se configura la division del clock del periferico, y se
	//configura el timer como modo temporizador y ademas se retesea el contador y se lo saca del reset
	TIM_ConfigMatch(LPC_TIM0, &struct_match); //carga todas las configuraciones del struct_match en ls registros correspondientes

	TIM_ResetCounter(LPC_TIM0);
	TIM_Cmd(LPC_TIM0, DISABLE); //habilita el contador del timer y prescaler

	return;
}

void config_timer_1(void)
{
	TIM_TIMERCFG_Type    struct_config;
	TIM_MATCHCFG_Type    struct_match;

	/*
	 * configuro el timer para que aumente cada 1 s
	 */

	struct_config.PrescaleOption    =    TIM_PRESCALE_USVAL;
	struct_config.PrescaleValue     =    De_S_uS(1);

	struct_match.MatchChannel       =    0;
	struct_match.IntOnMatch         =    ENABLE; //habilitamos las interrupciones por timer
	struct_match.ResetOnMatch       =    ENABLE;//resetea el contador del timer cuando se produce un match
	struct_match.StopOnMatch        =    DISABLE; //no detiene el contador del timer cuando se produce un match
	struct_match.ExtMatchOutputType =    TIM_EXTMATCH_NOTHING;
	struct_match.MatchValue         =    60; //Match cada 1 minuto

	TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &struct_config); //se prende el timer1, se configura la division del clock del periferico, y se
	//configura el timer como modo temporizador y ademas se retesea el contador y se lo saca del reset
	TIM_ConfigMatch(LPC_TIM1, &struct_match); //carga todas las configuraciones del struct_match en ls registros correspondientes

	TIM_ResetCounter(LPC_TIM1);
	TIM_Cmd(LPC_TIM1, DISABLE); //habilita el contador del timer y prescaler

	TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);

	NVIC_EnableIRQ(TIMER1_IRQn);

	return;
}

void confExtInt(void)
{
	/*
	 * Configuro para que los pines de entrada del teclado interrumpan
	 */
	GPIO_IntCmd(PINSEL_PORT_2, ENTRADA_TECLADO, EDGE_RISING);
	GPIO_ClearInt(PINSEL_PORT_2, ENTRADA_TECLADO);

	NVIC_EnableIRQ(EINT3_IRQn);

	return;
}

void confUART(void)
{
	PINSEL_CFG_Type PinCfg;
	//configuraci�n pin de Tx y Rx
	PinCfg.Funcnum = PINSEL_FUNC_1;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	PinCfg.Pinnum = PINSEL_PIN_2;
	PinCfg.Portnum = PINSEL_PORT_0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = PINSEL_PIN_3;
	PINSEL_ConfigPin(&PinCfg);

	UART_CFG_Type UARTConfigStruct;
	UART_FIFO_CFG_Type UARTFIFOConfigStruct;
	//configuraci�n por defecto:
	UART_ConfigStructInit(&UARTConfigStruct);
	//inicializa perif�rico
	UART_Init(LPC_UART0, &UARTConfigStruct);
	UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);
	//Inicializa FIFO
	UART_FIFOConfig(LPC_UART0, &UARTFIFOConfigStruct);
	//Habilita transmisi�n
	UART_TxCmd(LPC_UART0, ENABLE);
}
void confDMA(void)
{
	//Preparo la LLI del DMA
	DMA_LLI_Struct.SrcAddr= (uint32_t)mensaje;
	DMA_LLI_Struct.DstAddr= (uint32_t)&LPC_UART0->THR;
	DMA_LLI_Struct.NextLLI= (uint32_t)&DMA_LLI_Struct;
	DMA_LLI_Struct.Control= sizeof(mensaje)
		| 	(2<<12)
		| 	(1<<26) //source increment
		;

	// Desabilito la interrupcion de GDMA
	NVIC_DisableIRQ(DMA_IRQn);
	// Inicializo controlador de GPDMA
	GPDMA_Init();
	//Configuracion del canal 0
	GPDMA_Channel_CFG_Type GPDMACfg;
	// CANAL 0
	GPDMACfg.ChannelNum = CANAL_0;
	// Fuente de memoria
	GPDMACfg.SrcMemAddr = (uint32_t)mensaje;
	// Destino de memoria, al ser periferico es 0
	GPDMACfg.DstMemAddr = 0;
	// tamaño de trasnferencia
	GPDMACfg.TransferSize = sizeof(mensaje);
	// Ancho de trasnferencia
	GPDMACfg.TransferWidth = 0;
	// Tipo de memoria
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	// Coneccion de la Fuente, sin usar al ser periferico
	GPDMACfg.SrcConn = 0;
	// Coneccion de destino
	GPDMACfg.DstConn = GPDMA_CONN_UART0_Tx;
	// LLI
	GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;

	GPDMA_Setup(&GPDMACfg);

	// Habilito canal 0
	GPDMA_ChannelCmd(CANAL_0, ENABLE);
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------Utilidades----------------------------------------------------------------------------------------*/
/*
 * Segun el estado del modo prendemos un led u otro
 */
void actualizar_estado(void)
{
	if(LED_ON_OFF)
	{
		if (modo == LIBRE)
		{
			GPIO_SetValue(PINSEL_PORT_0, (LEDS_VERDE));
			GPIO_ClearValue(PINSEL_PORT_0, (LEDS_ROJO));
		}
		else if (modo == OCUPADO || modo ==  PARADO)
		{
			GPIO_SetValue(PINSEL_PORT_0, (LEDS_ROJO));
			GPIO_ClearValue(PINSEL_PORT_0, (LEDS_VERDE));
		}
	}
	else
	{
		FIO_ByteClearValue(0, 0, (0b11<<4));
	}
	return;
}
/*
 * Recibo el valor muestreado y obtengo la equivalencia a velocidad:
 * 3.3V -> 40 m/s
 *
 * Si la velocidad es 0, activamos el counter de vehiculo parado.
 */
uint16_t Convertir_Distancia(uint16_t adc_value)
{
	uint16_t velocidad = 0;
	velocidad = (adc_value * VELOCIDAD_MAX) / RESOLUCION;
	if (velocidad > 0)
	{
		car_state = 1;
		TIM_Cmd(LPC_TIM1, DISABLE);
	}
	else
	{
		if (car_state != PARADO)
		{
			TIM_Cmd(LPC_TIM1, ENABLE);
			car_state = PARADO;
		}
	}
	return velocidad;
}

/*
 * Funciona como una funcion pow, pero adaptado para que no use floats
 */

uint_fast16_t potencia(uint8_t numero, uint_fast8_t potencia)
{
	uint16_t resultado = numero;
	while (potencia > 1)
	{
		resultado = resultado * numero;
		potencia--;
	}
	if(potencia==0){
		resultado=1;
	}
	return resultado;
}

/*
 * Formatea el arreglo mensaje con los datos de las variables globales
 */

void actualizar_mensaje(void)
{
	switch (modo) {
		case LIBRE:
			mensaje[1]=(uint8_t)'L';
			break;
		case OCUPADO:
			mensaje[1]=(uint8_t)'O';
			break;
		case PARADO:
			mensaje[1]=(uint8_t)'P';
			break;
	}
	uint16_t temp=tarifa;
	uint8_t temp1=0;
	for(uint8_t i=4;i>0;i--)
	{
		temp1=temp/(potencia(10, i-1));

		mensaje[sizeof(mensaje)-i]=temp1 + '0';

		temp-=temp1*(potencia(10, i-1));
	}
}

uint8_t obtener_teclaMatricial(void)
{
	uint8_t fila=0,	columna=0;

	/*
	 * Voy pin a pin verificando cual es la fila apretada
	 */
	while (fila != 4)
	{
		if (GPIO_ReadValue(2) & (1 << fila))
			break;

		fila++;
	}
	if (fila >= 4)
		return 4; //Si no encontro devuelve una tecla sin uso

	/*
	 * Para obtener la columna barro un 0 por las columnas, cuando la fila
	 * cambia de estado es porque encontre la columna apretada
	 */
	while (columna != 4)
	{
		LPC_GPIO2->FIOPIN0 = ~(1 << (4+columna));

		if (!((FIO_ByteReadValue(2,0)) & ENTRADA_TECLADO))
			break;

		columna++;
	}
	if (columna >= 4)
		return 4;

	FIO_ByteSetValue(2, 0,SALIDA_TECLADO);

	/*
	* De 1			1|2|3|A
	* 				-------
	* 				4|5|6|B
	* 				-------
	* 				7|8|9|C
	*  				-------
	* 				*|0|#|D hasta 15.
	*/

	return (fila*4 + columna);
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------Handlers----------------------------------------------------------------------------------------*/

void EINT3_IRQHandler(void)
{
	GPIO_SetValue(PINSEL_PORT_0, (BUZZER));
	uint8_t tecla = obtener_teclaMatricial();
	for(uint64_t i=0; i<500000 ; i++){} //Pequeño delay para antirebote
	GPIO_ClearValue(PINSEL_PORT_0, (BUZZER));
	for(uint64_t i=0; i<500000 ; i++){}

	tecla=teclado_matricial[tecla]; //Del indice se obtiene el valor en el teclado

	//Si es modo valido, lo cambia, sino, lo deja pasar
	(tecla > 0 && tecla <= 3) ? (modo = tecla) : (modo = modo) ;
	/*
	 * Un modo luz apagada en el boton 4
	 */
	if(tecla==4)
	{
		if(LED_ON_OFF)
		{
			LED_ON_OFF=0;
		}
		else
		{
			LED_ON_OFF=1;
		}
		actualizar_estado();
	}

	GPIO_ClearInt(PINSEL_PORT_2, ENTRADA_TECLADO);
	return;
}
/*
 * Si interrumpe, aumenta el valor en una ficha
 */
void TIMER1_IRQHandler(void)
{
	tarifa += VALOR_FICHA;
	actualizar_mensaje();
	TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);

	return;
}

/* 
 * Simplemente obtengo el dato y lo convierto
 */

void ADC_IRQHandler(void)
{

	ADC0Value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_0);

	distancia += Convertir_Distancia(ADC0Value) * TIMEMUESTREO;

	ADC_GlobalGetStatus(LPC_ADC, 1);

	return;
}
/*
 * Si esta en modo PARADO, hace parpaderar el led
 */
void SysTick_Handler(void) {
	if(LED_ON_OFF){
		if(parpadeo<5)
		{
			parpadeo++;
			GPIO_ClearValue(PINSEL_PORT_0, (LEDS_ROJO));
		}
		else
		{
			parpadeo++;
			GPIO_SetValue(PINSEL_PORT_0, (LEDS_ROJO));
		}

		if(parpadeo==10)
			parpadeo=0;
	}

	SYSTICK_ClearCounterFlag();
	return;
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------*/


