
/**********************************************
* BeagleBone PRU Backscatter
* BLE advertisements backscatter
* Authors : Ambuj Varshney < ambuj_varshney@it.uu.se >
* (C) 2016 Uppsala Networked Objects (UNO)
************************************************/

#include "blebackscatter.h"

extern void *pru0DataMem;
extern void *pru1DataMem;


/* Setup the BeagleBone PRU
*  perform the initialization and other activities
**/

int pru_setup() {
  
   int rtn;
   tpruss_intc_initdata intc = PRUSS_INTC_INITDATA;

   /* initialize PRU */
   if((rtn = prussdrv_init()) != 0) {
      fprintf(stderr, "prussdrv_init() failed\n");
      return rtn;
   }

   /* open the interrupt */
   if((rtn = prussdrv_open(PRU_EVTOUT_0)) != 0) {
      fprintf(stderr, "prussdrv_open() failed\n");
      return rtn;
   }

   /* initialize interrupt */
   if((rtn = prussdrv_pruintc_init(&intc)) != 0) {
      fprintf(stderr, "prussdrv_pruintc_init() failed\n");
      return rtn;
   }

   if((rtn = prussdrv_pru_reset(PRU_NUM)) != 0) {
      fprintf(stderr, "prussdrv_pru_reset() failed\n");
      return rtn;
   }

   if ((rtn = prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru0DataMem)) != 0) {
     fprintf(stderr, "prussdrv_map_prumem() failed\n");
     return rtn;
   }

   if ((rtn = prussdrv_map_prumem(PRUSS0_PRU1_DATARAM, &pru1DataMem)) != 0) {
     fprintf(stderr, "prussdrv_map_prumem() failed\n");
     return rtn;
   }

   return rtn;
}


/*** pru_cleanup() -- halt PRU and release driver

Performs all necessary de-initialization tasks for the prussdrv library.

Returns 0 on success, non-0 on error.
***/
int pru_cleanup(void) {
   int rtn = 0;

   /* clear the event (if asserted) */
   if(prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT)) {
      fprintf(stderr, "prussdrv_pru_clear_event() failed\n");
      rtn = -1;
   }

   /* halt and disable the PRU (if running) */
   if((rtn = prussdrv_pru_disable(PRU_NUM)) != 0) {
      fprintf(stderr, "prussdrv_pru_disable() failed\n");
      rtn = -1;
   }

   /* release the PRU clocks and disable prussdrv module */
   if((rtn = prussdrv_exit()) != 0) {
      fprintf(stderr, "prussdrv_exit() failed\n");
      rtn = -1;
   }

   return rtn;
}
