/* Copyright (c) 2010-2012, 2014-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef JEDULE_OUTPUT_H_
#define JEDULE_OUTPUT_H_

#include <stdio.h>
#include "simgrid_config.h"

#include "jedule_events.hpp"
#include "jedule_platform.hpp"

#if HAVE_JEDULE
SG_BEGIN_DECL()
extern xbt_dynar_t jedule_event_list;

void jedule_init_output(void);
void jedule_cleanup_output(void);
void jedule_store_event(jed_event_t event);
void write_jedule_output(FILE *file, jedule_t jedule, xbt_dynar_t event_list, xbt_dict_t meta_info_dict);
void print_key_value_dict(FILE *file, std::unordered_map<char*, char*> key_value_dict);

SG_END_DECL()
#endif

#endif /* JEDULE_OUTPUT_H_ */