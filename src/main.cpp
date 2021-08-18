//xx123

#include <AsyncElegantOTA.h> //https://randomnerdtutorials.com/esp32-ota-over-the-air-arduino/#1-basic-elegantota
#include <elegantWebpage.h>

// Import required librariesrs 
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <esp_task_wdt.h>
#include "time.h"
#include <Ticker.h>
#include <EEPROM.h>
//#include "index.h"
#include "index.html"
#include "main.h"
#include "define.h"
#include "HelpFunction.h"
#include <ESP32Time.h>
//#include <SoftwareSerial.h>
	
// Replace with your network credentials
//const char* ssid = "Grabcovi";
//const char* password = "40177298";
const char *soft_ap_ssid = "aDum_Zavlaha_Leos";
const char *soft_ap_password = "sedykocur";
//const char *ssid = "semiart";
//const char *password = "aabbccddff";
//const char *ssid = "Tenda 2.4GHz";
//const char *password = "121213765";
char NazovSiete[30];
char Heslo[30];
 
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

JSONVar myObject, VentilJsn, ObjDatumCas, JSON_DebugMsg;
Ticker timer_10ms(Loop_10ms, 10, 0, MILLIS);
Ticker timer_100ms(Loop_100ms, 300, 0, MILLIS);
Ticker timer_1sek(Loop_1sek, 1000, 0, MILLIS);
Ticker timer_10sek(Loop_10sek, 10000, 0, MILLIS);

ESP32Time rtc;
//SoftwareSerial SoftSerial(32, 33);

IPAddress local_IP(192, 168, 1, 14);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);	//optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600; //letny cas
struct tm MyRTC_cas;
bool Internet_CasDostupny = false; //to je ze dostava cas z Inernetu
bool RTC_cas_OK = false;		   //ze mam RTC fakt nastaveny bud z interneru, alebo nastaveny manualne
								   //a to teda ze v RTC mam fakr realny cas
								   //Tento FLAG, nastavi len pri nacitanie casu z internutu, alebo do buducna manualne nastavenie casu cew WEB

u16_t cnt = 0;
static u8 AktualnenastavovanyVentil = 0;
static bool SystemZapnuty = false;

char gloBuff[200];

VENIL_t ventil[pocetVentilu];

bool LogEnebleWebPage = false;

void notifyClients()
{
	//ws.textAll(String(ledState));
}

//ked Webstrenaky - jejich ws posle nejake data napriklad "VratMiCas" tj ze strnaky chcu RTC aby ich napriklad zobrazili

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
	AwsFrameInfo *info = (AwsFrameInfo *)arg;
	if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
	{
		data[len] = 0;

		//Serial.printf ("WS stranky poslali toto:%s", data);

		JSONVar obj = JSON.parse((char *)data);
		//Serial.print ("JSON.typeof(obj) = ");
		//Serial.println (JSON.typeof(obj));   // prints: object
		if (obj.hasOwnProperty("IDobjektu"))
		{
			//Serial.print ("Mam ID objektu"); Serial.println (obj["IDobjektu"]);

			if (obj["IDobjektu"] == (String) "VentilData1") //"VentilData1")
			{
				Serial.print("Parada mam IDobjektu == VentilData1");

				if (obj.hasOwnProperty("CasOn"))
				{
					Serial.print("myObject[CasOn] = ");

					Serial.println(obj["CasOn"]);
				}

				if (obj.hasOwnProperty("CasOnTime"))
				{
					Serial.print("myObject[CasOnTime] = ");

					Serial.println(obj["CasOnTime"]);
				}
			}
		}

		if (strcmp((char *)data, "VratMiCas") == 0)
		{

			Serial.println("WS page pozaduje cas...");
			OdosliCasDoWS();

			//notifyClients();
		}
		else if (strcmp((char *)data, "VratVentilData") == 0)
		{
			Serial.println("stranky poslali: VratVentilData ");

			OdosliStrankeVentilData();
		}

		else if (strcmp((char *)data, "VratVsetkyVentilData") == 0)
		{
			VentilJsn["SystemONOFF"] = SystemZapnuty; // toto tu je len pri prvom naciteni WEB stranky aby zobrazil stav systemu ci je ON or OFF
			String jsonString = JSON.stringify(VentilJsn);
			ws.textAll(jsonString);
		}

		else if (strcmp((char *)data, "ToglujVentil") == 0)
		{
			String rr;
			DebugMsgToWebSocket("[HndlWebSocket] Dosel pozadavek (ToglujVentil)\r\n");

			if (SystemZapnuty == true)
			{
				Serial.println("stranky poslali: ze chcu toglovat ventil");
				if (ventil[AktualnenastavovanyVentil].Rele == 0)
				{
					ventil[AktualnenastavovanyVentil].CasZavlahyRest = 10;
					digitalWrite(ventil[AktualnenastavovanyVentil].pin, HIGH);
					ventil[AktualnenastavovanyVentil].Rele = 1;
				}
				else
				{
					ventil[AktualnenastavovanyVentil].CasZavlahyRest = 0;
					digitalWrite(ventil[AktualnenastavovanyVentil].pin, LOW);
					ventil[AktualnenastavovanyVentil].Rele = 0;
				}
				rr = "CasZavlahyRest";
				rr += AktualnenastavovanyVentil;
				VentilJsn[rr] = ventil[AktualnenastavovanyVentil].CasZavlahyRest;
				OdosliStrankeVentilData();
				rr = "[HndlWebSocket] System je ON preto TOGLUJEM ventil: " + (String)AktualnenastavovanyVentil + "\r\n ";
			}
			else
			{
				rr = "[HndlWebSocket] System je OFF preto !!! NEnahodim  !!!ventil: " + (String)AktualnenastavovanyVentil + "\r\n ";
			}
			DebugMsgToWebSocket(rr);
		}

		else if (strcmp((char *)data, "ZapniVentil") == 0)
		{
			String rr;
			DebugMsgToWebSocket("[HndlWebSocket] Dosel pozadavek (ZapniVentil)\r\n");

			if (SystemZapnuty == true)
			{
				Serial.println("stranky poslali: ze chcu zapnut ventil");
				ventil[AktualnenastavovanyVentil].CasZavlahyRest = 10;
				digitalWrite(ventil[AktualnenastavovanyVentil].pin, HIGH);
				ventil[AktualnenastavovanyVentil].Rele = 1;

				rr = "CasZavlahyRest";
				rr += AktualnenastavovanyVentil;
				VentilJsn[rr] = ventil[AktualnenastavovanyVentil].CasZavlahyRest;

				rr = "[HndlWebSocket] System je ON preto nahodim ventil: " + (String)AktualnenastavovanyVentil + "\r\n ";
			}
			else
			{
				rr = "[HndlWebSocket] System je OFF preto !!! NEnahodim  !!!ventil: " + (String)AktualnenastavovanyVentil + "\r\n ";
			}
			DebugMsgToWebSocket(rr);
		}

		else if (strcmp((char *)data, "VypniVentil") == 0)
		{
			Serial.println("stranky poslali: Vypnut ventil  ");
			ventil[AktualnenastavovanyVentil].CasZavlahyRest = 0;
			digitalWrite(ventil[AktualnenastavovanyVentil].pin, LOW);
			ventil[AktualnenastavovanyVentil].Rele = 0;
			String dsd = "CasZavlahyRest";
			dsd += AktualnenastavovanyVentil;
			VentilJsn[dsd] = ventil[AktualnenastavovanyVentil].CasZavlahyRest;
		}
		else if (strcmp((char *)data, "OnOFF_Zavlahu") == 0)
		{
			String bla = "[HndlWebSocket] stranky poslali: ON OFF zavlahu komplet\r\n";
			Serial.println(bla);
			DebugMsgToWebSocket(bla);
			bla = "[HndlWebSocket] ";
			if (SystemZapnuty == false)
			{
				SystemZapnuty = true;
				bla += "Zapinam zavlahu komplet\r\n";
			}
			else
			{
				SystemZapnuty = false;
				bla += "Vypinam zavlahu komplet\r\n";
			}
			Serial.println(bla);
			DebugMsgToWebSocket(bla);
			OdosliStrankeVentilData();
		}
	}
}

