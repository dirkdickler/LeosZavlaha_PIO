#ifndef __HelpFunction_H
#define __HelpFunction_H

#include <Arduino.h>

char **split(char **argv, int *argc, char *string, const char delimiter, int allowempty);
String NacitajReleK1_K7(void);
String ConvetWeekDay_UStoCZ(tm *timeInfoPRT);
String ConvetWeekDay_UStoSK(tm *timeInfoPRT);
bool SkontrolujCiJePovolenyDenvTyzdni ( u8 Obraz, tm *timeInfoPRT );

#endif