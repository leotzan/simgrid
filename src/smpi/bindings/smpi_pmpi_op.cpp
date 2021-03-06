/* Copyright (c) 2007-2018. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "private.hpp"
#include "smpi_op.hpp"

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(smpi_pmpi);

/* PMPI User level calls */

int PMPI_Op_create(MPI_User_function * function, int commute, MPI_Op * op)
{
  if (function == nullptr || op == nullptr) {
    return MPI_ERR_ARG;
  } else {
    *op = new simgrid::smpi::Op(function, (commute!=0));
    return MPI_SUCCESS;
  }
}

int PMPI_Op_free(MPI_Op * op)
{
  if (op == nullptr) {
    return MPI_ERR_ARG;
  } else if (*op == MPI_OP_NULL) {
    return MPI_ERR_OP;
  } else {
    delete (*op);
    *op = MPI_OP_NULL;
    return MPI_SUCCESS;
  }
}

int PMPI_Op_commutative(MPI_Op op, int* commute){
  if (op == MPI_OP_NULL) {
    return MPI_ERR_OP;
  } else if (commute==nullptr){
    return MPI_ERR_ARG;
  } else {
    *commute = op->is_commutative();
    return MPI_SUCCESS;
  }
}

MPI_Op PMPI_Op_f2c(MPI_Fint op){
  return static_cast<MPI_Op>(simgrid::smpi::Op::f2c(op));
}

MPI_Fint PMPI_Op_c2f(MPI_Op op){
  return op->c2f();
}