void onEvent(AsyncWebSocket *server,
			 AsyncWebSocketClient *client,
			 AwsEventType type,
			 void *arg,
			 uint8_t *data,
			 size_t len)
{
	switch (type)
	{
	case WS_EVT_CONNECT:
		Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
		break;
	case WS_EVT_DISCONNECT:
		Serial.printf("WebSocket client #%u disconnected\n", client->id());
		break;
	case WS_EVT_DATA:
		handleWebSocketMessage(arg, data, len);
		break;
	case WS_EVT_PONG:
	case WS_EVT_ERROR:
		break;
	}
}

void initWebSocket()
{
	ws.onEvent(onEvent);
	server.addHandler(&ws);
}

String processor(const String &var)
{
	Serial.println(var);
	if (var == "STATE")
	{
		/*if (ledState) {
			return "ON";
		}
		else {
			return "OFF";
		}*/
	}

	return "--";
}

/**********************************************************
 ***************        SETUP         **************
 **********************************************************/

void setup()
{
	Serial.begin(115200);
	Serial.println("Spuusm 21.");

	SystemInit();
	rtc.setTime(30, 24, 15, 17, 1, 2021); // 17th Jan 2021 15:24:30

	ESPinfo();

	NacitajEEPROM_setting();

	WiFi.mode(WIFI_MODE_APSTA);
	Serial.println("Creating Accesspoint- pristupovy bod");
	WiFi.softAP(soft_ap_ssid, soft_ap_password, 7, 0, 3);
	Serial.print("IP address:\t");
	Serial.println(WiFi.softAPIP());

	if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
	{
		Serial.println("STA Failed to configure");
	}

	WiFi.begin(NazovSiete, Heslo);
	u8_t aa = 0;
	while (WiFi.waitForConnectResult() != WL_CONNECTED && aa < 2)
	{
		Serial.print(".");
		aa++;
	}
	// Print ESP Local IP Address
	Serial.println(WiFi.localIP());

	initWebSocket();

	FuncServer_On();

	//https://randomnerdtutorials.com/esp32-ota-over-the-air-vs-code/
	AsyncElegantOTA.begin(&server, "admin", "sedykocur"); // Start ElegantOTA

	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	server.begin();

	timer_10ms.start();
	timer_100ms.start();
	timer_1sek.start();
	timer_10sek.start();
	esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
	esp_task_wdt_add(NULL);				  //add current thread to WDT watch

	//RS485 musis spustit az tu, lebo ak ju das hore a ESP ceka na konnect wifi, a pridu nejake data na RS485, tak FreeRTOS =RESET  asi overflow;
	Serial2.begin(9600);
}

void loop()
{
	esp_task_wdt_reset();
	ws.cleanupClients();
	//AsyncElegantOTA.loop();
	timer_10ms.update();
	timer_100ms.update();
	timer_1sek.update();
	timer_10sek.update();
}

