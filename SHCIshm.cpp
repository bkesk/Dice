#include "SHCIshm.h"
#include <boost/interprocess/managed_shared_memory.hpp>
#include "Determinants.h"
#include <vector>
#ifndef SERIAL
#include "mpi.h"
#endif

using namespace std;

boost::interprocess::shared_memory_object int2Segment;
boost::interprocess::mapped_region regionInt2;
boost::interprocess::shared_memory_object int2SHMSegment;
boost::interprocess::mapped_region regionInt2SHM;
boost::interprocess::shared_memory_object hHelpersSegment;
boost::interprocess::mapped_region regionHelpers;
string shciHelper;
boost::interprocess::shared_memory_object DetsCISegment;
boost::interprocess::mapped_region regionDetsCI;
std::string shciDetsCI;
boost::interprocess::shared_memory_object SortedDetsSegment;
boost::interprocess::mapped_region regionSortedDets;
std::string shciSortedDets;
boost::interprocess::shared_memory_object DavidsonSegment;
boost::interprocess::mapped_region regionDavidson;
std::string shciDavidson;
boost::interprocess::shared_memory_object cMaxSegment;
boost::interprocess::mapped_region regioncMax;
std::string shcicMax;
#ifndef SERIAL
MPI_Comm shmcomm;
#endif
int commrank, shmrank, localrank;
int commsize, shmsize, localsize;

void initSHM() {
#ifndef SERIAL
  MPI_Comm_rank(MPI_COMM_WORLD, &commrank);
  MPI_Comm_size(MPI_COMM_WORLD, &commsize);

  MPI_Comm localComm;
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
		      MPI_INFO_NULL, &localComm);
  MPI_Comm_rank(localComm, &localrank);
  MPI_Comm_size(localComm, &localsize);
  MPI_Comm_split(MPI_COMM_WORLD, localrank, commrank, &shmcomm);

  MPI_Comm_rank(shmcomm, &shmrank);
  MPI_Comm_size(shmcomm, &shmsize);

  MPI_Comm_free(&localComm);
#else
  commrank=0;shmrank=0;localrank=0;
  commsize=1;shmsize=1;localsize=1;
#endif

  //set up shared memory files to store the integrals
  string shciint2 = "SHCIint2" + to_string(static_cast<long long>(time(NULL) % 1000000));
  string shciint2shm = "SHCIint2shm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  shciHelper = "SHCIhelpershm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  shciDetsCI = "SHCIDetsCIshm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  shciSortedDets = "SHCISortedDetsshm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  shciDavidson = "SHCIDavidsonshm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  shcicMax = "SHCIcMaxshm" + to_string(static_cast<long long>(time(NULL) % 1000000));
  int2Segment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciint2.c_str(), boost::interprocess::read_write);
  int2SHMSegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciint2shm.c_str(), boost::interprocess::read_write);
  hHelpersSegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciHelper.c_str(), boost::interprocess::read_write);
  DetsCISegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciDetsCI.c_str(), boost::interprocess::read_write);
  SortedDetsSegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciSortedDets.c_str(), boost::interprocess::read_write);
  DavidsonSegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shciDavidson.c_str(), boost::interprocess::read_write);
  cMaxSegment = boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, shcicMax.c_str(), boost::interprocess::read_write);

}

void SHMVecFromMatrix(MatrixXx& vec, CItype* &SHMvec, std::string& SHMname,
		      boost::interprocess::shared_memory_object& SHMsegment,
		      boost::interprocess::mapped_region& SHMregion) {

  boost::interprocess::shared_memory_object::remove(SHMname.c_str());
  size_t totalMemory = 0;
  int comm_rank=0, comm_size=1;
#ifndef SERIAL
  MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
#endif
  
  if (comm_rank == 0) 
    totalMemory = vec.rows()*sizeof(CItype);
#ifndef SERIAL
  MPI_Bcast(&totalMemory, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
  
  
  SHMsegment.truncate(totalMemory);
  SHMregion = boost::interprocess::mapped_region{SHMsegment, boost::interprocess::read_write};
  memset(SHMregion.get_address(), 0., totalMemory);
  SHMvec = (CItype*)(SHMregion.get_address());
  
#ifndef SERIAL
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  if (comm_rank == 0) {
    for (size_t i=0; i<vec.rows(); i++) 
      SHMvec[i] = vec(i,0);
  }
#ifndef SERIAL
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  long intdim  = totalMemory;
  long maxint  = 26843540; //mpi cannot transfer more than these number of doubles
  long maxIter = intdim/maxint;
#ifndef SERIAL
  MPI_Barrier(MPI_COMM_WORLD);
  
  char* shrdMem = static_cast<char*>(SHMregion.get_address());
  for (int i=0; i<maxIter; i++) {
    MPI_Bcast  ( shrdMem+i*maxint,       maxint,                       MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
  }
  
  MPI_Bcast  ( shrdMem+(maxIter)*maxint, totalMemory - maxIter*maxint, MPI_CHAR, 0, MPI_COMM_WORLD);
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}