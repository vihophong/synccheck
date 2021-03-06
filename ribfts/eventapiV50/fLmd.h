#ifndef MbsLmdStruct
#define MbsLmdStruct
#define LMD__SUCCESS        0
#define LMD__FAILURE        1
#define LMD__CLOSE_ERR      3
#define GETLMD__NOFILE      2
#define GETLMD__NOLMDFILE   4
#define GETLMD__EOFILE      5
#define PUTLMD__FILE_EXIST  101
#define PUTLMD__TOOBIG      102
#define PUTLMD__OPEN_ERR    103
#define PUTLMD__EXCEED      104

typedef struct
{
  FILE    *fFile;         /* file descripter or server No.    */
  int16_t *pBuffer;       /* pointer to internal buffer  */
  uint32_t iBufferWords;  /* internal buffer size      */
  uint32_t iLeftWords;    /* left words in buffer */
  uint32_t iInternHeader; /* has intern allocated header buffer */
  uint32_t iInternBuffer; /* has intern allocated buffer */
  uint32_t iElements;     /* events since open     */
  uint64_t iBytes;        /* bytes since open  */
  char     cFile[512];    /* channel name */
  uint32_t iSwap;
  uint32_t iVerbose;
  char     *cHeader;      /* header data buffer */
  uint32_t *pOffset4;     /* offset table */
  lmdoff_t *pOffset8;     /* long offset table */
  lmdoff_t oTableOffset;  /* greater zero when Long offset in file */
  uint32_t iOffsetSize;   /* Offset size, 4 or 8 [bytes] */
  uint32_t iOffsetEntries;/* offset table length */
  sMbsFileHeader *pMbsFileHeader;
  sMbsHeader     *pMbsHeader;
  struct s_tcpcomm  *pTCP;
  uint32_t iTCP;
} sLmdControl;

sLmdControl * fLmdAllocateControl();
uint32_t   fLmdPutOpen(sLmdControl*,char*,sMbsFileHeader*,uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t   fLmdPutElement(sLmdControl*,sMbsHeader*);
uint32_t   fLmdPutBuffer(sLmdControl*, sMbsHeader*,uint32_t);
uint32_t   fLmdPutClose(sLmdControl*);
uint32_t   fLmdConnectMbs(sLmdControl*,char*,uint32_t,uint32_t*);
uint32_t   fLmdInitMbs(sLmdControl*,char*,uint32_t,uint32_t,uint32_t);
uint32_t   fLmdGetMbsBuffer(sLmdControl*,sMbsBufferHeader*,uint32_t,uint32_t*,uint32_t*);
sMbsHeader*fLmdGetMbsEvent(sLmdControl*);
uint32_t   fLmdCloseMbs(sLmdControl*);
uint32_t   fLmdGetOpen(sLmdControl*,char*,sMbsFileHeader*,uint32_t,uint32_t);
uint32_t   fLmdGetBuffer(sLmdControl*,sMbsHeader*,uint32_t,uint32_t*,uint32_t*);
int32_t    fLmdReadBuffer(sLmdControl*,char*,uint32_t);
sMbsHeader*fLmdGetElement(sLmdControl*,uint32_t);
uint32_t   fLmdGetClose(sLmdControl*);
void       fLmdPrintBufferHeader(uint32_t,sMbsBufferHeader*);
void       fLmdPrintFileHeader(uint32_t,sMbsFileHeader*);
void       fLmdPrintHeader(uint32_t,sMbsHeader*);
void       fLmdPrintEvent(uint32_t,sMbsEventHeader*);
void       fLmdPrintControl(uint32_t,sLmdControl*);
void       fLmdSwap4(uint32_t*,uint32_t);
void       fLmdSwap8(uint64_t*,uint32_t);
#endif