void Loop_10ms()
{
	static uint8_t TimeOut_RXdata = 0;	 //musi byt static lebo sem skaces z Loop
	static uint16_t KolkkoNplnenych = 0; //musi byt static lebo sem skaces z Loop
	static char budd[100];				 //musi byt static lebo sem skaces z Loop

	uint16_t aktualny;

	aktualny = Serial2.available();
	if (aktualny)
	{

		//Serial2.readBytes (temp, aktualny);
		for (uint16_t i = 0; i < aktualny; i++)
		{
			budd[KolkkoNplnenych + i] = Serial2.read();
		}
		KolkkoNplnenych += aktualny;
		TimeOut_RXdata = 5;
	}

	if (TimeOut_RXdata)
	{
		if (--TimeOut_RXdata == 0)
		{
			{
				Serial.printf("[Serial 1] doslo:%u a to %s\n", KolkkoNplnenych, budd);

				RS485_PACKET_t *loc_paket;
				loc_paket = (RS485_PACKET_t *)budd;

				Serial.printf("[Serial 1]  DST adresa je:%u \n", loc_paket->DSTadress);

				if (loc_paket->SCRadress == 10)
				{
					Serial.printf("[Serial 1] Mam adresu 10 a idem ulozit data z RS485");

					OdosliStrankeVentilData();
				}

				memset(budd, 0, sizeof(budd));
				KolkkoNplnenych = 0;
			}
		}
	}
}

void Loop_100ms(void)
{
	static bool LedNahodena = false;

	if (LedNahodena == false)
	{
		LedNahodena = true;
		//digitalWrite(LedGreen, LOW);
		digitalWrite(LedStatus, LOW);
	}
	else
	{
		LedNahodena = false;
		//digitalWrite(LedGreen, HIGH);
		digitalWrite(LedStatus, HIGH);

		//RS485_TxMode;
		//Serial1.println("test RS485..orange LED High");
	}
}

void Loop_1sek(void)
{
	Serial.print("[1sek Loop]  mam 1 sek....  ");
	//Serial.println( random(10000,20000));
	if (Internet_CasDostupny == false)
	{
		Serial.print("Internet cas nedostupny !!,  ");
	}
	else
	{
		Serial.print("Internet cas dostupny,  ");
	}

	Serial.print("RTC cas cez func rtc.getTime: ");
	Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
	MyRTC_cas = rtc.getTimeStruct();
	//Serial.print("[1sek Loop]  free Heap je:");
	// Serial.println(ESP.getFreeHeap());

	float flt = (float)ESP.getFreeHeap();
	flt /= 1000.0f;
	char locBuf[50];
	sprintf(locBuf, "%.3f", flt);
	String rr = "[1sek Loop] signalu: " + (String)WiFi.RSSI() + "dBm  a Heap: " + locBuf + " kB " +
				" IN:1..8 Rele:K1..K7 " + NacitajReleK1_K7() + "\r\n ";
	DebugMsgToWebSocket(rr);

	int loc_hodiny, loc_minuty;
	loc_minuty = rtc.getMinute();
	loc_hodiny = rtc.getHour(true);
	bool loc_update_VentilJSON = false;

	for (u8 i = 0; i < pocetVentilu; i++)
	{
		if (ventil[i].HodinyOn == loc_hodiny &&
			ventil[i].MinutyOn == loc_minuty &&
			ventil[i].CasZavlahy > 0 &&
			ventil[i].CasZavlahyRest == 0 &&
			RTC_cas_OK == true)
		{
			if (ventil[i].CasZavlahyRest == 0)
			{
				loc_update_VentilJSON = true;
				Serial.printf("[1sek Loop]  RTC sa rovna Ulozenmy hodnotam na ventile %u a jeho rest time je nula tak nastavujem  ", i);
				ventil[i].CasZavlahyRest = ventil[i].CasZavlahy;

				DebugMsgToWebSocket("[1sek Loop] Smycka na nahodeni ventilu, podla toho ci je System ON nahodim inak vystup nenehodim\r\n");
				if (SystemZapnuty == true)
				{

					Serial.print("Kontrola ci je povoleny den na ventil cislo: ");
					Serial.println(i);
					if (SkontrolujCiJePovolenyDenvTyzdni(ventil[i].DniVtyzdni, &MyRTC_cas) == true)
					{
						ventil[i].Rele = 1;
						digitalWrite(ventil[i].pin, HIGH);
						rr = "[1sek Loop] Smycka ventilu nahazdzuje vystup: " + (String)i + " lebo system je ON \r\n ";
					}
					else
					{
						rr = "[1sek Loop] Smycka ventilu chce nahodit vystup: " + (String)i + " ale nema povoleny den v tyzdni tak nenehadzujem!!!\r\n ";
					}
					DebugMsgToWebSocket(rr);
				}
				else
				{
					rr = "[1sek Loop] Smycka ventilu chce nahodit vystup: " + (String)i + " ale System je OFF!!\r\n ";
					DebugMsgToWebSocket(rr);
				}
			}
		}

		if (loc_update_VentilJSON == true)
		{
			String dsd = "CasZavlahyRest";
			dsd += i;
			VentilJsn[dsd] = ventil[i].CasZavlahyRest;
			String jsonString = JSON.stringify(VentilJsn);
			ws.textAll(jsonString);
		}
	}

	if (SystemZapnuty == false) // ked je system vypnuty tak furt vypinam vystupy
	{
		digitalWrite(Rele1, LOW);
		digitalWrite(Rele2, LOW);
		digitalWrite(Rele3, LOW);
		digitalWrite(Rele4, LOW);
		digitalWrite(Rele5, LOW);
		digitalWrite(Rele6, LOW);
		digitalWrite(Rele7, LOW);

		for (u8 i = 0; i < pocetVentilu; i++)
		{
			ventil[i].Rele = 0;
		}
	}

	//------------------------------------ MINUTA timer BEGIN -------------------------------
	static u8 loc_1minutaCnt = 60;
	if (loc_1minutaCnt)
	{
		if (--loc_1minutaCnt == 0)
		{
			loc_1minutaCnt = 60;
			Serial.println("[1 minuta Loop]  loop perioda...");
			//-----------------------
			for (u8 i = 0; i < pocetVentilu; i++)
			{
				if (ventil[i].CasZavlahyRest)
				{
					if (--ventil[i].CasZavlahyRest == 0)
					{
						ventil[i].Rele = 0;
						digitalWrite(ventil[i].pin, LOW);
					}
					Serial.printf("\r\n[1 minuta Loop] vypis ventilu %u  CasZavlahyRest:%u", i, ventil[i].CasZavlahyRest);
				}
				String dsd = "CasZavlahyRest";
				dsd += i;
				VentilJsn[dsd] = ventil[i].CasZavlahyRest;
			}
		}
	}
	//------------------------------------ MINUTA timer END -------------------------------
}

