/*********************************************************************
 *  GPS_SC_DEF.H
 *
 *  Include file MUST be EQUAL on GPS-Server, GPS-Client and SBS-Monitor
 *
 *  02-Feb-1994, RSM: file renamed from M_PAW_SC.H and def. names modified
 *
 *********************************************************************
 *  10. 3.98, H.G: implemented in Linux
 *********************************************************************
 */

#define EB__BUFFER_DATA 131072
#define GPS__OUTBUFSIZ   16384                    /* client output buffer siz*/
                                                  /*    bytes                */
#define GPS__EVT_BUFSIZ  16384                    /* event buffer size for   */
                                                  /*    spanned evt in bytes */
#define GPS__MAXFLT         32
#define GPS__MAXFLTDESCR    16
#define GPS__MAXCLIENTS      6

#define GPS__CLNT_SNDFLT   404                    /* (GPS__MAXFLT * 3 + 
                                                   *  5LW) * 4    
                                                   */
                                                  /* length in bytes!        */
                        /* the filter is used in "s_client.h". For filter    */
                        /* and descriptor definition see "s_filter.h", for   */
                        /* length definitions "s_clntbuf.h", "s_clntoutbuf.h"*/

  
/* +++++++++++++++++++++++++++++++++++++ */
/* +++ environment of the TX partner +++ */
/* +++++++++++++++++++++++++++++++++++++ */
#define GPS__ENV_TESTBIT 0x00000001

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++ the setting of GPS__ENV_ENDIEN is implementation dependent!! ++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + */
/* little endian (bit0 on the right side)  DEC, INTEL machines   */
/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + */
#ifdef VMS          /* DEC: VMS, OpenVMS */
#define GPS__ENV_ENDIAN  0x0 
#endif

#ifdef ultrix       /* DEC Ultrix */
#define GPS__ENV_ENDIAN  0x0
#endif 

#ifdef Linux        /* Linux */
#define GPS__ENV_ENDIAN  0x0
#endif 

#ifdef GSI__WINNT       /* Windows NT */
#define GPS__ENV_ENDIAN  0x0
#endif 


/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + */
/* big endian  (bit0 on the left side)  IBM, HP, 68xxx...        */
/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + */
#ifdef _AIX         /* AIX */
#define GPS__ENV_ENDIAN  0xFFFFFFFF
#endif 

#ifdef HPUX         /* HP-UX */
#define GPS__ENV_ENDIAN  0xFFFFFFFF
#endif 

#ifdef Lynx         /* LynxOS */
#define GPS__ENV_ENDIAN  0xFFFFFFFF
#endif 

