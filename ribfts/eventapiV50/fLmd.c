#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <memory.h>
#include <pwd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef Lynx /* LynxOS */
#include <timeb.h>
#define int32_t int
#define int16_t short
#define uint32_t unsigned int
#endif

#ifdef Linux /* Linux */
#include <stdint.h>
#include <sys/timeb.h>
#include <inttypes.h>
#endif

#include "f_stccomm.h"
#include "sMbs.h"
#include "fLmd.h"
int32_t  fLmdWriteBuffer(sLmdControl *, char *, uint32_t);
uint32_t fLmdCleanup(sLmdControl *);
void     fLmdOffsetResize(sLmdControl *, uint32_t);
uint32_t fLmdOffsetSet(sLmdControl *, uint32_t );
uint32_t fLmdOffsetRead(sLmdControl *);
uint32_t fLmdOffsetWrite(sLmdControl *);
lmdoff_t fLmdOffsetGet(sLmdControl *, uint32_t);
void     fLmdOffsetElements(sLmdControl *, uint32_t, uint32_t *, uint32_t *);
#define OFFSET__ENTRIES 250000

//===============================================================
uint32_t fLmdPutOpen(
sLmdControl *pLmdControl, 
char    *Filename, 
sMbsFileHeader *pBuffHead, // LMD__STANDARD_HEADER (NULL) or address
uint32_t iBytes,           // LMD__NO_BUFFER (0) or buffer size
uint32_t iOver,            // LMD__[NO_]OVERWRITE
uint32_t iUseOffset,       // LMD__[NO_]INDEX
uint32_t iLargeFile){      // LMD__[NO_]LARGE_FILE

  char c_mode[80];
  struct timespec clock;
  int32_t iReturn;

  memset(pLmdControl,0,sizeof(sLmdControl));

  // allocate header or take extern
  if(pBuffHead == LMD__STANDARD_HEADER){
    pLmdControl->pMbsFileHeader= (sMbsFileHeader *)malloc(sizeof(sMbsFileHeader));
    memset(pLmdControl->pMbsFileHeader,0,sizeof(sMbsFileHeader));
    pLmdControl->iInternHeader=1;
  }
  else {
    pLmdControl->pMbsFileHeader= pBuffHead;
    pLmdControl->iInternHeader=0;
  }
  clock_gettime(CLOCK_REALTIME,&clock);
  pLmdControl->pMbsFileHeader->iTimeSpecSec=clock.tv_sec;
  pLmdControl->pMbsFileHeader->iTimeSpecNanoSec=clock.tv_nsec;
  pLmdControl->pMbsFileHeader->iType=LMD__TYPE_FILE_HEADER_101_1;
  pLmdControl->pMbsFileHeader->iEndian=1;
  strcpy(pLmdControl->cFile,Filename);

  // optionally allocate buffer
  if(iBytes > 0){
    pLmdControl->pBuffer=(int16_t *)malloc(iBytes);
    pLmdControl->iInternBuffer=1;
  }
  pLmdControl->iBufferWords=iBytes/2;
  pLmdControl->iLeftWords=iBytes/2;
  // open file
  if(iOver == LMD__NO_OVERWRITE){  // do not overwrite
  if((pLmdControl->fFile=(FILE *)fopen64(Filename,"r") )!=NULL){
    printf("fLmdPutOpen: File exists: %s\n",Filename);
    fLmdCleanup(pLmdControl);
    fclose(pLmdControl->fFile);
    return(PUTLMD__FILE_EXIST);
  }}

  if((pLmdControl->fFile=(FILE *)fopen64(Filename,"w+") )== NULL){
    printf("fLmdPutOpen: Error open file %s\n",Filename);
    fLmdCleanup(pLmdControl);
    return(PUTLMD__OPEN_ERR);
  }

  if(iLargeFile == LMD__LARGE_FILE)pLmdControl->iOffsetSize=8;
  else                             pLmdControl->iOffsetSize=4;
  pLmdControl->pMbsFileHeader->iOffsetSize=pLmdControl->iOffsetSize;

  // write header
  iReturn=fLmdWriteBuffer(pLmdControl,(char *)pLmdControl->pMbsFileHeader,
		    (pLmdControl->pMbsFileHeader->iUsedWords)*2+sizeof(sMbsFileHeader));
  pLmdControl->iBytes+=iReturn;

  if(iUseOffset == LMD__INDEX)fLmdOffsetResize(pLmdControl,iReturn/4); // create and set first value
  printf("fLmdPutOpen: %s. Bytes:%d over:%d table:%d large:%d.\n",
	 Filename,iBytes,iOver,iUseOffset,iLargeFile);
return(LMD__SUCCESS);
}