void Loop_10sek(void)
{
	static u8_t loc_cnt = 0;
	//Serial.println("\r\n[10sek Loop]  Mam Loop 10 sek..........");
	DebugMsgToWebSocket("[10sek Loop]  loop perioda....\r\n");

	Serial.print("Wifi status:");
	Serial.println(WiFi.status());

	//https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
	if (WiFi.status() != WL_CONNECTED)
	{
		loc_cnt++;
		Internet_CasDostupny = false;
	}
	else
	{
		loc_cnt = 0;
		Serial.println("[10sek] Parada WIFI je Connect davam loc_cnt na Nula");

		//TODO ak je Wifi connect tak pocitam ze RTC cas bude OK este dorob
		Internet_CasDostupny = true;
		RTC_cas_OK = true;
	}

	if (loc_cnt == 2)
	{
		Serial.println("[10sek] Odpajam WIFI, lebo wifi nieje: WL_CONNECTED ");
		WiFi.disconnect(1, 1);
	}

	if (loc_cnt == 3)
	{
		loc_cnt = 255;
		WiFi.mode(WIFI_MODE_APSTA);
		Serial.println("znovu -Creating Accesspoint");
		WiFi.softAP(soft_ap_ssid, soft_ap_password, 7, 0, 3);

		if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
		{
			Serial.println("STA Failed to configure");
		}
		Serial.println("znovu -Wifi begin");
		WiFi.begin(NazovSiete, Heslo);
		u8_t aa = 0;
		while (WiFi.waitForConnectResult() != WL_CONNECTED && aa < 2)
		{
			Serial.print("."); 
			aa++;
		}
	}
}

void OdosliCasDoWS(void)
{
	String DenvTyzdni = "! Čas nedostupný !";
	char loc_buf[60];
	DenvTyzdni = ConvetWeekDay_UStoSK(&MyRTC_cas);
	strftime(loc_buf, sizeof(loc_buf), " %H:%M    %d.%m.%Y    ", &MyRTC_cas);

	ObjDatumCas["RTC"] = loc_buf + DenvTyzdni; // " 11:22 Stredaaaa";
	String jsonString = JSON.stringify(ObjDatumCas);
	Serial.print("[Func:OdosliCasDoWS] Odosielam strankam ws Cas:");
	Serial.println(jsonString);
	ws.textAll(jsonString);  
}

void DebugMsgToWebSocket(String textik)
{
	if (LogEnebleWebPage == true)
	{
		String sprava = rtc.getTime("%H:%M:%S ");
		JSON_DebugMsg["DebugMsg"] = sprava + textik;
		String jsonString = JSON.stringify(JSON_DebugMsg);
		ws.textAll(jsonString);
	}
}

