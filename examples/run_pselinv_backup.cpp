/*
   Copyright (c) 2012 The Regents of the University of California,
   through Lawrence Berkeley National Laboratory.  

Authors: Lin Lin and Mathias Jacquelin

This file is part of PEXSI. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
(2) Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
(3) Neither the name of the University of California, Lawrence Berkeley
National Laboratory, U.S. Dept. of Energy nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

You are under no obligation whatsoever to provide any bug fixes, patches, or
upgrades to the features, functionality or performance of the source code
("Enhancements") to anyone; however, if you choose to make your Enhancements
available either publicly, or directly to Lawrence Berkeley National
Laboratory, without imposing a separate written license agreement for such
Enhancements, then you hereby grant the following license: a non-exclusive,
royalty-free perpetual license to install, use, modify, prepare derivative
works, incorporate into other computer software, distribute, and sublicense
such enhancements or derivative works thereof, in binary and source code form.
*/
/// @file run_pselinv.cpp
/// @brief Test for the interface of SuperLU_DIST and SelInv.
/// @date 2013-04-15
#include  "ppexsi.hpp"

#include "pexsi/timer.h"

#define _MYCOMPLEX_

#ifdef _MYCOMPLEX_
#define MYSCALAR Complex
#else
#define MYSCALAR Real
#endif


using namespace PEXSI;
using namespace std;



void Usage(){
  std::cout << "Usage" << std::endl << "run_pselinv -T [isText] -F [doFacto -E [doTriSolve] -Sinv [doSelInv]]  -H <Hfile> -S [Sfile] -colperm [colperm] -r [nprow] -c [npcol] -npsymbfact [npsymbfact] -P [maxpipelinedepth] -SinvBcast [doSelInvBcast] -SinvPipeline [doSelInvPipeline] -SinvHybrid [doSelInvHybrid] -rshift [real shift] -ishift [imaginary shift] -ToDist [doToDist] -Diag [doDiag] -SS [symmetricStorage]" << std::endl;
}