//===============================================================
uint32_t fLmdPutElement(
sLmdControl *pLmdControl, 
sMbsHeader *pHeader){
  uint32_t *ps, *pd, i, elements;
  int64_t fileleft,used;
  int32_t iReturn;

  // enough space left?
  if(pLmdControl->iOffsetEntries && (pLmdControl->iOffsetSize == 4)){
    elements=pLmdControl->iElements+2;
    used=pLmdControl->iBytes/4;
    fileleft=0xffffffff - used - (4+elements); // size of table
    if((int64_t)(pHeader->iWords/2+2) > fileleft){
      printf("fLmdPutElement: File size exceed\n");
      return(PUTLMD__EXCEED);
    }
  }
  // save largest size in header
  if((pHeader->iWords+4) > pLmdControl->pMbsFileHeader->iMaxWords) 
      pLmdControl->pMbsFileHeader->iMaxWords=pHeader->iWords+4;
  // no buffer, write element directly
  if(pLmdControl->iBufferWords == 0){
    pLmdControl->pMbsHeader=pHeader;
    iReturn=fLmdWriteBuffer(pLmdControl,(char *)pHeader,(pHeader->iWords+4)*2);
    pLmdControl->iBytes+=iReturn;
    if(iReturn != (pHeader->iWords+4)*2){
      printf("fLmdPutElement: Write error \n");
     return(LMD__FAILURE);
    }
    pLmdControl->pMbsFileHeader->iElements++;
    pLmdControl->iElements++;
    if(pLmdControl->iOffsetEntries)fLmdOffsetSet(pLmdControl,iReturn/4);
    return(LMD__SUCCESS);

  } // end no buffer
  if((pHeader->iWords+4) > pLmdControl->iLeftWords){ // flash buffer to file  
    iReturn=fLmdWriteBuffer(pLmdControl, (char *)pLmdControl->pBuffer,
			    (pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2);
    pLmdControl->iBytes+=iReturn;
    if(iReturn != (pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2)return(LMD__FAILURE);
    pLmdControl->iLeftWords=pLmdControl->iBufferWords; // buffer free
  }
  if((pHeader->iWords+4) > pLmdControl->iLeftWords){ // element too big for buffer
    printf("fLmdPutElement: Element too big: %d words\n",pHeader->iWords+4);
    return(PUTLMD__TOOBIG); 
  } 
  // copy to buffer
  ps=(uint32_t *)pHeader;
  pd=(uint32_t *)pLmdControl->pBuffer+(pLmdControl->iBufferWords-pLmdControl->iLeftWords)/2;
  iReturn=(pHeader->iWords+4)/2; // here 32b words
  for(i=0;i<iReturn;i++) *pd++ = *ps++;
  pLmdControl->pMbsFileHeader->iElements++;
  pLmdControl->iElements++;
  pLmdControl->iLeftWords -= (pHeader->iWords+4);
  if(pLmdControl->iOffsetEntries)fLmdOffsetSet(pLmdControl,iReturn);
  return(LMD__SUCCESS);
}

//===============================================================
uint32_t fLmdPutBuffer(
sLmdControl *pLmdControl, 
sMbsHeader *pHeader, 
uint32_t Items){

  sMbsHeader *pH;
  uint32_t Bytes=0,TotalBytes=0,i, elements;
  int64_t fileleft,used;
  int32_t iReturn;

  // check if total buffer fits in file
  if(pLmdControl->iOffsetEntries && (pLmdControl->iOffsetSize == 4)){
    elements=pLmdControl->iElements+Items+2;
    used=pLmdControl->iBytes/4;
    fileleft=0xffffffff - used - (4+elements); // size of table
    for(i=0;i<Items;i++)
      {
	Bytes = (4+pH->iWords)*2;
	TotalBytes += Bytes;
	pH=(sMbsHeader *)((int16_t *)pH+Bytes/2);
      }
    if((int64_t)TotalBytes/4 > fileleft){
      printf("fLmdPutElement: File size exceed\n");
      return(PUTLMD__EXCEED);
    }
    Bytes=0;
    TotalBytes=0;
  }
  pH=pHeader;
  for(i=0;i<Items;i++)
    {
      pLmdControl->iElements++;
      Bytes = (4+pH->iWords)*2;
      TotalBytes += Bytes;
      if(pLmdControl->iOffsetEntries)fLmdOffsetSet(pLmdControl,Bytes/4);
      if((pH->iWords+4) > pLmdControl->pMbsFileHeader->iMaxWords) 
          pLmdControl->pMbsFileHeader->iMaxWords=pH->iWords+4;
      pH=(sMbsHeader *)((int16_t *)pH+Bytes/2);
    }
  iReturn=fLmdWriteBuffer(pLmdControl,(char *)pHeader,TotalBytes);
  pLmdControl->iBytes+=iReturn;
  if(iReturn != TotalBytes)return(LMD__FAILURE);
  else {
    pLmdControl->pMbsFileHeader->iElements += Items;
    return(LMD__SUCCESS);
  }
}

//===============================================================
uint32_t fLmdPutClose(
sLmdControl *pLmdControl){
  int32_t iReturn,i;
  lmdoff_t current,c;

  if(pLmdControl->iBufferWords > pLmdControl->iLeftWords){ // write last buffer
    iReturn=fLmdWriteBuffer(pLmdControl, (char *)pLmdControl->pBuffer,
			    (pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2);
    pLmdControl->iBytes+=iReturn;
    if(iReturn != (pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2){
      printf("fLmdPutClose: Error writing last buffer. Closing file.\n");
      // rewind file and rewrite header
      rewind(pLmdControl->fFile);  /* rewind file, rewrite header */
      fLmdWriteBuffer(pLmdControl, (char *)pLmdControl->pMbsFileHeader,
		      sizeof(sMbsFileHeader));
      fLmdCleanup(pLmdControl);
      return(LMD__FAILURE);
    }
  }
  if(pLmdControl->iOffsetEntries) 
    if(fLmdOffsetWrite(pLmdControl) != LMD__SUCCESS)
      pLmdControl->pMbsFileHeader->iTableOffset=0; // table could not be written
  
  // rewind file and rewrite header
  rewind(pLmdControl->fFile);  /* rewind file, rewrite header */
  fLmdWriteBuffer(pLmdControl, (char *)pLmdControl->pMbsFileHeader,
		  sizeof(sMbsFileHeader));
  return(fLmdGetClose(pLmdControl));
}

//===============================================================
uint32_t fLmdConnectMbs(
sLmdControl *pLmdControl, 
char    *Nodename,
uint32_t iPort,
uint32_t *iBufferBytes){ // LMD__GET_EVENTS (NULL) or address to return buffer size

  int32_t stat;
  sMbsTransportInfo sMbs;

  if(iPort == 0) iPort=6000;
  memset(pLmdControl,0,sizeof(sLmdControl));
  pLmdControl->pTCP=(struct s_tcpcomm *)malloc(sizeof(struct s_tcpcomm));
  printf("fLmdConnectMbs: Connect to server %s port %d\n",Nodename,iPort);
  stat=f_stc_connectserver(Nodename,iPort,&pLmdControl->iTCP,pLmdControl->pTCP);
  if (stat != STC__SUCCESS) {
    printf ("fLmdConnectMbs: Error connect to %s \n",Nodename);
    fLmdCleanup(pLmdControl);
    return(LMD__FAILURE);
  }
  stat=f_stc_read((int32_t *)&sMbs, sizeof(sMbsTransportInfo),pLmdControl->iTCP,-1);
  if (stat != STC__SUCCESS) {
    printf ("fLmdConnectMbs: Error read info from %s \n",Nodename);
    fLmdCleanup(pLmdControl);
    return(LMD__FAILURE);
  }
  if(sMbs.iEndian != 1)pLmdControl->iSwap=1;
  if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)&sMbs,sizeof(sMbsTransportInfo)/4);
  if(sMbs.iBuffers > 1){
    printf("fLmdConnectMbs: Event spanning not supported!\n");
    fLmdCleanup(pLmdControl);
    return(LMD__FAILURE);
  }
  if(sMbs.iStreams > 0){
    printf("fLmdConnectMbs: MBS not in DABC mode!\n");
    fLmdCleanup(pLmdControl);
    return(LMD__FAILURE); 
  }
  strcpy(pLmdControl->cFile,Nodename);
  if(iBufferBytes == LMD__GET_EVENTS){ // use internal buffer for fLmdGetMbsEvent
    pLmdControl->pBuffer = (int16_t *) malloc(sMbs.iMaxBytes);
    pLmdControl->iBufferWords=sMbs.iMaxBytes/2;
    pLmdControl->iInternBuffer=1;
  } else
  *iBufferBytes=sMbs.iMaxBytes;
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdInitMbs(
sLmdControl *pLmdControl, 
char    *Nodename,
uint32_t iMaxBytes,
uint32_t iBuffers,
uint32_t iStreams){

  int32_t stat;

  if(iBuffers > 1){printf("fLmdInittMbs: Event spanning not supported!\n");return(LMD__FAILURE);}
  if(iStreams > 0){printf("fLmdInitMbs: MBS not in DABC mode!\n");return(LMD__FAILURE);}
  strcpy(pLmdControl->cFile,Nodename);
  if(pLmdControl->pBuffer == NULL)pLmdControl->pBuffer= (int16_t *) malloc(iMaxBytes);
  pLmdControl->iBufferWords=iMaxBytes/2;
  pLmdControl->iInternBuffer=1;
  pLmdControl->iTCP=pLmdControl->pTCP->socket;
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdCloseMbs(sLmdControl *pLmdControl){

  int32_t stat;
  stat=f_stc_close(pLmdControl->pTCP);
  fLmdCleanup(pLmdControl);
  return(stat);
}
//===============================================================
sMbsHeader* fLmdGetMbsEvent(sLmdControl *pLmdControl){
  uint32_t stat;
  sMbsHeader *pM;
  if(pLmdControl->iLeftWords == 0){ // get new buffer
    stat=fLmdGetMbsBuffer(pLmdControl,NULL,0,NULL,NULL);
    if(stat != LMD__SUCCESS){
      return(NULL);
    }
    // first event behind header:
    pLmdControl->pMbsHeader=(sMbsHeader *)(pLmdControl->pBuffer+sizeof(sMbsBufferHeader)/2);
  }
  pM=pLmdControl->pMbsHeader; // current to be returned
  pLmdControl->iLeftWords -= (pLmdControl->pMbsHeader->iWords+4);
  pLmdControl->pMbsHeader = 
           (sMbsHeader *)((int16_t *)pLmdControl->pMbsHeader +
           pLmdControl->pMbsHeader->iWords+4);
  pLmdControl->iElements++;
  return(pM);
}
//===============================================================
uint32_t fLmdGetMbsBuffer(
sLmdControl *pLmdControl, 
sMbsBufferHeader *pBuffer, 
uint32_t iBytes, 
uint32_t *iElements, 
uint32_t *iBytesUsed){

  sMbsHeader *pm;
  sMbsBufferHeader *pBuf;
  uint32_t *ps, *pd, i,ii, elem=0, size=0, usedBytes=0,leftBytes=0;
  int32_t iReturn;

  leftBytes=iBytes;
  pBuf=pBuffer;
  if(pBuf == NULL){
    pBuf=(sMbsBufferHeader *)pLmdControl->pBuffer; // internal buffer
    leftBytes=pLmdControl->iBufferWords*2; // size of this buffer
  }
  if(pBuf == NULL){
    printf("fLmdGetMbsBuffer: Need buffer to read\n");
    return(LMD__FAILURE);
  }
  if(leftBytes < sizeof(sMbsBufferHeader)){
    printf("fLmdGetMbsBuffer: %s buffer size %d too small for %d bytes\n",
     pLmdControl->cFile,leftBytes,sizeof(sMbsBufferHeader));
    return(LMD__FAILURE);
  }
  iReturn=f_stc_read((int32_t *)pBuf,sizeof(sMbsBufferHeader),pLmdControl->iTCP,-1);
  if(iReturn != STC__SUCCESS) return(LMD__FAILURE);
  if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pBuf,sizeof(sMbsBufferHeader)/4);
  if(leftBytes < (sizeof(sMbsBufferHeader)+2*pBuf->iMaxWords)){
    printf("fLmdGetMbsBuffer: %s buffer size %d too small for %d bytes\n",
     pLmdControl->cFile,leftBytes,sizeof(sMbsBufferHeader)+2*pBuf->iMaxWords);
    return(LMD__FAILURE);
  }
  usedBytes=pBuf->iUsedWords*2;
  if(usedBytes > 0){ // otherwise keep alive buffer
    if((pBuf->iType&0xffff) == 100)
      iReturn=f_stc_read((int32_t *)(pBuf+1),usedBytes,pLmdControl->iTCP,-1);
    if(iReturn != STC__SUCCESS) return(LMD__FAILURE);
    if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)(pBuf+1),usedBytes/4);
  }
  if(iBytesUsed != NULL)*iBytesUsed =usedBytes+sizeof(sMbsBufferHeader);
  if(iElements  != NULL)*iElements  =pBuf->iElements;
  pLmdControl->iBytes += usedBytes;
  pLmdControl->iLeftWords = usedBytes/2; // without header
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdGetOpen(
sLmdControl *pLmdControl, 
char    *Filename, 
sMbsFileHeader *pBuffHead, // LMD__INTERNAL_HEADER (NULL) or address of file header
uint32_t iBytes,           // LMD__NO_BUFFER (0) or LMD__MIN_BUFFER or internal buffersize
uint32_t iUseOffset){      // LMD__[NO_]INDEX

  int32_t iReturn;
  uint32_t l=0,i,bufferBytes=0,h[12];
  lmdoff_t to;

  memset(pLmdControl,0,sizeof(sLmdControl));
  if(pBuffHead == LMD__INTERNAL_HEADER){
    pLmdControl->pMbsFileHeader= (sMbsFileHeader *)malloc(sizeof(sMbsFileHeader));
    pLmdControl->iInternHeader=1;
  }
  else {
    pLmdControl->pMbsFileHeader= pBuffHead;
    pLmdControl->iInternHeader=0;
  }
  memset(pLmdControl->pMbsFileHeader,0,sizeof(sMbsFileHeader));

  // copy file name to control structure
  strcpy(pLmdControl->cFile,Filename);
  if((pLmdControl->fFile=(FILE *)fopen64(Filename,"r"))== NULL)
    {
      printf("fLmdGetOpen: File not found: %d\n",Filename);
      fLmdCleanup(pLmdControl);
      return(GETLMD__NOFILE);
    }
 /* read header */
  iReturn=fLmdReadBuffer(pLmdControl,
		    (char *)pLmdControl->pMbsFileHeader,
			 sizeof(sMbsFileHeader));
    if(iReturn!=sizeof(sMbsFileHeader)) {
    printf("fLmdGetOpen: LMD format error: no LMD file: %s\n",Filename);
    fLmdGetClose(pLmdControl);
    return(GETLMD__NOLMDFILE);
  }
  // check type and subtype, and endian
  if(pLmdControl->pMbsFileHeader->iEndian != 1) pLmdControl->iSwap=1;
  if(pLmdControl->iSwap){
    fLmdSwap4((uint32_t *)pLmdControl->pMbsFileHeader,sizeof(sMbsFileHeader)/4);
    fLmdSwap8((uint64_t *)&pLmdControl->pMbsFileHeader->iTableOffset,1);
  }
  if(pLmdControl->pMbsFileHeader->iType != LMD__TYPE_FILE_HEADER_101_1){
    printf("fLmdGetOpen: LMD format error: no LMD file: %s, type is %0x\n",
        Filename,pLmdControl->pMbsFileHeader->iType);
    fLmdGetClose(pLmdControl);
    return(GETLMD__NOLMDFILE);
  }

  if((iUseOffset == LMD__INDEX)&&(pLmdControl->pMbsFileHeader->iTableOffset > 0)){
    // printf("fLmdGetOpen: use table in file: %s\n",Filename);
    pLmdControl->iOffsetSize=pLmdControl->pMbsFileHeader->iOffsetSize;
    iReturn=fLmdOffsetRead(pLmdControl); // read offset table
    if(iReturn != LMD__SUCCESS){
    printf("fLmdGetOpen: Index format error: %s\n",Filename);
    fLmdGetClose(pLmdControl);
      return(iReturn);
    }
  }

  pLmdControl->iBytes+=iReturn;
  // more of header?
  if(pLmdControl->pMbsFileHeader->iUsedWords > 0)
    {
      // Read this additional information without swapping.
      // Could be mostly strings. Caller must know.
      pLmdControl->cHeader=malloc(pLmdControl->pMbsFileHeader->iUsedWords*2);
      iReturn=fLmdReadBuffer(pLmdControl,pLmdControl->cHeader,
			     pLmdControl->pMbsFileHeader->iUsedWords*2 );
      if(iReturn!=pLmdControl->pMbsFileHeader->iUsedWords*2) {
	printf("fLmdGetOpen: LMD format error: no LMD file: %s\n",Filename);
	fLmdGetClose(pLmdControl);
	return(GETLMD__NOLMDFILE);
      }
    }

  bufferBytes=iBytes;
  if(bufferBytes < pLmdControl->pMbsFileHeader->iMaxWords*2) 
    bufferBytes=pLmdControl->pMbsFileHeader->iMaxWords*2;
  printf("fLmdGetOpen: %s\n",Filename);
  fLmdPrintFileHeader(1,pLmdControl->pMbsFileHeader);
  pLmdControl->pBuffer=(int16_t *)malloc(bufferBytes);
  pLmdControl->iBufferWords=bufferBytes/2; // will be increased if necessary
  pLmdControl->iLeftWords=0; // buffer empty, read with first fLmdGetElement
  pLmdControl->pMbsHeader=(sMbsHeader *)pLmdControl->pBuffer;
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdGetBuffer(
sLmdControl *pLmdControl, 
sMbsHeader *pMbsHeader, 
uint32_t iBytes, 
uint32_t *iElements, 
uint32_t *iBytesUsed){

  sMbsHeader *pm;
  uint32_t *ps, *pd, i,ii, elem=0, size=0, leftBytes=0, used;
  int32_t iReturn;

  if(iBytes < pLmdControl->pMbsFileHeader->iMaxWords){
    printf("fLmdGetBuffer: %s buffer size %d too small for %d bytes\n",
     pLmdControl->cFile,iBytes,pLmdControl->pMbsFileHeader->iMaxWords);
    return(LMD__FAILURE);
  }
  if(pMbsHeader == NULL){
    printf("fLmdGetBuffer: Need buffer to read\n");
    return(LMD__FAILURE);
  }
  *iBytesUsed=0;
  *iElements=0;
  if(pLmdControl->iElements == pLmdControl->pMbsFileHeader->iElements) return(GETLMD__EOFILE);

  // Offset table
  if(pLmdControl->iOffsetEntries){ // use offsets to read elements fitting in buffer
    fLmdOffsetElements(pLmdControl,iBytes, &elem, &used);
    //printf("Read %d bytes of %d, elements %d\n",used,iBytes,elem);
    iReturn=fLmdReadBuffer(pLmdControl,(char *)pMbsHeader,used);
    if(iReturn <= 0){
      printf("fLmdGetBuffer: EOF: %s\n",pLmdControl->cFile);
      return(GETLMD__EOFILE);
    }
    if(iReturn!=used) {
      printf("fLmdGetBuffer: LMD read error: unexpected EOF: %s\n",pLmdControl->cFile);
      return(GETLMD__NOLMDFILE);
    }
    *iBytesUsed=used;
    *iElements=elem;
    if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pMbsHeader,iReturn/4);
    pLmdControl->iBytes+=iReturn;
    return(LMD__SUCCESS);
  }
  // no offset table
  // do we have fragment stored?
  leftBytes=(pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2;
  if(leftBytes>0) memcpy(pMbsHeader,pLmdControl->pBuffer,leftBytes);
  iReturn=fLmdReadBuffer(pLmdControl,(char *)pMbsHeader+leftBytes, iBytes-leftBytes);
  if(iReturn <= 0) {
    printf("fLmdGetBuffer: EOF: %s\n",pLmdControl->cFile);
    return(GETLMD__EOFILE);
  }
  if(iReturn!=(iBytes-leftBytes)) {
      printf("fLmdGetBuffer: LMD read error: unexpected EOF: %s\n",pLmdControl->cFile);
      return(GETLMD__NOLMDFILE);
    }
  if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pMbsHeader+leftBytes/4,iReturn/4);
  pLmdControl->iBytes+=iReturn;
  iReturn += leftBytes; // thats what is in the buffer
  // step through buffer to get number of elements and size
  pm=pMbsHeader;
  while(1){
    if(pm->iType == LMD__TYPE_FILE_INDEX_101_2) break; // file index is last
    *iBytesUsed += (pm->iWords+4)*2;
    *iElements += 1;
    pLmdControl->iElements++;
    pm = (sMbsHeader *)((uint16_t *)pm+pm->iWords+4);
    leftBytes=iReturn - *iBytesUsed;
    if(leftBytes == 0)break; // then pm invalid, behind buffer!!
    if((pm->iWords+4)*2 > leftBytes) break; // pm valid but fragment
  }
  //printf("Read %d bytes of %d, elements %d\n",*iBytesUsed,iBytes,*iElements);
  // fragment left? copy to internal buffer
  if(leftBytes>0){
    if(leftBytes > pLmdControl->iBufferWords*2){
      printf("fLmdGetBuffer: ERROR: internal buffer overflow. Needed:%d available:%d\n",
	     leftBytes,pLmdControl->iBufferWords*2);
      return(LMD__FAILURE);
    } else {
      memcpy(pLmdControl->pBuffer,pm,leftBytes);
    }
  }
  pLmdControl->iLeftWords=pLmdControl->iBufferWords-leftBytes/2;
  return(LMD__SUCCESS);
}
//===============================================================
sMbsHeader * fLmdGetElement(sLmdControl *pLmdControl, uint32_t iEvent){
  sMbsHeader *pM;
  uint32_t *ps, *pd, i;
  int32_t iReturn;

  if(iEvent == LMD__NO_INDEX){
    if(pLmdControl->pBuffer==NULL)return(NULL); // internal buffer needed
    if(pLmdControl->pMbsFileHeader->iElements==0)return(NULL);

  // if buffer is exactly exhausted, get next
  if(pLmdControl->iLeftWords==0){
    iReturn=fLmdReadBuffer(pLmdControl,
                        (char *)pLmdControl->pBuffer,
                        pLmdControl->iBufferWords*2);
    if(iReturn <= 0) {
      printf("fLmdGetElement: EOF\n");
      return(NULL);
    }
  if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pLmdControl->pBuffer,iReturn/4);
    pLmdControl->iBytes+=iReturn;
    pLmdControl->iBufferWords=iReturn/2; // in case last buffer was not filled fully
    pLmdControl->iLeftWords=iReturn/2;
    pLmdControl->pMbsHeader=(sMbsHeader *)pLmdControl->pBuffer;
  }
  // check if full event is in buffer (element size > remaining buffer space)
  // yes: decrement element counter, return address
  if(pLmdControl->pMbsHeader->iWords+4 <= pLmdControl->iLeftWords){
    pLmdControl->iLeftWords -= (pLmdControl->pMbsHeader->iWords+4);
  }
  else{
    // no: copy last element fragment to begin of bufffer, fill rest of buffer from file.
    //     decrement counter, return address.
    ps=(uint32_t *)pLmdControl->pMbsHeader;
    pd=(uint32_t *)pLmdControl->pBuffer;
    for(i=0;i<pLmdControl->iLeftWords/2;i++) *pd++ = *ps++;
    iReturn=fLmdReadBuffer(pLmdControl,
			   (char *)(pLmdControl->pBuffer+pLmdControl->iLeftWords),
			   (pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2);
    if(iReturn <= 0){printf("fLmdGetElement: EOF\n");return(NULL);}
    if(iReturn!=((pLmdControl->iBufferWords-pLmdControl->iLeftWords)*2)) {
      printf("fLmdGetBuffer: LMD read error: unexpected EOF: %s\n",pLmdControl->cFile);
      return(NULL);
    }
    if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)(pLmdControl->pBuffer+pLmdControl->iLeftWords),iReturn/4);
    pLmdControl->iBytes+=iReturn;
    pLmdControl->iBufferWords=pLmdControl->iLeftWords+iReturn/2; // in case last buffer was not filled fully
    pLmdControl->pMbsHeader=(sMbsHeader *)pLmdControl->pBuffer;
    pLmdControl->iLeftWords = pLmdControl->iBufferWords-(pLmdControl->pMbsHeader->iWords+4);
  }
  pLmdControl->pMbsFileHeader->iElements--;
  pM=pLmdControl->pMbsHeader;
  pLmdControl->pMbsHeader = 
           (sMbsHeader *)((int16_t *)pLmdControl->pMbsHeader +
           pLmdControl->pMbsHeader->iWords+4);
  pLmdControl->iElements++;
  return(pM);
  }
  // get indexed event
  if(pLmdControl->iOffsetEntries){
    if(iEvent >= pLmdControl->iOffsetEntries)return(NULL);
    fseeko64(pLmdControl->fFile,fLmdOffsetGet(pLmdControl,iEvent-1)*4,SEEK_SET);
    i=(fLmdOffsetGet(pLmdControl,iEvent)-fLmdOffsetGet(pLmdControl,iEvent-1));
    iReturn=fLmdReadBuffer(pLmdControl,(char *)pLmdControl->pBuffer,i*4);
    if(iReturn <= 0) {printf("fLmdGetElement: EOF\n");return(NULL);}
    if(iReturn!=(i*4)) {
      printf("fLmdGetBuffer: LMD read error: unexpected EOF: %s\n",pLmdControl->cFile);
      return(NULL);
    }
    if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pLmdControl->pBuffer,iReturn/4);
    pLmdControl->pMbsHeader=(sMbsHeader *)pLmdControl->pBuffer;
    if((pLmdControl->pMbsHeader->iWords+4) != i*2){
      printf("fLmdGetElement: Error Event %d: size from table is %d, header %d\n",
       iEvent,i/2,pLmdControl->pMbsHeader->iWords+4);
      return(NULL);
    }
    pLmdControl->iBytes+=iReturn;
    return(pLmdControl->pMbsHeader);
  }
  else return(NULL);
  // return zero if no more events
}
//===============================================================
uint32_t fLmdGetClose(sLmdControl *pLmdControl){
  fLmdCleanup(pLmdControl); // cleanup except fFile
  if(fclose(pLmdControl->fFile)!=0) {
      pLmdControl->fFile=NULL;
      return(LMD__CLOSE_ERR);
  }
  pLmdControl->fFile=NULL;
  return(LMD__SUCCESS);
}
//===============================================================
int32_t fLmdReadBuffer(sLmdControl *pLmdControl, char *buffer, uint32_t bytes){
  int32_t IObytes;
  IObytes=(int32_t)fread(buffer,1,bytes,pLmdControl->fFile);
  //if(IObytes < bytes) printf("Read %s: request %d bytes, got %d\n",pLmdControl->cFile,bytes,IObytes);
  return(IObytes);
}
//===============================================================
int32_t fLmdWriteBuffer(sLmdControl *pLmdControl, char *buffer, uint32_t bytes){
  int32_t IObytes;
  IObytes=(int32_t)fwrite(buffer,1,bytes,pLmdControl->fFile);
  //if(IObytes < bytes) printf("Write %s: request %d bytes, put %d\n",
  // pLmdControl->cFile,bytes,IObytes);
  return(IObytes);
}
//===============================================================
uint32_t fLmdCleanup(sLmdControl *pLmdControl){
// do not clean fFile
  if(pLmdControl->pTCP     != NULL)free(pLmdControl->pTCP);
  if(pLmdControl->cHeader  != NULL)free(pLmdControl->cHeader);
  if(pLmdControl->pOffset4 != NULL)free(pLmdControl->pOffset4);
  if(pLmdControl->pOffset8 != NULL)free(pLmdControl->pOffset8);
  if(pLmdControl->pBuffer  != NULL && pLmdControl->iInternBuffer)
                                   free(pLmdControl->pBuffer);
  if(pLmdControl->pMbsFileHeader != NULL && pLmdControl->iInternHeader)
                                   free(pLmdControl->pMbsFileHeader);
  pLmdControl->pTCP=NULL;
  pLmdControl->cHeader=NULL;
  pLmdControl->pBuffer=NULL;
  pLmdControl->pOffset4=NULL;
  pLmdControl->pOffset8=NULL;
  pLmdControl->pMbsFileHeader=NULL;
  pLmdControl->pMbsHeader=NULL;
  return (LMD__SUCCESS);
}
//===============================================================
sLmdControl * fLmdAllocateControl(){
  sLmdControl *x;
  x=(sLmdControl *)malloc(sizeof(sLmdControl));
  memset(x,0,sizeof(sLmdControl));
  return(x);
}
//===============================================================
void fLmdOffsetElements(sLmdControl *pLmdControl, 
uint32_t bytes, 
uint32_t *elements, 
uint32_t *used){
  lmdoff_t *off1,*off2;
  uint32_t elem=0,i,*iff1,*iff2;

  if(pLmdControl->iOffsetSize == 4){
    iff1=pLmdControl->pOffset4+pLmdControl->iElements;
    iff2=iff1;
    for(i=pLmdControl->iElements;i<pLmdControl->iOffsetEntries-1;i++){
      if((*(iff1+1)-*iff2)>bytes/4) break;
      iff1++;
      elem++;
      pLmdControl->iElements++;
    }
    *used=(*iff1-*iff2)*4;
    *elements=elem;
  }
  else if(pLmdControl->iOffsetSize == 8){
    off1=pLmdControl->pOffset8+pLmdControl->iElements;
    off2=off1;
    for(i=pLmdControl->iElements;i<pLmdControl->iOffsetEntries-1;i++){
      if((*(off1+1)-*off2)>bytes/4) break;
      off1++;
      elem++;
      pLmdControl->iElements++;
    }
    *used=(*off1-*off2)*4;
    *elements=elem;
  }
}
//===============================================================
uint32_t fLmdOffsetRead(sLmdControl *pLmdControl)
{
  int32_t iReturn;
  sMbsHeader *pTableHead;

  pTableHead=(sMbsHeader *)malloc(16); // header with 8 bytes data for future use.
  fseeko64(pLmdControl->fFile,(lmdoff_t)pLmdControl->pMbsFileHeader->iTableOffset*4,SEEK_SET);
  iReturn=fLmdReadBuffer(pLmdControl, (char *)pTableHead,16);
    if(iReturn!=16) {
      printf("fLmdGetBuffer: LMD read error: unexpected EOF: %s\n",pLmdControl->cFile);
      free(pTableHead);
      return(GETLMD__NOLMDFILE);
    }
    if(pLmdControl->iSwap)fLmdSwap4((uint32_t *)pTableHead,4);
  if(pTableHead->iType != LMD__TYPE_FILE_INDEX_101_2){
    printf("fLmdOffsetTable: LMD format error: no index table: %s, type %0x\n",
	   pLmdControl->cFile,pTableHead->iType);
    free(pTableHead);
    return(GETLMD__NOLMDFILE);
  }
  //printf("Table: words:%d type:%08x\n",pTableHead->iWords,pTableHead->iType);
  free(pTableHead);
  pLmdControl->iOffsetEntries=pLmdControl->pMbsFileHeader->iElements+1;
  pLmdControl->pOffset8=(lmdoff_t *)malloc(pLmdControl->iOffsetEntries*pLmdControl->iOffsetSize);
  iReturn=fLmdReadBuffer(pLmdControl,
			 (char *)pLmdControl->pOffset8,
			 pLmdControl->iOffsetEntries*pLmdControl->iOffsetSize);
  if(iReturn!=pLmdControl->iOffsetEntries*pLmdControl->iOffsetSize) {
    printf("fLmdOffsetTable: LMD format error: no index table: %s\n",pLmdControl->cFile);
    pLmdControl->iOffsetEntries=0;
    free(pTableHead);
    return(GETLMD__NOLMDFILE);
  }
  if(pLmdControl->iSwap){
    fLmdSwap4((uint32_t *)pLmdControl->pOffset8,iReturn/4);
    if(pLmdControl->iOffsetSize == 8)
    fLmdSwap8((uint64_t *)pLmdControl->pOffset8,iReturn/8);
  }
  // go back behing header
  fseeko64(pLmdControl->fFile,(lmdoff_t)sizeof(sMbsFileHeader),SEEK_SET);
  // use small table
  if(pLmdControl->iOffsetSize == 4){
    pLmdControl->pOffset4= (uint32_t *)pLmdControl->pOffset8;
    pLmdControl->pOffset8=NULL;
  }
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdOffsetWrite(sLmdControl *pLmdControl)
{
  int32_t iReturn;
  char *pbuf;
  lmdoff_t current;
  sMbsHeader *pTableHead;
  pTableHead=(sMbsHeader *)malloc(16); // header with 8 bytes data for future use.
  memset(pTableHead,0,16);
  pTableHead->iWords=(pLmdControl->iElements+1)*pLmdControl->iOffsetSize/2+4;
  pTableHead->iType=LMD__TYPE_FILE_INDEX_101_2;
/*   printf("Table: words:%d type:%08x offbytes:%d\n", */
/*     pTableHead->iWords,pTableHead->iType,pLmdControl->iOffsetSize); */
  iReturn=fgetpos64(pLmdControl->fFile,(lmdoff_t *)&current);
  iReturn=fLmdWriteBuffer(pLmdControl, (char *)pTableHead,16);
  free(pTableHead);
  pbuf=(char *)pLmdControl->pOffset4; // try short table
  if(pbuf == NULL) pbuf=(char *)pLmdControl->pOffset8;
  iReturn=fLmdWriteBuffer(pLmdControl, pbuf,
		  (pLmdControl->iElements+1)*pLmdControl->iOffsetSize);
  if(pLmdControl->pOffset8)
    pLmdControl->pMbsFileHeader->iTableOffset = *(pLmdControl->pOffset8+pLmdControl->iElements);
  if(pLmdControl->pOffset4)
    pLmdControl->pMbsFileHeader->iTableOffset = *(pLmdControl->pOffset4+pLmdControl->iElements);
  if(current/4 != pLmdControl->pMbsFileHeader->iTableOffset){
    printf("Table offset mismatch: current:%lld calculated:%lld, cur-cal %lld\n",
	   current/4,pLmdControl->pMbsFileHeader->iTableOffset,
	   current/4-pLmdControl->pMbsFileHeader->iTableOffset);
    return(LMD__FAILURE);
    }
  if(iReturn != (pLmdControl->iElements+1)*pLmdControl->iOffsetSize){
    printf("Table write error \n");
    return(LMD__FAILURE);
  }
  return(LMD__SUCCESS);
}
//===============================================================
uint32_t fLmdOffsetSet(sLmdControl *pLmdControl, uint32_t lwords)
{
  int32_t iReturn;
  if(pLmdControl->iElements >= pLmdControl->iOffsetEntries)fLmdOffsetResize(pLmdControl,0);
  if(pLmdControl->pOffset8){
    *(pLmdControl->pOffset8+pLmdControl->iElements)=
    *(pLmdControl->pOffset8+pLmdControl->iElements-1)+(lmdoff_t)lwords;
  }
  if(pLmdControl->pOffset4){
    *(pLmdControl->pOffset4+pLmdControl->iElements)=
    *(pLmdControl->pOffset4+pLmdControl->iElements-1)+lwords;
  }
  return(LMD__SUCCESS);
}
//===============================================================
lmdoff_t fLmdOffsetGet(sLmdControl *pLmdControl, uint32_t index){
  if(pLmdControl->pOffset8)
    return(*(pLmdControl->pOffset8+index));
  if(pLmdControl->pOffset4)
    return((lmdoff_t)*(pLmdControl->pOffset4+index));
}
//===============================================================
void fLmdOffsetResize(sLmdControl *pLmdControl, uint32_t firstValue){
  lmdoff_t *new;
  uint32_t oldEntries,newEntries;

  oldEntries=pLmdControl->iOffsetEntries;
  newEntries=oldEntries+OFFSET__ENTRIES;
  new=(lmdoff_t *)malloc(newEntries*pLmdControl->iOffsetSize);
  memset(new,0,newEntries*pLmdControl->iOffsetSize);
  if(oldEntries > 0){ //table was expanded
    //printf("Resize table %d to %d entries\n",oldEntries,newEntries);
    if(pLmdControl->pOffset8){
      memcpy(new,pLmdControl->pOffset8,oldEntries*pLmdControl->iOffsetSize);
      free(pLmdControl->pOffset8);
      pLmdControl->pOffset8=new;
    }
    if(pLmdControl->pOffset4){
      memcpy(new,pLmdControl->pOffset4,oldEntries*pLmdControl->iOffsetSize);
      free(pLmdControl->pOffset4);
      pLmdControl->pOffset4=(uint32_t *)new;
    }
  }
  else { // table was new
    //printf("Create table %d entries, first offset %d\n",newEntries,firstValue);
    if(pLmdControl->iOffsetSize==8){
      pLmdControl->pOffset8=new;
      *pLmdControl->pOffset8=(lmdoff_t)firstValue;
    }
    if(pLmdControl->iOffsetSize==4){
      pLmdControl->pOffset4=(uint32_t *)new;
      *pLmdControl->pOffset4=firstValue;
    }
  }
  pLmdControl->iOffsetEntries=newEntries;
}
//===============================================================
void fLmdPrintBufferHeader(uint32_t iVerbose, sMbsBufferHeader *pMbsBufferHeader)
{
  if(iVerbose){
    if(pMbsBufferHeader){
    printf("BfHd: # %d, DataWords:%d Type:%08x Elements:%d sec:%d.%d MaxWords:%d\n",
	   pMbsBufferHeader->iBuffer,
	   pMbsBufferHeader->iUsedWords,
	   pMbsBufferHeader->iType,
	   pMbsBufferHeader->iElements,
	   pMbsBufferHeader->iTimeSpecSec,
	   pMbsBufferHeader->iTimeSpecNanoSec/1000,
	   pMbsBufferHeader->iMaxWords);
}}}
//===============================================================
void fLmdPrintFileHeader(uint32_t iVerbose, sMbsFileHeader *pMbsFileHeader)
{
  if(iVerbose){
    if(pMbsFileHeader){
    printf("FiHd: DataWords:%d Type:%d.%d Elements:%d sec:%d.%d MaxWords:%d Index: %llx[%d]\n",
	   pMbsFileHeader->iUsedWords,
	   pMbsFileHeader->iType&0xffff,
	   pMbsFileHeader->iType>>16,
	   pMbsFileHeader->iElements,
	   pMbsFileHeader->iTimeSpecSec,
	   pMbsFileHeader->iTimeSpecNanoSec/1000,
	   pMbsFileHeader->iMaxWords,
	   pMbsFileHeader->iTableOffset,
	   pMbsFileHeader->iOffsetSize);
}}}
//===============================================================
void fLmdPrintHeader(uint32_t iVerbose, sMbsHeader *pMbsHeader)
{
  if(iVerbose){
    if(pMbsHeader){
    printf("ElHd: words:%d type:%08x\n",
	   pMbsHeader->iWords,
	   pMbsHeader->iType);
}}}
//===============================================================
void fLmdPrintEvent(uint32_t iVerbose, sMbsEventHeader *pMbsEventHeader)
{
  if(iVerbose){
    if(pMbsEventHeader){
    printf("EvHd: words:%6d type:%08x trigger:%2d #:%4d\n",
	   pMbsEventHeader->iWords,
	   pMbsEventHeader->iType,
	   pMbsEventHeader->iTrigger>>16,
	   pMbsEventHeader->iEventNumber);
}}}
//===============================================================
void fLmdPrintControl(uint32_t iVerbose, sLmdControl *pLmdControl)
{
  if(iVerbose){
    printf("Ctrl: file:%s words:%d left:%d bytes read:%lld elements:%d\n",
	   pLmdControl->cFile,
	   pLmdControl->iBufferWords,
	   pLmdControl->iLeftWords,
           pLmdControl->iBytes,
           pLmdControl->iElements
    );
    fLmdPrintFileHeader(iVerbose,pLmdControl->pMbsFileHeader);
    fLmdPrintEvent(iVerbose,(sMbsEventHeader *)pLmdControl->pMbsHeader);
}}
//===============================================================
void fLmdSwap4(uint32_t *array, uint32_t items){
  uint32_t i, *pp;
  pp=array;
  for(i=0;i<items/2;i++){
    //printf("Swap 4 %08x ",*pp);
     *pp = 
     (*pp >> 24) + 
    ((*pp >>  8)&0x0000ff00) +
    ((*pp <<  8)&0x00ff0000) +  
     (*pp << 24);
     //printf("to %08x \n",*pp);
     pp++;
  }}
//===============================================================
void fLmdSwap8(uint64_t *array, uint32_t items){
  uint64_t *pp;
  uint32_t i,x;
  pp=array;
  for(i=0;i<items;i++){
    //printf("Swap 8 %016llx ",*pp);
    *pp = (*pp << 32) + (*pp >> 32);
    //printf("to %016llx\n",*pp,*(pp+1));
     pp++;
  }}

