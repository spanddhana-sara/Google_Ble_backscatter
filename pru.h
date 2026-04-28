
/**********************************************
* BeagleBone PRU Backscatter
* BLE advertisements backscatter
* Authors : Ambuj Varshney < ambuj_varshney@it.uu.se >
* (C) 2016 Uppsala Networked Objects (UNO)
************************************************/


#include <prussdrv.h>
#include <pruss_intc_mapping.h>



#define POPULATE_PRU \
  do \
  {\
  for(i=0; i <8; i++)\
  {\
  count = 0;\
  endtime = ( (time/1000) + 1)* 1000;\
  while(time < endtime)\
  {\
          count = count + 1;\
          if(barr[i] == 0)\
             time = time + BLE_FREQ1;\
          else if(barr[i] == 1)\
             time = time + BLE_FREQ2;\
  }\
  pruMem8[prumemCnt++] = count& 0xff;\
  if(barr[i] == 0)\
    pruMem8[prumemCnt++] = DELAY(BLE_FREQ1)& 0xff;\
  else\
    pruMem8[prumemCnt++] = DELAY(BLE_FREQ2)& 0xff;\
  }\
  }while(0)

  
int pru_setup();
int pru_cleanup(void);

