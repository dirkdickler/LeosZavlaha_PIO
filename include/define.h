#ifndef __DEFINE_H
#define __DEFINE_H

/* pro platformIO
[env:esp - wrover - kit] platform = espressif32
	board
	= esp - wrover - kit framework = arduino
		  monitor_speed
	= 115200

	  board_build.partitions
	= min_spiffs.csv
		  build_flags
	= -Os
	  - DBOARD_HAS_PSRAM
*/

#define firmware "ver20210725_1"

typedef struct
{
	String Nazov;
	float Tset;
}ROOM_t;


#define  pocetVentilu 7
#define WDT_TIMEOUT 5

const int Rele1 = 15;
const int Rele2 = 2;
const int Rele3 = 0;
const int Rele4 = 4;
const int Rele5 = 5;
const int Rele6 = 18;
const int Rele7 = 19;

const int LedStatus = 21;
const int RS485_DirPin = 16;
#define   RS485_RxMode  digitalWrite (RS485_DirPin, LOW);
#define   RS485_TxMode  digitalWrite (RS485_DirPin, HIGH);


//EEPROM adrese

#define EE_IPadresa 00				//16bytes
#define EE_SUBNET 16				//16bytes
#define EE_Brana 32					//16bytes
#define EE_NazovSiete 48			//16bytes
#define EE_Heslosiete 64			//20bytes
#define EE_citacZapisuDoEEPORM 84   //2bytes
#define EE_citac2_ZapisuDoEEPORM 86   //2bytes
#define EE_dalsi 88
#define EE_zacateVentilov 200



#define OffsetDalsihoVentilu 10
#define EE_Ventil1_HodinaON     EE_zacateVentilov + 0  //1byte
#define EE_Ventil1_MinutaON     EE_zacateVentilov + 1  //1byte
#define EE_Ventil1_CasZavlahy   EE_zacateVentilov + 2  //2byte
#define EE_Ventil1_Dnivtyzdni   EE_zacateVentilov + 4  //1byte
#define EE_Ventil1_dalsi        EE_zacateVentilov + 5  //xbyte

// #define EE_Ventil2_HodinaON     EE_zacateVentilov + OffsetDalsihoVentilu + 0   //1byte
// #define EE_Ventil2_MinutaON     EE_zacateVentilov + OffsetDalsihoVentilu + 1   //1byte
// #define EE_Ventil2_CasZavlahy   EE_zacateVentilov + OffsetDalsihoVentilu + 2   //2byte
// #define EE_Ventil2_dalsi        EE_zacateVentilov + OffsetDalsihoVentilu + 4   //2byte

#endif 