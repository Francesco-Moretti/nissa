#ifndef _DEBUG_HPP
#define _DEBUG_HPP

#define CRASH(...) internal_CRASH(__LINE__,__FILE__,__VA_ARGS__)
#define crash_printing_error(code,...) internal_crash_printing_error(__LINE__,__FILE__,code,__VA_ARGS__)
#define decript_MPI_error(...) internal_decript_MPI_error(__LINE__,__FILE__,__VA_ARGS__)

namespace nissa
{
  double take_time();
  void internal_CRASH(int line,const char *file,const char *templ,...);
  void internal_crash_printing_error(int line,const char *file,int err_code,const char *templ,...);
  void internal_decript_MPI_error(int line,const char *file,int rc,const char *templ,...);
  void print_backtrace_list();
  void signal_handler(int);
}

#endif