int main(int argc, char **argv) 
{

  if( argc < 3 ) {
    Usage();
    return 0;
  }

#if defined(PROFILE) || defined(PMPI)
  TAU_PROFILE_INIT(argc, argv);
#endif

  MPI_Init( &argc, &argv );


  int mpirank, mpisize;
  MPI_Comm_rank( MPI_COMM_WORLD, &mpirank );
  MPI_Comm_size( MPI_COMM_WORLD, &mpisize );

  try{
    MPI_Comm world_comm;

    // *********************************************************************
    // Input parameter
    // *********************************************************************
    std::map<std::string,std::string> options;

    OptionsCreate(argc, argv, options);

    // Default processor number
    Int nprow = 1;
    Int npcol = mpisize;

    if( options.find("-r") != options.end() ){
      if( options.find("-c") != options.end() ){
        nprow= atoi(options["-r"].c_str());
        npcol= atoi(options["-c"].c_str());
        if(nprow*npcol > mpisize){
          ErrorHandling("The number of used processors cannot be higher than the total number of available processors." );
        } 
      }
      else{
        ErrorHandling( "When using -r option, -c also needs to be provided." );
      }
    }
    else if( options.find("-c") != options.end() ){
      if( options.find("-r") != options.end() ){
        nprow= atoi(options["-r"].c_str());
        npcol= atoi(options["-c"].c_str());
        if(nprow*npcol > mpisize){
          ErrorHandling("The number of used processors cannot be higher than the total number of available processors." );
        } 
      }
      else{
        ErrorHandling( "When using -c option, -r also needs to be provided." );
      }
    }

    //Create a communicator with npcol*nprow processors
    //这里将processor分为两部分：在nprow * npcol内和外的，并且用world_comm表示新的COMM
    MPI_Comm_split(MPI_COMM_WORLD, mpirank<nprow*npcol, mpirank, &world_comm);

    if (mpirank<nprow*npcol){
      //这里得到了新的rank和size
      MPI_Comm_rank(world_comm, &mpirank );
      MPI_Comm_size(world_comm, &mpisize );
#if defined (PROFILE) || defined(PMPI) || defined(USE_TAU)
      TAU_PROFILE_SET_CONTEXT(world_comm);
#endif
      //打开每个processor的日至文件
      stringstream  ss;
      ss << "logTest" << mpirank;
      statusOFS.open( ss.str().c_str() );

      ///#if defined(COMM_PROFILE) || defined(COMM_PROFILE_BCAST)
      ///      stringstream  ss3;
      ///      ss3 << "comm_stat" << mpirank;
      ///      commOFS.open( ss3.str().c_str());
      ///#endif


      //if( mpisize != nprow * npcol || nprow != npcol ){
      //  ErrorHandling( "nprow == npcol is assumed in this test routine." );
      //}

      if( mpirank == 0 )
        cout << "nprow = " << nprow << ", npcol = " << npcol << endl;

      std::string Hfile, Sfile;
      int isCSC = true;
      if( options.find("-T") != options.end() ){
        //-T表示txt形式的CSC文件 
        isCSC= ! atoi(options["-T"].c_str());
      }

      int checkAccuracy = false;
      if( options.find("-E") != options.end() ){ 
        //-E还不清楚
        //好像是检查一下LU分解的正确性
        checkAccuracy= atoi(options["-E"].c_str());
      }


      int doSelInv = 1;
      if( options.find("-Sinv") != options.end() ){ 
        //为什么可以大于等于1还不清楚
        doSelInv= atoi(options["-Sinv"].c_str());
      }


      int doFacto = 1;
      if( options.find("-F") != options.end() ){ 
        //同上
        doFacto= atoi(options["-F"].c_str());
      }

      int doSymbolic = 1;
      if( options.find("-Symb") != options.end() ){ 
        //同上
        doSymbolic= atoi(options["-Symb"].c_str());
      }

      int doConvert = 1;
      if( options.find("-C") != options.end() ){ 
        //同上
        doConvert= atoi(options["-C"].c_str());
      }

      int doDistribute = 1;
      if( options.find("-D") != options.end() ){ 
        //同上
        doDistribute= atoi(options["-D"].c_str());
      }

      Int doConstructPattern = 1;
      if( options.find("-Pattern") != options.end() ){ 
        doConstructPattern = atoi(options["-Pattern"].c_str());
      }

      Int doPreSelinv = 1;
      if( options.find("-PreSelinv") != options.end() ){ 
        //同上
        doPreSelinv = atoi(options["-PreSelinv"].c_str());
      }



      if(doSelInv){
        doConvert=1;
      }

      if(doConvert){
        doFacto=1;
      }

      if(doFacto){
        doDistribute=1;
      }

      if(doDistribute){
        doSymbolic = 1;
      }











      int doToDist = true;
      if( options.find("-ToDist") != options.end() ){ 
        //?
        doToDist= atoi(options["-ToDist"].c_str());
      }

      int doDiag = false;
      if( options.find("-Diag") != options.end() ){ 
        //?
        doDiag = atoi(options["-Diag"].c_str());
      }


      if( options.find("-H") != options.end() ){ 
        //输入H矩阵的路径
        Hfile = options["-H"];
      }
      else{
        ErrorHandling("Hfile must be provided.");
      }

      if( options.find("-S") != options.end() ){ 
        //输入S矩阵的路径
        Sfile = options["-S"];
      }
      else{
        statusOFS << "-S option is not given. " 
          << "Treat the overlap matrix as an identity matrix." 
          << std::endl << std::endl;
      }

      Int maxPipelineDepth = -1;
      if( options.find("-P") != options.end() ){ 
        //?
        maxPipelineDepth = atoi(options["-P"].c_str());
      }
      else{
        statusOFS << "-P option is not given. " 
          << "Do not limit SelInv pipelining depth." 
          << std::endl << std::endl;
      }

      Int symmetricStorage = 0;
      if( options.find("-SS") != options.end() ){ 
        //?
        symmetricStorage = atoi(options["-SS"].c_str());
      }
      else{
        statusOFS << "-SS option is not given. " 
          << "Do not use symmetric storage." 
          << std::endl << std::endl;
      }





      Int numProcSymbFact;
      if( options.find("-npsymbfact") != options.end() ){ 
        //?
        numProcSymbFact = atoi( options["-npsymbfact"].c_str() );
      }
      else{
        statusOFS << "-npsymbfact option is not given. " 
          << "Use default value (maximum number of procs)." 
          << std::endl << std::endl;
        numProcSymbFact = 0;
      }

      //rshift和shift为最后组成z这个复数的两部分
      Real rshift = 0.0, ishift = 0.0;
      if( options.find("-rshift") != options.end() ){ 
        //?
        rshift = atof(options["-rshift"].c_str());
      }
      if( options.find("-ishift") != options.end() ){ 
        //?
        ishift = atof(options["-ishift"].c_str());
      }


      std::string ColPerm;
      if( options.find("-colperm") != options.end() ){ 
        //这是用在列进行置换的东西，用来加强LU的sparsity pattern
        ColPerm = options["-colperm"];
      }
      else{
        statusOFS << "-colperm option is not given. " 
          << "Use MMD_AT_PLUS_A." 
          << std::endl << std::endl;
        ColPerm = "MMD_AT_PLUS_A";
      }

      // *********************************************************************
      // Read input matrix
      // *********************************************************************

      // Setup grid.
      SuperLUGrid<MYSCALAR> g( world_comm, nprow, npcol );

      int      m, n;
      DistSparseMatrix<MYSCALAR>  AMat;

      DistSparseMatrix<Real> HMat;
      DistSparseMatrix<Real> SMat;
      Real timeSta, timeEnd;
      GetTime( timeSta );
      if(isCSC)
        ParaReadDistSparseMatrix( Hfile.c_str(), HMat, world_comm );  
      else{
        ReadDistSparseMatrixFormatted( Hfile.c_str(), HMat, world_comm ); 
        ParaWriteDistSparseMatrix( "H.csc", HMat, world_comm ); 
      }

      if( Sfile.empty() ){
        // Set the size to be zero.  This will tell PPEXSI.Solve to treat
        // the overlap matrix as an identity matrix implicitly.
        SMat.size = 0;  
      }
      else{
        if(isCSC)
          ParaReadDistSparseMatrix( Sfile.c_str(), SMat, world_comm ); 
        else{
          ReadDistSparseMatrixFormatted( Sfile.c_str(), SMat, world_comm ); 
          ParaWriteDistSparseMatrix( "S.csc", SMat, world_comm ); 
        }
      }

      GetTime( timeEnd );
      LongInt nnzH = HMat.Nnz();
      if( mpirank == 0 ){
        cout << "Time for reading H and S is " << timeEnd - timeSta << endl;
        cout << "H.size = " << HMat.size << endl;
        cout << "H.nnz  = " << nnzH  << endl;
      }

      // Get the diagonal indices for H and save it n diagIdxLocal_
      //虽然不是很懂这一步是为啥，但是还是跟着看吧
      //好像是用来在后面减去zshift的时候要减对角线的值用的
      std::vector<Int>  diagIdxLocal;
      { 
        Int numColLocal      = HMat.colptrLocal.m() - 1; //本地包含的column的数量
        Int numColLocalFirst = HMat.size / mpisize;
        Int firstCol         = mpirank * numColLocalFirst;//得到本processor的第一个列的序号

        diagIdxLocal.clear();

        for( Int j = 0; j < numColLocal; j++ ){
          Int jcol = firstCol + j + 1;
          for( Int i = HMat.colptrLocal(j)-1; 
              i < HMat.colptrLocal(j+1)-1; i++ ){
            Int irow = HMat.rowindLocal(i);
            if( irow == jcol ){
              diagIdxLocal.push_back( i );//把所有这个processor保存的矩阵在原来矩阵中为对角线的本地坐标保留了下来
            }
          }
        } // for (j)
      }


      GetTime( timeSta );

      AMat.size          = HMat.size;
      AMat.nnz           = HMat.nnz;
      AMat.nnzLocal      = HMat.nnzLocal;
      AMat.colptrLocal   = HMat.colptrLocal;
      AMat.rowindLocal   = HMat.rowindLocal;
      AMat.nzvalLocal.Resize( HMat.nnzLocal );
      AMat.comm = world_comm;

      MYSCALAR *ptr0 = AMat.nzvalLocal.Data();
      Real *ptr1 = HMat.nzvalLocal.Data();
      Real *ptr2 = SMat.nzvalLocal.Data();

#ifdef _MYCOMPLEX_
      Complex zshift = Complex(rshift, ishift);
#else
      Real zshift = Real(rshift);
#endif

      if( SMat.size != 0 ){
        // S is not an identity matrix
        for( Int i = 0; i < HMat.nnzLocal; i++ ){
          AMat.nzvalLocal(i) = HMat.nzvalLocal(i) - zshift * SMat.nzvalLocal(i);
        }
      }
      else{
        // S is an identity matrix
        for( Int i = 0; i < HMat.nnzLocal; i++ ){
          AMat.nzvalLocal(i) = HMat.nzvalLocal(i);
        }

        for( Int i = 0; i < diagIdxLocal.size(); i++ ){
          AMat.nzvalLocal( diagIdxLocal[i] ) -= zshift;
        }
      } // if (SMat.size != 0 )

      LongInt nnzA = AMat.Nnz();
      if( mpirank == 0 ){
        cout << "nonzero in A (DistSparseMatrix format) = " << nnzA << endl;
      }


      GetTime( timeEnd );
      if( mpirank == 0 )
        cout << "Time for constructing the matrix A is " << timeEnd - timeSta << endl;

      //目前为止就得到了A矩阵的所有内容
      // *********************************************************************
      // Symbolic factorization ：找到关于矩阵A的LU分解中的sparsity pattern，从而对supernode进行了分析
      // *********************************************************************
    

      GetTime( timeSta );
      SuperLUOptions luOpt;//一个传递Super_LU的接口
      luOpt.ColPerm = ColPerm;
      luOpt.numProcSymbFact = numProcSymbFact;


      SuperLUMatrix<MYSCALAR> luMat(g, luOpt );//设置初始化SuperLU Matrix，用来存储和Super_LU相关的数据


      luMat.DistSparseMatrixToSuperMatrixNRloc( AMat, luOpt );//将CSC格式转化为SuperLU的CSR压缩格式，结果保留在luMat的SuperMatrix中
      GetTime( timeEnd );
      if( mpirank == 0 )
        cout << "Time for converting to SuperLU format is " << timeEnd - timeSta << endl;

      if(doSymbolic){   
        GetTime( timeSta );
        luMat.SymbolicFactorize();//进行symbolic factorization，应该是只修改了Glu_persist_t，保留了这个列矩阵的supernode划分信息
        luMat.DestroyAOnly();//消除矩阵A
        GetTime( timeEnd );

        if( mpirank == 0 )
          cout << "Time for performing the symbolic factorization is " << timeEnd - timeSta << endl;
      }

      // *********************************************************************
      // Numerical factorization only : LU分解
      // *********************************************************************

      Real timeTotalFactorizationSta, timeTotalFactorizationEnd; 


      // Important: the distribution in pzsymbfact is going to mess up the
      // A matrix.  Recompute the matrix A here.
      //之前消除了无用的A矩阵，现在重新生成一下下
      luMat.DistSparseMatrixToSuperMatrixNRloc( AMat ,luOpt);

      GetTime( timeTotalFactorizationSta );

      if(doDistribute){
        GetTime( timeSta );
        luMat.Distribute();//再次并行分发数据，为numerical factorization做准备，说实话不是很懂
        GetTime( timeEnd );
        if( mpirank == 0 )
          cout << "Time for distribution is " << timeEnd - timeSta << " sec" << endl; 
      }


      if(doFacto){
        GetTime( timeSta );
        luMat.NumericalFactorize();//进行LU分解，不知道为什么要写成Numerical Factorization，结果记录在LUStruct中了
        GetTime( timeEnd );

        if( mpirank == 0 )
          cout << "Time for factorization is " << timeEnd - timeSta << " sec" << endl; 

        GetTime( timeTotalFactorizationEnd );
        if( mpirank == 0 )
          cout << "Time for total factorization is " << timeTotalFactorizationEnd - timeTotalFactorizationSta<< " sec" << endl; 


        // *********************************************************************
        // Test the accuracy of factorization by solve
        // *********************************************************************
        //这一个检查就暂时不看了，看不懂
        if( checkAccuracy ) {
          SuperLUMatrix<MYSCALAR> A1( g, luOpt );
          SuperLUMatrix<MYSCALAR> GA( g, luOpt );
          A1.DistSparseMatrixToSuperMatrixNRloc( AMat,luOpt );
          A1.ConvertNRlocToNC( GA );

          int n = A1.n();
          int nrhs = 5;
          NumMat<MYSCALAR> xTrueGlobal(n, nrhs), bGlobal(n, nrhs);
          NumMat<MYSCALAR> xTrueLocal, bLocal;
          DblNumVec berr;
          UniformRandom( xTrueGlobal );

          GA.MultiplyGlobalMultiVector( xTrueGlobal, bGlobal );

          A1.DistributeGlobalMultiVector( xTrueGlobal, xTrueLocal );
          A1.DistributeGlobalMultiVector( bGlobal,     bLocal );

          luMat.SolveDistMultiVector( bLocal, berr );
          luMat.CheckErrorDistMultiVector( bLocal, xTrueLocal );
        }

        // *********************************************************************
        // Selected inversion
        // *********************************************************************

        if(doConvert || doSelInv>=1)
        {
          Real timeTotalSelInvSta, timeTotalSelInvEnd;

          NumVec<MYSCALAR> diag;
          PMatrix<MYSCALAR> * PMlocPtr;
          SuperNodeType * superPtr;
          GridType * g1Ptr;

          GetTime( timeTotalSelInvSta );

          g1Ptr = new GridType( world_comm, nprow, npcol );//重点来了，这里申请了一个nprow * npcol的网格，可能是supernode划分了
          GridType &g1 = *g1Ptr;

          superPtr = new SuperNodeType();
          SuperNodeType & super = *superPtr;

          GetTime( timeSta );
          luMat.SymbolicToSuperNode( super ); //根据将它的supernode信息转移一下

          PSelInvOptions selInvOpt;//一些PSelInv的设置，不懂
          selInvOpt.maxPipelineDepth = maxPipelineDepth;
          selInvOpt.symmetricStorage = symmetricStorage;

          FactorizationOptions factOpt;
          factOpt.ColPerm = ColPerm;//一些factorization的设置，不懂
          PMlocPtr = new PMatrix<MYSCALAR>( &g1, &super, &selInvOpt, &factOpt  );//PSelInv的主要数据结构
          PMatrix<MYSCALAR> & PMloc = *PMlocPtr;

          if(doConvert){
            luMat.LUstructToPMatrix( PMloc );//把LU也放进来了？这个意思吧，应该是=。=
            GetTime( timeEnd );
          }

          LongInt nnzLU = PMloc.Nnz();
          if( mpirank == 0 ){
            cout << "nonzero in L+U  (PMatrix format) = " << nnzLU << endl;
          }

          if(doConvert){
            if( mpirank == 0 )
              cout << "Time for converting LUstruct to PMatrix is " << timeEnd  - timeSta << endl;
          }

          if(doSelInv>1){
            // Preparation for the selected inversion
            GetTime( timeSta );
            PMloc.ConstructCommunicationPattern();
            GetTime( timeEnd );


            if( mpirank == 0 )
              cout << "Time for constructing the communication pattern is " << timeEnd  - timeSta << endl;

            double timeTotalOffsetSta = 0;
            GetTime( timeTotalOffsetSta );

            PMatrix<MYSCALAR> PMlocIt = PMloc;
            for(int i=1; i<= doSelInv; ++i )
            {
              PMlocIt.CopyLU(PMloc);

              double timeTotalOffsetEnd = 0;
              GetTime( timeTotalOffsetEnd );

              GetTime( timeSta );
              PMlocIt.PreSelInv();
              GetTime( timeEnd );

              if( mpirank == 0 )
                cout << "Time for pre-selected inversion is " << timeEnd  - timeSta << endl;

              // Main subroutine for selected inversion
              GetTime( timeSta );
              PMlocIt.SelInv();
              GetTime( timeEnd );
              if( mpirank == 0 )
                cout << "Time for numerical selected inversion is " << timeEnd  - timeSta << endl;

              GetTime( timeTotalSelInvEnd );
              timeTotalSelInvEnd -= (timeTotalOffsetEnd - timeTotalOffsetSta);
              if( mpirank == 0 )
                cout << "Time for total selected inversion is " << timeTotalSelInvEnd  - timeTotalSelInvSta << endl;



              if(doToDist){



                ////// Convert to DistSparseMatrix and get the diagonal
                ////GetTime( timeSta );
                ////DistSparseMatrix<MYSCALAR> Ainv;
                ////PMlocIt.PMatrixToDistSparseMatrix( Ainv );
                ////GetTime( timeEnd );

                ////if( mpirank == 0 )
                ////  cout << "Time for converting PMatrix to DistSparseMatrix is " << timeEnd  - timeSta << endl;

                ////NumVec<MYSCALAR> diagDistSparse;
                ////GetTime( timeSta );
                ////GetDiagonal( Ainv, diagDistSparse );
                ////GetTime( timeEnd );
                ////if( mpirank == 0 )
                ////  cout << "Time for getting the diagonal of DistSparseMatrix is " << timeEnd  - timeSta << endl;

                ////if( mpirank == 0 ){
                ////  statusOFS << std::endl << "Diagonal of inverse from DistSparseMatrix format : " << std::endl << diagDistSparse << std::endl;
                ////  Real diffNorm = 0.0;;
                ////  for( Int i = 0; i < diag.m(); i++ ){
                ////    diffNorm += pow( std::abs( diag(i) - diagDistSparse(i) ), 2.0 );
                ////  }
                ////  diffNorm = std::sqrt( diffNorm );
                ////  statusOFS << std::endl << "||diag - diagDistSparse||_2 = " << diffNorm << std::endl;
                ////}

                // Convert to DistSparseMatrix in the 2nd format and get the diagonal
                GetTime( timeSta );
                DistSparseMatrix<MYSCALAR> Ainv2;
                PMlocIt.PMatrixToDistSparseMatrix( AMat, Ainv2 );
                GetTime( timeEnd );

                if( mpirank == 0 )
                  cout << "Time for converting PMatrix to DistSparseMatrix (2nd format) is " << timeEnd  - timeSta << endl;

                NumVec<MYSCALAR> diagDistSparse2;
                GetTime( timeSta );
                GetDiagonal( Ainv2, diagDistSparse2 );
                GetTime( timeEnd );
                if( mpirank == 0 )
                  cout << "Time for getting the diagonal of DistSparseMatrix is " << timeEnd  - timeSta << endl;

                if( mpirank == 0 ){
                  statusOFS << std::endl << "Diagonal of inverse from the 2nd conversion into DistSparseMatrix format : " << std::endl << diagDistSparse2 << std::endl;
                  Real diffNorm = 0.0;;
                  for( Int i = 0; i < diag.m(); i++ ){
                    diffNorm += pow( std::abs( diag(i) - diagDistSparse2(i) ), 2.0 );
                  }
                  diffNorm = std::sqrt( diffNorm );
                  statusOFS << std::endl << "||diag - diagDistSparse2||_2 = " << diffNorm << std::endl;
                }

                Complex traceLocal = blas::Dotu( AMat.nnzLocal, AMat.nzvalLocal.Data(), 1,
                    Ainv2.nzvalLocal.Data(), 1 );
                Complex trace = Z_ZERO;
                mpi::Allreduce( &traceLocal, &trace, 1, MPI_SUM, world_comm );

                if( mpirank == 0 ){

                  cout << "H.size = "  << HMat.size << endl;
                  cout << std::endl << "Tr[Ainv2 * AMat] = " <<  trace << std::endl;
                  statusOFS << std::endl << "Tr[Ainv2 * AMat] = " << std::endl << trace << std::endl;

                  cout << std::endl << "|N - Tr[Ainv2 * AMat]| = " << std::abs( Complex(HMat.size, 0.0) - trace ) << std::endl;
                  statusOFS << std::endl << "|N - Tr[Ainv2 * AMat]| = " << std::abs( Complex(HMat.size, 0.0) - trace ) << std::endl;

                }
              }





            }
          }
          else if (doSelInv==1) {
            // Preparation for the selected inversion
            GetTime( timeSta );
            PMloc.ConstructCommunicationPattern();//创建communication pattern，在后面会用到，默认用了elimination tree  
            GetTime( timeEnd );


            if( mpirank == 0 )
              cout << "Time for constructing the communication pattern is " << timeEnd  - timeSta << endl;

            GetTime( timeSta );
            PMloc.PreSelInv();//这里就是第二步！！L^和U^那个
            GetTime( timeEnd );

            if( mpirank == 0 )
              cout << "Time for pre-selected inversion is " << timeEnd  - timeSta << endl;

            // Main subroutine for selected inversion
            GetTime( timeSta );
            PMloc.SelInv(); //进行SelInv
            GetTime( timeEnd );
            if( mpirank == 0 )
              cout << "Time for numerical selected inversion is " << timeEnd  - timeSta << endl;


            GetTime( timeTotalSelInvEnd );
            if( mpirank == 0 )
              cout << "Time for total selected inversion is " << timeTotalSelInvEnd  - timeTotalSelInvSta << endl;



            // Output the diagonal elements
            if( doDiag ){
              NumVec<MYSCALAR> diag;

              GetTime( timeSta );
              PMloc.GetDiagonal( diag );
              GetTime( timeEnd );


              if( mpirank == 0 )
                cout << "Time for getting the diagonal is " << timeEnd  - timeSta << endl;


              if( mpirank == 0 ){
                statusOFS << std::endl << "Diagonal (pipeline) of inverse in natural order: " << std::endl << diag << std::endl;
                ofstream ofs("diag");
                if( !ofs.good() ) 
                  ErrorHandling("file cannot be opened.");
                serialize( diag, ofs, NO_MASK );
                ofs.close();
              }
            }

            //PMloc.DumpLU();


            if(doToDist){
              /////              // Convert to DistSparseMatrix and get the diagonal
              /////              GetTime( timeSta );
              /////              DistSparseMatrix<MYSCALAR> Ainv;
              /////              PMloc.PMatrixToDistSparseMatrix( Ainv );
              /////              GetTime( timeEnd );
              /////
              /////              if( mpirank == 0 )
              /////                cout << "Time for converting PMatrix to DistSparseMatrix is " << timeEnd  - timeSta << endl;
              /////
              /////              NumVec<MYSCALAR> diagDistSparse;
              /////              GetTime( timeSta );
              /////              GetDiagonal( Ainv, diagDistSparse );
              /////              GetTime( timeEnd );
              /////              if( mpirank == 0 )
              /////                cout << "Time for getting the diagonal of DistSparseMatrix is " << timeEnd  - timeSta << endl;
              /////
              /////              if( mpirank == 0 ){
              /////                statusOFS << std::endl << "Diagonal of inverse from DistSparseMatrix format : " << std::endl << diagDistSparse << std::endl;
              /////                Real diffNorm = 0.0;;
              /////                for( Int i = 0; i < diag.m(); i++ ){
              /////                  diffNorm += pow( std::abs( diag(i) - diagDistSparse(i) ), 2.0 );
              /////                }
              /////                diffNorm = std::sqrt( diffNorm );
              /////                statusOFS << std::endl << "||diag - diagDistSparse||_2 = " << diffNorm << std::endl;
              /////              }

              // Convert to DistSparseMatrix in the 2nd format and get the diagonal
              GetTime( timeSta );
              DistSparseMatrix<MYSCALAR> Ainv2;
              PMloc.PMatrixToDistSparseMatrix( AMat, Ainv2 );
              GetTime( timeEnd );

              if( mpirank == 0 )
                cout << "Time for converting PMatrix to DistSparseMatrix (2nd format) is " << timeEnd  - timeSta << endl;

              NumVec<MYSCALAR> diagDistSparse2;
              GetTime( timeSta );
              GetDiagonal( Ainv2, diagDistSparse2 );
              GetTime( timeEnd );
              if( mpirank == 0 )
                cout << "Time for getting the diagonal of DistSparseMatrix is " << timeEnd  - timeSta << endl;

              if( mpirank == 0 ){
                statusOFS << std::endl << "Diagonal of inverse from the 2nd conversion into DistSparseMatrix format : " << std::endl << diagDistSparse2 << std::endl;
                Real diffNorm = 0.0;;
                for( Int i = 0; i < diag.m(); i++ ){
                  diffNorm += pow( std::abs( diag(i) - diagDistSparse2(i) ), 2.0 );
                }
                diffNorm = std::sqrt( diffNorm );
                statusOFS << std::endl << "||diag - diagDistSparse2||_2 = " << diffNorm << std::endl;
              }

              Complex traceLocal = blas::Dotu( AMat.nnzLocal, AMat.nzvalLocal.Data(), 1,
                  Ainv2.nzvalLocal.Data(), 1 );
              Complex trace = Z_ZERO;
              mpi::Allreduce( &traceLocal, &trace, 1, MPI_SUM, world_comm );

              if( mpirank == 0 ){

                cout << "H.size = "  << HMat.size << endl;
                cout << std::endl << "Tr[Ainv2 * AMat] = " <<  trace << std::endl;
                statusOFS << std::endl << "Tr[Ainv2 * AMat] = " << std::endl << trace << std::endl;

                cout << std::endl << "|N - Tr[Ainv2 * AMat]| = " << std::abs( Complex(HMat.size, 0.0) - trace ) << std::endl;
                statusOFS << std::endl << "|N - Tr[Ainv2 * AMat]| = " << std::abs( Complex(HMat.size, 0.0) - trace ) << std::endl;

              }
            }
          }

          delete PMlocPtr;
          delete superPtr;
          delete g1Ptr;

        }


      }

      //#if defined(COMM_PROFILE) || defined(COMM_PROFILE_BCAST)
      //      commOFS.close();
      //#endif


      statusOFS.close();
    }
  }
  catch( std::exception& e )
  {
    std::cerr << "Processor " << mpirank << " caught exception with message: "
      << e.what() << std::endl;
  }

  MPI_Finalize();

  return 0;
}