void FuncServer_On(void)
{

	server.on("/", 
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  //if (!request->authenticate("ahoj", "xxxx"))
				  //return request->requestAuthentication();
				  //request->send_P(200, "text/html", index_html, processor);
				  AktualnenastavovanyVentil = 0;
				  request->send_P(200, "text/html", PageZavlaha);
			  });

	server.on("/NastavVentil",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  if (!request->authenticate("admin", "sedykocur"))
					  return request->requestAuthentication();

				  if (request->hasParam("ventil"))
				  {
					  String inputMessage;
					  inputMessage = request->getParam("ventil")->value();
					  Serial.println("\r\nPozadovany nastovovany ventil bude:" + inputMessage);
					  AktualnenastavovanyVentil = inputMessage.toInt();
					  if (AktualnenastavovanyVentil == 100)
					  {
						  Serial.print("\r\nPozadovaka na ON OFF systemu");
						  AktualnenastavovanyVentil = 0;
					  }
					  else
					  {
						  Serial.print("\r\nPozadovany nastovovany INT valventil bude:");
						  Serial.println(AktualnenastavovanyVentil);
						  request->send_P(200, "text/html", PageNastevVentil);
					  }
				  }
				  else
				  {
					  request->send_P(404, "text/html", "!! ZEDELS !! ");
				  }
			  });

	server.on("/UlozVentil",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  //if (!request->authenticate("qqq", "www"))
				  //return request->requestAuthentication();

				  //toto vati kolko JSON objektu mas v QUERY  , v tomto pripade 2
				  //ip/UlozVentil?{"IDobjektu":"VentilData1","CasOn":"20:57","CasOnTime":1991}&{"IDobjektu":"VentilData2","CasOn":"20:57","CasOnTime":1991}

				  u16_t PocetArgumentu = request->params();

				  Serial.print("\r\n\r\nPocet argumnetu na ULozVentil je:");
				  Serial.println(PocetArgumentu);

				  if (PocetArgumentu > 0)
				  {
					  char buff[200];
					  AsyncWebParameter *p = request->getParam(0);
					  sprintf(buff, "%s", p->name().c_str());
					  JSONVar myObject = JSON.parse(buff);

					  //toto zisti kolko clenu (keys) ma JSON objekt, v tomto pripdade 5, ale pridavanim hodnot sa to moze zmenit key of JSON objketu index 0!!
					  //ip/UlozVentil?{"IDobjektu":"VentilData1","CasOn":"20:57","CasOnTime":1991}&{"IDobjektu":"VentilData2","CasOn":"20:57","CasOnTime":1991}
					  JSONVar keys = myObject.keys();
					  int le = keys.length();
					  Serial.print("myObject key  ma lengh:");
					  Serial.println(le);
					  ///----------------------------

					  if (myObject.hasOwnProperty("IDobjektu"))
					  {
						  u8 hod = 0, min = 0;
						  int CasZavlahy = 0;
						  int Dnivtyzdni = 0;
						  char str[50];
						  //Serial.print ("Mam ID objektu"); Serial.println (obj["IDobjektu"]);

						  if (myObject["IDobjektu"] == (String) "VentilData1") //"VentilData1")
						  {
							  Serial.print("Parada mam IDobjektu == VentilData1");

							  if (myObject.hasOwnProperty("CasOn"))
							  {
								  Serial.print("\r\nmyObject[CasOn] = ");
								  Serial.println(myObject["CasOn"]);

								  const char *DestCodeBuf = myObject["CasOn"];
								  String dddde((char *)DestCodeBuf);
								  Serial.print("Valueee STRINg:");
								  Serial.println(dddde);

								  //Serial.print ("myObject[CasOn]:");
								  //Serial.println ( JSON.typeof(myObject["CasOn"] ));
								  if (JSON.typeof(myObject["CasOn"]) == "string")
								  {
									  Serial.println("myObject[CasOn]:Detekoval som STRING objekt");
									  char *argv[8];
									  int argc;
									  dddde.toCharArray(str, sizeof(str), 0);
									  split(argv, &argc, str, ':', 0);

									  printf("-------------------------\n");
									  for (int i = 0; i < argc; i++)
										  printf("argv[%d] = %s\n", i, argv[i]);
									  printf("-------------------------\n");

									  hod = atoi(argv[0]);
									  min = atoi(argv[1]);
									  Serial.print("hodnoty hod z ui_8 je: ");
									  Serial.print(hod);
									  Serial.print("  hodnota min z ui_8 je: ");
									  Serial.println(min);
								  }
							  }

							  //tento clen je  poslany zo stranok ako STRING
							  if (myObject.hasOwnProperty("CasOnTime"))
							  {
								  Serial.print("myObject[CasOnTime] = ");
								  Serial.println(myObject["CasOnTime"]);

								  Serial.print("myObject[CasOnTime] key typ = ");
								  Serial.println(JSON.typeof(myObject["CasOnTime"]));

								  const char *DestCodeBuf = myObject["CasOnTime"];
								  String dddde((char *)DestCodeBuf);
								  dddde.toCharArray(str, sizeof(str), 0);
								  CasZavlahy = atoi(str);
								  Serial.print("CasOO int hodnota = ");
								  Serial.println(CasZavlahy);
							  }

							  //tento clen je  poslany zo stranok ako CISLO
							  if (myObject.hasOwnProperty("DniVtyzdni"))
							  {
								  Serial.print("myObject[DniVtyzdni] = ");
								  Serial.println(myObject["DniVtyzdni"]);

								  Serial.print("myObject[DniVtyzdni] key typ = ");
								  Serial.println(JSON.typeof(myObject["DniVtyzdni"]));

								  if (JSON.typeof(myObject["DniVtyzdni"]) == "number")
								  {
									  Dnivtyzdni = myObject["DniVtyzdni"];
								  }
							  }
						  }

						  //tu uz mas vsetko spracovane, tak uloz do EEPROM
						  int Calcadresa_EEPROMzapisu_HodinaON = EE_Ventil1_HodinaON + (AktualnenastavovanyVentil * OffsetDalsihoVentilu);
						  int Calcadresa_EEPROMzapisu_MinutaON = EE_Ventil1_MinutaON + (AktualnenastavovanyVentil * OffsetDalsihoVentilu);
						  int Calcadresa_EEPROMzapisu_CasZavlahy = EE_Ventil1_CasZavlahy + (AktualnenastavovanyVentil * OffsetDalsihoVentilu);
						  int Calcadresa_EEPROMzapisu_DnivTyzdni = EE_Ventil1_Dnivtyzdni + (AktualnenastavovanyVentil * OffsetDalsihoVentilu);
						  //Serial.print ("Vypocitana adresa zapisu EEPOM hodiny = "); Serial.println (Calcadresa_EEPROMzapisu_HodinaON);
						  //Serial.print ("Vypocitana adresa zapisu EEPOM minuta = "); Serial.println (Calcadresa_EEPROMzapisu_MinutaON);
						  //Serial.print ("Vypocitana adresa zapisu EEPOM cas zavlahy = "); Serial.println (Calcadresa_EEPROMzapisu_CasZavlahy);

						  EEPROM.writeUChar(Calcadresa_EEPROMzapisu_HodinaON, hod);
						  EEPROM.writeUChar(Calcadresa_EEPROMzapisu_MinutaON, min);
						  EEPROM.writeUShort(Calcadresa_EEPROMzapisu_CasZavlahy, CasZavlahy);
						  EEPROM.writeByte(Calcadresa_EEPROMzapisu_DnivTyzdni, Dnivtyzdni);
						  EEPROM.commit();

						  ventil[AktualnenastavovanyVentil].HodinyOn = hod;
						  ventil[AktualnenastavovanyVentil].MinutyOn = min;
						  ventil[AktualnenastavovanyVentil].CasZavlahy = CasZavlahy;
						  ventil[AktualnenastavovanyVentil].DniVtyzdni = Dnivtyzdni;

						  //sprintf (str, "%02u:%02u", ventil[AktualnenastavovanyVentil].HodinyOn, ventil[AktualnenastavovanyVentil].MinutyOn);
						  //VentilJsn["CasOn"] = str;
						  //VentilJsn["VentilONTimeVal"] = ventil[AktualnenastavovanyVentil].CasZavlahy;

						  for (u8 i = 0; i < pocetVentilu; i++)
						  {
							  //ventil[i].CasZavlahyRest = 0;
							  String dsd = "CasOn";
							  dsd += i;
							  char rr[50];
							  sprintf(rr, "%02u:%02u", ventil[i].HodinyOn, ventil[i].MinutyOn);
							  VentilJsn[dsd] = rr;

							  dsd = "CasZavlahy";
							  dsd += i;
							  VentilJsn[dsd] = ventil[i].CasZavlahy;
							  dsd = "CasZavlahyRest";
							  dsd += i;
							  VentilJsn[dsd] = ventil[i].CasZavlahyRest;
						  }
					  }

					  //if (request->hasParam (buff))   // otestuj ic mam query tento parameter, inak by sa ti resetleo ESP
					  //{
					  //String par1 = request->getParam (buff)->value ( );   //server.arg (buff);
					  //Serial.printf ("buffer je: %s a par1: %s\r\n", buff, par1);
					  //}
				  }

				  /*String inputMessage;
			if (request->hasParam ("Hodina1"))
			{
				inputMessage = request->getParam ("Hodina1")->value ( );
				Serial.println ("\r\nHodina je:" + inputMessage);
			}
			if (request->hasParam ("Minuta1"))
			{
				inputMessage = request->getParam ("Minuta1")->value ( );
				Serial.println ("Minuta je:" + inputMessage);
			}
			if (request->hasParam ("VentilONTimeVal"))
			{
				inputMessage = request->getParam ("VentilONTimeVal")->value ( );
				Serial.println ("Cas zavlahy:" + inputMessage);
			}*/
				  AktualnenastavovanyVentil = 0;
				  request->send_P(200, "text/html", PageZavlaha);
			  });

	server.on("/nastavip",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  if (!request->authenticate("admin", "sedykocur"))
					  return request->requestAuthentication();
				  request->send(200, "text/html", handle_Zadavanie_IP_setting());
			  });

	server.on("/Nastaveni",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  handle_Nastaveni(request);
				  request->send(200, "text/html", "Nastavujem a ukladam do EEPROM");
				  Serial.println("Idem resetovat ESP");
				  delay(2000);
				  esp_restart();
			  });

	server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  char ttt[500];
				  //u16_t citac = EEPROM.readUShort (EE_citacZapisuDoEEPORM);
				  //u16_t citac2 = EEPROM.readUShort (EE_citac2_ZapisuDoEEPORM);

				  char loc_buf[20];
				  char loc_buf1[60];
				  char loc_buf2[100];
				  if (Internet_CasDostupny == true)
				  {
					  sprintf(loc_buf, "dostupny :-)");
				  }
				  else
				  {
					  sprintf(loc_buf, "nedostupny!!");
				  }

				  if (RTC_cas_OK == false)
				  {
					  sprintf(loc_buf2, "[RTC_cas_OK == flase] RTC NE-maju realny cas!!. RTC hodnota: ");
				  }
				  else
				  {
					  sprintf(loc_buf2, "[RTC_cas_OK == true] RTC hodnota: ");
				  }
				  strftime(loc_buf1, sizeof(loc_buf1), " %H:%M:%S    %d.%m.%Y    ", &MyRTC_cas);

				  sprintf(ttt, "Firmware :%s<br>"
							   "Sila signalu WIFI(-30 je extra suer): %i<br>"
							   "Internet cas: %s<br>"
							   "%s %s",
						  firmware, WiFi.RSSI(), loc_buf, loc_buf2, loc_buf1);

				  request->send(200, "text/html", ttt);
			  });

	server.on("/reset",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  if (!request->authenticate("admin", "leos123"))
					  return request->requestAuthentication();

				  request->send(200, "text/html", "resetujem!!!");
				  delay(1000);
				  esp_restart();
			  });
	server.on("/debug",
			  HTTP_GET,
			  [](AsyncWebServerRequest *request)
			  {
				  LogEnebleWebPage = true;
				  request->send_P(200, "text/html", DebugLog_html);
			  });
}

