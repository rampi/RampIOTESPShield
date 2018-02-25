#include "Arduino.h"
#include "Storage.h"
#include <EEPROM.h>

int size = 512;
byte dataLength = 36;

Storage::Storage(){}

void Storage::begin(){
	EEPROM.begin(size);	
}

int Storage::getMaxSlots(){
	return size / dataLength;
}

int Storage::getSize(){
	return size;
}

int getStartIndex(int slot){
	return slot <= (size/dataLength) ? dataLength * slot : -1;
}

void Storage::clearAll(){
	for (int i = 0; i < size; i++){
		EEPROM.write(i, 0);
	}
	EEPROM.commit();
}

void clearSlot(int slot){
	int start = getStartIndex(slot);
	int end = start + dataLength;	
	for (int i = start; i < end; i++){
		EEPROM.write(i, 0);
	}
	EEPROM.commit();
}

void Storage::saveStringData(int slot, String data){
	clearSlot(slot);	
	int start = getStartIndex(slot);
	int end = start + dataLength;
	int charIndex = 0;	
	for (int i = start; i < end; ++i){
		EEPROM.write(i, data[charIndex]);
		if( charIndex+1 >= data.length() ){
			break;
		}
		charIndex++;
	}
	EEPROM.commit();
}

void Storage::saveData(int slot, const char *data){
	clearSlot(slot);
	int start = getStartIndex(slot);
	int end = start + dataLength;
	int charIndex = 0;
	size_t sz = strlen(data);
	for (int i = start; i < end; i++){
		EEPROM.write(i, data[charIndex]);
		charIndex++;
		if( charIndex >= sz ){
			break;
		}		
	}	
	EEPROM.commit();
}

void Storage::clearSlot(int slot){
	int start = getStartIndex(slot);
	int end = start + dataLength;	
	for (int i = start; i < end; i++){
		EEPROM.write(i, 0);
	}
	EEPROM.commit();
}

bool Storage::isEmpty(int slot){
	int start = getStartIndex(slot);
	return EEPROM.read(start) == 0;
}

void Storage::getData(int slot, char *buff){
	int start = getStartIndex(slot);
	int end = start + dataLength;	
	int charIndex = 0;
	for (int i = start; i < end; i++){
		int c = EEPROM.read(i);		
		if( c >= 32 && c <= 126 ){
			buff[charIndex] = char(c);
			charIndex++;
		}else{
			break;			
		}	
	}
	buff[charIndex] = '\0';
}

int Storage::getSlotSize(int slot){
	int start = getStartIndex(slot);
	int end = start + dataLength;	
	int charIndex = 0;
	for (int i = start; i < end; i++){
		int c = EEPROM.read(i);
		if( c >= 32 && c <= 126 ){
			charIndex++;
		}
	}
	return charIndex;
}