// Linux version of rate output of MBS
// output can be configured

#include "typedefs.h"
#include <stdio.h>
#include <sys/file.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <signal.h>

#include "sys_def.h"
#include "sbs_def.h"
#include "ml_def.h"
#include "s_setup.h"
#include "s_set_ml.h"
#include "s_set_mo.h"
#include "s_daqst.h"
#include "f_mbs_status.h"

INTS4 STS_VALUE;

CHARS *f_ut_time(CHARS *);

main(argc,argv)
     int argc;
     char *argv[];
{

INTS4 l_sec = 3, l_lines=100;
INTS4 l_i, l_j, l_c, l_status;
INTS4 l_n, memdev;
INTS4 l_perm, l_quiet=0;
s_setup  *ps_setup;
s_set_ml *ps_set_ml;
s_daqst  *ps_daqst;
struct    timeb now;

INTS4     l_dr_id = -1;
CHARS     c_node[64];

CHARU bh_node_type;
INTU4 bl_ev_buf_len, bl_file=0, bl_server=0, bl_mbs=0, bl_streams=0;
INTU4 bl_n_ev_buf=1;
INTU4 bl_bufstream_len;
CHARS *pc_temp, *pc_task, *pc_argv, *pc, *pc_line, c_sec[32], c_line[256],c_date[32],c_out[256],c_head0[256],c_head[256];
REAL8 r_size_kb, r_file_filled, r_file_filled_perc;

INTU4  bStreams_n=0,      bBuffers_n=0,        bEvents_n=0,       bData_n=0;
INTU4  bStreams_r=0,      bBuffers_r=0,        bEvents_r=0,       bData_r=0;
INTU4  bSrvStreams_n=0,   bSrvEvents_n=0,      bSrvBuffers_n=0,   bSrvData_n=0;
INTU4  bSrvStreams_r=0,   bSrvEvents_r=0,      bSrvBuffers_r=0,   bSrvData_r=0;
INTU4  bFilename=0,       bFileFilled=0,       bFileData=0,       bFileData_r=0, bFileIndex=0;
INTU4  bStreamsFree=0,    bStreamsFilled=0,    bStreamsSrv=0;

REAL8 r_old_time,       r_new_time,            r_diff_time;
REAL8 r_old_kb,         r_new_kb  = 0.,        r_rate_kb;
REAL8 r_old_buf,        r_new_buf = 0.,        r_rate_buf;
REAL8 r_old_evt,        r_new_evt = 0.,        r_rate_evt;
REAL8 r_old_stream,     r_new_stream = 0.,     r_rate_stream;
REAL8 r_old_tape_kb,    r_new_tape_kb = 0.,    r_rate_tape_kb;
REAL8 r_old_evsrv_evt,  r_new_evsrv_evt = 0.,  r_rate_evsrv_evt;
REAL8 r_old_evsrv_kb,   r_new_evsrv_kb = 0.,   r_rate_evsrv_kb;
REAL8 r_old_strsrv_kb,  r_new_strsrv_kb = 0.,  r_rate_strsrv_kb;
REAL8 r_old_strsrv_buf, r_new_strsrv_buf = 0., r_rate_strsrv_buf;
REAL8 r_old_strsrv_str, r_new_strsrv_str = 0., r_rate_strsrv_str;
REAL8 r_old_file_kb,    r_new_file_kb = 0.,    r_rate_file_kb;

INTS4 bl_n_stream=1,l_free_stream,l_trans_stream,l_serv_stream;

/*  analyse parameters given with this process     */
l_c = 1; // first argument
l_quiet = 0; /* output */
l_sec = 1;
 if(argc > 1)strcpy(c_node,argv[1]);

  strcpy(c_line," ");
  strcpy(c_head,"#");
  strcpy(c_head0,"#");
  // sum up all switches
  for(l_i=1;l_i<argc;l_i++){strcat(c_line,argv[l_i]);strcat(c_line," ");}
  // add RATEFLAGS
  pc_temp=(CHARS *)getenv("RATEFLAGS");
  if(pc_temp != NULL) strcat(c_line,pc_temp);
  // if nothing given use default
  if(strstr(c_line," -")==NULL)strcat(c_line,"-user");
  strcat(c_line," ");
  if(strstr(c_line,"-h")   != NULL)
    {
      printf("m_daq_rate MBS-node [switches] -t seconds\n");
      printf("All switches are additiv\n");
      printf("-user             # default selection\n")   ;
      printf("-n|r[s]bu[ffers]  # number|rate of [server] buffers\n")   ;
      printf("-n|r[s]ev[ents]   # number|rate of [server] events\n")    ;
      printf("-n|r[s]da[ta]     # number|rate of [server] data\n")      ;
      printf("-s|f|r|n|i|fi[le] # size|%|rate|name|index of output file\n")      ;
      printf("The following are compound switches:\n");
      printf("-u[ser]           # default. Must be set if others shall be added\n")   ;
      printf("-mbs              # mbs part\n")   ;
      printf("-fi[le]           # all file switches\n")   ;
      printf("-se[rver]         # all server switches\n")   ;
      printf("-ra[tes]          # all rates\n")   ;
      printf("RATEFLAGS         # value of this environment variable is added\n")   ;
      printf("Examples:\n")   ;
      printf("rate 5 -rfile -ifile\n")   ;
      printf("rate -user -t 5\n")   ;
      printf("rate -mbs -t 3 -rsdata\n")   ;
      printf("rate -rates\n")   ;
      exit(0);
    }
ps_daqst   = (s_daqst *) malloc (sizeof( s_daqst));
l_status = f_mbs_status (c_node,ps_daqst);

  pc=strstr(c_line,"-t");
  if(pc != NULL)
    {
      pc++;pc++;
      while(*pc==' ')pc++;
      if(*pc != '-')
	{
	  strcpy(c_sec,pc);
	  pc=c_sec;
	  while(*pc!=' ')pc++;
	  *pc=0;
	  l_sec=atoi(c_sec);
	  if(l_sec == 0) l_sec=1;
	}
    }
  if(strstr(c_line,"-nbu")  != NULL) bBuffers_n=1;
  if(strstr(c_line,"-rbu")  != NULL) bBuffers_r=1;
  if(strstr(c_line,"-nsbu") != NULL) bSrvBuffers_n=1;
  if(strstr(c_line,"-rsbu") != NULL) bSrvBuffers_r=1;
  if(strstr(c_line,"-nev")  != NULL) bEvents_n=1;
  if(strstr(c_line,"-rev")  != NULL) bEvents_r=1;
  if(strstr(c_line,"-nsev") != NULL) bSrvEvents_n=1;
  if(strstr(c_line,"-rsev") != NULL) bSrvEvents_r=1;
  if(strstr(c_line,"-nda")  != NULL) bData_n=1;
  if(strstr(c_line,"-rda")  != NULL) bData_r=1;
  if(strstr(c_line,"-nsda") != NULL) bSrvData_n=1;
  if(strstr(c_line,"-rsda") != NULL) bSrvData_r=1;
  if(strstr(c_line,"-sfi")  != NULL) bFileData=1;
  if(strstr(c_line,"-ffi")  != NULL) bFileFilled=1;
  if(strstr(c_line,"-rfi")  != NULL) bFileData_r=1;
  if(strstr(c_line,"-nfi")  != NULL) bFilename=1;
  if(strstr(c_line,"-ifi")  != NULL) bFileIndex=1;
  if(strstr(c_line,"-mbs")  != NULL)
    {
      printf("-mbs              # mbs part\n")   ;
      bBuffers_n=1;
      bBuffers_r=1;
      bEvents_n=1;
      bEvents_r=1;
      bData_n=1;
      bData_r=1;
    }
  if(strstr(c_line,"-u")     != NULL)
    {
      printf("-user             # default selection\n")   ;
      bEvents_n=1;
      bEvents_r=1;
      bData_n=1;
      bData_r=1;
      if(ps_daqst->bh_running[SYS__event_serv])bSrvEvents_r=1;
      if(ps_daqst->bh_running[SYS__stream_serv])bSrvData_r=1;
      bFileData_r=1;
      bFileIndex=1;
    }
  if(strstr(c_line,"-fi")     != NULL)
    {
      printf("-fi[le]           # all file switches\n")   ;
      bFilename=1;
      bFileData=1;
      bFileFilled=1;
      bFileData_r=1;
    }
  if(strstr(c_line,"-se")     != NULL)
    {
      printf("-se[rver]         # all server switches\n")   ;
      if(ps_daqst->bh_running[SYS__event_serv])
	{
          bSrvEvents_n=1;
          bSrvEvents_r=1;
	}
      if(ps_daqst->bh_running[SYS__stream_serv])
	{
          bSrvBuffers_n=1;
          bSrvBuffers_r=1;
          bSrvData_n=1;
          bSrvData_r=1;
	}
    }
  if(strstr(c_line,"-ra")     != NULL)
    {
      printf("-ra[tes]          # all rates\n")   ;
      bBuffers_r=1; 
      bEvents_r=1;
      bData_r=1;
      bFileData_r=1;
      if(ps_daqst->bh_running[SYS__event_serv])bSrvEvents_r=1;
      if(ps_daqst->bh_running[SYS__stream_serv])
        {
          bSrvBuffers_r=1;
          bSrvData_r=1;
        }
    }
  bl_ev_buf_len=ps_daqst->l_block_size;
  printf ("Node %s: interval %d sec. buffer size %d b.\n\n",c_node,l_sec,(INTS4) bl_ev_buf_len);

  // the following gives the order of the items in header lines
  // should of cause match with the data !
  bl_mbs=bData_n+bEvents_n+bBuffers_n+bStreams_n+bData_r+bEvents_r+bBuffers_r+bStreams_r;
  if(bl_mbs)
    {
      strcat(c_head0," Event building");
      if(bData_n)       {strcat(c_head,"   MB      ");strcat(c_head0,"           "); }
      if(bEvents_n)     {strcat(c_head,"   Events  ");strcat(c_head0,"           "); }
      if(bBuffers_n)    {strcat(c_head,"   Buffers ");strcat(c_head0,"           "); }
      if(bData_r)       {strcat(c_head," Kb/sec ");   strcat(c_head0,"        "); }
      if(bEvents_r)     {strcat(c_head," Ev/sec ");   strcat(c_head0,"        "); }
      if(bBuffers_r)    {strcat(c_head,"Buf/sec ");   strcat(c_head0,"        "); }
      c_head0[strlen(c_head0)-15]=0;
    }
  bl_server=bSrvData_n+bSrvEvents_n+bSrvBuffers_n+bSrvStreams_n+bSrvData_r+bSrvEvents_r+bSrvBuffers_r+bSrvStreams_r;
  if(bl_server)
    {
      strcat(c_head0,"| Server ");
      strcat(c_head,"|");
      if(bSrvData_n)    {strcat(c_head,"   MB      ");strcat(c_head0,"           "); }
      if(bSrvEvents_n)  {strcat(c_head,"  Events   ");strcat(c_head0,"           "); }
      if(bSrvBuffers_n) {strcat(c_head,"  Buffers  ");strcat(c_head0,"           "); }
      if(bSrvData_r)    {strcat(c_head," Kb/sec ");   strcat(c_head0,"        "); }
      if(bSrvEvents_r)  {strcat(c_head," Ev/sec ");   strcat(c_head0,"        "); }
      if(bSrvBuffers_r) {strcat(c_head,"Buf/sec ");   strcat(c_head0,"        "); }
      c_head0[strlen(c_head0)-8]=0;
    }
  bl_streams=bStreamsFree+bStreamsFilled+bStreamsSrv;
  if(bl_streams)
    {
      strcat(c_head0,"| Streams ");
      strcat(c_head,"|");
      if(bStreamsFree)  {strcat(c_head,"Empty ");strcat(c_head0,"      "); }
      c_head0[strlen(c_head0)-9]=0;
    }
  bl_file=bFileData+bFileFilled+bFileData_r+bFilename+bFileIndex;
  if(bl_file)
    {
      strcat(c_head0,"| File output ");
      strcat(c_head,"|");
      if(bFileData)     strcat(c_head,"    MB   ");
      if(bFileFilled)   strcat(c_head,"Filled  "); 
      if(bFileData_r)   strcat(c_head,"  Kb/sec ");
      if(bFileIndex)    strcat(c_head,"Index ");
      if(bFilename)     strcat(c_head,"Filename");
    }
if(l_sec <= 0) l_sec = 1;

r_old_time = 0.;
r_new_time = 0.;
/* size of stream in KB */
r_size_kb = (REAL8) (bl_ev_buf_len);

/* ===================================== loop  ================================= */
while(1)
{
r_old_buf        = r_new_buf;
r_old_evt        = r_new_evt;
r_old_stream     = r_new_stream;
r_old_evsrv_evt  = r_new_evsrv_evt;
r_old_evsrv_kb   = r_new_evsrv_kb;
r_old_tape_kb    = r_new_tape_kb;
r_old_file_kb    = r_new_file_kb;
r_old_strsrv_str = r_new_strsrv_str;
r_old_strsrv_buf = r_new_strsrv_buf;
r_old_strsrv_kb  = r_new_strsrv_kb;

r_old_time  = r_new_time;
ftime (&now);
l_status = f_mbs_status (c_node,ps_daqst);

r_new_buf        = (REAL8) ps_daqst->bl_n_buffers;
r_new_evt        = (REAL8) ps_daqst->bl_n_events;
r_new_stream     = (REAL8) ps_daqst->bl_n_bufstream;
r_new_evsrv_evt  = (REAL8) ps_daqst->bl_n_evserv_events;
r_new_evsrv_kb   = (REAL8) ps_daqst->bl_n_evserv_kbytes * 1.024;
r_new_tape_kb    = (REAL8) ps_daqst->bl_n_kbyte_tape * 1.024;
r_new_file_kb    = (REAL8) ps_daqst->bl_n_kbyte_file * 1.024;
r_new_strsrv_str = (REAL8) ps_daqst->bl_n_strserv_bufs;
r_new_strsrv_buf = (REAL8) ps_daqst->bl_n_strserv_bufs;
r_new_strsrv_kb  = (REAL8) ps_daqst->bl_n_strserv_kbytes * 1.024;

r_new_time  = (REAL8) now.time + (REAL8) now.millitm / 1000.;
r_diff_time = r_new_time - r_old_time;

r_rate_buf        = (r_new_buf        - r_old_buf)        /r_diff_time;
r_rate_kb         = r_rate_buf * r_size_kb /1000.;
r_rate_evt        = (r_new_evt        - r_old_evt)        /r_diff_time;
r_rate_stream     = (r_new_stream     - r_old_stream)     /r_diff_time;
r_rate_evsrv_evt  = (r_new_evsrv_evt  - r_old_evsrv_evt)  /r_diff_time;
r_rate_evsrv_kb   = (r_new_evsrv_kb   - r_old_evsrv_kb)   /r_diff_time;
r_rate_tape_kb    = (r_new_tape_kb    - r_old_tape_kb)    /r_diff_time;
r_rate_file_kb    = (r_new_file_kb    - r_old_file_kb)    /r_diff_time;
r_rate_strsrv_str = (r_new_strsrv_str - r_old_strsrv_str) /r_diff_time/(REAL8)bl_n_ev_buf;
r_rate_strsrv_buf = (r_new_strsrv_buf - r_old_strsrv_buf) /r_diff_time;
r_rate_strsrv_kb  = (r_new_strsrv_kb  - r_old_strsrv_kb)  /r_diff_time;
if(r_rate_file_kb < 0.0) r_rate_file_kb=0.0;

if(l_quiet == 0) 
{
  if(l_lines > 20) {printf("%s\n",c_head0);printf("%s\n",c_head);l_lines=0;}
  l_lines++;
  strcpy(c_out," ");
  if(bl_mbs){
    if(bData_n)       {sprintf(c_line,"%10.0f " ,r_new_buf*r_size_kb/1000000.);strcat(c_out,c_line);}
    if(bEvents_n)     {sprintf(c_line,"%10d " ,ps_daqst->bl_n_events);strcat(c_out,c_line);}
    if(bBuffers_n)    {sprintf(c_line,"%10d " ,ps_daqst->bl_n_buffers);strcat(c_out,c_line);}
    if(bData_r)       {sprintf(c_line,"%7.1f ",r_rate_kb);strcat(c_out,c_line);}
    if(bEvents_r)     {sprintf(c_line,"%7.0f ",r_rate_evt);strcat(c_out,c_line);}
    if(bBuffers_r)    {sprintf(c_line,"%7.0f ",r_rate_buf);strcat(c_out,c_line);}
  }
  if(bl_server){
    strcat(c_out,"|");
    if(bSrvData_n)    {sprintf(c_line,"%10.0f " ,r_new_strsrv_kb/1000.);strcat(c_out,c_line);}
    if(bSrvEvents_n)  {sprintf(c_line,"%10d " ,ps_daqst->bl_n_evserv_events);strcat(c_out,c_line);}
    if(bSrvBuffers_n) {sprintf(c_line,"%10d " ,ps_daqst->bl_n_strserv_bufs);strcat(c_out,c_line);}
    if(bSrvData_r)    {sprintf(c_line,"%7.1f ",r_rate_strsrv_kb);strcat(c_out,c_line);}
    if(bSrvEvents_r)  {sprintf(c_line,"%7.0f ",r_rate_evsrv_evt);strcat(c_out,c_line);}
    if(bSrvBuffers_r) {sprintf(c_line,"%7.0f ",r_rate_strsrv_buf);strcat(c_out,c_line);}
  }
  if(bl_streams){
    strcat(c_out,"|");
    if(bStreamsFree)  {sprintf(c_line,"%5d ",ps_daqst->l_free_streams);strcat(c_out,c_line);}
  }
  if(bl_file){
    strcat(c_out,"|");
    if(bFileData)     {sprintf(c_line,"%8.0f "      ,r_new_file_kb/1000.);strcat(c_out,c_line);}
    if(bFileFilled)   {sprintf(c_line,"%5.1f %% "  ,r_new_file_kb/(REAL8)ps_daqst->l_file_size*0.1);strcat(c_out,c_line);}
    if(bFileData_r)   {sprintf(c_line,"%8.1f "      ,r_rate_file_kb);strcat(c_out,c_line);}
    if(bFileIndex)    {sprintf(c_line," %04d "        ,ps_daqst->l_file_cur);strcat(c_out,c_line);}
    if(bFilename)     {sprintf(c_line,"%s"          ,ps_daqst->c_file_name);strcat(c_out,c_line);}
    if(ps_daqst->l_open_file)strcat(c_out," op");
    else                     strcat(c_out," cl");
  }
  printf("%s\n",c_out);
}
sleep (l_sec);
} // loop
}
