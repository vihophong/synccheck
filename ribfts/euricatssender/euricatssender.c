/*
 * last modified : 15/02/16 14:30:29 
 * 
 * Hidetada Baba (RIKEN)
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define DB(x)

#include "transfer.h"

#include <s_filhe_swap.h>
#include <s_bufhe_swap.h>
#include <s_ve10_1_swap.h>
#include <s_ves10_1_swap.h>
#include <s_evhe_swap.h>
#include <f_evt.h>

/* Default values */
const char defmbsevserv[] = "e01\0";

/* Globals */
s_evt_channel *s_gl_chan_i; //channel
s_ve10_1  *ps_ve10_1 = NULL;  //event header
s_ves10_1 *ps_ves10_1 = NULL; //sub event header
s_evhe    *ps_evhe = NULL; //event header
s_filhe   *ps_filhe = NULL; 
s_bufhe   *ps_bufhe = NULL; //buffer header

int endtime = 2;
int mbscnt;
unsigned long long int *mbsbuff = NULL;

/* Buffer */
#define MAXTSBUFF 1000000
#define MAXTSCNT   700000

void signal_block(){
}
int msgDefineMessage(unsigned int id, unsigned int xclass, unsigned int level, char *source, char *body) {
  return 0;
}

struct stsyncdata{
  unsigned int id;
  unsigned int adc;
  unsigned long long int ts;
};

int transid = 0;
char *data;
void *quit(void){
  transferMultiClose(transid);

  f_evt_get_close(s_gl_chan_i);

  free(mbsbuff);
  free(data);
  
  exit(0);
}

int unpackmbs(){
  int subevsize, *data, nsubevents;
  unsigned int t1, t2;
  unsigned long long int ts;
  nsubevents = f_evt_get_subevent(ps_ve10_1,0,
                                  (int **)&ps_ves10_1,
                                  (int **)&data,
                                  (int *)&subevsize);
  if(nsubevents <= 0){
    return 0;
  };
   
  // Loop over them
  int uCall[8]; // count how many times a given unpacker is called in a suevent
  memset(uCall, 0, sizeof(uCall)); // initialize to 0
  int nsubevent;
   
  for(nsubevent = 1 ; nsubevent < nsubevents+1 ; ++nsubevent){
    int status;

    //Read the sub-event
    status = f_evt_get_subevent(ps_ve10_1,nsubevent,
                                (int **)&ps_ves10_1,
                                (int **)&data,
                                (int *)&subevsize);
    if(status != GETEVT__SUCCESS){
      return 0 ;
    };
    
    if( subevsize <= 0 ){
      continue;
    };

    status = 0;
    switch( ps_ves10_1->i_procid ){
    case 60 : //status = UnpackSimba(60) ; 
      ++uCall[0] ; status = 1; break ;
    case 31 : //status = UnpackDGF(31) ; 
      ++uCall[1] ; status = 1; break ;
    case 40 : //status = UnpackTimeGe(40) ; 
      ++uCall[2] ; status = 1; break ;
    case 11 : //status = UnpackLupo(11) ; 
      t1 = data[1];
      t2 = data[2];
      ts = (unsigned long long int)t2<<32 | (unsigned long long int)t1;
      //printf(" ts=%lld\n", ts);
      status = 1;
      mbsbuff[mbscnt] = ts;
      mbscnt++;
      ++uCall[3] ; break ;
    case 10 : //status = UnpackBigRips(10) ; 
      ++uCall[4] ; status = 1; break ;
    case 70 : //status = UnpackSimbaTime(70) ; 
      ++uCall[5] ; status = 1; break ;
    default : 
      status = 0;
      break;
    };
      
    if(status == 0) return 0;
  }
   
  int nunpacker;
  for(nunpacker = 0 ; nunpacker < 8; ++nunpacker)
    if(uCall[nunpacker]>1)
      printf("unpacker %d wad called %d times\n", nunpacker, uCall[nunpacker]);
   
  return 1;
}


int main(int argc, char argv[]){
  const int buffersize = 64*1024;
  int i;
  int dataidx = 0;
  unsigned long long int ts, tts;
  struct stsyncdata syncdata;
  time_t tstart, now;

  char c_source[64];
  int l_server = GETEVT__EVENT;
  int eventype;
  int dbmode = 0;


  if(argc==2){
    dbmode = 1;
  }

  syncdata.id = 0; //1; //3;

  data = malloc(buffersize+32);
  if(!data){
    printf("Cannot' malloc sender data%d\n", buffersize);
    return 0;
  }

  printf("euricatssender\n");

  mbsbuff = (unsigned long long int *)malloc(MAXTSBUFF * sizeof(unsigned long long int));

  if(mbsbuff == NULL){
    printf("Can't allocate buffer memory\n");
    quit();
  }

  /* Signal */
  signal(SIGINT, (void *)quit);

  transferMultiPort(transid, 10305);
  transferMultiBlockSize(transid, buffersize);
  transferMultiInit(transid, "10.32.0.12");



  /* MBS */
  s_gl_chan_i = f_evt_control();
  strcpy(c_source, defmbsevserv);
  
  if(f_evt_get_open(l_server, c_source, s_gl_chan_i, (char **)&ps_filhe, 0,0)
     != GETEVT__SUCCESS){
    printf("Can't connect Event Server = %s\n", c_source);
    quit();
  }
  printf("Connected to %s\n", c_source);

  while(1){
    time(&tstart);

    mbscnt = 0;
    while(1){
      time(&now);
      if(now > tstart + endtime) break;
      eventype = f_evt_get_event(s_gl_chan_i, (int **)&ps_ve10_1,
                                 (int **)&ps_bufhe);
      ps_evhe = (s_evhe *)ps_ve10_1;
      
      switch (eventype){
      case GETEVT__SUCCESS :
        unpackmbs();
        break;
      case GETEVT__NOMORE  :
        printf("no more\n");
        break;
      case GETEVT__FAILURE :
        printf("failure\n");
        break;
      case GETEVT__FRAGMENT:
        break;
      case GETEVT__NOFILE  :
        break;
      case GETEVT__NOSERVER:
      break;
      default :
	break;
      }
      
      if(mbscnt > MAXTSCNT){
	break;
      }
      usleep(1);
    }

    memset(data, 0, buffersize);
    dataidx = 0;
    printf("satmbs=%d\n", mbscnt);

    for(i=0;i<mbscnt;i++){
      //printf("buffsize: %d \n",buffersize);

      if(buffersize>(32+dataidx)){
	syncdata.adc = 10;
	syncdata.ts = (mbsbuff[i] >> 2) & 0x0000ffffffffffffLL;
	memcpy(data+32+dataidx, (char *)&syncdata, sizeof(syncdata));
	dataidx += sizeof(syncdata);
	if(dbmode){
	  //printf("we're here...\n");
	  printf("TS=%llu \n", mbsbuff[i]);
	}
      }
    }
    
    if(mbscnt > 0){
      transferMultiTxData(transid, &data[32], 1, buffersize-32);
    }
  }

  quit();

  return 0;
}
