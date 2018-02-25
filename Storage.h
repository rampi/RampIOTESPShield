#ifndef Storage_h
#define Storage_h
#include "Arduino.h"
#include "EEPROM.h"
class Storage
{
  public:
    Storage();
    void begin();    
    void saveStringData(int slot, String data);
    void saveData(int slot, const char *data);
    void getData(int slot, char *buff);
    void clearAll();
    void clearSlot(int slot);
    bool isEmpty(int slot);
    int getMaxSlots();        
    int getSize();
    int getSlotSize(int slot);
};
#endif