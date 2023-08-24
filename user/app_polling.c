/*
 * Below is the given application for lab5_1.
 * The goal of this app is to control the car via Bluetooth. 
 */

#include "user_lib.h"
#include "util/types.h"

int main(void) {
  printu("please input the instruction through bluetooth!\n");
  while(1)
  {
    char temp = (char)uartgetchar();
	  if(temp == 'q')
		  break;
	  car_control(temp);
  }
  exit(0);
  return 0;
}
