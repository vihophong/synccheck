/***********************************************************************************
 >  *        File:   syncSpy.cpp                                                 *
 >  * Description:   read the "DataSink" (MIDAS data shared program) 
 >  *                                  shared memory directly                *
 >  *      Author:   Alvaro Tolosa Delgado  IFIC/CSIC spain                           *
 >  *Site Contact:   alvaro.tolosa@ific.uv.es                                           *
 >  * Last Change:   02/11/2016                                                    *
 >  *******************************************************************************/

#include <iostream>
#include <fstream>
#include <stdint.h>

int main()
{

  std::ifstream input( "/dev/shm/SHM_110205", std::ios::binary );

  uint32_t buffer; //read 1 -word each time

  uint16_t lineCounter(0);

  while(input.read((char *)&buffer,sizeof(buffer)) ){

  if( buffer )

  std::cout << std::hex << buffer << "\t\t\t" << lineCounter << std::endl;

  ++lineCounter;

  }

input.close();
return 0;

}