void SystemInit(void)
{
	pinMode(Rele1, OUTPUT);
	pinMode(Rele2, OUTPUT);
	pinMode(Rele3, OUTPUT);
	pinMode(Rele4, OUTPUT);
	pinMode(Rele5, OUTPUT);
	pinMode(Rele6, OUTPUT);
	pinMode(Rele7, OUTPUT);

	digitalWrite(Rele1, LOW);
	digitalWrite(Rele2, LOW);
	digitalWrite(Rele3, LOW);
	digitalWrite(Rele4, LOW);
	digitalWrite(Rele5, LOW);
	digitalWrite(Rele6, LOW);
	digitalWrite(Rele7, LOW);

	for (u8 i = 0; i < pocetVentilu; i++)
	{
		ventil[i].Rele = 0;
	}

	ventil[0].pin = Rele1;
	ventil[1].pin = Rele2;
	ventil[2].pin = Rele3;
	ventil[3].pin = Rele4;
	ventil[4].pin = Rele5;
	ventil[5].pin = Rele6;
	ventil[6].pin = Rele7;
}

/*******************************************************************************************************/
void ESPinfo(void)
{
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	Serial.println("\r\n*******************************************************************");
	Serial.print("ESP Board MAC Address:  ");
	Serial.println(WiFi.macAddress());
	Serial.println("\r\nHardware info");
	Serial.printf("%d cores Wifi %s%s\n",
				  chip_info.cores,
				  (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
				  (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	Serial.printf("\r\nSilicon revision: %d\r\n ", chip_info.revision);
	Serial.printf("%dMB %s flash\r\n",
				  spi_flash_get_chip_size() / (1024 * 1024),
				  (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embeded" : "external");

	Serial.printf("\r\nCPU frekvencia: %d[MHz]   XTAL frek: %d[MHz]\r\n ", getCpuFrequencyMhz(), getXtalFrequencyMhz());

	Serial.printf("\r\nTotal heap: %d\r\n", ESP.getHeapSize());
	Serial.printf("Free heap: %d\r\n", ESP.getFreeHeap());
	Serial.printf("Total PSRAM: %d\r\n", ESP.getPsramSize());
	Serial.printf("Free PSRAM: %d\r\n", ESP.getFreePsram()); // log_d("Free PSRAM: %d", ESP.getFreePsram());
	Serial.println("\r\n*******************************************************************");
}

int getIpBlock(int index, String str)
{
	char separator = '.';
	int found = 0;
	int strIndex[] = {0, -1};
	int maxIndex = str.length() - 1;

	for (int i = 0; i <= maxIndex && found <= index; i++)
	{
		if (str.charAt(i) == separator || i == maxIndex)
		{
			found++;
			strIndex[0] = strIndex[1] + 1;
			strIndex[1] = (i == maxIndex) ? i + 1 : i;
		}
	}

	return found > index ? str.substring(strIndex[0], strIndex[1]).toInt() : 0;
}

String ipToString(IPAddress ip)
{
	String s = "";
	for (int i = 0; i < 4; i++)
		s += i ? "." + String(ip[i]) : String(ip[i]);
	return s;
}

IPAddress str2IP(String str)
{

	IPAddress ret(getIpBlock(0, str), getIpBlock(1, str), getIpBlock(2, str), getIpBlock(3, str));
	return ret;
}

String handle_LenZobraz_IP_setting()
{
	char ttt[1000];
	char NazovSiete[56] = {"nazovSiete\0"};
	char ippadresa[56];
	char maskaIP[56];
	char brana[56];

	EEPROM.readString(EE_NazovSiete, NazovSiete, 16);
	IPAddress ip = WiFi.localIP();
	String stt = ipToString(ip);
	stt.toCharArray(ippadresa, 16);
	stt = ipToString(WiFi.subnetMask());
	stt.toCharArray(maskaIP, 16);
	stt = ipToString(WiFi.gatewayIP());
	stt.toCharArray(brana, 16);

	Serial.print("\r\nVyparsrovane IP: ");
	Serial.print(ippadresa);
	Serial.print("\r\nVyparsrovane MASKa: ");
	Serial.print(maskaIP);
	Serial.print("\r\nVyparsrovane Brana: ");
	Serial.print(brana);

	sprintf(ttt, LenzobrazIP_html, ippadresa, maskaIP, brana, NazovSiete);
	return ttt;
	//server.send (200, "text/html", ttt);
}

String handle_Zadavanie_IP_setting()
{
	char ttt[1000];
	char NazovSiete[56] = {"nazovSiete\0"};
	char HesloSiete[56] = {"HesloSiete\0"};
	char ippadresa[56];
	char maskaIP[56];
	char brana[56];

	IPAddress ip = WiFi.localIP();
	String stt = ipToString(ip);
	stt.toCharArray(ippadresa, 16);
	stt = ipToString(WiFi.subnetMask());
	stt.toCharArray(maskaIP, 16);
	stt = ipToString(WiFi.gatewayIP());
	stt.toCharArray(brana, 16);

	EEPROM.readString(EE_NazovSiete, NazovSiete, 16);
	Serial.print("\r\nNazov siete: ");
	Serial.print(NazovSiete);

	EEPROM.readString(EE_Heslosiete, HesloSiete, 16);
	Serial.print("\r\nHeslo siete: ");
	Serial.print(HesloSiete);

	Serial.print("\r\nVyparsrovane IP: ");
	Serial.print(ippadresa);
	Serial.print("\r\nVyparsrovane MASKa: ");
	Serial.print(maskaIP);
	Serial.print("\r\nVyparsrovane Brana: ");
	Serial.print(brana);

	sprintf(ttt, zadavaci_html, ippadresa, maskaIP, brana, NazovSiete, HesloSiete);
	//Serial.print ("\r\nToto je bufer pre stranky:\r\n");
	//Serial.print(ttt);

	return ttt;
}

void handle_Nastaveni(AsyncWebServerRequest *request)
{
	String inputMessage;
	String inputParam;
	Serial.println("Mam tu nastaveni");

	if (request->hasParam("input1"))
	{
		inputMessage = request->getParam("input1")->value();
		if (inputMessage.length() < 16)
		{
			EEPROM.writeString(EE_IPadresa, inputMessage);
		}
	}

	if (request->hasParam("input2"))
	{
		inputMessage = request->getParam("input2")->value();
		if (inputMessage.length() < 16)
		{
			EEPROM.writeString(EE_SUBNET, inputMessage);
		}
	}

	if (request->hasParam("input3"))
	{
		inputMessage = request->getParam("input3")->value();
		if (inputMessage.length() < 16)
		{
			EEPROM.writeString(EE_Brana, inputMessage);
		}
	}

	if (request->hasParam("input4"))
	{
		inputMessage = request->getParam("input4")->value();
		if (inputMessage.length() < 16)
		{
			EEPROM.writeString(EE_NazovSiete, inputMessage);
		}
	}

	if (request->hasParam("input5"))
	{
		inputMessage = request->getParam("input5")->value();
		if (inputMessage.length() < 20)
		{
			EEPROM.writeString(EE_Heslosiete, inputMessage);
		}
	}

	EEPROM.commit();
}

int8_t NacitajEEPROM_setting(void)
{
	if (!EEPROM.begin(1000))
	{ 
		Serial.println("Failed to initialise EEPROM");
		return -1;
	}

	Serial.println("[NacitajEEPROM_setting]Succes to initialise EEPROM");

	EEPROM.readBytes(EE_NazovSiete, NazovSiete, 16);

	if (NazovSiete[0] != 0xff) //ak mas novy modul tak EEPROM vrati prazdne hodnoty, preto ich neprepisem z EEPROM, ale necham default
	{
		String apipch = EEPROM.readString(EE_IPadresa); // "192.168.1.11";
		local_IP = str2IP(apipch);

		apipch = EEPROM.readString(EE_SUBNET);
		subnet = str2IP(apipch);

		apipch = EEPROM.readString(EE_Brana);
		gateway = str2IP(apipch);

		memset(NazovSiete, 0, sizeof(NazovSiete));
		memset(Heslo, 0, sizeof(Heslo));
		EEPROM.readBytes(EE_NazovSiete, NazovSiete, 16);
		EEPROM.readBytes(EE_Heslosiete, Heslo, 20);
		Serial.printf("[NacitajEEPROM_setting] Nacitany nazov siete a heslo z EEPROM: %s  a %s\r\n", NazovSiete, Heslo);
	}
	else
	{
		Serial.println("[NacitajEEPROM_setting] EEPROM je este prazna, nachavma default hodnoty");
		sprintf(NazovSiete, "semiart");
		sprintf(Heslo, "aabbccddff");
		return 1;
	}

	for (u8 i = 0; i < pocetVentilu; i++)
	{
		int Calcadresa_EEPROMcitaniaHodinaON = EE_Ventil1_HodinaON + (i * OffsetDalsihoVentilu);
		int Calcadresa_EEPROMcitaniaMinutaON = EE_Ventil1_MinutaON + (i * OffsetDalsihoVentilu);
		int Calcadresa_EEPROMcitaniaCasZavlahy = EE_Ventil1_CasZavlahy + (i * OffsetDalsihoVentilu);
		int Calcadresa_EEPROMcitaniaDnivtyzdni = EE_Ventil1_Dnivtyzdni + (i * OffsetDalsihoVentilu);
		ventil[i].HodinyOn = EEPROM.readUChar(Calcadresa_EEPROMcitaniaHodinaON);
		ventil[i].MinutyOn = EEPROM.readUChar(Calcadresa_EEPROMcitaniaMinutaON);
		ventil[i].CasZavlahy = EEPROM.readUShort(Calcadresa_EEPROMcitaniaCasZavlahy);
		ventil[i].DniVtyzdni = EEPROM.readByte(Calcadresa_EEPROMcitaniaDnivtyzdni);
	}
	//Serial.printf ("[NacitajEEPROM_setting] Ventil 0 nacitany: %u:%u %u\n\r", ventil[0].HodinyOn, ventil[0].MinutyOn, ventil[0].CasZavlahy);

	//JSON obje init
	myObject["hello"] = " 11:22 Streda";

	for (u8 i = 0; i < pocetVentilu; i++)
	{
		ventil[i].CasZavlahyRest = 0;
		String dsd = "CasOn";
		dsd += i;
		char rr[50];
		sprintf(rr, "%02u:%02u", ventil[i].HodinyOn, ventil[i].MinutyOn);
		VentilJsn[dsd] = rr;

		dsd = "CasZavlahy";
		dsd += i;
		VentilJsn[dsd] = ventil[i].CasZavlahy;
		dsd = "CasZavlahyRest";
		dsd += i;
		VentilJsn[dsd] = ventil[i].CasZavlahyRest;
	}
	Serial.println("[NacitajEEPROM_setting] vypis JSONobjektu:");
	String jsonString = JSON.stringify(VentilJsn);
	Serial.println(jsonString);
	return 0;
}

void OdosliStrankeVentilData(void)
{
	Serial.println("Func [OdosliStrankeVentilData]  ide spracovat ");

	JSONVar Ventiljns_loc;

	char buuu[50];
	sprintf(buuu, "%02u:%02u", ventil[AktualnenastavovanyVentil].HodinyOn, ventil[AktualnenastavovanyVentil].MinutyOn);
	Ventiljns_loc["CasOn"] = buuu;
	Ventiljns_loc["CasZavlahy"] = ventil[AktualnenastavovanyVentil].CasZavlahy;
	Ventiljns_loc["DniVtyzdni"] = ventil[AktualnenastavovanyVentil].DniVtyzdni;
	Ventiljns_loc["VentilONOFF"] = ventil[AktualnenastavovanyVentil].Rele;
	Ventiljns_loc["SystemONOFF"] = SystemZapnuty;

	String jsonString = JSON.stringify(Ventiljns_loc);
	Serial.print("[ event -VratNamerane_TaH] Odosielam strankam ObjTopeni:");
	//Serial.println(jsonString);
	ws.textAll(jsonString);
}
