/* backtrace_linux - backtrace displaying on linux platform                 */
/* This file is included by ex.cpp on need (have execinfo.h, popen & addrline)*/

/* Copyright (c) 2008-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <sstream>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <unistd.h>
#include <execinfo.h>
#include <sys/stat.h>

/* This file is to be included in ex.cpp, so the following headers are not mandatory, but it's to make sure that eclipse see them too */
#include <xbt/string.hpp>
#include "xbt/ex.h"
#include "src/xbt/ex_interface.h"
#include "xbt/log.h"
#include "xbt/str.h"
#include "xbt/module.h"         /* xbt_binary_name */
#include "src/xbt_modinter.h"       /* backtrace initialization headers */
#if HAVE_MC
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif
/* end of "useless" inclusions */

extern char **environ;          /* the environment, as specified by the opengroup */

/* Module creation/destruction: nothing to do on linux */
void xbt_backtrace_preinit(void)
{
}

void xbt_backtrace_postexit(void)
{
}

#include <unwind.h>
struct trace_arg {
  void **array;
  int cnt, size;
};

static _Unwind_Reason_Code
backtrace_helper (struct _Unwind_Context *ctx, void *a)
{
  struct trace_arg *arg = (struct trace_arg *) a;

  /* We are first called with address in the __backtrace function.
     Skip it.  */
  if (arg->cnt != -1)
    {
      arg->array[arg->cnt] = (void *) _Unwind_GetIP(ctx);

      /* Check whether we make any progress.  */
      if (arg->cnt > 0 && arg->array[arg->cnt - 1] == arg->array[arg->cnt])
        return _URC_END_OF_STACK;
    }
  if (++arg->cnt == arg->size)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

/** @brief reimplementation of glibc backtrace based directly on gcc library, without implicit malloc
 *
 * See http://webloria.loria.fr/~quinson/blog/2012/0208/system_programming_fun_in_SimGrid/
 * for the motivation behind this function
 * */

int xbt_backtrace_no_malloc(void **array, int size) {
  int i = 0;
  for(i=0; i < size; i++)
    array[i] = NULL;

  struct trace_arg arg;
  arg .array = array;
  arg.size = size;
  arg.cnt = -1;

  if (size >= 1)
    _Unwind_Backtrace(backtrace_helper, &arg);

  /* _Unwind_Backtrace on IA-64 seems to put NULL address above
     _start.  Fix it up here.  */
  if (arg.cnt > 1 && arg.array[arg.cnt - 1] == NULL)
    --arg.cnt;
  return arg.cnt != -1 ? arg.cnt : 0;
}

size_t xbt_backtrace_current(xbt_backtrace_location_t* loc, std::size_t count)
{
  std::size_t used = backtrace(loc, count);
  if (used == 0) {
    std::fprintf(stderr, "The backtrace() function failed, which probably means that the memory is exhausted\n.");
    std::fprintf(stderr, "Bailing out now since there is nothing I can do without a decent amount of memory\n.");
    std::fprintf(stderr, "Please go fix the memleaks\n");
    std::exit(1);
  }
  return used;
}

namespace simgrid {
namespace xbt {

/** Find the path of the binary file from the name found in argv */
static std::string get_binary_path()
{
  struct stat stat_buf;

  // We found it, we are happy:
  if (stat(xbt_binary_name, &stat_buf) == 0)
    return xbt_binary_name;

  // Not found, look in the PATH:
  char* path = getenv("PATH");
  if (path == nullptr)
    return "";
  XBT_DEBUG("Looking in the PATH: %s\n", path);
  std::vector<std::string> path_list;
  // TODO, on Windows, this is ";"
  boost::split(path_list, path, boost::is_any_of(":"));
  for (std::string const& path_item : path_list) {
    std::string binary_name = simgrid::xbt::string_printf(
      "%s/%s", path_item.c_str(), xbt_binary_name);
    bool found = (stat(binary_name.c_str(), &stat_buf) == 0);
    XBT_DEBUG("Looked in the PATH for the binary. %s %s",
      found ? "Found" : "Not found",
      binary_name.c_str());
    if (found)
      return binary_name;
  }

  // Not found at all:
  return "";
}

//FIXME: This code could be greatly improved/simplifyied with
//   http://cairo.sourcearchive.com/documentation/1.9.4/backtrace-symbols_8c-source.html
std::vector<std::string> resolveBacktrace(
  xbt_backtrace_location_t* loc, std::size_t count)
{
  std::vector<std::string> result;

  /* no binary name, nothing to do */
  if (xbt_binary_name == NULL)
    return result;

  if (count == 0)
    return result;

  // Drop the first one:
  loc++; count--;

  char** backtrace_syms = backtrace_symbols(loc, count);
  std::string binary_name = get_binary_path();

  // Create the system command for add2line:
  std::ostringstream stream;
  stream << ADDR2LINE << " -f -e " << binary_name << ' ';
  std::vector<std::string> addrs(count);
  for (std::size_t i = 0; i < count; i++) {
    /* retrieve this address */
    XBT_DEBUG("Retrieving address number %zd from '%s'", i, backtrace_syms[i]);
    char buff[256];
    snprintf(buff, 256, "%s", strchr(backtrace_syms[i], '[') + 1);
    char* p = strchr(buff, ']');
    *p = '\0';
    if (strcmp(buff, "(nil)"))
      addrs[i] = buff;
    else
      addrs[i] = "0x0";
    XBT_DEBUG("Set up a new address: %zd, '%s'", i, addrs[i].c_str());
    /* Add it to the command line args */
    stream << addrs[i] << ' ';
  }
  std::string cmd = stream.str();

  /* size (in char) of pointers on this arch */
  int addr_len = addrs[0].size();

  XBT_VERB("Fire a first command: '%s'", cmd.c_str());
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    xbt_die("Cannot fork addr2line to display the backtrace");
  }

  /* To read the output of addr2line */
  char line_func[1024], line_pos[1024];
  for (std::size_t i = 0; i < count; i++) {
    XBT_DEBUG("Looking for symbol %zd, addr = '%s'", i, addrs[i].c_str());
    if (fgets(line_func, 1024, pipe)) {
      line_func[strlen(line_func) - 1] = '\0';
    } else {
      XBT_VERB("Cannot run fgets to look for symbol %zd, addr %s", i, addrs[i].c_str());
      strncpy(line_func, "???",3);
    }
    if (fgets(line_pos, 1024, pipe)) {
      line_pos[strlen(line_pos) - 1] = '\0';
    } else {
      XBT_VERB("Cannot run fgets to look for symbol %zd, addr %s", i, addrs[i].c_str());
      strncpy(line_pos, backtrace_syms[i],1024);
    }

    if (strcmp("??", line_func) != 0) {
      XBT_DEBUG("Found static symbol %s() at %s", line_func, line_pos);
      result.push_back(simgrid::xbt::string_printf(
        "%s() at %s", line_func, line_pos
      ));
    } else {
      /* Damn. The symbol is in a dynamic library. Let's get wild */

      char maps_buff[512];
      long int offset = 0;
      char *p, *p2;
      int found = 0;

      /* let's look for the offset of this library in our addressing space */
      char* maps_name = bprintf("/proc/%d/maps", (int) getpid());
      FILE* maps = fopen(maps_name, "r");

      long int addr = strtol(addrs[i].c_str(), &p, 16);
      if (*p != '\0') {
        XBT_CRITICAL("Cannot parse backtrace address '%s' (addr=%#lx)",
          addrs[i].c_str(), addr);
      }
      XBT_DEBUG("addr=%s (as string) =%#lx (as number)",
        addrs[i].c_str(), addr);

      while (!found) {
        long int first, last;

        if (fgets(maps_buff, 512, maps) == NULL)
          break;
        if (i == 0) {
          maps_buff[strlen(maps_buff) - 1] = '\0';
          XBT_DEBUG("map line: %s", maps_buff);
        }
        sscanf(maps_buff, "%lx", &first);
        p = strchr(maps_buff, '-') + 1;
        sscanf(p, "%lx", &last);
        if (first < addr && addr < last) {
          offset = first;
          found = 1;
        }
        if (found) {
          XBT_DEBUG("%#lx in [%#lx-%#lx]", addr, first, last);
          XBT_DEBUG("Symbol found, map lines not further displayed (even if looking for next ones)");
        }
      }
      fclose(maps);
      free(maps_name);
      addrs[i].clear();

      if (!found) {
        XBT_VERB("Problem while reading the maps file. Following backtrace will be mangled.");
        XBT_DEBUG("No dynamic. Static symbol: %s", backtrace_syms[i]);
        result.push_back(simgrid::xbt::string_printf("?? (%s)", backtrace_syms[i]));
        continue;
      }

      /* Ok, Found the offset of the maps line containing the searched symbol.
         We now need to substract this from the address we got from backtrace.
       */

      addrs[i] = simgrid::xbt::string_printf("0x%0*lx", addr_len - 2, addr - offset);
      XBT_DEBUG("offset=%#lx new addr=%s", offset, addrs[i].c_str());

      /* Got it. We have our new address. Let's get the library path and we are set */
      p = xbt_strdup(backtrace_syms[i]);
      if (p[0] == '[') {
        /* library path not displayed in the map file either... */
        free(p);
        snprintf(line_func,3, "??");
      } else {
        p2 = strrchr(p, '(');
        if (p2)
          *p2 = '\0';
        p2 = strrchr(p, ' ');
        if (p2)
          *p2 = '\0';

        /* Here we go, fire an addr2line up */
        char* subcmd = bprintf("%s -f -e %s %s", ADDR2LINE, p, addrs[i].c_str());
        free(p);
        XBT_VERB("Fire a new command: '%s'", subcmd);
        FILE* subpipe = popen(subcmd, "r");
        if (!subpipe) {
          xbt_die("Cannot fork addr2line to display the backtrace");
        }
        if (fgets(line_func, 1024, subpipe)) {
          line_func[strlen(line_func) - 1] = '\0';
        } else {
          XBT_VERB("Cannot read result of subcommand %s", subcmd);
          strncpy(line_func, "???",3);
        }
        if (fgets(line_pos, 1024, subpipe)) {
          line_pos[strlen(line_pos) - 1] = '\0';
        } else {
          XBT_VERB("Cannot read result of subcommand %s", subcmd);
          strncpy(line_pos, backtrace_syms[i],1024);
        }
        pclose(subpipe);
        free(subcmd);
      }

      /* check whether the trick worked */
      if (strcmp("??", line_func)) {
        XBT_DEBUG("Found dynamic symbol %s() at %s", line_func, line_pos);
        result.push_back(simgrid::xbt::string_printf(
          "%s() at %s", line_func, line_pos));
      } else {
        /* damn, nothing to do here. Let's print the raw address */
        XBT_DEBUG("Dynamic symbol not found. Raw address = %s", backtrace_syms[i]);
        result.push_back(simgrid::xbt::string_printf(
          "?? at %s", backtrace_syms[i]));
      }
    }
    addrs[i].clear();

    /* Mask the bottom of the stack */
    if (!strncmp("main", line_func, strlen("main")) ||
        !strncmp("xbt_thread_context_wrapper", line_func, strlen("xbt_thread_context_wrapper"))
        || !strncmp("smx_ctx_sysv_wrapper", line_func, strlen("smx_ctx_sysv_wrapper")))
      break;
  }
  pclose(pipe);
  free(backtrace_syms);
  return result;
}

}
}

#if HAVE_MC
int xbt_libunwind_backtrace(void** bt, int size){
  int i = 0;
  for(i=0; i < size; i++)
    bt[i] = NULL;

  i=0;

  unw_cursor_t c;
  unw_context_t uc;

  unw_getcontext (&uc);
  unw_init_local (&c, &uc);

  unw_word_t ip;

  unw_step(&c);

  while(unw_step(&c) >= 0 && i < size){
    unw_get_reg(&c, UNW_REG_IP, &ip);
    bt[i] = (void*)(long)ip;
    i++;
  }

  return i;
}
#endif