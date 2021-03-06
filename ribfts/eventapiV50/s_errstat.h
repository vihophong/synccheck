#include "typedefs.h"
/*  s_errstat.h
 *  ===========
 *  purpose        : structure for f_ut_error control
 *  date           : 22-Nov-1994
 *  author         : R.S. Mayer
 *
 *  update         : 12-Jan-1995: Message log flag /RSM
 *
 */
#ifndef __S_ERRSTAT__

   struct s_errstat
   {
      INTS4          if_verbose;
      INTS4          if_msglog;
      CHARS         c_taskname[16];
   };

#define __S_ERRSTAT__
#endif
