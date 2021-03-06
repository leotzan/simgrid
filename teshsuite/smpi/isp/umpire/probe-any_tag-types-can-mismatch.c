/* -*- Mode: C; -*- */
/* Creator: Bronis R. de Supinski (bronis@llnl.gov) Wed Oct 30 2002 */
/* probe-any_tag-types-can-mismatch.c -- do nondeterministic recv after */
/* Probe; types can mismatch as a result but may not; require that we */
/* detect the mismatch if occurs; OK to detect  (or not) if it doesn't... */


#include <stdio.h>
#include "mpi.h"


int
main (int argc, char **argv)
{
  int nprocs = -1;
  int rank = -1;
  char processor_name[128];
  int namelen = 128;
  int i, j;
  double x;
  MPI_Status status;
  MPI_Request req0, req1;

  /* init */
  MPI_Init (&argc, &argv);
  MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Get_processor_name (processor_name, &namelen);
  printf ("(%d) is alive on %s\n", rank, processor_name);
  fflush (stdout);

  MPI_Barrier (MPI_COMM_WORLD);

  /* because the probe and the receives both use */
  /* MPI_ANY_TAG, we are not guaranteed that the */
  /* first recv matches the probed msg, thus types can mismatch... */
  if (nprocs < 2)
    {
      printf ("not enough tasks\n");
    }
  else if (rank == 0)
    {
      i = 0;
      x = 1.0;

      MPI_Isend (&i, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &req0);

      MPI_Isend (&x, 1, MPI_DOUBLE, 1, 1, MPI_COMM_WORLD, &req1);

      MPI_Wait (&req1, &status);

      MPI_Wait (&req0, &status);
    }
  else if (rank == 1)
    {
      for (j = 0; j < 2; j++) {
	MPI_Probe (0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	if (status.MPI_TAG == 0) {
	  MPI_Recv (&i, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	  if (status.MPI_TAG != 0)
	    printf ("(%d) Type mismatch from matching other message\n", rank);
	}
	else {
	  MPI_Recv (&x, 1, MPI_DOUBLE, 0,
		    MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	  if (status.MPI_TAG == 0)
	    printf ("(%d) Type mismatch from matching other message\n", rank);
	}
      }
    }

  MPI_Barrier (MPI_COMM_WORLD);

  MPI_Finalize ();
  printf ("(%d) Finished normally\n", rank);
}

/* EOF */
