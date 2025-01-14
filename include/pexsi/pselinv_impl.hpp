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
/// @file pselinv_impl.hpp
/// @brief Implementation of the parallel SelInv.
/// @date 2013-08-05
#ifndef _PEXSI_PSELINV_IMPL_HPP_
#define _PEXSI_PSELINV_IMPL_HPP_

#include <list>
#include <limits>
#include <cublas_v2.h>
#include <cuda_runtime.h>
#include "pexsi/timer.h"
#include "pexsi/superlu_dist_interf.hpp"

#include "pexsi/flops.hpp"
#include <omp.h>

// Multiply the arrays A and B on GPU and save the result in C
// C(m,n) = A(m,k) * B(k,n)
void gpu_blas_smmul(cublasHandle_t& handle, char transA, char transB, int m, int n, int k, 
  float alpha, const float* A, int lda, const float* B, int ldb,
  float beta,        float* C, int ldc ){

	// Allocate 3 arrays on GPU
	float *d_A, *d_B, *d_C;
	cudaMalloc(&d_A,m * k * sizeof(float));
	cudaMalloc(&d_B,k * n * sizeof(float));
	cudaMalloc(&d_C,m * n * sizeof(float));

    // Copy the data to device
    cudaMemcpy(d_A, A, m * k * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, B, n * k * sizeof(float), cudaMemcpyHostToDevice);
    // if(abs(beta - 0) != 0.000001){
    //   cudaMemcpy(d_C, C, m * n * sizeof(float), cudaMemcpyHostToDevice);
    // }


	// Create a handle for CUBLAS
	// cublasHandle_t handle;
	// cublasCreate(&handle);
	cublasOperation_t opA = CUBLAS_OP_N;
	cublasOperation_t opB = CUBLAS_OP_N;
	if(transA == 'T'){
		opA = CUBLAS_OP_T;
	}
	if(transB == 'T'){
		opB = CUBLAS_OP_T;
	}
	
	// Do the actual multiplication
	cublasSgemm(handle, opA, opB, m, n, k, &alpha, d_A, lda, d_B, ldb, &beta, d_C, ldc);
	// Destroy the handle
	// cublasDestroy(handle);

	// Copy (and print) the result on host memory
	cudaMemcpy(C,d_C,m * n * sizeof(float),cudaMemcpyDeviceToHost);

	//Free GPU memory
	cudaFree(d_A);
	cudaFree(d_B);
	cudaFree(d_C);	
}
void gpu_blas_dmmul(cublasHandle_t& handle, char transA, char transB, int m, int n, int k, 
  double alpha, const double* A, int lda, const double* B, int ldb,
  double beta,        double* C, int ldc ){

	// Allocate 3 arrays on GPU
	double *d_A, *d_B, *d_C;
	cudaMalloc(&d_A,m * k * sizeof(double));
	cudaMalloc(&d_B,k * n * sizeof(double));
	cudaMalloc(&d_C,m * n * sizeof(double));

    // Copy the data to device
    cudaMemcpy(d_A, A, m * k * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, B, n * k * sizeof(double), cudaMemcpyHostToDevice);
    // if(abs(beta - 0) != 0.000001){
    cudaMemcpy(d_C, C, m * n * sizeof(double), cudaMemcpyHostToDevice);
    // }
    
    


	// Create a handle for CUBLAS
	// cublasHandle_t handle;
	// cublasCreate(&handle);
	cublasOperation_t opA = CUBLAS_OP_N;
	cublasOperation_t opB = CUBLAS_OP_N;
	if(transA == 'T'){
		opA = CUBLAS_OP_T;
	}
	if(transB == 'T'){
		opB = CUBLAS_OP_T;
	}
	
	// Do the actual multiplication
	cublasDgemm(handle, opA, opB, m, n, k, &alpha, d_A, lda, d_B, ldb, &beta, d_C, ldc);
	// Destroy the handle
	// cublasDestroy(handle);

	// Copy (and print) the result on host memory
	cudaMemcpy(C,d_C,m * n * sizeof(double),cudaMemcpyDeviceToHost);

	//Free GPU memory
	cudaFree(d_A);
	cudaFree(d_B);
	cudaFree(d_C);	
}
void gpu_blas_dtrsm(cublasHandle_t& handle, char side, char uplo, char trans, char unit, int m, int n,
  double alpha, const double* A, int lda, double* B, int ldb ){

	// settings in gpu
	cublasSideMode_t cuSide;
	cublasFillMode_t cuUplo;
	cublasOperation_t cuTrans;
	cublasDiagType_t cuUnit;

	int rowA, colA, rowB = m, colB = n;
	// modify the settings
	if(side == 'L'){
		rowA = lda;
		colA = m;
		cuSide = CUBLAS_SIDE_LEFT;
	}else if(side == 'R'){
		rowA = lda;
		colA = n;
		cuSide = CUBLAS_SIDE_RIGHT;
	}


	if(uplo == 'L'){
		cuUplo = CUBLAS_FILL_MODE_LOWER;
	}else if(uplo == 'U'){
		cuUplo = CUBLAS_FILL_MODE_UPPER;
	}else{ //这里不知道FULL是什么字符就用了else了，后面会补上的
		cuUplo = CUBLAS_FILL_MODE_FULL;
	}

	if(trans == 'T'){
		cuTrans = CUBLAS_OP_T;
	}else if(trans == 'N'){
		cuTrans = CUBLAS_OP_N;
	}

	if(unit == 'U'){
		cuUnit = CUBLAS_DIAG_UNIT;
	}else{ //这里不知道NON_UNIT是什么字符就用了else了，后面会补上的
		cuUnit = CUBLAS_DIAG_NON_UNIT;
	}

	// Allocate 3 arrays on GPU
	double *d_A, *d_B;
	cudaMalloc(&d_A,rowA * colA * sizeof(double));
	cudaMalloc(&d_B,rowB * colB * sizeof(double));

	// Copy the data to device
  cudaMemcpy(d_A, A, rowA * colA * sizeof(double), cudaMemcpyHostToDevice);
  cudaMemcpy(d_B, B, rowB * colB * sizeof(double), cudaMemcpyHostToDevice);

	// Create a handle for CUBLAS
	// cublasHandle_t handle;
	// cublasCreate(&handle);

	// Do the actual trsm
	cublasDtrsm(handle, cuSide, cuUplo, cuTrans, cuUnit, m, n, &alpha, d_A, lda, d_B, ldb);
	// Destroy the handle
	// cublasDestroy(handle);

	// Copy (and print) the result on host memory
	cudaMemcpy(B, d_B, m * n * sizeof(double), cudaMemcpyDeviceToHost);

	//Free GPU memory
	cudaFree(d_A);
	cudaFree(d_B);
}


void gpu_blas_strsm(cublasHandle_t& handle, char side, char uplo, char trans, char unit, int m, int n,
  float alpha, const float* A, int lda, float* B, int ldb ){

	// settings in gpu
	cublasSideMode_t cuSide;
	cublasFillMode_t cuUplo;
	cublasOperation_t cuTrans;
	cublasDiagType_t cuUnit;

	int rowA, colA, rowB = m, colB = n;
	// modify the settings
	if(side == 'L'){
		rowA = lda;
		colA = m;
		cuSide = CUBLAS_SIDE_LEFT;
	}else if(side == 'R'){
		rowA = lda;
		colA = n;
		cuSide = CUBLAS_SIDE_RIGHT;
	}


	if(uplo == 'L'){
		cuUplo = CUBLAS_FILL_MODE_LOWER;
	}else if(uplo == 'U'){
		cuUplo = CUBLAS_FILL_MODE_UPPER;
	}else{ //这里不知道FULL是什么字符就用了else了，后面会补上的
		cuUplo = CUBLAS_FILL_MODE_FULL;
	}

	if(trans == 'T'){
		cuTrans = CUBLAS_OP_T;
	}else if(trans == 'N'){
		cuTrans = CUBLAS_OP_N;
	}

	if(unit == 'U'){
		cuUnit = CUBLAS_DIAG_UNIT;
	}else{ //这里不知道NON_UNIT是什么字符就用了else了，后面会补上的
		cuUnit = CUBLAS_DIAG_NON_UNIT;
	}

	// Allocate 3 arrays on GPU
	float *d_A, *d_B;
	cudaMalloc(&d_A,rowA * colA * sizeof(float));
	cudaMalloc(&d_B,rowB * colB * sizeof(float));

	// Copy the data to device
    cudaMemcpy(d_A, A, rowA * colA * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, B, rowB * colB * sizeof(float), cudaMemcpyHostToDevice);

	// Create a handle for CUBLAS
	// cublasHandle_t handle;
	// cublasCreate(&handle);

	// Do the actual trsm
	cublasStrsm(handle, cuSide, cuUplo, cuTrans, cuUnit, m, n, &alpha, d_A, lda, d_B, ldb);
	// Destroy the handle
	// cublasDestroy(handle);

	// Copy (and print) the result on host memory
	cudaMemcpy(B, d_B, m * n * sizeof(float), cudaMemcpyDeviceToHost);

	//Free GPU memory
	cudaFree(d_A);
	cudaFree(d_B);
}


#define MPI_MAX_COMM (1024)
#define BCAST_THRESHOLD 16

#define MOD(a,b) \
  ( ((a)%(b)+(b))%(b))

#ifdef COMMUNICATOR_PROFILE
namespace PEXSI{
  class CommunicatorStat{
    public:
      typedef std::map<std::vector<bool> , std::vector<Int> > bitMaskSet;
      //Communicators for the Bcast variant
      NumVec<Int>                       countSendToBelow_;
      bitMaskSet                        maskSendToBelow_;

      NumVec<Int>                       countRecvFromBelow_;
      bitMaskSet                        maskRecvFromBelow_;

      NumVec<Int>                       countSendToRight_;
      bitMaskSet                        maskSendToRight_;
  };
}
#endif



namespace PEXSI{
  inline GridType::GridType	( MPI_Comm Bcomm, int nprow, int npcol )
  {
    Int info;
    MPI_Initialized( &info );
    if( !info ){
      ErrorHandling( "MPI has not been initialized." );
    }
    MPI_Group  comm_group;
    MPI_Comm_group( Bcomm, &comm_group );
    MPI_Comm_create( Bcomm, comm_group, &comm );
    //		comm = Bcomm;

    MPI_Comm_rank( comm, &mpirank );
    MPI_Comm_size( comm, &mpisize );
    if( mpisize != nprow * npcol ){
      ErrorHandling( "mpisize != nprow * npcol." ); 
    }


    numProcRow = nprow;
    numProcCol = npcol;
#ifdef SWAP_ROWS_COLS
    Int myrow = mpirank % npcol;
    Int mycol = mpirank / npcol;
#else
    Int myrow = mpirank / npcol;
    Int mycol = mpirank % npcol;
#endif

    MPI_Comm_split( comm, myrow, mycol, &rowComm );
    MPI_Comm_split( comm, mycol, myrow, &colComm );

    MPI_Group_free( &comm_group );


    return ;
  } 		// -----  end of method GridType::GridType  ----- 


  inline GridType::~GridType	(  )
  {
    // Dot not free grid.comm which is not generated by GridType().

    MPI_Comm_free( &rowComm );
    MPI_Comm_free( &colComm ); 
    MPI_Comm_free( &comm );

    return ;
  } 		// -----  end of method GridType::~GridType  ----- 
}


namespace PEXSI{
  template<typename T>
    void PMatrix<T>::deallocate(){

      grid_ = NULL;
      super_ = NULL;
      options_ = NULL;
      optionsFact_ = NULL;

      ColBlockIdx_.clear();
      RowBlockIdx_.clear();
      U_.clear();
      L_.clear();
      workingSet_.clear();

      // Communication variables
      isSendToBelow_.Clear();
      isSendToRight_.Clear();
      isSendToDiagonal_.Clear();
      isSendToCrossDiagonal_.Clear();

      isRecvFromBelow_.Clear();
      isRecvFromAbove_.Clear();
      isRecvFromLeft_.Clear();
      isRecvFromCrossDiagonal_.Clear();


      //Cleanup tree information
      for(int i =0;i<fwdToBelowTree_.size();++i){
        if(fwdToBelowTree_[i]!=NULL){
          delete fwdToBelowTree_[i];
          fwdToBelowTree_[i] = NULL;
        }
      }      

      for(int i =0;i<fwdToRightTree_.size();++i){
        if(fwdToRightTree_[i]!=NULL){
          delete fwdToRightTree_[i];
          fwdToRightTree_[i] = NULL;
        }
      }

      for(int i =0;i<redToLeftTree_.size();++i){
        if(redToLeftTree_[i]!=NULL){
          delete redToLeftTree_[i];
          redToLeftTree_[i] = NULL;
        }
      }
      for(int i =0;i<redToAboveTree_.size();++i){
        if(redToAboveTree_[i]!=NULL){
          delete redToAboveTree_[i];
          redToAboveTree_[i] = NULL;
        }
      }

      //dump comm_profile info
#if defined(COMM_PROFILE) || defined(COMM_PROFILE_BCAST)
      if(commOFS.is_open()){
        commOFS.close();
      }
#endif


    }

  template<typename T>
    PMatrix<T> & PMatrix<T>::operator = ( const PMatrix<T> & C){
      if(&C!=this){
        //If we have some memory allocated, delete it
        deallocate();

        grid_ = C.grid_;
        super_ = C.super_;
        options_ = C.options_;
        optionsFact_ = C.optionsFact_;


        maxTag_ = C.maxTag_;
        limIndex_ = C.limIndex_;

        ColBlockIdx_ = C.ColBlockIdx_;
        RowBlockIdx_ = C.RowBlockIdx_;
        L_ = C.L_;
        U_ = C.U_;

        workingSet_ = C.workingSet_;


        // Communication variables
        isSendToBelow_ = C.isSendToBelow_;
        isSendToRight_ = C.isSendToRight_;
        isSendToDiagonal_ = C.isSendToDiagonal_;
        isSendToCrossDiagonal_ = C.isSendToCrossDiagonal_;

        isRecvFromBelow_ = C.isRecvFromBelow_;
        isRecvFromAbove_ = C.isRecvFromAbove_;
        isRecvFromLeft_ = C.isRecvFromLeft_;
        isRecvFromCrossDiagonal_ = C.isRecvFromCrossDiagonal_;

//#ifndef _SYM_STORAGE_
        if(options_!=NULL/*nullptr*/){
          if (options_->symmetricStorage!=1){
            fwdToBelowTree_.resize(C.fwdToBelowTree_.size());
            for(int i = 0 ; i< C.fwdToBelowTree_.size();++i){
              if(C.fwdToBelowTree_[i]!=NULL){
                fwdToBelowTree_[i] = C.fwdToBelowTree_[i]->clone();
              }
            }
            fwdToRightTree_.resize(C.fwdToRightTree_.size());
            for(int i = 0 ; i< C.fwdToRightTree_.size();++i){
              if(C.fwdToRightTree_[i]!=NULL){
                fwdToRightTree_[i] = C.fwdToRightTree_[i]->clone();
              }
            }

            redToLeftTree_.resize(C.redToLeftTree_.size());
            for(int i = 0 ; i< C.redToLeftTree_.size();++i){
              if(C.redToLeftTree_[i]!=NULL){
                redToLeftTree_[i] = C.redToLeftTree_[i]->clone();
              }
            }
            redToAboveTree_.resize(C.redToAboveTree_.size());
            for(int i = 0 ; i< C.redToAboveTree_.size();++i){
              if(C.redToAboveTree_[i]!=NULL){
                redToAboveTree_[i] = C.redToAboveTree_[i]->clone();
              }
            }
          }
        }
//#else
//#endif

      }

      return *this;
    }

  template<typename T>
    PMatrix<T>::PMatrix( const PMatrix<T> & C):PMatrix(){
      //If we have some memory allocated, delete it
      deallocate();

      grid_ = C.grid_;
      super_ = C.super_;
      options_ = C.options_;
      optionsFact_ = C.optionsFact_;


      limIndex_ = C.limIndex_;
      maxTag_ = C.maxTag_;
      ColBlockIdx_ = C.ColBlockIdx_;
      RowBlockIdx_ = C.RowBlockIdx_;
      L_ = C.L_;
      U_ = C.U_;

      workingSet_ = C.workingSet_;


      // Communication variables
      isSendToBelow_ = C.isSendToBelow_;
      isSendToRight_ = C.isSendToRight_;
      isSendToDiagonal_ = C.isSendToDiagonal_;
      isSendToCrossDiagonal_ = C.isSendToCrossDiagonal_;

      isRecvFromBelow_ = C.isRecvFromBelow_;
      isRecvFromAbove_ = C.isRecvFromAbove_;
      isRecvFromLeft_ = C.isRecvFromLeft_;
      isRecvFromCrossDiagonal_ = C.isRecvFromCrossDiagonal_;



//#ifndef _SYM_STORAGE_
      if(options_!=nullptr){
        if (options_->symmetricStorage!=1){
          fwdToBelowTree_.resize(C.fwdToBelowTree_.size());
          for(int i = 0 ; i< C.fwdToBelowTree_.size();++i){
            if(C.fwdToBelowTree_[i]!=NULL){
              fwdToBelowTree_[i] = C.fwdToBelowTree_[i]->clone();
            }
          }
          fwdToRightTree_.resize(C.fwdToRightTree_.size());
          for(int i = 0 ; i< C.fwdToRightTree_.size();++i){
            if(C.fwdToRightTree_[i]!=NULL){
              fwdToRightTree_[i] = C.fwdToRightTree_[i]->clone();
            }
          }

          redToLeftTree_.resize(C.redToLeftTree_.size());
          for(int i = 0 ; i< C.redToLeftTree_.size();++i){
            if(C.redToLeftTree_[i]!=NULL){
              redToLeftTree_[i] = C.redToLeftTree_[i]->clone();
            }
          }
          redToAboveTree_.resize(C.redToAboveTree_.size());
          for(int i = 0 ; i< C.redToAboveTree_.size();++i){
            if(C.redToAboveTree_[i]!=NULL){
              redToAboveTree_[i] = C.redToAboveTree_[i]->clone();
            }
          }
        }
      }
//#else
//#endif

    }


  template<typename T>
    PMatrix<T>::PMatrix ( ){
      grid_ = nullptr;
      super_ = nullptr;
      options_ = nullptr;
      optionsFact_ = nullptr;
    }

  template<typename T>
    PMatrix<T>::PMatrix ( 
        const GridType* g, 
        const SuperNodeType* s, 
        const PEXSI::PSelInvOptions * o, 
        const PEXSI::FactorizationOptions * oFact
        ):PMatrix()
    {

      this->Setup( g, s, o, oFact );
      return ;
    } 		// -----  end of method PMatrix::PMatrix  ----- 

  template<typename T>
    PMatrix<T>::~PMatrix ( )
    {


      deallocate();

      return ;
    } 		// -----  end of method PMatrix::~PMatrix  ----- 

  template<typename T>
    void PMatrix<T>::Setup( 
        const GridType* g, 
        const SuperNodeType* s, 
        const PEXSI::PSelInvOptions * o, 
        const PEXSI::FactorizationOptions * oFact
        ) 
    {

      grid_          = g;
      super_         = s;
      options_       = o;
      optionsFact_       = oFact;

      //    if( grid_->numProcRow != grid_->numProcCol ){
      //ErrorHandling( "The current version of SelInv only works for square processor grids." ); }

      L_.clear();
      U_.clear();
      ColBlockIdx_.clear();
      RowBlockIdx_.clear();

      L_.resize( this->NumLocalBlockCol() );
      U_.resize( this->NumLocalBlockRow() );

      ColBlockIdx_.resize( this->NumLocalBlockCol() );
      RowBlockIdx_.resize( this->NumLocalBlockRow() );

      //workingSet_.resize(this->NumSuper());
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "PMatrix is constructed. The grid information: " << std::endl;
      statusOFS << "mpirank = " << MYPROC(grid_) << std::endl;
      statusOFS << "myrow   = " << MYROW(grid_) << std::endl; 
      statusOFS << "mycol   = " << MYCOL(grid_) << std::endl; 
#endif

      //TODO enable this
      //Get maxTag value and compute the max depth we can use
      //int * pmaxTag;
      int pmaxTag = 4000;

      void *v = NULL;
      int flag;
      MPI_Comm_get_attr(grid_->comm, MPI_TAG_UB, &v, &flag);
      if (flag) {
        pmaxTag = *(int*)v;
      }


      maxTag_ = pmaxTag;
      limIndex_ = (Int)maxTag_/(Int)SELINV_TAG_COUNT -1; 


#if defined(COMM_PROFILE) || defined(COMM_PROFILE_BCAST)
      std::stringstream  ss3;
      ss3 << "comm_stat" << MYPROC(this->grid_);
      if(!commOFS.is_open()){
        commOFS.open( ss3.str().c_str());
      }
#endif


      return ;
    } 		// -----  end of method PMatrix::Setup   ----- 

  template<typename T>
    inline  void PMatrix<T>::SelInv_lookup_indexes(
        SuperNodeBufferType & snode, 
        std::vector<LBlock<T> > & LcolRecv, 
        std::vector<UBlock<T> > & UrowRecv, 
        NumMat<T> & AinvBuf,
        NumMat<T> & UBuf,
        NumMat<float> & UBuf_quant,
        std::set<std::string> & quantSuperNode,
        bool & quantUBuf )
    {
      TIMER_START(Compute_Sinv_LT_Lookup_Indexes);
      
      TIMER_START(Build_colptr_rowptr);
      // rowPtr[ib] gives the row index in snode.LUpdateBuf for the first
      // nonzero row in LcolRecv[ib]. The total number of rows in
      // snode.LUpdateBuf is given by rowPtr[end]-1
      std::vector<Int> rowPtr(LcolRecv.size() + 1);//它保存了每个L block的在最后的存储矩阵LUpdateBuf中的第一个非零行，并且总行数记录在最后一个元素
      // colPtr[jb] gives the column index in UBuf for the first
      // nonzero column in UrowRecv[jb]. The total number of rows in
      // UBuf is given by colPtr[end]-1
      std::vector<Int> colPtr(UrowRecv.size() + 1);//它保存了每个U block的在最后的存储矩阵UBuf第一个非零列，并且总列数记录在最后一个元素

      rowPtr[0] = 0;
      for( Int ib = 0; ib < LcolRecv.size(); ib++ ){
        rowPtr[ib+1] = rowPtr[ib] + LcolRecv[ib].numRow;//后面存储的时候全都只存了非零元素，所以这种累加的效果相当于每个block的在最后存储中的偏移量了
      }
      colPtr[0] = 0;
      for( Int jb = 0; jb < UrowRecv.size(); jb++ ){
        colPtr[jb+1] = colPtr[jb] + UrowRecv[jb].numCol;
      }

      Int numRowAinvBuf = *rowPtr.rbegin();//总的非零行数量
      Int numColAinvBuf = *colPtr.rbegin();//总的非零列数量
      TIMER_STOP(Build_colptr_rowptr);

      TIMER_START(Allocate_lookup);
      // Allocate for the computational storage
      //分配了Ainv和U矩阵的空间
      AinvBuf.Resize( numRowAinvBuf, numColAinvBuf );//Ainv为非零行 * 非零列
      UBuf.Resize( SuperSize( snode.Index, super_ ), numColAinvBuf );//U矩阵为supernode行 * 非零列，合理
      UBuf_quant.Resize( SuperSize( snode.Index, super_ ), numColAinvBuf );//UBuf_quant和UBuf一样

      SetValue(AinvBuf, (double)0.0);//填充它们为0，当为低精度的时候就不用计算了，后面再用低精度的计算补上
      SetValue(UBuf, (double)0.0);//填充它们为0，当为低精度的时候就不用计算了，后面再用低精度的计算补上
      SetValue(UBuf_quant, (float)0.0);//填充它们为0，用于做高精度的补充

      int snode_index = snode.Index;//记录snode的index，对于Ublock而言它是行，Ublock自身的index是列，对于Lblock而言它是列，Lblock自身的index是行
      std::stringstream ss;//记录supernode的index

      TIMER_STOP(Allocate_lookup);


      TIMER_START(Fill_UBuf);
      // Fill UBuf first.  Make the transpose later in the Gemm phase.
      for( Int jb = 0; jb < UrowRecv.size(); jb++ ){
        UBlock<T>& UB = UrowRecv[jb];
        if( UB.numRow != SuperSize(snode.Index, super_) ){
          ErrorHandling( "The size of UB is not right.  Something is seriously wrong." );
        }
        //进行量化！ 
        ss.str("");
        ss<< UB.blockIdx << "," << snode_index;//这里反一下，用来查L Block就行了
        if(quantSuperNode.find(ss.str()) == quantSuperNode.end()){
          //如果它不是量化的supernode
          //将接受到的U矩阵复制到Ubuf中
          //第一个参数’A’表示把整个矩阵进行一个复制
          //第二、三个参数表示要复制的行和列，第四个参数表示要复制的矩阵
          //第五和第七个参数不用管
          //第六个参数是output
          lapack::Lacpy( 'A', UB.numRow, UB.numCol, UB.nzval.Data(),
              UB.numRow, UBuf.VecData( colPtr[jb] ), SuperSize( snode.Index, super_ ) );	//复制
        }else{
          //如果它是量化的supernode
          //首先将它的值复制到一个临时矩阵中，不过不知道这一步后面能不能优化，就是直接复制了
          //因为感觉可能Lacpy有一些优化所以这类还是先临时复制一下
          quantUBuf = true;
          // std::cout<<"Step 3 : Quant:"<<ss.str()<<std::endl;
          NumMat<float> UB_float(UB.numRow, UB.numCol);
          for(int j = 0;j<UB.numCol;j++){
            for(int i = 0;i<UB.numRow;i++){
              UB_float(i, j) = (float)UB.nzval(i, j);
            }
          }
          lapack::Lacpy( 'A', UB.numRow, UB.numCol, UB_float.Data(),
              UB.numRow, UBuf_quant.VecData( colPtr[jb] ), SuperSize( snode.Index, super_ ) );	//复制
          UB_float.deallocate();
        }
        
      }
      TIMER_STOP(Fill_UBuf);
      //至此U矩阵已经复制完毕，即传入的参数填充成功

      // Calculate the relative indices for (isup, jsup)
      // Fill AinvBuf with the information in L or U block.
      TIMER_START(JB_Loop);

#ifdef STDFIND
      
#else
      for( Int jb = 0; jb < UrowRecv.size(); jb++ ){//column major的遍历，芜湖～
        for( Int ib = 0; ib < LcolRecv.size(); ib++ ){
          LBlock<T>& LB = LcolRecv[ib];//得到接收到的Lblock
          UBlock<T>& UB = UrowRecv[jb];//得到接收到的Ublock
          Int isup = LB.blockIdx;//找到对应的L block的supernode行
          Int jsup = UB.blockIdx;//找到对应的U block的supernode列
          T* nzvalAinv = &AinvBuf( rowPtr[ib], colPtr[jb] );//得到(isup, jsup)的第一个数据位置，即第j列第i行，这里是column major
          Int     ldAinv    = AinvBuf.m();//得到有非零行，用于后续列序遍历使用
          //此时本地的L Block和U Block都是Ainv的形状了

          // Pin down the corresponding block in the part of Sinv.
          if( isup >= jsup ){//如果是下三角，用L矩阵
            std::vector<LBlock<T> >&  LcolSinv = this->L( LBj(jsup, grid_ ) );//找到本processor保存的对应列的所有L Sinv block
            bool isBlockFound = false;                                        //是因为现在算法已经开始了进行覆盖了吗？好像是用Ainv block将L block覆盖掉了
            TIMER_START(PARSING_ROW_BLOCKIDX);
            for( Int ibSinv = 0; ibSinv < LcolSinv.size(); ibSinv++ ){
              // Found the (isup, jsup) block in Sinv
              if( LcolSinv[ibSinv].blockIdx == isup ){//找到了isup, jsup的block
                LBlock<T> & SinvB = LcolSinv[ibSinv];

                // Row relative indices
                Int* rowsLBPtr    = LB.rows.Data();//传入L矩阵的非零行的下标
                Int* rowsSinvBPtr = SinvB.rows.Data();//本地Sinv block的非零行的下标

                // Column relative indicies
                Int SinvColsSta = FirstBlockCol( jsup, super_ );//这个jsup对应supernode的第一个列

#ifdef _OPENMP_BLOCKS_
                Int lastRowIdxSinv = 0;
                Int blockRowsSize = 0; 
                Int blockColsSize = 0;
                Int * blockRows = new Int[3*LB.numRow]; 
                Int * blockCols = new Int[2*UB.numCol]; 


                Int* colsUBPtr    = UB.cols.Data();

                if(LB.numRow>0){
                  //find blocks
                  Int ifr = 0;
                  Int fr = rowsLBPtr[0];
                  Int nrows = 1;
                  for( Int i = 1; i < LB.numRow; i++ ){
                    if(rowsLBPtr[i]==rowsLBPtr[i-1]+1){ 
                      nrows++;
                    }
                    else{
                      blockRows[blockRowsSize++] = fr;
                      blockRows[blockRowsSize++] = nrows;
                      blockRows[blockRowsSize++] = ifr;
                      fr = rowsLBPtr[i];
                      nrows = 1;
                      ifr = i;
                    }
                  }
                  blockRows[blockRowsSize++] = fr;
                  blockRows[blockRowsSize++] = nrows;
                  blockRows[blockRowsSize++] = ifr;


                  for(Int ii = 0; ii<blockRowsSize;ii+=3){
                    {
                      auto fr = blockRows[ii];
                      auto nr = blockRows[ii+1];
                      Int ifr = blockRows[ii+2];

                      bool isRowFound = false;
                      for( lastRowIdxSinv; lastRowIdxSinv < SinvB.numRow; lastRowIdxSinv++ ){
                        if( fr == rowsSinvBPtr[lastRowIdxSinv] ){
                          isRowFound = true;
                          blockRows[ii] = lastRowIdxSinv;
                          break;
                        }
                      }
                      if( isRowFound == false ){
                        std::ostringstream msg;
                        msg << "Row " << rowsLBPtr[ifr] << "("<<fr<<")"
                          " in pLB cannot find the corresponding row in SinvB" << std::endl
                          << "LB.rows    = " << LB.rows << std::endl
                          << "SinvB.rows = " << SinvB.rows << std::endl;
                        ErrorHandling( msg.str().c_str() );
                      }
                    }
                  }
                }

                if(UB.numCol>0){
                  //find blocks
                  Int fc = colsUBPtr[0]- SinvColsSta;
                  Int ncols = 1;
                  for( Int j = 1; j < UB.numCol; j++ ){
                    if(colsUBPtr[j]==colsUBPtr[j-1]+1){ 
                      ncols++;
                    }
                    else{
                      blockCols[blockColsSize++] = fc;
                      blockCols[blockColsSize++] = ncols;
                      fc = colsUBPtr[j]- SinvColsSta;
                      ncols = 1;
                    }
                  }
                  blockCols[blockColsSize++] = fc;
                  blockCols[blockColsSize++] = ncols;
                }



                // Transfer the values from Sinv to AinvBlock
                T* nzvalSinv = SinvB.nzval.Data();
                Int     ldSinv    = SinvB.numRow;

                Int j = 0;
                for(Int jj = 0; jj<blockColsSize;jj+=2){
                  auto fc = blockCols[jj];
                  auto nc = blockCols[jj+1];
                  Int offsetJ = j+colPtr[jb];
                  Int i = 0;
                  for(Int ii = 0; ii<blockRowsSize;ii+=3){
                    auto fr = blockRows[ii];
                    auto nr = blockRows[ii+1];
                    Int ifr = blockRows[ii+2];
                    assert(i==ifr);
                    lapack::Lacpy( 'A', nr, nc, &nzvalSinv[fr+fc*ldSinv],ldSinv,&nzvalAinv[i+j*ldAinv],ldAinv );
                    i+=nr;
                  }
                  j+=nc;
                }

                delete [] blockCols;
                delete [] blockRows;
#else
                std::vector<Int> relCols( UB.numCol );//记录U block的相对列
                for( Int j = 0; j < UB.numCol; j++ ){
                  relCols[j] = UB.cols[j] - SinvColsSta;
                }

                std::vector<Int> relRows( LB.numRow );//记录L block的相对行
                for( Int i = 0; i < LB.numRow; i++ ){
                  bool isRowFound = false;
                  for( Int i1 = 0; i1 < SinvB.numRow; i1++ ){
                    if( rowsLBPtr[i] == rowsSinvBPtr[i1] ){//将LB的row转化为Sinv Bblock的本地row
                      isRowFound = true;
                      relRows[i] = i1;
                      break;
                    }
                  }
                  if( isRowFound == false ){
                    std::ostringstream msg;
                    msg << "Row " << rowsLBPtr[i] << 
                      " in LB cannot find the corresponding row in SinvB" << std::endl
                      << "LB.rows    = " << LB.rows << std::endl
                      << "SinvB.rows = " << SinvB.rows << std::endl;
                    ErrorHandling( msg.str().c_str() );
                  }
                }
                
                // Transfer the values from Sinv to AinvBlock
                T* nzvalSinv = SinvB.nzval.Data();
                Int     ldSinv    = SinvB.numRow;//得到Sinv Block的行
                
                  for( Int j = 0; j < UB.numCol; j++ ){
                    for( Int i = 0; i < LB.numRow; i++ ){
                      nzvalAinv[i+j*ldAinv] =
                        nzvalSinv[relRows[i] + relCols[j] * ldSinv];//这里就是将Sinv Block复制到Ainv的对应地方去了
                    }
                  }

                
#endif

                isBlockFound = true;
                break;
              }	
            } // for (ibSinv )
            TIMER_STOP(PARSING_ROW_BLOCKIDX);
            if( isBlockFound == false ){
              std::ostringstream msg;
              msg << "Block(" << isup << ", " << jsup 
                << ") did not find a matching block in Sinv." << std::endl;
              ErrorHandling( msg.str().c_str() );
            }
          } // if (isup, jsup) is in L
          else{//如果是上三角，用U矩阵，同上
            std::vector<UBlock<T> >&   UrowSinv = this->U( LBi( isup, grid_ ) );//找到本processor保存的对应列的所有U Sinv block，不懂为什么现在是U sinv了
            bool isBlockFound = false;
            TIMER_START(PARSING_COL_BLOCKIDX);
            for( Int jbSinv = 0; jbSinv < UrowSinv.size(); jbSinv++ ){
              // Found the (isup, jsup) block in Sinv
              if( UrowSinv[jbSinv].blockIdx == jsup ){
                UBlock<T> & SinvB = UrowSinv[jbSinv];

                // Row relative indices
                Int SinvRowsSta = FirstBlockCol( isup, super_ );

                Int* colsUBPtr    = UB.cols.Data();
                Int* colsSinvBPtr = SinvB.cols.Data();
#ifdef _OPENMP_BLOCKS_
                Int* rowsLBPtr    = LB.rows.Data();

                Int lastColIdxSinv = 0;
                Int * blockCols = new Int[3*UB.numCol];
                Int * blockRows = new Int[2*LB.numRow];
                Int blockColsSize = 0;
                Int blockRowsSize = 0;

                if(UB.numCol>0){
                  //find blocks
                  Int jfc = 0;
                  Int fc = colsUBPtr[0];
                  Int ncols = 1;
                  for( Int j = 1; j < UB.numCol; j++ ){
                    if(colsUBPtr[j]==colsUBPtr[j-1]+1){ 
                      ncols++;
                    }
                    else{
                      blockCols[blockColsSize++] = fc;
                      blockCols[blockColsSize++] = ncols;
                      blockCols[blockColsSize++] = jfc;
                      fc = colsUBPtr[j];
                      ncols = 1;
                      jfc = j;
                    }
                  }
                  blockCols[blockColsSize++] = fc;
                  blockCols[blockColsS欧提葡萄ize++] = ncols;
                  blockCols[blockColsSize++] = jfc;

                  for(Int jj = 0; jj<blockColsSize;jj+=3){
                    auto fc = blockCols[jj];
                    auto nc = blockCols[jj+1];
                    Int jfc = blockCols[jj+2];

                    bool isColFound = false;
                    for( lastColIdxSinv; lastColIdxSinv < SinvB.numCol; lastColIdxSinv++ ){
                      if( fc == colsSinvBPtr[lastColIdxSinv] ){
                        isColFound = true;
                        blockCols[jj] = lastColIdxSinv;
                        break;
                      }
                    }
                    if( isColFound == false ){
                      std::ostringstream msg;
                      msg << "Col " << colsUBPtr[jfc] << "("<<fc<<")"
                        " in UB cannot find the corresponding row in SinvB" << std::endl
                        << "UB.cols    = " << UB.cols << std::endl
                        << "SinvB.cols = " << SinvB.cols << std::endl;
                      ErrorHandling( msg.str().c_str() );
                    }
                  }
                }

                if(LB.numRow>0){
                  //find blocks
                  Int fr = rowsLBPtr[0]- SinvRowsSta;
                  Int nrows = 1;
                  for( Int j = 1; j < LB.numRow; j++ ){
                    if(rowsLBPtr[j]==rowsLBPtr[j-1]+1){ 
                      nrows++;
                    }
                    else{
                      blockRows[blockRowsSize++] = fr;
                      blockRows[blockRowsSize++] = nrows;
                      fr = rowsLBPtr[j]- SinvRowsSta;
                      nrows = 1;
                    }
                  }
                  blockRows[blockRowsSize++] = fr;
                  blockRows[blockRowsSize++] = nrows;
                }


                // Transfer the values from Sinv to AinvBlock
                T* nzvalSinv = SinvB.nzval.Data();
                Int     ldSinv    = SinvB.numRow;

                Int j = 0;
                for(Int jj = 0; jj<blockColsSize;jj+=3){
                  auto fc = blockCols[jj];
                  auto nc = blockCols[jj+1];
                  Int jfc = blockCols[jj+2];
                  Int offsetJ = j+colPtr[jb];

                  Int i = 0;
                  for(Int ii = 0; ii<blockRowsSize;ii+=2){
                    auto fr = blockRows[ii];
                    auto nr = blockRows[ii+1];
                    Int offsetI = i+rowPtr[ib];
                    lapack::Lacpy( 'A', nr, nc, &nzvalSinv[fr+fc*ldSinv],ldSinv,&nzvalAinv[i+j*ldAinv],ldAinv );
                    i+=nr;
                  }
                  j+=nc;
                }

                delete [] blockCols;
                delete [] blockRows;
#else
                std::vector<Int> relRows( LB.numRow );
                for( Int i = 0; i < LB.numRow; i++ ){
                  relRows[i] = LB.rows[i] - SinvRowsSta;
                }

                // Column relative indices
                std::vector<Int> relCols( UB.numCol );
                for( Int j = 0; j < UB.numCol; j++ ){
                  bool isColFound = false;
                  for( Int j1 = 0; j1 < SinvB.numCol; j1++ ){
                    if( colsUBPtr[j] == colsSinvBPtr[j1] ){
                      isColFound = true;
                      relCols[j] = j1;
                      break;
                    }
                  }
                  if( isColFound == false ){
                    std::ostringstream msg;
                    msg << "Col " << colsUBPtr[j] << 
                      " in UB cannot find the corresponding row in SinvB" << std::endl
                      << "UB.cols    = " << UB.cols << std::endl
                      << "UinvB.cols = " << SinvB.cols << std::endl;
                    ErrorHandling( msg.str().c_str() );
                  }
                }


                // Transfer the values from Sinv to AinvBlock
                T* nzvalSinv = SinvB.nzval.Data();
                Int     ldSinv    = SinvB.numRow;
                

                  for( Int j = 0; j < UB.numCol; j++ ){
                    for( Int i = 0; i < LB.numRow; i++ ){
                      nzvalAinv[i+j*ldAinv] =
                        nzvalSinv[relRows[i] + relCols[j] * ldSinv];//这里就是将Sinv Block复制到Ainv的对应地方去了
                    }
                  }

#endif

                isBlockFound = true;
                break;
              }
            } // for (jbSinv)
            TIMER_STOP(PARSING_COL_BLOCKIDX);
            if( isBlockFound == false ){
              std::ostringstream msg;
              msg << "Block(" << isup << ", " << jsup 
                << ") did not find a matching block in Sinv." << std::endl;
              ErrorHandling( msg.str().c_str() );
            }
          } // if (isup, jsup) is in U

        } // for( ib )
      } // for ( jb )


#endif
      TIMER_STOP(JB_Loop);


      TIMER_STOP(Compute_Sinv_LT_Lookup_Indexes);
    }

  template<typename T>
    inline void PMatrix<T>::SendRecvCD_UpdateU(
        std::vector<SuperNodeBufferType > & arrSuperNodes, 
        Int stepSuper)
    {

      TIMER_START(Send_CD_Update_U);
      //compute the number of requests
      Int sendCount = 0;//发送的数量
      Int recvCount = 0;//接受的数量
      Int sendOffset[stepSuper];//每个supernode的发送偏移量
      Int recvOffset[stepSuper];//每个supernode的接受偏移量
      Int recvIdx=0;
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        sendOffset[supidx]=sendCount;
        recvOffset[supidx]=recvCount;
        sendCount+= CountSendToCrossDiagonal(snode.Index);//需要发送到对角线的数量
        recvCount+= CountRecvFromCrossDiagonal(snode.Index);//需要从对角线接受的数量
      }


      std::vector<MPI_Request > arrMpiReqsSendCD(sendCount, MPI_REQUEST_NULL );//发送内容的句柄数组？
      std::vector<MPI_Request > arrMpiReqsSizeSendCD(sendCount, MPI_REQUEST_NULL );//发送Size的句柄数组？

      std::vector<MPI_Request > arrMpiReqsRecvCD(recvCount, MPI_REQUEST_NULL );//接受内容的句柄数组？
      std::vector<MPI_Request > arrMpiReqsSizeRecvCD(recvCount, MPI_REQUEST_NULL );//接受Size的句柄数组？
      std::vector<std::vector<char> > arrSstrLcolSendCD(sendCount);//保存发送内容的本地数组
      std::vector<int > arrSstrLcolSizeSendCD(sendCount);//保存发送大小的本地数组
      std::vector<std::vector<char> > arrSstrLcolRecvCD(recvCount);//保存接受内容的本地数组
      std::vector<int > arrSstrLcolSizeRecvCD(recvCount);//保存接受大小的本地数组

      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];

        // Send LUpdateBufReduced to the cross diagonal blocks. 
        // NOTE: This assumes square processor grid

        TIMER_START(Send_L_CrossDiag);

        if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) && isSendToCrossDiagonal_(grid_->numProcCol, snode.Index ) ){//如果本processor保存了这个supernode的一部分，并且确实需要发送这个supernode

          Int sendIdx = 0;
          for(Int dstCol = 0; dstCol<grid_->numProcCol; dstCol++){
            if(isSendToCrossDiagonal_(dstCol,snode.Index) ){
              Int dest = PNUM(PROW(snode.Index,grid_),dstCol,grid_);//得到目标processor

              if( MYPROC( grid_ ) != dest	){//自己不是目标processor
                MPI_Request & mpiReqSizeSend = arrMpiReqsSizeSendCD[sendOffset[supidx]+sendIdx];//发送大小句柄
                MPI_Request & mpiReqSend = arrMpiReqsSendCD[sendOffset[supidx]+sendIdx];//发送内容句柄


                std::stringstream sstm;
                std::vector<char> & sstrLcolSend = arrSstrLcolSendCD[sendOffset[supidx]+sendIdx];
                Int & sstrSize = arrSstrLcolSizeSendCD[sendOffset[supidx]+sendIdx];
                //打包Ainv的内容
                serialize( snode.RowLocalPtr, sstm, NO_MASK );
                serialize( snode.BlockIdxLocal, sstm, NO_MASK );
                serialize( snode.LUpdateBuf, sstm, NO_MASK );

                sstrLcolSend.resize( Size(sstm) );
                sstm.read( &sstrLcolSend[0], sstrLcolSend.size() );//将内容发送到本地数组
                sstrSize = sstrLcolSend.size();//将大小保存在本地数组

#if ( _DEBUGlevel_ >= 1 )
                assert(IDX_TO_TAG(snode.Rank,SELINV_TAG_L_SIZE_CD,limIndex_)<=maxTag_);
                assert(IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_)<=maxTag_);
#endif
                //发送
                MPI_Isend( &sstrSize, sizeof(sstrSize), MPI_BYTE, dest, IDX_TO_TAG(snode.Rank,SELINV_TAG_L_SIZE_CD,limIndex_), grid_->comm, &mpiReqSizeSend );
                MPI_Isend( (void*)&sstrLcolSend[0], sstrSize, MPI_BYTE, dest, IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_), grid_->comm, &mpiReqSend );

                PROFILE_COMM(MYPROC(this->grid_),dest,IDX_TO_TAG(snode.Rank,SELINV_TAG_L_SIZE_CD,limIndex_),sizeof(sstrSize));
                PROFILE_COMM(MYPROC(this->grid_),dest,IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_),sstrSize);


                sendIdx++;
              }
            }
          }


        } // sender
        TIMER_STOP(Send_L_CrossDiag);
      }


      //Do Irecv for sizes
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        //If I'm a receiver
        if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) && isRecvFromCrossDiagonal_(grid_->numProcRow, snode.Index ) ){//如果我是这个supernode的row
          Int recvIdx=0;
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,snode.Index) ){
              Int src = PNUM(srcRow,PCOL(snode.Index,grid_),grid_);//得到src的processor
              if( MYPROC( grid_ ) != src ){
                Int & sstrSize = arrSstrLcolSizeRecvCD[recvOffset[supidx]+recvIdx];
                MPI_Request & mpiReqSizeRecv = arrMpiReqsSizeRecvCD[recvOffset[supidx]+recvIdx];

#if ( _DEBUGlevel_ >= 1 )
                assert(IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_)<=maxTag_);
#endif
                //接受大小
                MPI_Irecv( &sstrSize, 1, MPI_INT, src, IDX_TO_TAG(snode.Rank,SELINV_TAG_L_SIZE_CD,limIndex_), grid_->comm, &mpiReqSizeRecv);
                recvIdx++;
              }
            }
          }
        }//end if I'm a receiver
      }

      //waitall sizes
      mpi::Waitall(arrMpiReqsSizeRecvCD);//等所有的size接受完
      //Allocate content and do Irecv
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        //If I'm a receiver
        if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) && isRecvFromCrossDiagonal_(grid_->numProcRow, snode.Index ) ){//如果本processor是一个receiver
          Int recvIdx=0;
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,snode.Index) ){
              Int src = PNUM(srcRow,PCOL(snode.Index,grid_),grid_);
              if( MYPROC( grid_ ) != src ){
                Int & sstrSize = arrSstrLcolSizeRecvCD[recvOffset[supidx]+recvIdx];
                std::vector<char> & sstrLcolRecv = arrSstrLcolRecvCD[recvOffset[supidx]+recvIdx];
                MPI_Request & mpiReqRecv = arrMpiReqsRecvCD[recvOffset[supidx]+recvIdx];
                sstrLcolRecv.resize( sstrSize);

#if ( _DEBUGlevel_ >= 1 )
                assert(IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_)<=maxTag_);
#endif
                MPI_Irecv( (void*)&sstrLcolRecv[0], sstrSize, MPI_BYTE, src, IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT_CD,limIndex_), grid_->comm, &mpiReqRecv );//接受内容
                recvIdx++;
              }
            }
          }
        }//end if I'm a receiver
      }

      //waitall content
      mpi::Waitall(arrMpiReqsRecvCD);//等所有内容都接受完
      //Do the work
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];

        // Send LUpdateBufReduced to the cross diagonal blocks. 
        // NOTE: This assumes square processor grid
        if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) && isRecvFromCrossDiagonal_(grid_->numProcRow, snode.Index ) ){

#if ( _DEBUGlevel_ >= 1 )
          statusOFS << std::endl <<  " ["<<snode.Index<<"] "<<  "Update the upper triangular block" << std::endl << std::endl; 
          statusOFS << std::endl << " ["<<snode.Index<<"] "<<   "blockIdxLocal:" << snode.BlockIdxLocal << std::endl << std::endl; 
          statusOFS << std::endl << " ["<<snode.Index<<"] "<<   "rowLocalPtr:" << snode.RowLocalPtr << std::endl << std::endl; 
#endif

          std::vector<UBlock<T> >&  Urow = this->U( LBi( snode.Index, grid_ ) );//找到本地保存的所有U block，准备进行覆盖了
          std::vector<bool> isBlockFound(Urow.size(),false);

          recvIdx=0;
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,snode.Index) ){
              Int src = PNUM(srcRow,PCOL(snode.Index,grid_),grid_);//找到src processor
              TIMER_START(Recv_L_CrossDiag);

              std::vector<Int> rowLocalPtrRecv;
              std::vector<Int> blockIdxLocalRecv;
              NumMat<T> UUpdateBuf;
              //对应src processor对数据进行解码
              if( MYPROC( grid_ ) != src ){
                std::stringstream sstm;
                Int & sstrSize = arrSstrLcolSizeRecvCD[recvOffset[supidx]+recvIdx];
                std::vector<char> & sstrLcolRecv = arrSstrLcolRecvCD[recvOffset[supidx]+recvIdx];
                sstm.write( &sstrLcolRecv[0], sstrSize );

                deserialize( rowLocalPtrRecv, sstm, NO_MASK );
                deserialize( blockIdxLocalRecv, sstm, NO_MASK );
                deserialize( UUpdateBuf, sstm, NO_MASK );	

                recvIdx++;

              } // sender is not the same as receiver
              else{
                rowLocalPtrRecv   = snode.RowLocalPtr;
                blockIdxLocalRecv = snode.BlockIdxLocal;
                UUpdateBuf = snode.LUpdateBuf;
              } // sender is the same as receiver



              TIMER_STOP(Recv_L_CrossDiag);

#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<" ["<<snode.Index<<"] P"<<MYPROC(grid_)<<" ("<<MYROW(grid_)<<","<<MYCOL(grid_)<<") <--- LBj("<<snode.Index<<") <--- P"<<src<<std::endl;
              statusOFS << std::endl << " ["<<snode.Index<<"] "<<   "rowLocalPtrRecv:" << rowLocalPtrRecv << std::endl << std::endl; 
              statusOFS << std::endl << " ["<<snode.Index<<"] "<<   "blockIdxLocalRecv:" << blockIdxLocalRecv << std::endl << std::endl; 
#endif


              // Update U
              for( Int ib = 0; ib < blockIdxLocalRecv.size(); ib++ ){
                for( Int jb = 0; jb < Urow.size(); jb++ ){
                  UBlock<T>& UB = Urow[jb];
                  if( UB.blockIdx == blockIdxLocalRecv[ib] ){//找到对应的block进行覆盖
                    NumMat<T> Ltmp ( UB.numCol, UB.numRow );
                    lapack::Lacpy( 'A', Ltmp.m(), Ltmp.n(), 
                        &UUpdateBuf( rowLocalPtrRecv[ib], 0 ),
                        UUpdateBuf.m(), Ltmp.Data(), Ltmp.m() );
                    isBlockFound[jb] = true;
                    Transpose( Ltmp, UB.nzval );//当然覆盖过来的需要转置一下
                    break;
                  }
                }
              }
            }
          }

          for( Int jb = 0; jb < Urow.size(); jb++ ){
            UBlock<T>& UB = Urow[jb];
            if( !isBlockFound[jb] ){
              ErrorHandling( "UBlock cannot find its update. Something is seriously wrong." );
            }
          }
        } // receiver
      }

      TIMER_STOP(Send_CD_Update_U);

      mpi::Waitall(arrMpiReqsSizeSendCD);
      mpi::Waitall(arrMpiReqsSendCD);
    }; 

  template<typename T>
    inline void PMatrix<T>::UnpackData(
        SuperNodeBufferType & snode, 
        std::vector<LBlock<T> > & LcolRecv, 
        std::vector<UBlock<T> > & UrowRecv )
    {
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Unpack the received data for processors participate in Gemm. " << std::endl << std::endl; 
#endif
      // U part
      if( MYROW( grid_ ) != PROW( snode.Index, grid_ ) ){
        std::stringstream sstm;
        sstm.write( &snode.SstrUrowRecv[0], snode.SstrUrowRecv.size() );
        std::vector<Int> mask( UBlockMask::TOTAL_NUMBER, 1 );
        Int numUBlock;
        deserialize( numUBlock, sstm, NO_MASK );
        UrowRecv.resize( numUBlock );
        for( Int jb = 0; jb < numUBlock; jb++ ){
          deserialize( UrowRecv[jb], sstm, mask );
        } 
      } // sender is not the same as receiver
      else{
        // U is obtained locally, just make a copy. Include everything
        // (there is no diagonal block)
        // Is it a copy ?  LL: YES. Maybe we should replace the copy by
        // something more efficient especially for mpisize == 1
        UrowRecv.resize(this->U( LBi( snode.Index, grid_ ) ).size());
        std::copy(this->U( LBi( snode.Index, grid_ ) ).begin(),this->U( LBi( snode.Index, grid_ )).end(),UrowRecv.begin());
      } // sender is the same as receiver


      //L part
      if( MYCOL( grid_ ) != PCOL( snode.Index, grid_ ) ){
        std::stringstream     sstm;
        sstm.write( &snode.SstrLcolRecv[0], snode.SstrLcolRecv.size() );
        std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
        mask[LBlockMask::NZVAL] = 0; // nzval is excluded
        Int numLBlock;
        deserialize( numLBlock, sstm, NO_MASK );
        LcolRecv.resize( numLBlock );
        for( Int ib = 0; ib < numLBlock; ib++ ){
          deserialize( LcolRecv[ib], sstm, mask );
        }
      } // sender is not the same as receiver
      else{
        // L is obtained locally, just make a copy. 
        // Do not include the diagonal block
        std::vector<LBlock<T> >& Lcol =  this->L( LBj( snode.Index, grid_ ) );
        Int startIdx = ( MYROW( grid_ ) == PROW( snode.Index, grid_ ) )?1:0;
        LcolRecv.resize( Lcol.size() - startIdx );
        std::copy(Lcol.begin()+startIdx,Lcol.end(),LcolRecv.begin());
      } // sender is the same as receiver
    }

  template<typename T>
    inline void PMatrix<T>::ComputeDiagUpdate(SuperNodeBufferType & snode, cublasHandle_t& handle, std::set<std::string> quantSuperNode)
    {

      //---------Computing  Diagonal block, all processors in the column are participating to all pipelined supernodes
      if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){ //如果本processor和这个supernode在一列，即Ljk
#if ( _DEBUGlevel_ >= 2 )
        statusOFS << std::endl << "["<<snode.Index<<"] "<<   "Updating the diagonal block" << std::endl << std::endl; 
#endif
        std::vector<LBlock<T> >&  Lcol = this->L( LBj( snode.Index, grid_ ) ); //得到本地的所有L block

        //Allocate DiagBuf even if Lcol.size() == 0
        snode.DiagBuf.Resize(SuperSize( snode.Index, super_ ), SuperSize( snode.Index, super_ )); //设置本地的Lkk
        SetValue(snode.DiagBuf, ZERO<T>());

        
        std::stringstream ss;
        // Do I own the diagonal block ?
        Int startIb = (MYROW( grid_ ) == PROW( snode.Index, grid_ ))?1:0;//如果我拥有Lkk，那么要跳过这个block

        // NumMat<float> LUpdateBuf_quant(snode.LUpdateBuf.m(), snode.LUpdateBuf.n());
        // for(int j = 0;j<LUpdateBuf_quant.n();j++){
        //   for(int i = 0;i<LUpdateBuf_quant.m();i++){
        //     LUpdateBuf_quant(i, j) = (float)snode.LUpdateBuf(i, j);
        //   }
        // }
        for( Int ib = startIb; ib < Lcol.size(); ib++ ){

#ifdef GEMM_PROFILE
          gemm_stat.push_back(snode.DiagBuf.m());
          gemm_stat.push_back(snode.DiagBuf.n());
          gemm_stat.push_back(Lcol[ib].numRow);
#endif

          LBlock<T> & LB = Lcol[ib];
          
          ss.str("");
          ss<<LB.blockIdx << "," <<snode.Index;

          if(quantSuperNode.find(ss.str()) == quantSuperNode.end()){
            //下面应该是第四步，意思是：
            //Lkk = -1 * (A^-1)^T * L + 1 * Lkk
            //但是这里为什么是A-1的转置不太懂
            //这里Lik和A-1的下标都是一样的，所以直接判断Lik的坐标就可以进行量化了

            // NumMat<double> tmpCuda(snode.DiagBuf.m(), snode.DiagBuf.n());
            NumMat<double> tmp_quant(LB.numRow, snode.DiagBuf.m());
            for(int j = 0;j<tmp_quant.n(); j++){
              for(int i = 0;i<tmp_quant.m();i++){
                tmp_quant(i, j) = snode.LUpdateBuf(snode.RowLocalPtr[ib - startIb] + i, j);
              }
            }
            gpu_blas_dmmul(handle, 'T', 'N', snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow, 
                MINUS_ONE<T>(), tmp_quant.Data(), tmp_quant.m(),
                LB.nzval.Data(), LB.nzval.m(),ONE<T>(), snode.DiagBuf.Data(), snode.DiagBuf.m() );

          }else{
            //进行量化，但是由于精度不同，应该分为两步
            //先把A-1和L进行量化
            // std::cout<<"Step 5 : Quant "<<ss.str()<<std::endl;
            

            // double* LUpdateBuf_data =  &snode.LUpdateBuf( snode.RowLocalPtr[ib-startIb], 0 );
            // for(int j = 0;j<LUpdateBuf_quant.n();j++){
            //   for(int i = 0;i<LUpdateBuf_quant.m();i++){
            //     LUpdateBuf_quant(i, j) = (float)LUpdateBuf_data[i + j * LUpdateBuf_quant.m()];
            //   }
            // }
            NumMat<float> tmp_quant(LB.numRow, snode.DiagBuf.m());
            for(int j = 0;j<tmp_quant.n(); j++){
              for(int i = 0;i<tmp_quant.m();i++){
                tmp_quant(i, j) = (float)snode.LUpdateBuf(snode.RowLocalPtr[ib - startIb] + i, j);
              }
            }
            NumMat<float> LB_quant(LB.numRow, LB.numCol);
            for(int j = 0;j<LB_quant.n();j++){
              for(int i = 0;i<LB_quant.m();i++){
                LB_quant(i, j) = (float)LB.nzval(i, j);
              }
            }

            //然后用一个临时的量化Lkk保存它们的结果，而不能直接加进来
            NumMat<float> DiagBuf_quant(SuperSize( snode.Index, super_ ), SuperSize( snode.Index, super_ ));

            gpu_blas_smmul(handle, 'T', 'N', snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow, 
                MINUS_ONE<float>(), tmp_quant.Data(), tmp_quant.m(),
                LB_quant.Data(), LB.nzval.m(), ZERO<float>(), DiagBuf_quant.Data(), snode.DiagBuf.m() );
            // blas::Gemm( 'T', 'N', snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow, 
            //     MINUS_ONE<float>(), tmp_quant.Data(), tmp_quant.m(),
            //     LB_quant.Data(), LB.nzval.m(), ZERO<float>(), DiagBuf_quant.Data(), snode.DiagBuf.m() );

            // blas::Gemm( 'T', 'N', snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow, 
            //     MINUS_ONE<float>(), &LUpdateBuf_quant( snode.RowLocalPtr[ib-startIb], 0 ), snode.LUpdateBuf.m(),
            //     LB_quant.Data(), LB.nzval.m(), ZERO<float>(), DiagBuf_quant.Data(), snode.DiagBuf.m() );
            //最后把结果加到Lkk中
            for(int j = 0;j<DiagBuf_quant.n();j++){
              for(int i = 0;i<DiagBuf_quant.m();i++){
                snode.DiagBuf(i, j) = snode.DiagBuf(i, j) + (double)DiagBuf_quant(i, j);
              }
            }

            //释放资源
            DiagBuf_quant.deallocate();
            LB_quant.deallocate();
            tmp_quant.deallocate();
          }
          
#ifdef _PRINT_STATS_
                this->localFlops_+=flops::Gemm<T>(snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow);
#endif
        } 
        // LUpdateBuf_quant.deallocate();

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "["<<snode.Index<<"] "<<   "Updated the diagonal block" << std::endl << std::endl; 
#endif
      }
    }



  template<typename T>
    void PMatrix<T>::GetEtree(std::vector<Int> & etree_supno )
    {
      Int nsupers = this->NumSuper();

#ifndef _RELEASE_
      double begin =  MPI_Wtime( );
#endif


      if( optionsFact_->ColPerm != "PARMETIS" ) {
        /* Use the etree computed from serial symb. fact., and turn it
           into supernodal tree.  */
        const SuperNodeType * superNode = this->SuperNode();


        //translate from columns to supernodes etree using supIdx
        etree_supno.resize(this->NumSuper());
        for(Int i = 0; i < superNode->etree.m(); ++i){
          Int curSnode = superNode->superIdx[i];
          Int parentSnode = (superNode->etree[i]>= superNode->etree.m()) ?this->NumSuper():superNode->superIdx[superNode->etree[i]];
          if( curSnode != parentSnode){
            etree_supno[curSnode] = parentSnode;
          }
        }

      } else { /* ParSymbFACT==YES and SymPattern==YES  and RowPerm == NOROWPERM */
        /* Compute an "etree" based on struct(L), 
           assuming struct(U) = struct(L').   */

        /* find the first block in each supernodal-column of local L-factor */
        std::vector<Int> etree_supno_l( nsupers, nsupers  );
        for( Int ksup = 0; ksup < nsupers; ksup++ ){
          if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
            // L part
            std::vector<LBlock<T> >& Lcol = this->L( LBj(ksup, grid_) );
            if(Lcol.size()>0){
              Int firstBlk = 0;
              if( MYROW( grid_ ) == PROW( ksup, grid_ ) )
                firstBlk=1;

              for( Int ib = firstBlk; ib < Lcol.size(); ib++ ){
                etree_supno_l[ksup] = std::min(etree_supno_l[ksup] , Lcol[ib].blockIdx);
              }
            }
          }
        }


#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << " Local supernodal elimination tree is " << etree_supno_l <<std::endl<<std::endl;

#endif
        /* form global e-tree */
        etree_supno.resize( nsupers );
        mpi::Allreduce( (Int*) &etree_supno_l[0],(Int *) &etree_supno[0], nsupers, MPI_MIN, grid_->comm );
        etree_supno[nsupers-1]=nsupers;
      }

#ifndef _RELEASE_
      double end =  MPI_Wtime( );
      statusOFS<<"Building the list took "<<end-begin<<"s"<<std::endl;
#endif
    } 		// -----  end of method PMatrix::GetEtree  ----- 


  template<typename T>
    inline void PMatrix<T>::GetWorkSet(
        std::vector<Int> & snodeEtree, 
        std::vector<std::vector<Int> > & WSet)
    {
      TIMER_START(Compute_WorkSet);
      Int numSuper = this->NumSuper();


      if (options_->maxPipelineDepth!=1){

        Int maxDepth = options_->maxPipelineDepth;
        maxDepth=maxDepth==-1?std::numeric_limits<Int>::max():maxDepth;
#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"MaxDepth is "<<maxDepth<<std::endl;
#endif

        //find roots in the supernode etree (it must be postordered)
        //initialize the parent we are looking at 
        //Int rootParent = snodeEtree[numSuper-2];
        Int rootParent = numSuper;

        //compute the level of each supernode and the total number of levels
        //IntNumVec level(numSuper);
        //level(rootParent)=0;
        IntNumVec level(numSuper+1);
        level(rootParent)=-1;
        Int numLevel = 0; 
        for(Int i=rootParent-1; i>=0; i-- ){ 
          level[i] = level[snodeEtree[i]]+1;
          numLevel = std::max(numLevel, level[i]);
        }
        numLevel++;

        //Compute the number of supernodes at each level
        IntNumVec levelSize(numLevel);
        SetValue(levelSize,I_ZERO);
        //for(Int i=rootParent-1; i>=0; i-- ){ levelSize(level(i))++; } 
        for(Int i=rootParent-1; i>=0; i-- ){ 
          if(level[i]>=0){ 
            levelSize[level[i]]++; 
          }
        }

        //Allocate memory
        WSet.resize(numLevel,std::vector<Int>());
        for(Int i=0; i<numLevel; i++ ){
          WSet[i].reserve(levelSize(i));
        }

        //Fill the worklist based on the level of each supernode
        for(Int i=rootParent-1; i>=0; i-- ){
          ////TODO do I have something to do here ?
          //  TreeBcast * bcastLTree = fwdToRightTree_[i];
          //  TreeBcast * bcastUTree = fwdToBelowTree_[i];
          //  TreeReduce<T> * redLTree = redToLeftTree_[i];
          //  TreeReduce<T> * redDTree = redToAboveTree_[i];
          //
          //bool participating = MYROW( grid_ ) == PROW( i, grid_ ) || MYCOL( grid_ ) == PCOL( i, grid_ )
          //  || CountSendToRight(i) > 0
          //  || CountSendToBelow(i) > 0
          //  || CountSendToCrossDiagonal(i) > 0
          //  || CountRecvFromCrossDiagonal(i) >0
          //  || ( isRecvFromLeft_( i ) ) 
          //  || ( isRecvFromAbove_( i ) )
          //  || isSendToDiagonal_(i)
          //  || (bcastUTree!=NULL)
          //  || (bcastLTree!=NULL)
          //  || (redLTree!=NULL)
          //  || (redDTree!=NULL) ;
          //participating = true;
          //if( participating){
          WSet[level[i]].push_back(i);  
          //}

        }

#if 1
        //Constrain the size of each list to be min(MPI_MAX_COMM,options_->maxPipelineDepth)
        Int limit = maxDepth; //(options_->maxPipelineDepth>0)?std::min(MPI_MAX_COMM,options_->maxPipelineDepth):MPI_MAX_COMM;
        Int rank = 0;
        for (Int lidx=0; lidx<WSet.size() ; lidx++){
          //Assign a rank in the order they are processed ?

          bool split = false;
          Int splitIdx = 0;

          Int orank = rank;
          Int maxRank = rank + WSet[lidx].size()-1;
          //          Int maxRank = WSet[lidx].back();
#if ( _DEBUGlevel_ >= 1 )
          statusOFS<< (Int)(rank/limIndex_) << "  vs2  "<<(Int)(maxRank/limIndex_)<<std::endl;
#endif
          if( (Int)(rank/limIndex_) != (Int)(maxRank/limIndex_)){
            split = true;
            //splitIdx = maxRank - maxRank%limIndex_ - rank; 
            splitIdx = limIndex_-(rank%limIndex_)-1;
          }

          Int splitPoint = std::min((Int)WSet[lidx].size()-1,splitIdx>0?std::min(limit-1,splitIdx):limit-1);
#if ( _DEBUGlevel_ >= 1 )
          if(split){ 
            statusOFS<<"TEST SPLIT at "<<splitIdx<<" "<<std::endl;
          }
#endif
          split = split || (WSet[lidx].size()>limit);

          rank += splitPoint+1;

          if( split && splitPoint>0)
          {
            statusOFS<<"SPLITPOINT is "<<splitPoint<<" "<<std::endl;
#if ( _DEBUGlevel_ >= 1 )
            if(splitPoint != limit-1){
              statusOFS<<"-----------------------"<<std::endl;
              statusOFS<<lidx<<": "<<orank<<" -- "<<maxRank<<std::endl;
              statusOFS<<"            is NOW         "<<std::endl;
              statusOFS<<lidx<<": "<<orank<<" -- "<<rank-1<<std::endl;
              statusOFS<<lidx+1<<": "<<rank<<" -- "<<maxRank<<std::endl;
              statusOFS<<"-----------------------"<<std::endl;
            }
#endif
            std::vector<std::vector<Int> >::iterator pos = WSet.begin()+lidx+1;               
            WSet.insert(pos,std::vector<Int>());
            WSet[lidx+1].insert(WSet[lidx+1].begin(),WSet[lidx].begin() + splitPoint+1 ,WSet[lidx].end());
            WSet[lidx].erase(WSet[lidx].begin()+splitPoint+1,WSet[lidx].end());

#if ( _DEBUGlevel_ >= 1 )
            if(splitPoint != limit-1){
              assert((orank+WSet[lidx].size()-1)%limIndex_==0);
            }
#endif




            //statusOFS<<"-----------------------"<<std::endl;
            //for(auto it = WSet[lidx].begin();it !=WSet[lidx].end(); it++){statusOFS<<*it<<" ";}statusOFS<<std::endl;
            //for(auto it = WSet[lidx+1].begin();it !=WSet[lidx+1].end(); it++){statusOFS<<*it<<" ";}statusOFS<<std::endl;
            //statusOFS<<"-----------------------"<<std::endl;

            ////            std::vector<std::vector<Int> >::iterator pos2 = workingRanks_.begin()+lidx+1;               
            ////            workingRanks_.insert(pos2,std::vector<Int>());
            ////            workingRanks_[lidx+1].insert(workingRanks_[lidx+1].begin(),workingRanks_[lidx].begin() + splitPoint+1 ,workingRanks_[lidx].end());
            ////
            ////            workingRanks_[lidx].erase(workingRanks_[lidx].begin()+splitPoint+1,workingRanks_[lidx].end());
            ////
            ////
            ////statusOFS<<"-----------------------"<<std::endl;
            ////statusOFS<<"            is NOW         "<<std::endl;
            ////statusOFS<<workingRanks_[lidx].front()<<" -- "<<workingRanks_[lidx].back()<<std::endl;
            ////statusOFS<<workingRanks_[lidx+1].front()<<" -- "<<workingRanks_[lidx+1].back()<<std::endl;
            ////statusOFS<<"-----------------------"<<std::endl;

            //         //THERE IS A SPECIAL CASE: list longer than limit AND splitidx in between: we have to check if we don't have to do multiple splits 
            //            if(splitPoint<splitIdx){
            //              Int newSplitIdx = splitIdx - splitPoint;
            //
            //              pos = WSet.begin()+lidx+2;               
            //              WSet.insert(pos,std::vector<Int>());
            //              WSet[lidx+2].insert(WSet[lidx+2].begin(),WSet[lidx+1].begin() + newSplitIdx+1 ,WSet[lidx+1].end());
            //              WSet[lidx+1].erase(WSet[lidx+1].begin()+newSplitIdx+1,WSet[lidx+1].end());
            //
            //              pos2 = workingRanks_.begin()+lidx+2;               
            //              workingRanks_.insert(pos2,std::vector<Int>());
            //              workingRanks_[lidx+2].insert(workingRanks_[lidx+2].begin(),workingRanks_[lidx+1].begin() + newSplitIdx+1 ,workingRanks_[lidx+1].end());
            //
            //statusOFS<<"SECOND SPLITPOINT IS "<<newSplitIdx<<std::endl;
            //statusOFS<<"-----------------------"<<std::endl;
            //statusOFS<<workingRanks_[lidx+1].front()<<" -- "<<workingRanks_[lidx+1].back()<<std::endl;
            //              workingRanks_[lidx+1].erase(workingRanks_[lidx+1].begin()+newSplitIdx+1,workingRanks_[lidx+1].end());
            //statusOFS<<"            is NOW         "<<std::endl;
            //statusOFS<<workingRanks_[lidx+1].front()<<" -- "<<workingRanks_[lidx+1].back()<<std::endl;
            //statusOFS<<workingRanks_[lidx+2].front()<<" -- "<<workingRanks_[lidx+2].back()<<std::endl;
            //statusOFS<<"-----------------------"<<std::endl;
            //            }

          }


        }

        //for (Int lidx=0; lidx<WSet.size() ; lidx++){
        //  std::sort(WSet[lidx].begin(),WSet[lidx].end();WSet[lidx].size());
        //}

        //filter out non participating ?
        // for(Int i=rootParent-1; i>=0; i-- ){
        //     ////TODO do I have something to do here ?
        //     //  TreeBcast * bcastLTree = fwdToRightTree_[i];
        //     //  TreeBcast * bcastUTree = fwdToBelowTree_[i];
        //     //  TreeReduce<T> * redLTree = redToLeftTree_[i];
        //     //  TreeReduce<T> * redDTree = redToAboveTree_[i];
        //     //
        //     //bool participating = MYROW( grid_ ) == PROW( i, grid_ ) || MYCOL( grid_ ) == PCOL( i, grid_ )
        //     //  || CountSendToRight(i) > 0
        //     //  || CountSendToBelow(i) > 0
        //     //  || CountSendToCrossDiagonal(i) > 0
        //     //  || CountRecvFromCrossDiagonal(i) >0
        //     //  || ( isRecvFromLeft_( i ) ) 
        //     //  || ( isRecvFromAbove_( i ) )
        //     //  || isSendToDiagonal_(i)
        //     //  || (bcastUTree!=NULL)
        //     //  || (bcastLTree!=NULL)
        //     //  || (redLTree!=NULL)
        //     //  || (redDTree!=NULL) ;
        //     //participating = true;
        //     //if(! participating){
        //     //  WSet[level[i]].erase(i);  
        //     //}

        // }
#endif



      }
      else{
        for( Int ksup = numSuper - 2; ksup >= 0; ksup-- ){
          WSet.push_back(std::vector<Int>());
          WSet.back().push_back(ksup);
        }

      }




      TIMER_STOP(Compute_WorkSet);
#if ( _DEBUGlevel_ >= 1 )
      for (Int lidx=0; lidx<WSet.size() ; lidx++){
        statusOFS << std::endl << "L"<< lidx << " is: {";
        for (Int supidx=0; supidx<WSet[lidx].size() ; supidx++){
          statusOFS << WSet[lidx][supidx] << " ["<<snodeEtree[WSet[lidx][supidx]]<<"] ";
        }
        statusOFS << " }"<< std::endl;
      }
#endif
    }

  template<typename T>
    inline void PMatrix<T>::SelInvIntra_P2p(Int lidx,Int & rank, cublasHandle_t& handle, std::set<std::string> & quantSuperNode ) {
      //这是一次并行
      if (options_->symmetricStorage!=1){
#if defined (PROFILE) || defined(PMPI) || defined(USE_TAU)
      Real begin_SendULWaitContentFirst, end_SendULWaitContentFirst, time_SendULWaitContentFirst = 0;
#endif
      Int numSuper = this->NumSuper(); //得到总的supernode数量
      std::vector<std::vector<Int> > & superList = this->WorkingSet();//得到可并行操作的supernode数组
      Int numSteps = superList.size();//总的并行次数
      Int stepSuper = superList[lidx].size(); //本次并行的supernode数量




      TIMER_START(AllocateBuffer);//分配缓存

      stepSuper = 0;//为啥又赋值0了= - =，逗我玩呢
      for (Int supidx=0; supidx<superList[lidx].size(); supidx++){//这里应该是对本次并行的每个supernode进行操作
        Int snodeIdx = superList[lidx][supidx]; //得到要操作的supernode下标
        TreeBcast * bcastLTree = fwdToRightTree_[snodeIdx];//这个supernode要往右边发送的L数据
        TreeBcast * bcastUTree = fwdToBelowTree_[snodeIdx];//这个supernode要往下面发送的U数据
        TreeReduce<T> * redLTree = redToLeftTree_[snodeIdx];//这个supernode要从左边接收到的L数据
        TreeReduce<T> * redDTree = redToAboveTree_[snodeIdx];//这个supernode要从上面接受到的U数据（对称的，L矩阵是右边为root，U矩阵是下面为root）
        bool participating = MYROW( grid_ ) == PROW( snodeIdx, grid_ ) 
          || MYCOL( grid_ ) == PCOL( snodeIdx, grid_ )
          || CountSendToRight(snodeIdx) > 0
          || CountSendToBelow(snodeIdx) > 0
          || CountSendToCrossDiagonal(snodeIdx) > 0
          || CountRecvFromCrossDiagonal(snodeIdx) >0
          || ( isRecvFromLeft_( snodeIdx ) ) 
          || ( isRecvFromAbove_( snodeIdx ) )
          || isSendToDiagonal_(snodeIdx)
          || (bcastUTree!=NULL)
          || (bcastLTree!=NULL)
          || (redLTree!=NULL)
          || (redDTree!=NULL) ;//应该是如果这个supernode有上面任何一个情况，就是后面需要操作的supernode
        if(participating){
          stepSuper++;//记录需要操作的supernode数量
        }
      }



      //This is required to send the size and content of U/L
      std::vector<std::vector<MPI_Request> >  arrMpireqsSendToBelow;//发送到下边的句柄
      arrMpireqsSendToBelow.resize( stepSuper, std::vector<MPI_Request>( 2 * grid_->numProcRow, MPI_REQUEST_NULL ));//2倍是因为LU，下面同理
      std::vector<std::vector<MPI_Request> >  arrMpireqsSendToRight;//发送到右边的句柄
      arrMpireqsSendToRight.resize(stepSuper, std::vector<MPI_Request>( 2 * grid_->numProcCol, MPI_REQUEST_NULL ));


      //This is required to receive the size and content of U/L
      std::vector<MPI_Request>   arrMpireqsRecvSizeFromAny;//接受size的句柄
      arrMpireqsRecvSizeFromAny.resize(stepSuper*2 , MPI_REQUEST_NULL);
      std::vector<MPI_Request>   arrMpireqsRecvContentFromAny;//接受内容的句柄
      arrMpireqsRecvContentFromAny.resize(stepSuper*2 , MPI_REQUEST_NULL);

      //allocate the buffers for this supernode
      //用来缓冲supernode的数组
      std::vector<SuperNodeBufferType> arrSuperNodes(stepSuper);
      Int pos = 0;
      for (Int supidx=0; supidx<superList[lidx].size(); supidx++){ 
        Int snodeIdx = superList[lidx][supidx]; 
        TreeBcast * bcastLTree = fwdToRightTree_[snodeIdx];
        TreeBcast * bcastUTree = fwdToBelowTree_[snodeIdx];
        TreeReduce<T> * redLTree = redToLeftTree_[snodeIdx];
        TreeReduce<T> * redDTree = redToAboveTree_[snodeIdx];
        bool participating = MYROW( grid_ ) == PROW( snodeIdx, grid_ ) 
          || MYCOL( grid_ ) == PCOL( snodeIdx, grid_ )
          || CountSendToRight(snodeIdx) > 0
          || CountSendToBelow(snodeIdx) > 0
          || CountSendToCrossDiagonal(snodeIdx) > 0
          || CountRecvFromCrossDiagonal(snodeIdx) >0
          || ( isRecvFromLeft_( snodeIdx ) ) 
          || ( isRecvFromAbove_( snodeIdx ) )
          || isSendToDiagonal_(snodeIdx)
          || (bcastUTree!=NULL)
          || (bcastLTree!=NULL)
          || (redLTree!=NULL)
          || (redDTree!=NULL) ;

        if(participating){
          arrSuperNodes[pos].Index = superList[lidx][supidx];  //保存要操作的supernode下标，对于U矩阵而言就是行，对于L矩阵而言就是列
          arrSuperNodes[pos].Rank = rank; //设置要操作的supernode的rank？是priority list中的rank吗？不懂


          SuperNodeBufferType & snode = arrSuperNodes[pos];//这一步干嘛不懂

          pos++;
        }
        rank++;
      }



      int numSentToLeft = 0;
      std::vector<int> reqSentToLeft;


      NumMat<T> AinvBuf, UBuf;

      TIMER_STOP(AllocateBuffer);

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "Communication to the Schur complement." << std::endl << std::endl; 
#endif
      {
        //Receivers have to resize their buffers
        TIMER_START(IRecv_Content_UL);//接受UL矩阵



        // Receivers (Content)
        for (Int supidx=0; supidx<stepSuper ; supidx++){
          SuperNodeBufferType & snode = arrSuperNodes[supidx];//得到要操作的supernode的缓存
          MPI_Request * mpireqsRecvFromAbove = &arrMpireqsRecvContentFromAny[supidx*2];//得到接受上面传来的数据的句柄
          MPI_Request * mpireqsRecvFromLeft = &arrMpireqsRecvContentFromAny[supidx*2+1];//得到从左边传来的数据的句柄

          if( isRecvFromAbove_( snode.Index ) && 
              MYROW( grid_ ) != PROW( snode.Index, grid_ ) ){//如果我需要从上面接受数据并且我确实和这个supernode不在同一行
            TreeBcast * bcastUTree = fwdToBelowTree_[snode.Index];
            if(bcastUTree!=NULL){
              Int myRoot = bcastUTree->GetRoot();//得到发送数据的root
              snode.SizeSstrUrowRecv = bcastUTree->GetMsgSize();//得到接受数据的大小
              snode.SstrUrowRecv.resize( snode.SizeSstrUrowRecv);//调整保存数据的char数组的大小
              MPI_Irecv( &snode.SstrUrowRecv[0], snode.SizeSstrUrowRecv, MPI_BYTE, 
                  myRoot, IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_), 
                  grid_->colComm, mpireqsRecvFromAbove );//接受这一列上面的数据，和U矩阵相关的东西，具体是啥？不懂
#if ( _DEBUGlevel_ >= 1 )
              statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Receiving U " << snode.SizeSstrUrowRecv << " BYTES from "<<myRoot<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_)<< std::endl <<  std::endl; 
#endif
            }
          } // if I need to receive from up

          if( isRecvFromLeft_( snode.Index ) &&
              MYCOL( grid_ ) != PCOL( snode.Index, grid_ ) ){//如果我需要从左面接受数据并且我确实和这个supernode不在同一列
            TreeBcast * bcastLTree = fwdToRightTree_[snode.Index];
            if(bcastLTree!=NULL){
              Int myRoot = bcastLTree->GetRoot();//得到发送数据的root
              snode.SizeSstrLcolRecv = bcastLTree->GetMsgSize();//得到接受数据的大小
              snode.SstrLcolRecv.resize(snode.SizeSstrLcolRecv);//调整保存数据的char数组大小
              MPI_Irecv( &snode.SstrLcolRecv[0], snode.SizeSstrLcolRecv, MPI_BYTE, 
                  myRoot, IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_), 
                  grid_->rowComm, mpireqsRecvFromLeft );//接受这一行左边的数据，这里应该是L矩阵相关的东西，也不太懂
#if ( _DEBUGlevel_ >= 1 )
              statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Receiving L " << snode.SizeSstrLcolRecv << " BYTES from "<<myRoot<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_)<< std::endl <<  std::endl; 
#endif
            }
          } // if I need to receive from left
        }
        TIMER_STOP(IRecv_Content_UL);

        // Senders
        TIMER_START(ISend_Content_UL);//发送UL矩阵
        for (Int supidx=0; supidx<stepSuper; supidx++){
          SuperNodeBufferType & snode = arrSuperNodes[supidx];//得到要发送的supernode缓存
          std::vector<MPI_Request> & mpireqsSendToBelow = arrMpireqsSendToBelow[supidx];//得到发送到下边的句柄
          std::vector<MPI_Request> & mpireqsSendToRight = arrMpireqsSendToRight[supidx];//得到发送到右边的句柄

#if ( _DEBUGlevel_ >= 1 )
          statusOFS << std::endl <<  "["<<snode.Index<<"] "
            << "Communication for the U part." << std::endl << std::endl; 
#endif
          // Communication for the U part.

          if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) ){//如果本processor和这个supernode在同一行，即本processor包含了这个supernode的一部分数据
            std::vector<UBlock<T> >&  Urow = this->U( LBi(snode.Index, grid_) );//得到这个supernode在本processor拥有的所有U block
            // Pack the data in U
            TIMER_START(Serialize_UL);//串行化UL矩阵，打包它们
            std::stringstream sstm;

            std::vector<Int> mask( UBlockMask::TOTAL_NUMBER, 1 );
            // All blocks are to be sent down.
            serialize( (Int)Urow.size(), sstm, NO_MASK );//打包U Block数量
            for( Int jb = 0; jb < Urow.size(); jb++ ){
              serialize( Urow[jb], sstm, mask );//打包每个U Block的内容
            }
            snode.SstrUrowSend.resize( Size( sstm ) );
            sstm.read( &snode.SstrUrowSend[0], snode.SstrUrowSend.size() );//将发送的内容放到supernode缓存中
            snode.SizeSstrUrowSend = snode.SstrUrowSend.size();
            TIMER_STOP(Serialize_UL);

            TreeBcast * bcastUTree = fwdToBelowTree_[snode.Index];
            if(bcastUTree!=NULL){//应该是在这里判断是否需要向下发送数据
              bcastUTree->ForwardMessage((char*)&snode.SstrUrowSend[0], snode.SizeSstrUrowSend, 
                  IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_), &mpireqsSendToBelow[0]);//发送数据
              for( Int idxRecv = 0; idxRecv < bcastUTree->GetDestCount(); ++idxRecv ){
                Int iProcRow = bcastUTree->GetDest(idxRecv);
#if ( _DEBUGlevel_ >= 1 )
                statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Sending U " << snode.SizeSstrUrowSend << " BYTES on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_) << std::endl <<  std::endl; 
#endif
              }
            }
          } // if I am the sender


#if ( _DEBUGlevel_ >= 1 )
          statusOFS << std::endl << "["<<snode.Index<<"] "<< "Communication for the L part." << std::endl << std::endl; 
#endif



          // Communication for the L part.
          if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){//如果本processor在这个supernode同一列，即本processor确实包含了这个supernode的一部分数据
            std::vector<LBlock<T> >&  Lcol = this->L( LBj(snode.Index, grid_) );//得到这个supernode在这一列的所有L Block
            TIMER_START(Serialize_UL);//串行化UL矩阵，打包它们
            // Pack the data in L 
            std::stringstream sstm;
            std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
            mask[LBlockMask::NZVAL] = 0; // nzval is excluded 

            // All blocks except for the diagonal block are to be sent right
            //打包Block size
            if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) )
              serialize( (Int)Lcol.size() - 1, sstm, NO_MASK );
            else
              serialize( (Int)Lcol.size(), sstm, NO_MASK );

            for( Int ib = 0; ib < Lcol.size(); ib++ ){
              if( Lcol[ib].blockIdx > snode.Index ){
#if ( _DEBUGlevel_ >= 2 )
                statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Serializing Block index " << Lcol[ib].blockIdx << std::endl;
#endif
                serialize( Lcol[ib], sstm, mask );//打包数据
              }
            }
            snode.SstrLcolSend.resize( Size( sstm ) );
            sstm.read( &snode.SstrLcolSend[0], snode.SstrLcolSend.size() );//将发送内容存到supernode缓存中
            snode.SizeSstrLcolSend = snode.SstrLcolSend.size();
            TIMER_STOP(Serialize_UL);

            TreeBcast * bcastLTree = fwdToRightTree_[snode.Index];
            if(bcastLTree!=NULL){//在这里判断是否需要给右边传送数据
              bcastLTree->ForwardMessage((char*)&snode.SstrLcolSend[0], snode.SizeSstrLcolSend, 
                  IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_), &mpireqsSendToRight[0]);//发送数据

              for( Int idxRecv = 0; idxRecv < bcastLTree->GetDestCount(); ++idxRecv ){
                Int iProcCol = bcastLTree->GetDest(idxRecv);
#if ( _DEBUGlevel_ >= 1 )
                statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Sending L " << snode.SizeSstrLcolSend << " BYTES on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_) << std::endl <<  std::endl; 
#endif
              }
            }
          } // if I am the sender
        } //Senders
        TIMER_STOP(ISend_Content_UL);
      }

      vector<char> redLdone(stepSuper,0);//不懂
      //这一步也不太懂干嘛
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        TreeReduce<T> * redLTree = redToLeftTree_[snode.Index];

        if(redLTree != NULL){
          redLTree->SetTag(IDX_TO_TAG(snode.Rank,SELINV_TAG_L_REDUCE,limIndex_));
          //Initialize the tree
          redLTree->AllocRecvBuffers();
          //Post All Recv requests;
          redLTree->PostFirstRecv();
        }
      }


      TIMER_START(Compute_Sinv_LT);//计算-Aji^-1 * Lij？有可能是
      {
        Int msgForwarded = 0;
        Int msgToFwd = 0;
        Int gemmProcessed = 0;
        Int gemmToDo = 0;
        //      Int toRecvGemm = 0;
        //copy the list of supernodes we need to process
        std::list<Int> readySupidx;
        //find local things to do
        for(Int supidx = 0;supidx<stepSuper;supidx++){//在这里进行一些本地操作？
          SuperNodeBufferType & snode = arrSuperNodes[supidx];


          if( isRecvFromAbove_( snode.Index ) && isRecvFromLeft_( snode.Index )){//如果我接受了两边的数据
            gemmToDo++;//不懂
            //剩下几个ready应该是判断这个processor是不是Lkk，即对角线上的processor
            if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){
              snode.isReady++;
            }

            if(  MYROW( grid_ ) == PROW( snode.Index, grid_ ) ){
              snode.isReady++;
            }

            if(snode.isReady==2){
              readySupidx.push_back(supidx);
#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<std::endl<<"Locally processing ["<<snode.Index<<"]"<<std::endl;
#endif
            }
          }
          else if( (isRecvFromLeft_( snode.Index )  ) && MYCOL( grid_ ) != PCOL( snode.Index, grid_ ) )//如果接受了左边的数据并且确实不是同一列
          {                                                                                            //这里应该是只计算L或者U部分所以才只管了从左边来的数据
            //Get the reduction tree
            TreeReduce<T> * redLTree = redToLeftTree_[snode.Index];
            //下面的都不太懂
            if(redLTree != NULL){
              TIMER_START(Reduce_Sinv_LT_Isend);
              //send the data to NULL to ensure 0 byte send
              redLTree->SetLocalBuffer(NULL);
              redLTree->SetDataReady(true);

              bool done = redLTree->Progress();

#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce L "<<done<<std::endl;
#endif
              TIMER_STOP(Reduce_Sinv_LT_Isend);
            }
          }// if( isRecvFromAbove_( snode.Index ) && isRecvFromLeft_( snode.Index ))
          //不懂，这里是要统计需要发送的信息吗？异步的原因
          if(MYROW(grid_)!=PROW(snode.Index,grid_)){
            TreeBcast * bcastUTree = fwdToBelowTree_[snode.Index];
            if(bcastUTree != NULL){
              if(bcastUTree->GetDestCount()>0){
                msgToFwd++;
              }
            }
          }
          //不懂
          if(MYCOL(grid_)!=PCOL(snode.Index,grid_)){
            TreeBcast * bcastLTree = fwdToRightTree_[snode.Index];
            if(bcastLTree != NULL){
              if(bcastLTree->GetDestCount()>0){
                msgToFwd++;
              }
            }
          }
        }

#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<std::endl<<"gemmToDo ="<<gemmToDo<<std::endl;
        statusOFS<<std::endl<<"msgToFwd ="<<msgToFwd<<std::endl;
#endif


#if defined (PROFILE) 
        end_SendULWaitContentFirst=0;
        begin_SendULWaitContentFirst=0;
#endif
        
        while(gemmProcessed<gemmToDo || msgForwarded < msgToFwd)
        {
          Int reqidx = MPI_UNDEFINED;
          Int supidx = -1;


          //while I don't have anything to do, wait for data to arrive 
          do{



            //then process with the remote ones

            TIMER_START(WaitContent_UL);
#if defined(PROFILE)
            if(begin_SendULWaitContentFirst==0){
              begin_SendULWaitContentFirst=1;
              TIMER_START(WaitContent_UL_First);
            }
#endif





            int reqIndices[arrMpireqsRecvContentFromAny.size()];
            int numRecv = 0; 
            numRecv = 0;
            int err = MPI_Waitsome(2*stepSuper, &arrMpireqsRecvContentFromAny[0], &numRecv, reqIndices, MPI_STATUSES_IGNORE);//等待数据,numRecv记录接收到的数据,reqIndices记录了完成的操作下标
            assert(err==MPI_SUCCESS);

            for(int i =0;i<numRecv;i++){
              reqidx = reqIndices[i];
              //I've received something
              //下面好像是发送数据，不懂
              if(reqidx!=MPI_UNDEFINED)
              {
                //this stays true
                supidx = reqidx/2;
                SuperNodeBufferType & snode = arrSuperNodes[supidx];//得到supernode缓存

                //If it's a U block 
                if(reqidx%2==0){

                  TreeBcast * bcastUTree = fwdToBelowTree_[snode.Index];//得到它的broad cast树
                  if(bcastUTree != NULL){
                    if(bcastUTree->GetDestCount()>0){

                      std::vector<MPI_Request> & mpireqsSendToBelow = arrMpireqsSendToBelow[supidx];//得到发送到下面数据的句柄
#if ( _DEBUGlevel_ >= 1 )
                      for( Int idxRecv = 0; idxRecv < bcastUTree->GetDestCount(); ++idxRecv ){
                        Int iProcRow = bcastUTree->GetDest(idxRecv);
                        statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Forwarding U " << snode.SizeSstrUrowRecv << " BYTES to "<<iProcRow<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_)<< std::endl <<  std::endl; 
                      }
#endif

                      bcastUTree->ForwardMessage( (char*)&snode.SstrUrowRecv[0], snode.SizeSstrUrowRecv, 
                          IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_), &mpireqsSendToBelow[0] );//发送U矩阵行数据
#if ( _DEBUGlevel_ >= 1 )
                      for( Int idxRecv = 0; idxRecv < bcastUTree->GetDestCount(); ++idxRecv ){
                        Int iProcRow = bcastUTree->GetDest(idxRecv);
                        statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Forwarded U " << snode.SizeSstrUrowRecv << " BYTES to "<<iProcRow<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_U_CONTENT,limIndex_)<< std::endl <<  std::endl; 
                      }
#endif

                      msgForwarded++;
                    }
                  }
                }
                //If it's a L block 
                else if(reqidx%2==1){
                  TreeBcast * bcastLTree = fwdToRightTree_[snode.Index];//得到它的broad cast树
                  if(bcastLTree != NULL){
                    if(bcastLTree->GetDestCount()>0){

                      std::vector<MPI_Request> & mpireqsSendToRight = arrMpireqsSendToRight[supidx];//得到发送右边数据的句柄
#if ( _DEBUGlevel_ >= 1 )
                      for( Int idxRecv = 0; idxRecv < bcastLTree->GetDestCount(); ++idxRecv ){
                        Int iProcCol = bcastLTree->GetDest(idxRecv);
                        statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Forwarding L " << snode.SizeSstrLcolRecv << " BYTES to "<<iProcCol<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_)<< std::endl <<  std::endl; 
                      }
#endif

                      bcastLTree->ForwardMessage( (char*)&snode.SstrLcolRecv[0], snode.SizeSstrLcolRecv, 
                          IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_), &mpireqsSendToRight[0]);//发送L矩阵列数据

                      //                    for( Int idxRecv = 0; idxRecv < bcastLTree->GetDestCount(); ++idxRecv ){
                      //                      Int iProcCol = bcastLTree->GetDest(idxRecv);
                      //                      PROFILE_COMM(MYPROC(this->grid_),PNUM(MYROW(this->grid_),iProcCol,this->grid_),IDX_TO_TAG(snode.Index,SELINV_TAG_L_CONTENT),snode.SizeSstrLcolRecv);
                      //                    }
#if ( _DEBUGlevel_ >= 1 )
                      for( Int idxRecv = 0; idxRecv < bcastLTree->GetDestCount(); ++idxRecv ){
                        Int iProcCol = bcastLTree->GetDest(idxRecv);
                        statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Forwarded L " << snode.SizeSstrLcolRecv << " BYTES to "<<iProcCol<<" on tag "<<IDX_TO_TAG(snode.Rank,SELINV_TAG_L_CONTENT,limIndex_)<< std::endl <<  std::endl; 
                      }
#endif
                      msgForwarded++;
                    }
                  }
                }


#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<std::endl<<"Received data for ["<<snode.Index<<"] reqidx%2="<<reqidx%2<<" is ready ?"<<snode.isReady<<std::endl;
#endif
                //下面检查是否接受到了全部的LU矩阵
                if( isRecvFromAbove_( snode.Index ) && isRecvFromLeft_( snode.Index )){
                  snode.isReady++;

                  //if we received both L and U, the supernode is ready
                  if(snode.isReady==2){
                    readySupidx.push_back(supidx);

#if defined(PROFILE)
                    if(end_SendULWaitContentFirst==0){
                      TIMER_STOP(WaitContent_UL_First);
                      end_SendULWaitContentFirst=1;
                    }
#endif
                  }
                }
              }

            }//end for waitsome

            TIMER_STOP(WaitContent_UL);

          } while( (gemmProcessed<gemmToDo && readySupidx.size()==0) || (gemmProcessed==gemmToDo && msgForwarded<msgToFwd) );
          //上面应该都是为了U，L矩阵数据的接收

          //If I have some work to do 
          //如果LU矩阵都接受到了就可以开始工作了
          if(readySupidx.size()>0)
          {
            supidx = readySupidx.back();
            readySupidx.pop_back();
            SuperNodeBufferType & snode = arrSuperNodes[supidx];


            // Only the processors received information participate in the Gemm 
            if( isRecvFromAbove_( snode.Index ) && isRecvFromLeft_( snode.Index ) ){

              std::vector<LBlock<T> > LcolRecv;
              std::vector<UBlock<T> > UrowRecv;
              // Save all the data to be updated for { L( isup, snode.Index ) | isup > snode.Index }.
              // The size will be updated in the Gemm phase and the reduce phase
          
              NumMat<float> UBuf_other;//记录量化部分的UBuf
              
              bool quantUBuf = false;//记录UBuf是否有被量化的部分

              UnpackData(snode, LcolRecv, UrowRecv);//将接收到的数据放回LU block中，即它接收到的L block和U block现在都在LcolRecv以及UrowRecv里面了
              //NumMat<T> AinvBuf, UBuf;
              SelInv_lookup_indexes(snode,LcolRecv, UrowRecv,AinvBuf, UBuf, UBuf_other, quantSuperNode, quantUBuf);//这一步是把UrowRecv里面的东西都复制到UBuf中，把LcolRecv和UrowRecv里面的东西复制到AinvBuf中

              snode.LUpdateBuf.Resize( AinvBuf.m(), SuperSize( snode.Index, super_ ) );//调整LUpdateBuf的大小，为AinvBuf行，snode对应supernode大小的列
              if(!quantUBuf){//如果左右都没有量化的supernode，那么就是一个普通的GEMM
  #ifdef GEMM_PROFILE
                gemm_stat.push_back(AinvBuf.m());
                gemm_stat.push_back(UBuf.m());
                gemm_stat.push_back(AinvBuf.n());
  #endif
                TIMER_START(Compute_Sinv_LT_GEMM);
                //进行一次GEMM，计算的结果保存在snode的LUpdateBuf中，这一步对应在算法中的哪一步还不清楚，但感觉像是第三步
                //没错！这里就是第三步，这里有一个U的转置！
                //所以一定是第三步！
                //下面的意思表示:LUpdateBuf = -1 * AinvBuf * Ubuf^T + 0 * LUpdateBuf
                // blas::Gemm( 'N', 'T', AinvBuf.m(), UBuf.m(), AinvBuf.n(), MINUS_ONE<T>(), 
                //     AinvBuf.Data(), AinvBuf.m(), 
                //     UBuf.Data(), UBuf.m(), ZERO<T>(),
                //     snode.LUpdateBuf.Data(), snode.LUpdateBuf.m() );
                gpu_blas_dmmul(handle, 'N', 'T', AinvBuf.m(), UBuf.m(), AinvBuf.n(), MINUS_ONE<T>(), 
                    AinvBuf.Data(), AinvBuf.m(), 
                    UBuf.Data(), UBuf.m(), ZERO<T>(),
                    snode.LUpdateBuf.Data(), snode.LUpdateBuf.m() );
                TIMER_STOP(Compute_Sinv_LT_GEMM);
              }else{//如果只有UBuf进行量化，AinvBuf没有量化的supernode
                // std::cout<<"Step 3 : UBuf Quant!"<<std::endl;
                NumMat<float> quantBuf(AinvBuf.m(), SuperSize( snode.Index, super_ ));//记录公式的临时量化数据大小
                NumMat<float> AinvBuf_quant(AinvBuf.m(), AinvBuf.n());//记录AinvBuf的量化

                //对AinvBuf和UBuf进行量化，保存到AinfBuf_quant和UBuf_quant中，后面公式要用到
                for(int j = 0;j<AinvBuf.n();j++){
                  for(int i = 0;i<AinvBuf.m();i++){
                    AinvBuf_quant(i, j) = (float)AinvBuf(i, j);
                  }
                }

  #ifdef GEMM_PROFILE
                gemm_stat.push_back(AinvBuf.m());
                gemm_stat.push_back(UBuf.m());
                gemm_stat.push_back(AinvBuf.n());
  #endif
                TIMER_START(Compute_Sinv_LT_GEMM);
                //公式为-1 * AinvBuf * (UBuf + UBuf_other)^T
                //即(1) -1 * AinvBuf * Ubuf^T + (2) -1 * AinvBuf* UBuf_other^T
                //这里首先计算(1)，并且把结果保存在最终的结果LUpdateBuf中，后面的结果都是直接加在上面的
                gpu_blas_dmmul(handle, 'N', 'T', AinvBuf.m(), UBuf.m(), AinvBuf.n(), MINUS_ONE<T>(), 
                    AinvBuf.Data(), AinvBuf.m(), 
                    UBuf.Data(), UBuf.m(), ZERO<T>(),
                    snode.LUpdateBuf.Data(), snode.LUpdateBuf.m() ); 
                // blas::Gemm( 'N', 'T', AinvBuf.m(), UBuf.m(), AinvBuf.n(), MINUS_ONE<T>(), 
                //     AinvBuf.Data(), AinvBuf.m(), 
                //     UBuf.Data(), UBuf.m(), ZERO<T>(),
                //     snode.LUpdateBuf.Data(), snode.LUpdateBuf.m() ); 
                
                //然后计算(2)，将结果保存在quantBuf中
                gpu_blas_smmul(handle, 'N', 'T', AinvBuf_quant.m(), UBuf_other.m(), AinvBuf_quant.n(), MINUS_ONE<float>(),
                    AinvBuf_quant.Data(), AinvBuf_quant.m(),
                    UBuf_other.Data(), UBuf_other.m(), ZERO<float>(),
                    quantBuf.Data(), quantBuf.m());
                // blas::Gemm('N', 'T', AinvBuf_quant.m(), UBuf_other.m(), AinvBuf_quant.n(), MINUS_ONE<float>(),
                //     AinvBuf_quant.Data(), AinvBuf_quant.m(),
                //     UBuf_other.Data(), UBuf_other.m(), ZERO<float>(),
                //     quantBuf.Data(), quantBuf.m());
                
                //将quantBuf的结果添加到LUpadateBuf中
                for(int j = 0;j<quantBuf.n();j++){
                  for(int i = 0;i<quantBuf.m();i++){
                    snode.LUpdateBuf(i, j) = snode.LUpdateBuf(i, j) + (double)quantBuf(i, j);
                  }
                }

                TIMER_STOP(Compute_Sinv_LT_GEMM);
                //最后释放这些资源
                UBuf_other.deallocate();
                AinvBuf_quant.deallocate();
                quantBuf.deallocate();
            }
              
            //第三步量化已经完成
              
              
              


#if ( _DEBUGlevel_ >= 2 )
              statusOFS << std::endl << "["<<snode.Index<<"] "<<  "snode.LUpdateBuf: " << snode.LUpdateBuf << std::endl;
#endif
            } // if Gemm is to be done locally

            //Get the reduction tree
            //下面这一步不太懂，按照论文中的算法来说应该是把结果Reduce到Ljk去了
            //那这一步是干嘛？不懂
            TreeReduce<T> * redLTree = redToLeftTree_[snode.Index];
            if(redLTree != NULL){ //这里好像是通过判断是否为NULL来判断是否需要进行reduce的
              assert( snode.LUpdateBuf.m() != 0 && snode.LUpdateBuf.n() != 0 );
                TIMER_START(Reduce_Sinv_LT_Isend);
                //send the data
                redLTree->SetLocalBuffer(snode.LUpdateBuf.Data());//设置reduce数据？
                redLTree->SetDataReady(true);

                bool done = redLTree->Progress();//进行reduce？
#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce L "<<done<<std::endl;
#endif
                TIMER_STOP(Reduce_Sinv_LT_Isend);
            }

            gemmProcessed++;

#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<std::endl<<"gemmProcessed ="<<gemmProcessed<<"/"<<gemmToDo<<std::endl;
#endif

            //advance reductions
            //这里也不懂
            for (Int supidx=0; supidx<stepSuper; supidx++){
              SuperNodeBufferType & snode = arrSuperNodes[supidx];
              TreeReduce<T> * redLTree = redToLeftTree_[snode.Index];
              if(redLTree != NULL && !redLdone[supidx]){
                bool done = redLTree->Progress();//继续reduce？

#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce L "<<done<<std::endl;
#endif
              }
            }
          }
        }

      }
      TIMER_STOP(Compute_Sinv_LT);

      //Reduce Sinv L^T to the processors in PCOL(ksup,grid_)


      TIMER_START(Reduce_Sinv_LT);
      //好像从这里开始才是开始reduce Ljk了
      //blocking wait for the reduction
      bool all_done = false;
      while(!all_done)
      {
        all_done = true;

        for (Int supidx=0; supidx<stepSuper; supidx++){
          SuperNodeBufferType & snode = arrSuperNodes[supidx];

          TreeReduce<T> * redLTree = redToLeftTree_[snode.Index];

            if(redLTree != NULL && !redLdone[supidx])//如果需要reduce但是还没有reduce成功？
            {

              //TODO restore this
              //bool done = redLTree->Progress();
              redLTree->Wait();//等待reduce成功
              bool done = true;
#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce L "<<done<<std::endl;
#endif
              redLdone[supidx]=done?1:0;
              if(done){

#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<snode.Index<<"] "<<" DONE reduce L"<<std::endl;
#endif

//if(redLTree->GetTag() == 2344 && MYPROC(grid_)==0){gdb_lock();}
                if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){ // 如果本processor和supernode在同一列，即Ljk
                  //determine the number of rows in LUpdateBufReduced
                  //下面好像是根据本地的L Block将LUpdateBufReduced进行分行，方便后面进行GEMM
                  Int numRowLUpdateBuf;
                  std::vector<LBlock<T> >&  Lcol = this->L( LBj( snode.Index, grid_ ) );
                  if( MYROW( grid_ ) != PROW( snode.Index, grid_ ) ){// 如果本processor和supernode不在同一行，即不是Lkk
                    snode.RowLocalPtr.resize( Lcol.size() + 1 ); //这个记录每个L block对应的第一个非零行
                    snode.BlockIdxLocal.resize( Lcol.size() ); //这个记录本地L block的supernode行
                    snode.RowLocalPtr[0] = 0;
                    for( Int ib = 0; ib < Lcol.size(); ib++ ){
                      snode.RowLocalPtr[ib+1] = snode.RowLocalPtr[ib] + Lcol[ib].numRow;
                      snode.BlockIdxLocal[ib] = Lcol[ib].blockIdx;
                    }
                  } // I do not own the diagonal block
                  else{ //如果是Lkk
                    snode.RowLocalPtr.resize( Lcol.size() );//如果是Lkk那么实际需要操作的block就比原来少一个
                    snode.BlockIdxLocal.resize( Lcol.size() - 1 );//同上，少一个
                    snode.RowLocalPtr[0] = 0;
                    for( Int ib = 1; ib < Lcol.size(); ib++ ){
                      snode.RowLocalPtr[ib] = snode.RowLocalPtr[ib-1] + Lcol[ib].numRow;
                      snode.BlockIdxLocal[ib-1] = Lcol[ib].blockIdx;
                    }
                  } // I own the diagonal block, skip the diagonal block
                  numRowLUpdateBuf = *snode.RowLocalPtr.rbegin();//总的行数
                  //下面也不懂在干嘛，但是应该不涉及计算
                  if( numRowLUpdateBuf > 0 ){
                    if( snode.LUpdateBuf.m() == 0 && snode.LUpdateBuf.n() == 0 ){
                      snode.LUpdateBuf.Resize( numRowLUpdateBuf,SuperSize( snode.Index, super_ ) );
                      SetValue(snode.LUpdateBuf, ZERO<T>());
                    }
                  }

                  //copy the buffer from the reduce tree
                  redLTree->SetLocalBuffer(snode.LUpdateBuf.Data());
                }
                redLdone[supidx]=1;
                redLTree->CleanupBuffers();
              }

              all_done = all_done && (done || redLdone[supidx]);
            }
        }
      }
      TIMER_STOP(Reduce_Sinv_LT);



      //--------------------- End of reduce of LUpdateBuf-------------------------

      TIMER_START(Update_Diagonal);

      for (Int supidx=0; supidx<stepSuper; supidx++){ //对于每一个要操作的supernode
        SuperNodeBufferType & snode = arrSuperNodes[supidx];

        ComputeDiagUpdate(snode, handle, quantSuperNode);//计算第四步Lck^T * Ack ^ -1，这里有一个量化，

        //Get the reduction tree
        TreeReduce<T> * redDTree = redToAboveTree_[snode.Index];
        //下面都不是很懂的样子
        //不知道进行什么reduce
        if(redDTree != NULL){
          //send the data
          if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) ){
            if(snode.DiagBuf.Size()==0){
              snode.DiagBuf.Resize( SuperSize( snode.Index, super_ ), SuperSize( snode.Index, super_ ));
              SetValue(snode.DiagBuf, ZERO<T>());
            }
          }


          redDTree->SetLocalBuffer(snode.DiagBuf.Data());
          if(!redDTree->IsAllocated()){
            redDTree->SetTag(IDX_TO_TAG(snode.Rank,SELINV_TAG_D_REDUCE,limIndex_));
            redDTree->AllocRecvBuffers();
            //Post All Recv requests;
            redDTree->PostFirstRecv();
          }

          redDTree->SetDataReady(true);
          bool done = redDTree->Progress();//又是什么reduce？
#if ( _DEBUGlevel_ >= 1 )
          statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce D "<<done<<std::endl;
#endif
        }

        //advance reductions
        for (Int supidx=0; supidx<stepSuper; supidx++){
          SuperNodeBufferType & snode = arrSuperNodes[supidx];
          TreeReduce<T> * redDTree = redToAboveTree_[snode.Index];//又是什么reduce，看不太懂
          if(redDTree != NULL){
            if(redDTree->IsAllocated())
            {
              bool done = redDTree->Progress();
#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce D "<<done<<std::endl;
#endif
            }
          }
        }
      }
      TIMER_STOP(Update_Diagonal);

      TIMER_START(Reduce_Diagonal);
      //blocking wait for the reduction
      {
        vector<char> is_done(stepSuper,0);
        bool all_done = false;
        while(!all_done)
        {
          all_done = true;

          for (Int supidx=0; supidx<stepSuper; supidx++){
            SuperNodeBufferType & snode = arrSuperNodes[supidx];
            TreeReduce<T> * redDTree = redToAboveTree_[snode.Index];

              if(redDTree != NULL && !is_done[supidx])
              {

                bool done = redDTree->Progress();
#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<snode.Index<<"] "<<" trying to progress reduce D "<<done<<std::endl;
#endif
                is_done[supidx]=done?1:0;
                if(done){//如果reduce完了，是说Lkk的数据完事了么

#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<snode.Index<<"] "<<" DONE reduce D"<<std::endl;
#endif
                  if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){
                    if( MYROW( grid_ ) == PROW( snode.Index, grid_ ) ){//如果是Lkk的话
                      LBlock<T> &  LB = this->L( LBj( snode.Index, grid_ ) )[0]; //得到Lkk
                      // Symmetrize LB
                      //Lkk = DiagBuf + Lkk
                      blas::Axpy( LB.numRow * LB.numCol, ONE<T>(), snode.DiagBuf.Data(), 1, LB.nzval.Data(), 1 );
                      Symmetrize( LB.nzval );//这里好像是第四步的最后一步
                    }
                  }
                  is_done[supidx]=1;
                  redDTree->CleanupBuffers();
                }

                all_done = all_done && (done || is_done[supidx]);
              }
          }
        }
      }
      TIMER_STOP(Reduce_Diagonal);






      SendRecvCD_UpdateU(arrSuperNodes, stepSuper);//这里是第五步！将更新的Ainv发送到U那一边！芜湖！



      TIMER_START(Update_L);
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "["<<snode.Index<<"] "<<  "Finish updating the L part by filling LUpdateBufReduced back to L" << std::endl << std::endl; 
#endif

        if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) && snode.LUpdateBuf.m() > 0 ){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj( snode.Index, grid_ ) );
          //Need to skip the diagonal block if present
          Int startBlock = (MYROW( grid_ ) == PROW( snode.Index, grid_ ))?1:0;
          for( Int ib = startBlock; ib < Lcol.size(); ib++ ){
            LBlock<T> & LB = Lcol[ib];
            lapack::Lacpy( 'A', LB.numRow, LB.numCol, &snode.LUpdateBuf( snode.RowLocalPtr[ib-startBlock], 0 ),
                snode.LUpdateBuf.m(), LB.nzval.Data(), LB.numRow );//这里对L block进行Ainv的覆盖，问题不大
          }
        } // Finish updating L	
      } // for (snode.Index) : Main loop
      TIMER_STOP(Update_L);



      TIMER_START(Barrier);

      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];

        TreeReduce<T> * &redLTree = redToLeftTree_[snode.Index];

        if(redLTree != NULL){
          redLTree->Wait();
          redLTree->CleanupBuffers();
          //          delete redLTree;
          //          redLTree = NULL;
        }
      }

      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        TreeReduce<T> * &redDTree = redToAboveTree_[snode.Index];

        if(redDTree != NULL){
          redDTree->Wait();
          redDTree->CleanupBuffers();
          //          delete redDTree;
          //          redDTree = NULL;
        }
      }

      mpi::Waitall(arrMpireqsRecvContentFromAny);
      for (Int supidx=0; supidx<stepSuper; supidx++){
        SuperNodeBufferType & snode = arrSuperNodes[supidx];
        Int ksup = snode.Index;
        std::vector<MPI_Request> & mpireqsSendToRight = arrMpireqsSendToRight[supidx];
        std::vector<MPI_Request> & mpireqsSendToBelow = arrMpireqsSendToBelow[supidx];

#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"["<<ksup<<"] mpireqsSendToRight"<<std::endl;
#endif
        mpi::Waitall( mpireqsSendToRight );
#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"["<<ksup<<"] mpireqsSendToBelow"<<std::endl;
#endif

        mpi::Waitall( mpireqsSendToBelow );
      }

#if ( _DEBUGlevel_ >= 1 )
      statusOFS<<"barrier done"<<std::endl;
#endif
      TIMER_STOP(Barrier);



      }
      else
      {
        Int numSuper = this->NumSuper();

        std::vector<std::vector<Int> > & superList = this->WorkingSet();

        Int numSteps = superList.size();
        Int stepSuper = superList[lidx].size(); 


        TIMER_START(AllocateBuffer);
        //allocate the buffers for this supernode
        std::vector<SuperNodeBufferType> arrSuperNodes(stepSuper);
        for (Int supidx=0; supidx<stepSuper; supidx++){ 
          auto & snode = arrSuperNodes[supidx];
          snode.Index = superList[lidx][supidx];  
          snode.Rank = rank++;
        }

        Int totTreeCount =0 ;
        for (Int supidx=0; supidx<stepSuper ; supidx++){
          auto & snode = arrSuperNodes[supidx];
          totTreeCount += snodeTreeOffset_[snode.Index+1] - snodeTreeOffset_[snode.Index]; 
        }


        std::list<int> bcastLDataIdx; 
        std::vector<int> bcastLDataDone(totTreeCount+1,0);
        std::vector<Int> gemmCount(totTreeCount,0);        
        std::vector<Int> gemmCountDiag(stepSuper,0);        
        std::vector<Int> treeIdx;
        treeIdx.reserve(totTreeCount);
        std::vector<Int> treeToSupidx;
        treeToSupidx.reserve(totTreeCount);
        std::vector<Int> treeToIb;
        treeToIb.reserve(totTreeCount);

        std::list<int> redDIdx; 
        std::vector<bool> redDdone(stepSuper,false);
        bool all_doneD = false;
        bool all_doneBCastL = false;
        bool all_doneL = false;
        std::list<int> redLIdx; 
        std::vector<bool> redLdone(totTreeCount,false);




        std::vector<Int> treeToBufIdx;
        treeToBufIdx.reserve(totTreeCount);


        //NumMat<T> AinvBuf, LBuf;
        TIMER_STOP(AllocateBuffer);

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "Communication to the Schur complement." << std::endl << std::endl; 
#endif

        //These are debug toggles
        //#define BLOCK_BCAST_LDATA
        //#define BLOCK_REDUCE_L
        //#define BLOCK_REDUCE_D

        TIMER_START(WaitContentLU);
        {
          std::vector<Int> treeToBufIdxHeads(stepSuper,0);
          for (Int supidx=0; supidx<stepSuper ; supidx++){
            auto & snode = arrSuperNodes[supidx];
            Int treeCount = snodeTreeOffset_[snode.Index+1] - snodeTreeOffset_[snode.Index]; 
            for(Int offset = 0; offset<treeCount; offset++){
              Int tidx = snodeTreeOffset_[snode.Index] + offset;
              Int blkIdx = snodeTreeToBlkidx_[snode.Index][offset];
              auto & bcastLData = this->bcastLDataTree_[tidx];
              if(bcastLData != nullptr){
                //add the global index of the tree to the watch list
                treeIdx.push_back(tidx);
                treeToSupidx.push_back(supidx);
                treeToIb.push_back(-1);
                treeToBufIdx.push_back(treeToBufIdxHeads[supidx]++);

                if(bcastLData->IsRoot()){
                  auto&  Lcol = this->L( LBj(snode.Index, this->grid_) );
                  Int ibFound = -1;
                  for( Int ib = 0; ib < Lcol.size(); ib++ ){
                    if(Lcol[ib].blockIdx == blkIdx){
                      ibFound = ib;
                      break;
                    }
                  }
                  assert(ibFound>=0);
                  treeToIb.back() = ibFound;
                }
              }
            }
          }

          for (Int supidx=0; supidx<stepSuper ; supidx++){
            auto & snode = arrSuperNodes[supidx];
            snode.LUpdateBufBlk.resize(treeToBufIdxHeads[supidx]);
            snode.SstrLcolSendBlk.resize(treeToBufIdxHeads[supidx]);
            snode.SizeSstrLcolSendBlk.resize(treeToBufIdxHeads[supidx]);
          }

          TIMER_START(Serialize_LcolL);
                std::stringstream sstm;
                std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
          for(Int ltidx=0;ltidx<treeIdx.size();ltidx++){
            Int tidx = treeIdx[ltidx];
            Int supidx = treeToSupidx[ltidx];
            auto & snode = arrSuperNodes[supidx];
            auto & bcastLData = this->bcastLDataTree_[tidx];
            if(bcastLData != nullptr){
              if(bcastLData->IsRoot()){
                Int ibFound = treeToIb[ltidx];
                auto&  Lcol = this->L( LBj(snode.Index, this->grid_) );
                assert(ibFound>=0);
                auto & LB = Lcol[ibFound];

                // Only LB is to be sent down
                auto & SstrLcolSend = snode.SstrLcolSendBlk[treeToBufIdx[ltidx]];
                auto & SizeSstrLcolSend = snode.SizeSstrLcolSendBlk[treeToBufIdx[ltidx]];
                //serialize( LB, sstm, mask );

                SizeSstrLcolSend = bcastLData->GetMsgSize();
                SstrLcolSend.resize( SizeSstrLcolSend );
                sstm.rdbuf()->pubsetbuf((char*)SstrLcolSend.data(), SizeSstrLcolSend);
                // Only LB is to be sent down
                serialize( LB, sstm, mask );
                //SstrLcolSend.resize( Size( sstm ) );
                //sstm.read( &SstrLcolSend[0], SstrLcolSend.size() );
                //SizeSstrLcolSend = SstrLcolSend.size();
                sstm.rdbuf()->pubsetbuf(nullptr,0);

                assert(bcastLData->GetMsgSize()==SizeSstrLcolSend);
                bcastLData->SetLocalBuffer(&SstrLcolSend[0]);
                bcastLData->SetDataReady(true);
              }
              bool done = bcastLData->Progress();
            }
          }
          TIMER_STOP(Serialize_LcolL);


        }
        TIMER_STOP(WaitContentLU);



#ifdef BLOCK_BCAST_LDATA
        TreeBcast_Waitall( treeIdx, this->bcastLDataTree_);
#endif


        for(int ltidx = 0; ltidx<treeIdx.size();ltidx++){
          Int tidx = treeIdx[ltidx];
          Int supidx = treeToSupidx[ltidx]; 
          auto & snode = arrSuperNodes[supidx];

          Int offset = tidx - snodeTreeOffset_[snode.Index];  
          Int blkIdx = snodeTreeToBlkidx_[snode.Index][offset];

          auto & redLTree = this->redLTree2_[tidx];
          if(redLTree != nullptr){
            auto & snode = arrSuperNodes[supidx];
            redLTree->Progress();
          }


          if(MYPROC(this->grid_) == PNUM(PROW(blkIdx,this->grid_),PCOL(blkIdx,this->grid_),this->grid_)){
            gemmCount[ltidx]++;
          }

          for(Int ltidx2 = ltidx-1;ltidx2>=0;ltidx2--){
            if (treeToSupidx[ltidx2]!=supidx){
              break;
            }
            Int tidx2 = treeIdx[ltidx2];
            Int offset2 = tidx2 - snodeTreeOffset_[snode.Index];  
            Int blkIdx2 = snodeTreeToBlkidx_[snode.Index][offset2];
            if(blkIdx2>blkIdx){
              if(MYROW(this->grid_)==PROW(blkIdx2,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx,this->grid_)){
                gemmCount[ltidx]++;
                gemmCount[ltidx2]++;
              }
            }
          }
          for(Int ltidx2 = ltidx+1;ltidx2<treeIdx.size();ltidx2++){
            if (treeToSupidx[ltidx2]!=supidx){
              break;
            }
            Int tidx2 = treeIdx[ltidx2];
            Int offset2 = tidx2 - snodeTreeOffset_[snode.Index];  
            Int blkIdx2 = snodeTreeToBlkidx_[snode.Index][offset2];
            if(blkIdx2>blkIdx){
              if(MYROW(this->grid_)==PROW(blkIdx2,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx,this->grid_)){
                gemmCount[ltidx]++;
                gemmCount[ltidx2]++;
              }
            }
          }
        }

        for (auto && snode : arrSuperNodes){
          auto & redDTree = this->redDTree2_[snode.Index];
          if(redDTree != nullptr){
            //if(redDTree->IsRoot()){
            //  auto&  Lcol = this->L( LBj(snode.Index, this->grid_) );
            //  assert( redDTree->GetMsgSize() == Lcol[0].nzval.Size() );
            //}           
            redDTree->Progress();
          }
        }

        TIMER_START(Compute_Sinv_LU);
        {
          //list of supidx / blkIdx1 / blkIdx2 / ltidx / ltidx2 
          std::list< std::tuple< Int,Int,Int,Int,Int> > readySnode;

          std::stringstream sstm;
          std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
          auto UnpackLBlock = [&mask,&sstm](const std::shared_ptr<TreeBcast_v2<char>> & tree, LBlock<T> & LB){
            TIMER_START(UnpackLBlock);
            //sstm.write( tree->GetLocalBuffer(), tree->GetMsgSize() );
sstm.rdbuf()->pubsetbuf((char*)tree->GetLocalBuffer(), tree->GetMsgSize());
            deserialize( LB, sstm, mask );
                sstm.rdbuf()->pubsetbuf(nullptr,0);
            TIMER_STOP(UnpackLBlock);
          };

          auto getBlocks = [this] (Int ksup, LBlock<T> * pLB, LBlock<T> * pSinvB, Int * blockRows, Int & blockRowsSize){
            TIMER_START(GET_BLOCKS);
            if(pLB->numRow>0){
              Int* rowsSinvBPtr    = pSinvB->rows.Data();
              Int* rowsLBPtr    = pLB->rows.Data();

              //find blocks
              Int ifr = 0;
              Int fr = rowsLBPtr[0];
              Int nrows = 1;
              for( Int i = 1; i < pLB->numRow; i++ ){
                if(rowsLBPtr[i]==rowsLBPtr[i-1]+1){ 
                  nrows++;
                }
                else{
                  blockRows[blockRowsSize++] = fr;
                  blockRows[blockRowsSize++] = nrows;
                  blockRows[blockRowsSize++] = ifr;
                  fr = rowsLBPtr[i];
                  nrows = 1;
                  ifr = i;
                }
              }
              blockRows[blockRowsSize++] = fr;
              blockRows[blockRowsSize++] = nrows;
              blockRows[blockRowsSize++] = ifr;

              if(pLB->blockIdx == pSinvB->blockIdx){

                Int lastRowIdxSinv = 0;
                Int SinvBnumRow = pSinvB->numRow;
                for(Int ii = 0; ii<blockRowsSize;ii+=3){
                  {
                    auto fr = blockRows[ii];
                    auto nr = blockRows[ii+1];
                    Int ifr = blockRows[ii+2];

                    bool isRowFound = false;
                    for( lastRowIdxSinv; lastRowIdxSinv < SinvBnumRow; lastRowIdxSinv++ ){
                      if( fr == rowsSinvBPtr[lastRowIdxSinv] ){
                        isRowFound = true;
                        blockRows[ii] = lastRowIdxSinv;
                        break;
                      }
                    }
                    if( isRowFound == false ){
                      std::ostringstream msg;
                      msg << "Row " << fr
                        <<" in LB cannot find the corresponding row in SinvB" << std::endl
                        << "LB.rows    = " << pLB->rows << std::endl
                        << "SinvB.rows = " << pSinvB->rows << std::endl;
                      ErrorHandling( msg.str().c_str() );
                    }
                  }
                }
              }
              else{
                Int SinvColsSta = FirstBlockCol( ksup, this->super_ );

                for(Int ii = 0; ii<blockRowsSize;ii+=3){
                  {
                    auto fc = blockRows[ii];
                    auto nc = blockRows[ii+1];
                    Int ifc = blockRows[ii+2];

                    bool isColFound = (fc >= SinvColsSta) && (fc+nc <= SinvColsSta + pSinvB->nzval.n()); 

                    if( isColFound == false ){
                      std::ostringstream msg;
                      msg << "Column " << fc
                        <<" in LB cannot find the corresponding column in SinvB" << std::endl
                        << "LB->rows    = " << pLB->rows << std::endl
                        << "SinvB.columns = ["<<SinvColsSta<<" - "<< SinvColsSta + pSinvB->nzval.n()<<"]"<<std::endl;
                      ErrorHandling( msg.str().c_str() );
                    }
                    else{
                      blockRows[ii] = fc - SinvColsSta;
                    }
                  }
                }

              }
            }
            TIMER_STOP(GET_BLOCKS);
          };


          auto ComputeLUpdateBuf = [&getBlocks,this](SuperNodeBufferType & snode, LBlock<T> & LB1, LBlock<T> & LB2, NumMat<T> & LUpdateBuf1, NumMat<T> & LUpdateBuf2) {
            Int superSize = SuperSize( snode.Index, this->super_ );

            Int isup = LB1.blockIdx;
            Int jsup = LB2.blockIdx;

            //pLB1 should point to the one with the largest blockIdx
            LBlock<T> * pLB1 = &LB1;
            LBlock<T> * pLB2 = &LB2;

            TIMER_START(Compute_Sinv_L_Resize);
            T beta1 = ONE<T>();
            if(LUpdateBuf1.Size()==0){
              LUpdateBuf1.Resize(pLB1->nzval.m(),SuperSize( snode.Index, this->super_ ));
#ifdef _OPENMP_BLOCKS_
              SetValue(LUpdateBuf1,ZERO<T>());
#else
              beta1 = ZERO<T>();
#endif
            }
            TIMER_STOP(Compute_Sinv_L_Resize);

            T * nzvalLUpd1 =  LUpdateBuf1.Data();
            size_t ldLUBuf1 = LUpdateBuf1.m(); 


            T * nzvalLUpd2 =  nullptr;
            size_t ldLUBuf2 = 0;
            T beta2 = ONE<T>();
            if(pLB1!=pLB2){
              TIMER_START(Compute_Sinv_L_Resize);
              if(LUpdateBuf2.Size()==0){
                LUpdateBuf2.Resize(pLB2->nzval.m(),SuperSize( snode.Index, this->super_ ));
#ifdef _OPENMP_BLOCKS_
                SetValue(LUpdateBuf2,ZERO<T>());
#else
                beta2 = ZERO<T>();
#endif
              }
              TIMER_STOP(Compute_Sinv_L_Resize);

              nzvalLUpd2 =  LUpdateBuf2.Data();
              ldLUBuf2 = LUpdateBuf2.m(); 
            }


            if(jsup>isup){
              isup = LB2.blockIdx;
              jsup = LB1.blockIdx;
              pLB1 = &LB2;
              pLB2 = &LB1;

              nzvalLUpd1 =  LUpdateBuf2.Data();
              nzvalLUpd2 =  LUpdateBuf1.Data();
              ldLUBuf1 = LUpdateBuf2.m(); 
              ldLUBuf2 = LUpdateBuf1.m(); 
              T tmp = beta1;
              beta1 = beta2;
              beta2 = tmp;
            }

            size_t ldLB1 = pLB1->nzval.m(); 
            size_t ldLB2 = pLB2->nzval.m(); 
            T * nzvalLB1 =  pLB1->nzval.Data();
            T * nzvalLB2 =  pLB2->nzval.Data();

#ifndef _OPENMP_BLOCKS_
            TIMER_START(Compute_AinvBuf_Resize);
            NumMat<T> AinvBuf;
            AinvBuf.Resize(pLB1->numRow,pLB2->numRow);
            TIMER_STOP(Compute_AinvBuf_Resize);
            T * nzvalAinv = AinvBuf.Data();
            size_t ldAinv = AinvBuf.m();
#endif


            std::vector<Int> cblockRows1; 
            std::vector<Int> cblockRows2;

            // Pin down the corresponding block in the part of Sinv.
            {
              std::vector<LBlock<T> >&  LcolSinv = this->L( LBj(jsup, grid_ ) );
              bool isBlockFound = false;
              TIMER_START(PARSING_ROW_BLOCKIDX);
              Int ibSinv = 0;
              for( ibSinv = 0; ibSinv < LcolSinv.size(); ibSinv++ ){
                // Found the (isup, jsup) block in Sinv
                if( LcolSinv[ibSinv].blockIdx == isup ){
                  isBlockFound = true;
                  break;
                }
              }
              TIMER_STOP(PARSING_ROW_BLOCKIDX);

              assert(isBlockFound);
              {
                {
                  TIMER_START(PARSING_ROW_STUFF);


                  Int lastRowIdxSinv = 0;
                  LBlock<T> * pSinvB = &LcolSinv[ibSinv];

                  // Row relative indices
                  Int* rowsLB1Ptr    = pLB1->rows.Data();
                  Int* rowsSinvBPtr = pSinvB->rows.Data();
                  Int SinvBnumRow = pSinvB->numRow;

                  // Column relative indicies
                  Int * rowsLB2Ptr = pLB2->rows.Data();

                  //find contiguous blocks in pLB1 and pLB2

                  Int blockRows1Size = 0; 
                  Int blockRows2Size = 0;
                  Int * blockRows1 = nullptr;
                  Int * blockRows2 = nullptr;

                  cblockRows1.resize(3*pLB1->numRow); 
                  blockRows1 = cblockRows1.data();

                  getBlocks(jsup, pLB1, pSinvB, blockRows1, blockRows1Size);


                  if(pLB1!=pLB2){
                    cblockRows2.resize(3*pLB2->numRow);
                    blockRows2 = cblockRows2.data();
                    getBlocks(jsup, pLB2, pSinvB, blockRows2, blockRows2Size);
                  }
                  else{
                    blockRows2 = blockRows1;
                    blockRows2Size = blockRows1Size;
                  }


                  // Transfer the values from Sinv to AinvBlock
                  T* nzvalSinv = pSinvB->nzval.Data();
                  Int ldSinv = pSinvB->numRow;

                  TIMER_STOP(PARSING_ROW_STUFF);

#ifdef _OPENMP_BLOCKS_
                  if(pLB1==pLB2){
                    Int j = 0;
                    for(Int jj = 0; jj<blockRows2Size;jj+=3){
                      auto fc = blockRows2[jj];
                      auto nc = blockRows2[jj+1];
                      Int  ifc = blockRows2[jj+2];
                      Int offsetJ = j;
                      Int i = 0;
                      for(Int ii = 0; ii<blockRows1Size;ii+=3){
                        auto fr = blockRows1[ii];
                        auto nr = blockRows1[ii+1];
                        Int ifr = blockRows1[ii+2];
                        Int offsetI = i;
                        if(nr>0 && nc > 0 ){
            TIMER_START(Compute_Sinv_LT_GEMM);
                          blas::Gemm('N','N',nr, superSize, nc, MINUS_ONE<T>(), 
                              &nzvalSinv[fr+fc*ldSinv], ldSinv, 
                              &nzvalLB2[j], ldLB2, beta1,
                              &nzvalLUpd1[i], ldLUBuf1);
            TIMER_STOP(Compute_Sinv_LT_GEMM);
#ifdef _PRINT_STATS_  
                          this->localFlops_+=flops::Gemm<T>(nr, superSize, nc);
#endif
                        }


                        i+=nr;
                      }
                      j+=nc;
                    }
                  }
                  else if(pLB1!=pLB2){
                    Int j = 0;
                    for(Int jj = 0; jj<blockRows2Size;jj+=3){
                      auto fc = blockRows2[jj];
                      auto nc = blockRows2[jj+1];
                      Int  ifc = blockRows2[jj+2];
                      Int offsetJ = j;
                      Int i = 0;
                      for(Int ii = 0; ii<blockRows1Size;ii+=3){
                        auto fr = blockRows1[ii];
                        auto nr = blockRows1[ii+1];
                        Int ifr = blockRows1[ii+2];
                        Int offsetI = i;
                        if(nr>0 && nc > 0 ){
            TIMER_START(Compute_Sinv_LT_GEMM);
                          blas::Gemm('N','N',nr, superSize, nc, MINUS_ONE<T>(), 
                              &nzvalSinv[fr+fc*ldSinv], ldSinv, 
                              &nzvalLB2[j], ldLB2, beta1,
                              &nzvalLUpd1[i], ldLUBuf1);
            TIMER_STOP(Compute_Sinv_LT_GEMM);

#ifdef _PRINT_STATS_  
                          this->localFlops_+=flops::Gemm<T>(nr, superSize, nc);
#endif
                          assert(ldSinv>=nr);
                          assert(ldLB1>=nr);
                          assert(ldLUBuf2>=nc);
            TIMER_START(Compute_Sinv_LT_GEMM);
                          blas::Gemm('T','N', nc,  superSize , nr , MINUS_ONE<T>(), 
                              &nzvalSinv[fc*ldSinv+fr], ldSinv, 
                              &nzvalLB1[i], ldLB1, beta2,
                              &nzvalLUpd2[j], ldLUBuf2);
            TIMER_STOP(Compute_Sinv_LT_GEMM);
#ifdef _PRINT_STATS_  
                          this->localFlops_+=flops::Gemm<T>(nc, superSize, nr);
#endif

                        }


                        i+=nr;
                      }
                      j+=nc;
                    }
                  }
#endif
#ifndef _OPENMP_BLOCKS_
                  TIMER_START(PARSING_ROW_COPY);
                  //copy data in AinvBuf
                  Int j = 0;
                  for(Int jj = 0; jj<blockRows2Size;jj+=3){
                    auto fc = blockRows2[jj];
                    auto nc = blockRows2[jj+1];
                    Int  ifc = blockRows2[jj+2];
                    Int offsetJ = j;
                    Int i = 0;
                    for(Int ii = 0; ii<blockRows1Size;ii+=3){
                      auto fr = blockRows1[ii];
                      auto nr = blockRows1[ii+1];
                      Int ifr = blockRows1[ii+2];
                      Int offsetI = i;
                      if(nr>0 && nc > 0 ){
                        lapack::Lacpy( 'A', nr, nc, &nzvalSinv[fr+fc*ldSinv],ldSinv,&nzvalAinv[i+j*ldAinv],ldAinv );
                        //for(Int ii = 0; ii<nr; ii++){
                        //  for(Int jj = 0; jj<nc; jj++){
                        //    nzvalAinv[i+ii+(j+jj)*ldAinv] = nzvalSinv[fr+ii+(fc+jj)*ldSinv];
                        //  }
                        //}
                      }
                      i+=nr;
                    }
                    j+=nc;
                  }
                  TIMER_STOP(PARSING_ROW_COPY);
#endif
                  TIMER_STOP(PARSING_ROW_STUFF);

                  //isBlockFound = true;
                  //break;
                }	
              } // for (ibSinv )
              if( isBlockFound == false ){
                std::ostringstream msg;
                msg << "Block(" << isup << ", " << jsup 
                  << ") did not find a matching block in Sinv." << std::endl;
                ErrorHandling( msg.str().c_str() );
              }
            } // if (isup, jsup) is in L
#ifndef _OPENMP_BLOCKS_
            TIMER_START(Compute_Sinv_LT_GEMM);
            blas::Gemm('N','N',AinvBuf.m(), superSize,AinvBuf.n(), MINUS_ONE<T>(), 
                AinvBuf.Data(), AinvBuf.m(), 
                nzvalLB2, ldLB2, beta1,
                nzvalLUpd1, ldLUBuf1);
#ifdef _PRINT_STATS_  
            this->localFlops_+=flops::Gemm<T>(AinvBuf.m(), superSize,AinvBuf.n());
#endif

            if(pLB1 != pLB2){
              blas::Gemm('T','N', AinvBuf.n() ,  superSize, AinvBuf.m() , MINUS_ONE<T>(), 
                  AinvBuf.Data(), AinvBuf.m(), 
                  nzvalLB1, ldLB1, beta2,
                  nzvalLUpd2, ldLUBuf2);
#ifdef _PRINT_STATS_  
              this->localFlops_+=flops::Gemm<T>(AinvBuf.n(), superSize,AinvBuf.m());
#endif
            }
            TIMER_STOP(Compute_Sinv_LT_GEMM);
#endif

          };







          while(!(all_doneBCastL = std::all_of(bcastLDataDone.begin(), bcastLDataDone.end()-1, [](int v) { return v>0; })))//gemmProcessed<gemmToDo)
          {

            TIMER_START(WaitContentLU);
            readySnode.clear();
            while( !(all_doneBCastL = std::all_of(bcastLDataDone.begin(), bcastLDataDone.end()-1, [](int v) { return v>0; })) && readySnode.empty()) {
              //TODO this ensure that one of the Tree is IsDone(). Ideally, we should react at IsDataReceived() 
              TreeBcast_Testsome( treeIdx, this->bcastLDataTree_, bcastLDataIdx, bcastLDataDone);

              for(auto ltidx : bcastLDataIdx){
                Int tidx = treeIdx[ltidx];
                Int supidx = treeToSupidx[ltidx]; 
                auto & snode = arrSuperNodes[supidx];

                Int offset = tidx - snodeTreeOffset_[snode.Index];  
                Int blkIdx = snodeTreeToBlkidx_[snode.Index][offset];

                if(MYPROC(this->grid_) == PNUM(PROW(blkIdx,this->grid_),PCOL(blkIdx,this->grid_),this->grid_)){
                  readySnode.push_back(std::make_tuple(supidx,blkIdx,blkIdx,ltidx,ltidx));
                }

                for(Int ltidx2 = ltidx-1;ltidx2>=0;ltidx2--){
                  if (treeToSupidx[ltidx2]!=supidx){
                    break;
                  }

                  if( bcastLDataDone[ltidx2] ){
                    Int tidx2 = treeIdx[ltidx2];
                    Int offset2 = tidx2 - snodeTreeOffset_[snode.Index];  
                    Int blkIdx2 = snodeTreeToBlkidx_[snode.Index][offset2];
                    if(blkIdx2<blkIdx){
                      if(MYROW(this->grid_)==PROW(blkIdx,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx2,this->grid_)){
//#if ( _DEBUGlevel_ >= 1 )
//                        statusOFS<<"["<<ksup<<"] Tree "<<blkIdx<<" and Tree "<<blkIdx2<<" are both done"<<std::endl;
//#endif
                        readySnode.push_back(std::make_tuple(supidx,blkIdx2,blkIdx,ltidx2,ltidx));
                      }
                    }
                    else{
                      if(MYROW(this->grid_)==PROW(blkIdx2,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx,this->grid_)){
                        if ( bcastLDataDone[ltidx2] < bcastLDataDone[ltidx] ){
//#if ( _DEBUGlevel_ >= 1 )
//                          statusOFS<<"["<<ksup<<"] Tree "<<blkIdx<<" and Tree "<<blkIdx2<<" are both done"<<std::endl;
//#endif
                          readySnode.push_back(std::make_tuple(supidx,blkIdx,blkIdx2,ltidx,ltidx2));
                        }
                      }
                    }
                  }
                }


                for(Int ltidx2 = ltidx+1;ltidx2<treeIdx.size();ltidx2++){
                  if (treeToSupidx[ltidx2]!=supidx){
                    break;
                  }

                  if(bcastLDataDone[ltidx2]){
                    Int tidx2 = treeIdx[ltidx2];
                    Int offset2 = tidx2 - snodeTreeOffset_[snode.Index];  
                    Int blkIdx2 = snodeTreeToBlkidx_[snode.Index][offset2];
                    if(blkIdx2>blkIdx){
                      if(MYROW(this->grid_)==PROW(blkIdx2,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx,this->grid_)){
                        if ( bcastLDataDone[ltidx2] < bcastLDataDone[ltidx] ){
//#if ( _DEBUGlevel_ >= 1 )
//                          statusOFS<<"["<<ksup<<"] Tree "<<blkIdx<<" and Tree "<<blkIdx2<<" are both done"<<std::endl;
//#endif
                          readySnode.push_back(std::make_tuple(supidx,blkIdx,blkIdx2,ltidx,ltidx2));
                        }
                      }
                    }
                    else {
                      if(MYROW(this->grid_)==PROW(blkIdx,this->grid_) && MYCOL(this->grid_)==PCOL(blkIdx2,this->grid_)){
//#if ( _DEBUGlevel_ >= 1 )
//                        statusOFS<<"["<<ksup<<"] Tree "<<blkIdx<<" and Tree "<<blkIdx2<<" are both done"<<std::endl;
//#endif
                        readySnode.push_back(std::make_tuple(supidx,blkIdx2,blkIdx,ltidx2,ltidx));
                      }
                    }
                  }
                }
              }
            }
            TIMER_STOP(WaitContentLU);

            //If I have some work to do 
            if(readySnode.size()>0)
            {
              //gdb_lock();
              for(auto && operation: readySnode){
                auto & supidx = std::get<0>(operation);
                auto & blkIdx1 = std::get<1>(operation);
                auto & blkIdx2 = std::get<2>(operation);
                auto & ltidx1 = std::get<3>(operation);
                auto & ltidx2 = std::get<4>(operation);
                auto & snode = arrSuperNodes[supidx];

                if (blkIdx1==blkIdx2){
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<snode.Index<<"] Processing operation A^-1 * L("<<blkIdx1<<","<<snode.Index<<")"<<std::endl;
#endif

                  auto & bcastLDataTree1 = this->bcastLDataTree_[ treeIdx[ltidx1] ];
                  LBlock<T> LB1;
                  //Unpack the data
                  UnpackLBlock(bcastLDataTree1, LB1);
                  auto & LUpdateBuf1 = snode.LUpdateBufBlk[treeToBufIdx[ltidx1]];
                  ComputeLUpdateBuf(snode, LB1, LB1, LUpdateBuf1, LUpdateBuf1);
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<snode.Index<<"] Decreasing gemmCount of "<<ltidx1<<" after U("<<blkIdx1<<")"<<std::endl;
                  assert(gemmCount[ltidx1]>0);
#endif
                  gemmCount[ltidx1]--;
                }
                else{
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<snode.Index<<"] Processing operations A^-1 * L("<<blkIdx1<<","<<snode.Index<<") and A^-1 * L("<<blkIdx2<<","<<snode.Index<<")"<<std::endl;
#endif

                  auto & bcastLDataTree1 = this->bcastLDataTree_[ treeIdx[ltidx1] ];
                  auto & bcastLDataTree2 = this->bcastLDataTree_[ treeIdx[ltidx2] ];

                  LBlock<T> LB1,LB2;
                  //Unpack the data
                  UnpackLBlock(bcastLDataTree1, LB1);
                  UnpackLBlock(bcastLDataTree2, LB2);

                  auto & LUpdateBuf1 = snode.LUpdateBufBlk[treeToBufIdx[ltidx1]];
                  auto & LUpdateBuf2 = snode.LUpdateBufBlk[treeToBufIdx[ltidx2]];
                  ComputeLUpdateBuf(snode, LB1, LB2,LUpdateBuf1,LUpdateBuf2);
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<snode.Index<<"] Decreasing gemmCount of "<<ltidx1<<" after U("<<blkIdx1<<","<<blkIdx2<<")"<<std::endl;
                  statusOFS<<"["<<snode.Index<<"] Decreasing gemmCount of "<<ltidx2<<" after U("<<blkIdx1<<","<<blkIdx2<<")"<<std::endl;
                  assert(gemmCount[ltidx1]>0);
                  assert(gemmCount[ltidx2]>0);
#endif
                  gemmCount[ltidx1]--;
                  gemmCount[ltidx2]--;
                }
                if (gemmCount[ltidx1]==0){
                  auto & bcastLDataTree1 = this->bcastLDataTree_[ treeIdx[ltidx1] ];
                  auto & LUpdateBuf1 = snode.LUpdateBufBlk[treeToBufIdx[ltidx1]];
                  //Now participate to the reduction of L1
                  auto & redLTree1 = this->redLTree2_[ treeIdx[ltidx1] ];
                  assert(redLTree1!=nullptr);
                  assert(LUpdateBuf1.Size() == redLTree1->GetMsgSize());
                  redLTree1->SetLocalBuffer(LUpdateBuf1.Data());
#ifdef _PRINT_STATS_
                  this->localFlops_+=flops::Axpy<T>(LUpdateBuf1.Size());
#endif
                  redLTree1->SetDataReady(true);
                  redLTree1->Progress();
                  bcastLDataTree1->cleanupBuffers();
                }
                if (blkIdx1!=blkIdx2 && gemmCount[ltidx2]==0){
                  auto & bcastLDataTree2 = this->bcastLDataTree_[ treeIdx[ltidx2] ];
                  auto & LUpdateBuf2 = snode.LUpdateBufBlk[treeToBufIdx[ltidx2]];
                  //Now participate to the reduction of L2
                  auto & redLTree2 = this->redLTree2_[ treeIdx[ltidx2] ];
                  assert(redLTree2!=nullptr);
                  //redLTree2->SetMsgSize( LUpdateBuf2.Size() );
                  assert(LUpdateBuf2.Size() == redLTree2->GetMsgSize());
                  redLTree2->SetLocalBuffer(LUpdateBuf2.Data());
#ifdef _PRINT_STATS_
                  this->localFlops_+=flops::Axpy<T>(LUpdateBuf2.Size());
#endif
                  redLTree2->SetDataReady(true);
                  redLTree2->Progress();
                  bcastLDataTree2->cleanupBuffers();
                }
              }
            }

            //Progress the reduction trees
            //        TIMER_START(Progress_all_reductions);
            //        TIMER_START(Progress_all_reductions_L);
            //            TreeReduce_ProgressAll(superList[lidx],this->redLTree2_);
            //        TIMER_STOP(Progress_all_reductions_L);
            //        TIMER_START(Progress_all_reductions_U);
            //            TreeReduce_ProgressAll(superList[lidx],this->redUTree2_);
            //        TIMER_STOP(Progress_all_reductions_U);
            //        TIMER_STOP(Progress_all_reductions);
          }



        }
        TIMER_STOP(Compute_Sinv_LU);


#ifdef BLOCK_REDUCE_L
        TreeReduce_Waitall( treeIdx, this->redLTree2_);
#endif

        TIMER_START(Reduce_Sinv_L);




        for(Int ltidx=0;ltidx<treeIdx.size();ltidx++){
          Int tidx = treeIdx[ltidx];
          auto & redLTree = this->redLTree2_[tidx];
          if(redLTree!=nullptr){
            if(redLTree->IsRoot()){
              redLTree->AllocRecvBuffers();
              redLTree->SetDataReady(true);
              redLTree->Progress();
            }
          }
        }


        while(!(all_doneL = std::all_of(redLdone.begin(), redLdone.end(), [](bool v) { return v; }))){
          TreeReduce_Testsome( treeIdx, this->redLTree2_, redLIdx, redLdone);

          for(auto ltidx : redLIdx){
            Int tidx = treeIdx[ltidx];
            Int supidx = treeToSupidx[ltidx]; 
            auto & snode = arrSuperNodes[supidx];

            Int offset = tidx - snodeTreeOffset_[snode.Index];  
            Int blkIdx = snodeTreeToBlkidx_[snode.Index][offset];

            auto & redLTree = this->redLTree2_[tidx];

#if ( _DEBUGlevel_ >= 1 )
            statusOFS << std::endl << "["<<snode.Index<<"] REDUCE L is done"<<std::endl;
#endif
            if(redLTree->IsRoot()){

              //TODO this has to be index by ib instead of blkidx ? or use a map instead....
              auto & LUpdateBuf = snode.LUpdateBufBlk[treeToBufIdx[ltidx]];

              if( LUpdateBuf.ByteSize() == 0 ){
                Int n = SuperSize( snode.Index, this->super_);
                Int m = redLTree->GetMsgSize() / n;
                LUpdateBuf.Resize(m,n); 
                SetValue(LUpdateBuf, ZERO<T>());
              }

              //copy the buffer from the reduce tree
              redLTree->SetLocalBuffer(LUpdateBuf.Data());

              {
                //---------Computing  Diagonal block, all processors in the column are participating to all pipelined supernodes
                if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){
#if ( _DEBUGlevel_ >= 2 )
                  statusOFS << std::endl << "["<<snode.Index<<"] "<<   "Updating the diagonal block" << std::endl << std::endl; 
#endif
                  std::vector<LBlock<T> >&  Lcol = this->L( LBj( snode.Index, grid_ ) );

                  //If this is the first time we are updating that supernode
                  if(gemmCountDiag[supidx]==0){
                    //Allocate DiagBuf even if Lcol.size() == 0
                    snode.DiagBuf.Resize(SuperSize( snode.Index, super_ ), SuperSize( snode.Index, super_ ));
                    SetValue(snode.DiagBuf, ZERO<T>());
                  }

                  // Do I own the diagonal block ?
                  Int startIb = (MYROW( grid_ ) == PROW( snode.Index, grid_ ))?1:0;
                  Int ib = treeToIb[ltidx];
                  LBlock<T> & LB = Lcol[ib];
                  {
#ifdef GEMM_PROFILE
                    gemm_stat.push_back(snode.DiagBuf.m());
                    gemm_stat.push_back(snode.DiagBuf.n());
                    gemm_stat.push_back(Lcol[ib].numRow);
#endif
                    assert(LB.nzval.Size()==LUpdateBuf.Size());
                    assert(LUpdateBuf.m()>=LB.numRow);
                    blas::Gemm( 'T', 'N', snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow,
                        MINUS_ONE<T>(), 
                        LUpdateBuf.Data(), LUpdateBuf.m(),
                        LB.nzval.Data(), LB.nzval.m(), 
                        ONE<T>(), snode.DiagBuf.Data(), snode.DiagBuf.m() );
                    gemmCountDiag[supidx]++;
#ifdef _PRINT_STATS_
                    this->localFlops_+=flops::Gemm<T>(snode.DiagBuf.m(), snode.DiagBuf.n(), LB.numRow);
#endif
                  }
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS << std::endl << "["<<snode.Index<<"] "<<   "Updated the diagonal block" << std::endl << std::endl; 
#endif
                }
              }

              if( MYCOL( grid_ ) == PCOL( snode.Index, grid_ ) ){
                auto & redDTree = this->redDTree2_[snode.Index];
                if(redDTree!=nullptr){
                  std::vector<LBlock<T> >&  Lcol = this->L( LBj( snode.Index, grid_ ) );
                  Int startIb = (MYROW( grid_ ) == PROW( snode.Index, grid_ ))?1:0;
                  if(gemmCountDiag[supidx]==Lcol.size()-startIb){
                    //send the data
                    if( redDTree->IsRoot() &&  snode.DiagBuf.Size()==0){
                      snode.DiagBuf.Resize( SuperSize( snode.Index, this->super_ ), SuperSize( snode.Index, this->super_ ));
                      SetValue(snode.DiagBuf, ZERO<T>());
                    }

                    if(!redDTree->IsAllocated()){
                      redDTree->AllocRecvBuffers();
                    }

                    //set the buffer and mark as active
                    redDTree->SetLocalBuffer(snode.DiagBuf.Data());
#ifdef _PRINT_STATS_
                    this->localFlops_+=flops::Axpy<T>(snode.DiagBuf.Size());
#endif

                    redDTree->SetDataReady(true);
                    redDTree->Progress();
                  }
                }
              }


              //Overwrite L with LUpdateBuf
              TIMER_START(Update_L);
#if ( _DEBUGlevel_ >= 1 )
              statusOFS << std::endl << "["<<snode.Index<<"] "
                << "Finish updating the L part by filling LUpdateBufReduced"
                << " back to L" << std::endl << std::endl; 
#endif

              auto & Lcol = this->L(LBj(snode.Index,this->grid_));
              Int ib = treeToIb[ltidx];
              auto & LB = Lcol[ib];

#if ( _DEBUGlevel_ >= 1 )
              statusOFS << "["<<snode.Index<<"] LB "<<blkIdx<<" rows "<<LB.rows<<std::endl;
              statusOFS << "["<<snode.Index<<"] LB "<<blkIdx<<" before "<<LB.nzval<<std::endl;
#endif
              //Indices follow L order... look at Lrow
              lapack::Lacpy( 'A', LB.numRow, LB.numCol, 
                  LUpdateBuf.Data(),
                  LUpdateBuf.m(), LB.nzval.Data(), LB.numRow );

#if ( _DEBUGlevel_ >= 1 )
              statusOFS << "["<<snode.Index<<"] LB "<<blkIdx<<" after "<<LB.nzval<<std::endl;
#endif
              TIMER_STOP(Update_L);
            }
            redLTree->cleanupBuffers();
          }

          //Progress U and D reduction trees
          //        TIMER_START(Progress_all_reductions);
          //        TIMER_START(Progress_all_reductions_U);
          //          TreeReduce_ProgressAll(superList[lidx],this->redUTree2_);
          //        TIMER_STOP(Progress_all_reductions_U);
          //        TIMER_START(Progress_all_reductions_D);
          //          TreeReduce_ProgressAll(superList[lidx],this->redDTree2_);
          //        TIMER_STOP(Progress_all_reductions_D);
          //        TIMER_STOP(Progress_all_reductions);

          //Check if we are done with the reductions of L

        }
        TIMER_STOP(Reduce_Sinv_L);
#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"All reductions of L are done"<<std::endl;
#endif


        for(auto && snode : arrSuperNodes){
          auto & redDTree = this->redDTree2_[snode.Index];
          if(redDTree!=nullptr){
            if(redDTree->IsRoot()){
              if( snode.DiagBuf.Size()==0){
                snode.DiagBuf.Resize( SuperSize( snode.Index, this->super_ ), SuperSize( snode.Index, this->super_ ));
                SetValue(snode.DiagBuf, ZERO<T>());
              }
              redDTree->AllocRecvBuffers();
              redDTree->SetDataReady(true);
              redDTree->Progress();
            }
          }
        }

#ifdef BLOCK_REDUCE_D
        TreeReduce_Waitall( superList[lidx], this->redDTree2_);
#endif

        //Reduce D
        TIMER_START(Reduce_Diagonal);
                while(!(all_doneD = std::all_of(redDdone.begin(), redDdone.end(), [](bool v) { return v; }))){
          TreeReduce_Testsome( superList[lidx], this->redDTree2_, redDIdx, redDdone);

          for(auto supidx : redDIdx){ 
            auto & snode = arrSuperNodes[supidx];


            auto & redDTree = this->redDTree2_[snode.Index];
#if ( _DEBUGlevel_ >= 1 )
            statusOFS << std::endl << "["<<snode.Index<<"] REDUCE D is done"<<std::endl;
#endif
            if(redDTree->IsRoot()){
              //copy the buffer from the reduce tree
              if( snode.DiagBuf.Size()==0 ){
                snode.DiagBuf.Resize( SuperSize( snode.Index, this->super_ ), SuperSize( snode.Index, this->super_ ));
                SetValue(snode.DiagBuf, ZERO<T>());
              }
              redDTree->SetLocalBuffer(snode.DiagBuf.Data());

              LBlock<T> &  LB = this->L( LBj( snode.Index, this->grid_ ) ).front();
              //Transpose(LB.nzval, LB.nzval);
              blas::Axpy( LB.numRow * LB.numCol, ONE<T>(), snode.DiagBuf.Data(), 1, LB.nzval.Data(), 1 );
              Symmetrize( LB.nzval );

#ifdef _PRINT_STATS_
              this->localFlops_+=flops::Axpy<T>(LB.numRow * LB.numCol);
#endif

#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"["<<snode.Index<<"] Diag after update:"<<std::endl<<LB.nzval<<std::endl;
#endif
            }
            redDTree->cleanupBuffers();
          }

          //Check if we are done with the reduction of D
          //all_doneD = std::all_of(redDdone.begin(), redDdone.end(), [](bool v) { return v; });
        }
        TIMER_STOP(Reduce_Diagonal);
        //--------------------- End of reduce of D -------------------------

#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"All reductions of D are done"<<std::endl;
#endif


        TIMER_START(Barrier);
        TreeBcast_Waitall( treeIdx,  this->bcastLDataTree_);
        TreeReduce_Waitall( treeIdx, this->redLTree2_);
        TreeReduce_Waitall( superList[lidx], this->redDTree2_);
        TIMER_STOP(Barrier);




      }
    }

  template<typename T>
    void PMatrix<T>::ConstructCommunicationPattern	(  )
    {
      ConstructCommunicationPattern_P2p();
    } 		// -----  end of method PMatrix::ConstructCommunicationPattern  ----- 



  template<typename T>
    void PMatrix<T>::ConstructCommunicationPattern_P2p	(  )
    {
      if (options_->symmetricStorage!=1){

#ifdef COMMUNICATOR_PROFILE
        CommunicatorStat stats;
        stats.countSendToBelow_.Resize(numSuper);
        stats.countSendToRight_.Resize(numSuper);
        stats.countRecvFromBelow_.Resize( numSuper );
        SetValue( stats.countSendToBelow_, 0 );
        SetValue( stats.countSendToRight_, 0 );
        SetValue( stats.countRecvFromBelow_, 0 );
#endif




        Int numSuper = this->NumSuper();

        TIMER_START(Allocate);



        fwdToBelowTree_.resize(numSuper, NULL );
        fwdToRightTree_.resize(numSuper, NULL );
        redToLeftTree_.resize(numSuper, NULL );
        redToAboveTree_.resize(numSuper, NULL );

        isSendToBelow_.Resize(grid_->numProcRow, numSuper);
        isSendToRight_.Resize(grid_->numProcCol, numSuper);
        isSendToDiagonal_.Resize( numSuper );
        SetValue( isSendToBelow_, false );
        SetValue( isSendToRight_, false );
        SetValue( isSendToDiagonal_, false );

        isSendToCrossDiagonal_.Resize(grid_->numProcCol+1, numSuper );
        SetValue( isSendToCrossDiagonal_, false );
        isRecvFromCrossDiagonal_.Resize(grid_->numProcRow+1, numSuper );
        SetValue( isRecvFromCrossDiagonal_, false );

        isRecvFromAbove_.Resize( numSuper );
        isRecvFromLeft_.Resize( numSuper );
        isRecvFromBelow_.Resize( grid_->numProcRow, numSuper );
        SetValue( isRecvFromAbove_, false );
        SetValue( isRecvFromBelow_, false );
        SetValue( isRecvFromLeft_, false );

        TIMER_STOP(Allocate);

        TIMER_START(GetEtree);
        std::vector<Int> snodeEtree(this->NumSuper());
        GetEtree(snodeEtree);//将eliminatino tree保存在snodeEtree中，每个数组内容表示它的parent supernode
        TIMER_STOP(GetEtree);



#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "Local column communication" << std::endl;
#endif
        // localColBlockRowIdx stores the nonzero block indices for each local block column.
        // The nonzero block indices including contribution from both L and U.
        // Dimension: numLocalBlockCol x numNonzeroBlock
        std::vector<std::set<Int> >   localColBlockRowIdx;//用来存储本地存储的supernode中的nonzero block的row indices

        localColBlockRowIdx.resize( this->NumLocalBlockCol() );//大小是每个processor平均分配的supernode数量


        TIMER_START(Column_communication);


        for( Int ksup = 0; ksup < numSuper; ksup++ ){
          // All block columns perform independently
          if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){//如果supernode在本processor上面


            // Communication
            std::vector<Int> tAllBlockRowIdx;
            std::vector<Int> & colBlockIdx = ColBlockIdx(LBj(ksup, grid_));//返回这个supernode的所有列？这个数组不太懂什么意思
            TIMER_START(Allgatherv_Column_communication);
            if( grid_ -> mpisize != 1 )
              mpi::Allgatherv( colBlockIdx, tAllBlockRowIdx, grid_->colComm );//这里的意思应该是把所有的这一列block的nonzero row都汇聚起来了？
            else
              tAllBlockRowIdx = colBlockIdx;

            TIMER_STOP(Allgatherv_Column_communication);

            localColBlockRowIdx[LBj( ksup, grid_ )].insert(
                tAllBlockRowIdx.begin(), tAllBlockRowIdx.end() );//对于这一个supernode插入所有的nonzero block row indices

#if ( _DEBUGlevel_ >= 1 )
            statusOFS 
              << " Column block " << ksup 
              << " has the following nonzero block rows" << std::endl;
            for( std::set<Int>::iterator si = localColBlockRowIdx[LBj( ksup, grid_ )].begin();
                si != localColBlockRowIdx[LBj( ksup, grid_ )].end();
                si++ ){
              statusOFS << *si << "  ";
            }
            statusOFS << std::endl; 
#endif

          } // if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) )
        } // for(ksup)



        TIMER_STOP(Column_communication);

        TIMER_START(Row_communication);
#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "Local row communication" << std::endl;
#endif
        // localRowBlockColIdx stores the nonzero block indices for each local block row.
        // The nonzero block indices including contribution from both L and U.
        // Dimension: numLocalBlockRow x numNonzeroBlock
        std::vector<std::set<Int> >   localRowBlockColIdx;//这个和上面同理，但是是对于每一行保存所有nonzero column block indices 

        localRowBlockColIdx.resize( this->NumLocalBlockRow() );

        for( Int ksup = 0; ksup < numSuper; ksup++ ){
          // All block columns perform independently
          if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){

            // Communication
            std::vector<Int> tAllBlockColIdx;
            std::vector<Int> & rowBlockIdx = RowBlockIdx(LBi(ksup, grid_));
            TIMER_START(Allgatherv_Row_communication);
            if( grid_ -> mpisize != 1 )
              mpi::Allgatherv( rowBlockIdx, tAllBlockColIdx, grid_->rowComm );
            else
              tAllBlockColIdx = rowBlockIdx;

            TIMER_STOP(Allgatherv_Row_communication);

            localRowBlockColIdx[LBi( ksup, grid_ )].insert(
                tAllBlockColIdx.begin(), tAllBlockColIdx.end() );

#if ( _DEBUGlevel_ >= 1 )
            statusOFS 
              << " Row block " << ksup 
              << " has the following nonzero block columns" << std::endl;
            for( std::set<Int>::iterator si = localRowBlockColIdx[LBi( ksup, grid_ )].begin();
                si != localRowBlockColIdx[LBi( ksup, grid_ )].end();
                si++ ){
              statusOFS << *si << "  ";
            }
            statusOFS << std::endl; 
#endif

          } // if( MYROW( grid_ ) == PROW( ksup, grid_ ) )
        } // for(ksup)


        TIMER_STOP(Row_communication);

        TIMER_START(STB_RFA);
        for( Int ksup = 0; ksup < numSuper - 1; ksup++ ){

#ifdef COMMUNICATOR_PROFILE
          std::vector<bool> sTB(grid_->numProcRow,false);
#endif
          // Loop over all the supernodes to the right of ksup
          Int jsup = snodeEtree[ksup];//从子supernode开始
          while(jsup<numSuper){
            Int jsupLocalBlockCol = LBj( jsup, grid_ );//找到这个supernode的local block
            Int jsupProcCol = PCOL( jsup, grid_ );//找到这个supernode对应了processor column
            if( MYCOL( grid_ ) == jsupProcCol ){//如果本processor和这个supernode对应的processor在同一个column
              // SendToBelow / RecvFromAbove only if (ksup, jsup) is nonzero.
              if( localColBlockRowIdx[jsupLocalBlockCol].count( ksup ) > 0 ) {
                for( std::set<Int>::iterator si = localColBlockRowIdx[jsupLocalBlockCol].begin();
                    si != localColBlockRowIdx[jsupLocalBlockCol].end(); si++	 ){//遍历这个processor的这个supernode的非空的row block
                  Int isup = *si;
                  Int isupProcRow = PROW( isup, grid_ );//找到这个row block存储的对应的processor row
                  if( isup > ksup ){//如果row block在这个supernode下面
                    if( MYROW( grid_ ) == isupProcRow ){//如果本processor和row block对应一个processor row
                      isRecvFromAbove_(ksup) = true;//就从上面接受信息
                    }
                    if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){//如果本processor和ksup是一个processor row
                      isSendToBelow_( isupProcRow, ksup ) = true;//那么就发送信息到下面
                    }
                  } // if( isup > ksup )
                } // for (si)
              } // if( localColBlockRowIdx[jsupLocalBlockCol].count( ksup ) > 0 )
#ifdef COMMUNICATOR_PROFILE
              sTB[ PROW(ksup,grid_) ] = true;
              if( localColBlockRowIdx[jsupLocalBlockCol].count( ksup ) > 0 ) {
                for( std::set<Int>::iterator si = localColBlockRowIdx[jsupLocalBlockCol].begin();
                    si != localColBlockRowIdx[jsupLocalBlockCol].end(); si++	 ){
                  Int isup = *si;
                  Int isupProcRow = PROW( isup, grid_ );
                  if( isup > ksup ){
                    sTB[isupProcRow] = true;
                  } // if( isup > ksup )
                } // for (si)
              }
#endif



            } // if( MYCOL( grid_ ) == PCOL( jsup, grid_ ) )
            jsup = snodeEtree[jsup];//继续得到elimation tree的父节点
          } // for(jsup)

#ifdef COMMUNICATOR_PROFILE
          Int count= std::count(sTB.begin(), sTB.end(), true);
          Int color = sTB[MYROW(grid_)];
          if(count>1){
            std::vector<Int> & snodeList = stats.maskSendToBelow_[sTB];
            snodeList.push_back(ksup);
          }
          stats.countSendToBelow_(ksup) = count * color;
#endif


        } // for(ksup)
        TIMER_STOP(STB_RFA);

        TIMER_START(STR_RFL_RFB);
        for( Int ksup = 0; ksup < numSuper - 1; ksup++ ){//同理，只不过是对左右进行操作

#ifdef COMMUNICATOR_PROFILE
          std::vector<bool> sTR(grid_->numProcCol,false);
          std::vector<bool> rFB(grid_->numProcRow,false);
#endif
          // Loop over all the supernodes below ksup
          Int isup = snodeEtree[ksup];
          while(isup<numSuper){
            Int isupLocalBlockRow = LBi( isup, grid_ );
            Int isupProcRow       = PROW( isup, grid_ );
            if( MYROW( grid_ ) == isupProcRow ){
              // SendToRight / RecvFromLeft only if (isup, ksup) is nonzero.
              if( localRowBlockColIdx[isupLocalBlockRow].count( ksup ) > 0 ){
                for( std::set<Int>::iterator si = localRowBlockColIdx[isupLocalBlockRow].begin();
                    si != localRowBlockColIdx[isupLocalBlockRow].end(); si++ ){
                  Int jsup = *si;
                  Int jsupProcCol = PCOL( jsup, grid_ );
                  if( jsup > ksup ){
                    if( MYCOL( grid_ ) == jsupProcCol ){
                      isRecvFromLeft_(ksup) = true;
                    }
                    if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
                      isSendToRight_( jsupProcCol, ksup ) = true;
                    }
                  }
                } // for (si)
              } // if( localRowBlockColIdx[isupLocalBlockRow].count( ksup ) > 0 )

#ifdef COMMUNICATOR_PROFILE
              sTR[ PCOL(ksup,grid_) ] = true;
              if( localRowBlockColIdx[isupLocalBlockRow].count( ksup ) > 0 ){
                for( std::set<Int>::iterator si = localRowBlockColIdx[isupLocalBlockRow].begin();
                    si != localRowBlockColIdx[isupLocalBlockRow].end(); si++ ){
                  Int jsup = *si;
                  Int jsupProcCol = PCOL( jsup, grid_ );
                  if( jsup > ksup ){
                    sTR[ jsupProcCol ] = true;
                  } // if( jsup > ksup )
                } // for (si)
              }
#endif


            } // if( MYROW( grid_ ) == isupProcRow )

            if( MYCOL( grid_ ) == PCOL(ksup, grid_) ){
              if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){ 
                isRecvFromBelow_(isupProcRow,ksup) = true;
              }    
              else if (MYROW(grid_) == isupProcRow){
                isSendToDiagonal_(ksup)=true;
              }    
            } // if( MYCOL( grid_ ) == PCOL(ksup, grid_) )

#ifdef COMMUNICATOR_PROFILE
            rFB[ PROW(ksup,grid_) ] = true;
            if( MYCOL( grid_ ) == PCOL(ksup, grid_) ){
              rFB[ isupProcRow ] = true;
            } // if( MYCOL( grid_ ) == PCOL(ksup, grid_) )
#endif


            isup = snodeEtree[isup];
          } // for (isup)

#ifdef COMMUNICATOR_PROFILE
          Int count= std::count(rFB.begin(), rFB.end(), true);
          Int color = rFB[MYROW(grid_)];
          if(count>1){
            std::vector<Int> & snodeList = stats.maskRecvFromBelow_[rFB];
            snodeList.push_back(ksup);
          }
          stats.countRecvFromBelow_(ksup) = count * color;

          count= std::count(sTR.begin(), sTR.end(), true);
          color = sTR[MYCOL(grid_)];
          if(count>1){
            std::vector<Int> & snodeList = stats.maskSendToRight_[sTR];
            snodeList.push_back(ksup);
          }
          stats.countSendToRight_(ksup) = count * color;
#endif


        }	 // for (ksup)

        TIMER_STOP(STR_RFL_RFB);

#ifdef COMMUNICATOR_PROFILE
        {


          //      statusOFS << std::endl << "stats.countSendToBelow:" << stats.countSendToBelow_ << std::endl;
          stats.maskSendToBelow_.insert(stats.maskRecvFromBelow_.begin(),stats.maskRecvFromBelow_.end());
          statusOFS << std::endl << "stats.maskSendToBelow_:" << stats.maskSendToBelow_.size() <<std::endl; 
          //      bitMaskSet::iterator it;
          //      for(it = stats.maskSendToBelow_.begin(); it != stats.maskSendToBelow_.end(); it++){
          //        //print the involved processors
          //        for(int curi = 0; curi < it->first.size(); curi++){
          //          statusOFS << it->first[curi] << " "; 
          //        }
          //
          //        statusOFS<< "    ( ";
          //        //print the involved supernode indexes
          //        for(int curi = 0; curi < it->second.size(); curi++){
          //          statusOFS<< it->second[curi]<<" ";
          //        }
          //
          //        statusOFS << ")"<< std::endl;
          //      }
          //      statusOFS << std::endl << "stats.countRecvFromBelow:" << stats.countRecvFromBelow_ << std::endl;
          //      statusOFS << std::endl << "stats.maskRecvFromBelow_:" << stats.maskRecvFromBelow_.size() <<std::endl; 
          //      statusOFS << std::endl << "stats.countSendToRight:" << stats.countSendToRight_ << std::endl;
          statusOFS << std::endl << "stats.maskSendToRight_:" << stats.maskSendToRight_.size() <<std::endl; 
        }
#endif


        TIMER_START(BUILD_BCAST_TREES);//从下面就基本看不懂了
        //Allgather RFL values within column
        {
          vector<double> SeedRFL(numSuper,0.0); //每个supernode对应的种子？
          vector<Int> aggRFL(numSuper); //包含了发送或者接受的数量？
          vector<Int> globalAggRFL(numSuper*grid_->numProcCol); 
          for( Int ksup = 0; ksup < numSuper ; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"1 "<<ksup<<std::endl;
#endif
            if(MYCOL(grid_)==PCOL(ksup,grid_)){
              std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );//得到这个supernode在本地的L block
              // All blocks except for the diagonal block are to be sent right

              Int totalSize = 0;
              //one integer holding the number of Lblocks
              totalSize+=sizeof(Int);
              for( Int ib = 0; ib < Lcol.size(); ib++ ){
                if( Lcol[ib].blockIdx > ksup ){
                  //three indices + one IntNumVec
                  totalSize+=3*sizeof(Int);
                  totalSize+= sizeof(Int)+Lcol[ib].rows.ByteSize();
                }
              }


              aggRFL[ksup]=totalSize;
              SeedRFL[ksup]=rand();

            }
            else if(isRecvFromLeft_(ksup)){
              aggRFL[ksup]=1;
            }
            else{
              aggRFL[ksup]=0;
            }
          }
          //      //allgather
          MPI_Allgather(&aggRFL[0],numSuper*sizeof(Int),MPI_BYTE,
              &globalAggRFL[0],numSuper*sizeof(Int),MPI_BYTE,
              grid_->rowComm);
          MPI_Allreduce(MPI_IN_PLACE,&SeedRFL[0],numSuper,MPI_DOUBLE,MPI_MAX,grid_->rowComm);


          for( Int ksup = 0; ksup < numSuper; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"3 "<<ksup<<std::endl;
#endif
            if( isRecvFromLeft_(ksup) || CountSendToRight(ksup)>0 ){
              Int msgSize = 0;
              vector<Int> tree_ranks;
              Int countRFL= 0;
              for( Int iProcCol = 0; iProcCol < grid_->numProcCol; iProcCol++ ){
                if( iProcCol != PCOL( ksup, grid_ ) ){
                  Int isRFL = globalAggRFL[iProcCol*numSuper + ksup];
                  if(isRFL>0){
                    ++countRFL;
                  }
                }
              }

              tree_ranks.reserve(countRFL+1);
              tree_ranks.push_back(PCOL(ksup,grid_));

              for( Int iProcCol = 0; iProcCol < grid_->numProcCol; iProcCol++ ){
                Int isRFL = globalAggRFL[iProcCol*numSuper + ksup];
                if(isRFL>0){
                  if( iProcCol != PCOL( ksup, grid_ ) ){
                    tree_ranks.push_back(iProcCol);
                  }
                  else{
                    msgSize = isRFL;
                  }
                }
              }

              TreeBcast * & BcastLTree = fwdToRightTree_[ksup];
              if(BcastLTree!=NULL){delete BcastLTree; BcastLTree=NULL;}
              BcastLTree = TreeBcast::Create(this->grid_->rowComm,&tree_ranks[0],tree_ranks.size(),msgSize,SeedRFL[ksup]);
#ifdef COMM_PROFILE_BCAST
              BcastLTree->SetGlobalComm(grid_->comm);
#endif

            }
          }





        }
        {
          vector<double> SeedRFA(numSuper,0.0); 
          vector<Int> aggRFA(numSuper); 
          vector<Int> globalAggRFA(numSuper*grid_->numProcRow); 
          for( Int ksup = 0; ksup < numSuper ; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"2 "<<ksup<<std::endl;
#endif
            if(MYROW(grid_)==PROW(ksup,grid_)){
              std::vector<UBlock<T> >&  Urow = this->U( LBi(ksup, grid_) );
              // All blocks except for the diagonal block are to be sent right

              Int totalSize = 0;
              //one integer holding the number of Lblocks
              totalSize+=sizeof(Int);
              for( Int jb = 0; jb < Urow.size(); jb++ ){
                if( Urow[jb].blockIdx >= ksup ){
                  //three indices + one IntNumVec + one NumMat<T>
                  totalSize+=3*sizeof(Int);
                  totalSize+= sizeof(Int)+Urow[jb].cols.ByteSize();
                  totalSize+= 2*sizeof(Int)+Urow[jb].nzval.ByteSize();
                }
              }
              aggRFA[ksup]=totalSize;
              SeedRFA[ksup]=rand();
            }
            else if(isRecvFromAbove_(ksup)){
              aggRFA[ksup]=1;
            }
            else{
              aggRFA[ksup]=0;
            }
          }


          //allgather
          MPI_Allgather(&aggRFA[0],numSuper*sizeof(Int),MPI_BYTE,
              &globalAggRFA[0],numSuper*sizeof(Int),MPI_BYTE,
              grid_->colComm);
          MPI_Allreduce(MPI_IN_PLACE,&SeedRFA[0],numSuper,MPI_DOUBLE,MPI_MAX,grid_->colComm);

          for( Int ksup = 0; ksup < numSuper; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"4 "<<ksup<<std::endl;
#endif
            if( isRecvFromAbove_(ksup) || CountSendToBelow(ksup)>0 ){
              vector<Int> tree_ranks;
              Int msgSize = 0;
              Int countRFA= 0;
              for( Int iProcRow = 0; iProcRow < grid_->numProcRow; iProcRow++ ){
                if( iProcRow != PROW( ksup, grid_ ) ){
                  Int isRFA = globalAggRFA[iProcRow*numSuper + ksup];
                  if(isRFA>0){
                    ++countRFA;
                  }
                }
              }

              tree_ranks.reserve(countRFA+1);
              tree_ranks.push_back(PROW(ksup,grid_));

              for( Int iProcRow = 0; iProcRow < grid_->numProcRow; iProcRow++ ){
                Int isRFA = globalAggRFA[iProcRow*numSuper + ksup];
                if(isRFA>0){
                  if( iProcRow != PROW( ksup, grid_ ) ){
                    tree_ranks.push_back(iProcRow);
                  }
                  else{
                    msgSize = isRFA;
                  }
                }
              }

              TreeBcast * & BcastUTree = fwdToBelowTree_[ksup];
              if(BcastUTree!=NULL){delete BcastUTree; BcastUTree=NULL;}
              BcastUTree = TreeBcast::Create(this->grid_->colComm,&tree_ranks[0],tree_ranks.size(),msgSize,SeedRFA[ksup]);
#ifdef COMM_PROFILE_BCAST
              BcastUTree->SetGlobalComm(grid_->comm);
#endif
            }

          }
        }
        //do the same for the other arrays
        TIMER_STOP(BUILD_BCAST_TREES);
        TIMER_START(BUILD_REDUCE_D_TREE);
        {
          vector<double> SeedSTD(numSuper,0.0); 
          vector<Int> aggSTD(numSuper); 
          vector<Int> globalAggSTD(numSuper*grid_->numProcRow); 
          for( Int ksup = 0; ksup < numSuper; ksup++ ){
            if( MYCOL( grid_ ) == PCOL(ksup, grid_) &&  MYROW(grid_)==PROW(ksup,grid_)){
              Int totalSize = sizeof(T)*SuperSize( ksup, super_ )*SuperSize( ksup, super_ );
              aggSTD[ksup]=totalSize;
              SeedSTD[ksup]=rand();
            }
            else if(isSendToDiagonal_(ksup)){
              aggSTD[ksup]=1;
            }
            else{
              aggSTD[ksup]=0;
            }
          }


          //allgather
          MPI_Allgather(&aggSTD[0],numSuper*sizeof(Int),MPI_BYTE,
              &globalAggSTD[0],numSuper*sizeof(Int),MPI_BYTE,
              grid_->colComm);
          MPI_Allreduce(MPI_IN_PLACE,&SeedSTD[0],numSuper,MPI_DOUBLE,MPI_MAX,grid_->colComm);


          for( Int ksup = 0; ksup < numSuper; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"5 "<<ksup<<std::endl;
#endif
            if( MYCOL( grid_ ) == PCOL(ksup, grid_) ){

              Int amISTD = globalAggSTD[MYROW(grid_)*numSuper + ksup];
              //      if( MYCOL( grid_ ) == PCOL(ksup, grid_) &&  MYROW(grid_)==PROW(ksup,grid_)){
              //        assert(amISTD>0);
              //      }

              if( amISTD ){
                vector<Int> tree_ranks;
                Int msgSize = 0;
#if 0
                set<Int> set_ranks;
                for( Int iProcRow = 0; iProcRow < grid_->numProcRow; iProcRow++ ){
                  Int isSTD = globalAggSTD[iProcRow*numSuper + ksup];
                  if(isSTD>0){
                    if( iProcRow != PROW( ksup, grid_ ) ){
                      set_ranks.insert(iProcRow);
                    }
                    else{
                      msgSize = isSTD;
                    }
                  }
                }


                tree_ranks.push_back(PROW(ksup,grid_));
                tree_ranks.insert(tree_ranks.end(),set_ranks.begin(),set_ranks.end());
#else
                Int countSTD= 0;
                for( Int iProcRow = 0; iProcRow < grid_->numProcRow; iProcRow++ ){
                  if( iProcRow != PROW( ksup, grid_ ) ){
                    Int isSTD = globalAggSTD[iProcRow*numSuper + ksup];
                    if(isSTD>0){
                      ++countSTD;
                    }
                  }
                }

                tree_ranks.reserve(countSTD+1);
                tree_ranks.push_back(PROW(ksup,grid_));

                for( Int iProcRow = 0; iProcRow < grid_->numProcRow; iProcRow++ ){
                  Int isSTD = globalAggSTD[iProcRow*numSuper + ksup];
                  if(isSTD>0){
                    if( iProcRow != PROW( ksup, grid_ ) ){
                      tree_ranks.push_back(iProcRow);
                    }
                    else{
                      msgSize = isSTD;
                    }
                  }
                }
#endif
                //assert(set_ranks.find(MYROW(grid_))!= set_ranks.end() || MYROW(grid_)==tree_ranks[0]);

                TreeReduce<T> * & redDTree = redToAboveTree_[ksup];


                if(redDTree!=NULL){delete redDTree; redDTree=NULL;}
                redDTree = TreeReduce<T>::Create(this->grid_->colComm,&tree_ranks[0],tree_ranks.size(),msgSize,SeedSTD[ksup]);
#ifdef COMM_PROFILE
                redDTree->SetGlobalComm(grid_->comm);
#endif
              }
            }
          }
        }
        TIMER_STOP(BUILD_REDUCE_D_TREE);



        TIMER_START(BUILD_REDUCE_L_TREE);
        {
          vector<double> SeedRTL(numSuper,0.0); 
          vector<Int> aggRTL(numSuper); 
          vector<Int> globalAggRTL(numSuper*grid_->numProcCol); 
          for( Int ksup = 0; ksup < numSuper ; ksup++ ){
            if(MYCOL(grid_)==PCOL(ksup,grid_)){
              std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );
              // All blocks except for the diagonal block are to be sent right

              Int totalSize = 0;

              //determine the number of rows in LUpdateBufReduced
              Int numRowLUpdateBuf = 0;
              if( MYROW( grid_ ) != PROW( ksup, grid_ ) ){
                for( Int ib = 0; ib < Lcol.size(); ib++ ){
                  numRowLUpdateBuf += Lcol[ib].numRow;
                }
              } // I do not own the diagonal block
              else{
                for( Int ib = 1; ib < Lcol.size(); ib++ ){
                  numRowLUpdateBuf += Lcol[ib].numRow;
                }
              } // I own the diagonal block, skip the diagonal block

              //if(ksup==297){gdb_lock();}
              totalSize = numRowLUpdateBuf*SuperSize( ksup, super_ )*sizeof(T);

              aggRTL[ksup]=totalSize;

              SeedRTL[ksup]=rand();
            }
            else if(isRecvFromLeft_(ksup)){
              aggRTL[ksup]=1;
            }
            else{
              aggRTL[ksup]=0;
            }
          }
          //      //allgather
          MPI_Allgather(&aggRTL[0],numSuper*sizeof(Int),MPI_BYTE,
              &globalAggRTL[0],numSuper*sizeof(Int),MPI_BYTE,
              grid_->rowComm);
          MPI_Allreduce(MPI_IN_PLACE,&SeedRTL[0],numSuper,MPI_DOUBLE,MPI_MAX,grid_->rowComm);


          for( Int ksup = 0; ksup < numSuper ; ksup++ ){
#if ( _DEBUGlevel_ >= 1 )
            statusOFS<<"6 "<<ksup<<std::endl;
#endif
            if( isRecvFromLeft_(ksup) || CountSendToRight(ksup)>0 ){
              vector<Int> tree_ranks;
              Int msgSize = 0;
#if 0
              set<Int> set_ranks;
              for( Int iProcCol = 0; iProcCol < grid_->numProcCol; iProcCol++ ){
                Int isRTL = globalAggRTL[iProcCol*numSuper + ksup];
                if(isRTL>0){
                  if( iProcCol != PCOL( ksup, grid_ ) ){
                    set_ranks.insert(iProcCol);
                  }
                  else{
                    msgSize = isRTL;
                  }
                }
              }
              tree_ranks.push_back(PCOL(ksup,grid_));
              tree_ranks.insert(tree_ranks.end(),set_ranks.begin(),set_ranks.end());
#else
              Int countRTL= 0;
              for( Int iProcCol = 0; iProcCol < grid_->numProcCol; iProcCol++ ){
                if( iProcCol != PCOL( ksup, grid_ ) ){
                  Int isRTL = globalAggRTL[iProcCol*numSuper + ksup];
                  if(isRTL>0){
                    ++countRTL;
                  }
                }
              }

              tree_ranks.reserve(countRTL+1);
              tree_ranks.push_back(PCOL(ksup,grid_));

              for( Int iProcCol = 0; iProcCol < grid_->numProcCol; iProcCol++ ){
                Int isRTL = globalAggRTL[iProcCol*numSuper + ksup];
                if(isRTL>0){
                  if( iProcCol != PCOL( ksup, grid_ ) ){
                    tree_ranks.push_back(iProcCol);
                  }
                  else{
                    msgSize = isRTL;
                  }
                }
              }

#endif


              TreeReduce<T> * & redLTree = redToLeftTree_[ksup];
              if(redLTree!=NULL){delete redLTree; redLTree=NULL;}
              redLTree = TreeReduce<T>::Create(this->grid_->rowComm,&tree_ranks[0],tree_ranks.size(),msgSize,SeedRTL[ksup]);
#ifdef COMM_PROFILE
              redLTree->SetGlobalComm(grid_->comm);
#endif
            }
          }
        }
        TIMER_STOP(BUILD_REDUCE_L_TREE);







#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "isSendToBelow:" << std::endl;
        for(int j = 0;j< isSendToBelow_.n();j++){
          statusOFS << "["<<j<<"] ";
          for(int i =0; i < isSendToBelow_.m();i++){
            statusOFS<< isSendToBelow_(i,j) << " ";
          }
          statusOFS<<std::endl;
        }

        statusOFS << std::endl << "isRecvFromAbove:" << std::endl;
        for(int j = 0;j< isRecvFromAbove_.m();j++){
          statusOFS << "["<<j<<"] "<< isRecvFromAbove_(j)<<std::endl;
        }
#endif
#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "isSendToRight:" << std::endl;
        for(int j = 0;j< isSendToRight_.n();j++){
          statusOFS << "["<<j<<"] ";
          for(int i =0; i < isSendToRight_.m();i++){
            statusOFS<< isSendToRight_(i,j) << " ";
          }
          statusOFS<<std::endl;
        }

        statusOFS << std::endl << "isRecvFromLeft:" << std::endl;
        for(int j = 0;j< isRecvFromLeft_.m();j++){
          statusOFS << "["<<j<<"] "<< isRecvFromLeft_(j)<<std::endl;
        }

        statusOFS << std::endl << "isRecvFromBelow:" << std::endl;
        for(int j = 0;j< isRecvFromBelow_.n();j++){
          statusOFS << "["<<j<<"] ";
          for(int i =0; i < isRecvFromBelow_.m();i++){
            statusOFS<< isRecvFromBelow_(i,j) << " ";
          }
          statusOFS<<std::endl;
        }
#endif







        TIMER_START(STCD_RFCD);


        for( Int ksup = 0; ksup < numSuper - 1; ksup++ ){
          if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
            for( std::set<Int>::iterator si = localColBlockRowIdx[LBj( ksup, grid_ )].begin();
                si != localColBlockRowIdx[LBj( ksup, grid_ )].end(); si++ ){
              Int isup = *si;
              Int isupProcRow = PROW( isup, grid_ );
              Int isupProcCol = PCOL( isup, grid_ );
              if( isup > ksup && MYROW( grid_ ) == isupProcRow ){
                isSendToCrossDiagonal_(grid_->numProcCol, ksup ) = true;
                isSendToCrossDiagonal_(isupProcCol, ksup ) = true;
              }
            } // for (si)
          } // if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) )
        } // for (ksup)

        for( Int ksup = 0; ksup < numSuper - 1; ksup++ ){
          if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
            for( std::set<Int>::iterator si = localRowBlockColIdx[ LBi(ksup, grid_) ].begin();
                si != localRowBlockColIdx[ LBi(ksup, grid_) ].end(); si++ ){
              Int jsup = *si;
              Int jsupProcCol = PCOL( jsup, grid_ );
              Int jsupProcRow = PROW( jsup, grid_ );
              if( jsup > ksup && MYCOL(grid_) == jsupProcCol ){
                isRecvFromCrossDiagonal_(grid_->numProcRow, ksup ) = true;
                isRecvFromCrossDiagonal_(jsupProcRow, ksup ) = true;
              }
            } // for (si)
          } // if( MYROW( grid_ ) == PROW( ksup, grid_ ) )
        } // for (ksup)
#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "isSendToCrossDiagonal:" << std::endl;
        for(int j =0; j < isSendToCrossDiagonal_.n();j++){
          if(isSendToCrossDiagonal_(grid_->numProcCol,j)){
            statusOFS << "["<<j<<"] ";
            for(int i =0; i < isSendToCrossDiagonal_.m()-1;i++){
              if(isSendToCrossDiagonal_(i,j))
              {
                statusOFS<< PNUM(PROW(j,grid_),i,grid_)<<" ";
              }
            }
            statusOFS<<std::endl;
          }
        }

        statusOFS << std::endl << "isRecvFromCrossDiagonal:" << std::endl;
        for(int j =0; j < isRecvFromCrossDiagonal_.n();j++){
          if(isRecvFromCrossDiagonal_(grid_->numProcRow,j)){
            statusOFS << "["<<j<<"] ";
            for(int i =0; i < isRecvFromCrossDiagonal_.m()-1;i++){
              if(isRecvFromCrossDiagonal_(i,j))
              {
                statusOFS<< PNUM(i,PCOL(j,grid_),grid_)<<" ";
              }
            }
            statusOFS<<std::endl;
          }
        }


#endif


        TIMER_STOP(STCD_RFCD);


        //Build the list of supernodes based on the elimination tree from SuperLU
        GetWorkSet(snodeEtree,this->WorkingSet());

        return ;
      }
      else{
        TIMER_START(ConstructCommunicationPattern);
        Int numSuper = this->NumSuper();

        TIMER_START(GetEtree);
        snodeEtree_.resize(numSuper);
        GetEtree(snodeEtree_);
        TIMER_STOP(GetEtree);

        //Build the list of supernodes based on the elimination tree from SuperLU
        GetWorkSet(snodeEtree_,this->WorkingSet());


        //Now we could figure out how many tags we need ?
        auto & wset = this->WorkingSet();


        // THIS IS THE SHAPE OF THE COMMUNICATIONS PERFORMED
        // _________________
        // xxx|   ..      ...
        // xxx|   ..      ...
        // xxx|   ..      ...
        // ---|-------------
        //    |
        // xxx|-->oo
        // xxx|<--oo
        //    |   ^|
        //    |   ||(1)
        //    |   |v
        // xxx|-->oo----->ooo
        // xxx|   oo  (2) ooo
        // xxx|<--oo<-----ooo


        Real timeSta, timeEnd;
        GetTime( timeSta );
        //First Gather the row structure of the each supernodal column
        std::vector<Int> allColBlockIdx;
        std::vector<int> recvCount(this->grid_->numProcRow,0);
        std::vector<int> recvDispls(this->grid_->numProcRow+1);

        {
          int sendCount = 0;
          std::vector<Int> sendBuffer;
          std::vector<bool> hasEntries(numSuper,false);
          for( Int ksup = 0; ksup < numSuper; ksup++ ){
            // All block columns perform independently
            if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
              std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );
              Int startIdx = ( MYROW( grid_ ) == PROW( ksup, grid_ ) )?1:0;

              //+1 for the supernode index, +1 for the number of blocks
              Int count = 0;
              count+=Lcol.size()-startIdx;
              //for(auto && val: colBlockIdx){ if (val > ksup){ count++; } }
              if (count>0){
                sendCount += 2 + count;
                hasEntries[ksup] = true;
              } 
            }
          }

#if MPI_VERSION >= 3
          MPI_Request request_size = MPI_REQUEST_NULL;
          MPI_Iallgather(&sendCount,sizeof(sendCount),MPI_BYTE,
              recvCount.data(),sizeof(sendCount),MPI_BYTE,
              this->grid_->colComm, &request_size);
#else
          MPI_Allgather(&sendCount,sizeof(sendCount),MPI_BYTE,
              recvCount.data(),sizeof(sendCount),MPI_BYTE,
              this->grid_->colComm);
#endif
          //PACK
          sendBuffer.reserve(sendCount);
          for( Int ksup = 0; ksup < numSuper; ksup++ ){
            // All block columns perform independently
            if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
              //std::vector<Int> & colBlockIdx = ColBlockIdx(LBj(ksup, grid_));
              if (hasEntries[ksup]) {
                sendBuffer.push_back(ksup);
                //push dummy value
                sendBuffer.push_back(0);
                Int & count = sendBuffer.back();

                std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );
                Int startIdx = ( MYROW( grid_ ) == PROW( ksup, grid_ ) )?1:0;
                for( Int ib = startIdx; ib < Lcol.size(); ib++ ){ 
                  sendBuffer.push_back(Lcol[ib].blockIdx);
                  count++;
                }
              }
            }
          }

          assert(sendBuffer.size()==sendCount);

#if MPI_VERSION >= 3
          MPI_Status status;
          MPI_Wait(&request_size,&status);
#endif
          recvDispls[0] = 0;
          std::partial_sum(recvCount.begin(),recvCount.end(),&recvDispls[1]);
          allColBlockIdx.resize(recvDispls.back());

          MPI_Datatype Int_type;
          MPI_Type_contiguous( sizeof(Int), MPI_BYTE, &Int_type );
          MPI_Type_commit(&Int_type);

          MPI_Allgatherv(sendBuffer.data(),sendCount,Int_type,
              allColBlockIdx.data(),recvCount.data(),recvDispls.data(),Int_type,
              this->grid_->colComm);

          MPI_Type_free(&Int_type);
        }
        GetTime( timeEnd );
        statusOFS<<"Column block structure exchange time: "<<timeEnd - timeSta<<std::endl; 

        //Now unpack the allColBlockIdx

        GetTime( timeSta );
        std::set<Int> curColStructure;

        //special case for symmetric implementation
        this->redDTree2_.resize(numSuper);

        std::vector< std::list< std::vector<Int> > > communicators;
        communicators.resize(numSuper);

        std::vector< std::vector<Int> > communicatorsD;
        communicatorsD.resize(numSuper);

        auto processCol = [&] (Int ksup){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );
          Int startIdx = ( MYROW( grid_ ) == PROW( ksup, grid_ ) )?1:0;
#if ( _DEBUGlevel_ >= 1 )
          statusOFS<<"["<<ksup<<"] curColStructure: ";for(auto && blk:curColStructure){statusOFS<<blk<<" ";}statusOFS<<std::endl;
#endif

          for( Int ib = startIdx; ib < Lcol.size(); ib++ ){
            if( Lcol[ib].blockIdx > ksup ){
              Int msgSize = 0;
              //three indices + one IntNumVec + one numMat
              msgSize+=3*sizeof(Int);
              msgSize+= sizeof(Int)+Lcol[ib].rows.ByteSize();
              msgSize+= 2*sizeof(Int)+ Lcol[ib].nzval.ByteSize();

              Int proot = MYPROC(this->grid_);
              //now compute list of receiving ranks
              std::vector<Int> receivers;
              std::vector<bool> mask(this->grid_->mpisize,false);
              receivers.reserve(this->grid_->mpisize);
              receivers.push_back(proot);
              mask[proot] = true;
              for( auto blkIdx : curColStructure){
                if(blkIdx>ksup){
                  Int iproc,jproc,pdest;
                  if(blkIdx<Lcol[ib].blockIdx){
                    jproc = PCOL(blkIdx,this->grid_);
                    iproc = PROW(Lcol[ib].blockIdx,this->grid_); 
                  }
                  else if(blkIdx==Lcol[ib].blockIdx){
                    jproc = PCOL(blkIdx,this->grid_);
                    iproc = PROW(Lcol[ib].blockIdx,this->grid_); 
                  }
                  else{
                    iproc = PROW(blkIdx,this->grid_);
                    jproc = PCOL(Lcol[ib].blockIdx,this->grid_); 
                  }
                  pdest = PNUM(iproc,jproc,this->grid_);


                  assert(pdest < this->grid_->mpisize);

                  if(!mask[pdest]){
                    receivers.push_back(pdest);
                    mask[pdest] = true;
                  }
                }
              }

              communicators[ksup].push_back(std::vector<Int>());
              auto & curList = communicators[ksup].back();//[ib-startIdx];
              curList.reserve(receivers.size()+1+1+1+1); //+1 for ksup +1 for the blockIdx +1 for the messageSize +1 for the NumMat size
              //make the rankLists
              curList.push_back(ksup);
              curList.push_back(Lcol[ib].blockIdx);
              curList.push_back(msgSize);
              curList.push_back(Lcol[ib].nzval.Size());
              curList.insert(curList.end(),receivers.begin(),receivers.end());

              //statusOFS<<"["<<ksup<<"] block "<<Lcol[ib].blockIdx<<" sent from P"<<proot<<" to { ";
              //for(auto && p:receivers){ statusOFS<<p<<" ";}
              //statusOFS<<"} msgSize = "<<msgSize<<std::endl; 
              //statusOFS<<"["<<ksup<<"] curList "<<" { ";
              //for(auto && p:curList){ statusOFS<<p<<" ";}
              //statusOFS<<"} "<<std::endl; 
            }
          }

          if(Lcol.size()>0){
            //Create the Reduce to Diagonal tree
            std::vector<Int> & senders = communicatorsD[ksup];
            senders.reserve(this->grid_->numProcRow);
            std::vector<bool> mask(this->grid_->mpisize,false);

            Int proot = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
            double seed = ( (double)ksup / (double)numSuper);
            Int supSize = SuperSize(ksup, this->super_);  
            Int msgSize = supSize * supSize;

            senders.push_back(proot);
            mask[proot] = true;
            for( auto blkIdx : curColStructure){
              if(blkIdx>ksup){
                Int psender = PNUM(PROW(blkIdx,this->grid_),PCOL(ksup,this->grid_),this->grid_);
                if(!mask[psender]){
                  senders.push_back(psender);
                  mask[psender] = true;
                }
              }
            }

            auto & redDTree = this->redDTree2_[ksup];
            redDTree.reset(TreeReduce_v2<T>::Create(this->grid_->comm,senders.data(),senders.size(),msgSize,seed));
#ifdef COMM_PROFILE_BCAST
            redDTree->SetGlobalComm(this->grid_->comm);
#endif
          }
        };

        std::partial_sum(recvCount.begin(),recvCount.end(),recvCount.begin());

        for( Int ksup = 0; ksup < numSuper; ksup++ ){
          // All block columns perform independently
          if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
            //loop through the processors and use recvDispls to keep track of the progress
            bool done = true;

            for(Int p = 0; p < recvCount.size(); p++){
              int & i = recvDispls[p];
              if ( i < recvCount[p] ){
                Int cur_ksup = allColBlockIdx[i];

                assert(cur_ksup>=ksup);
                if ( cur_ksup == ksup){
                  ksup = cur_ksup;
                  //advance the pointer
                  i++;
                  Int numIdx = allColBlockIdx[i++];
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"Processing ["<<ksup<<"]"<<std::endl;
#endif
                  curColStructure.insert(&allColBlockIdx[i],&allColBlockIdx[i]+numIdx);
                  i+=numIdx;
                }

              }

              //i may have changed so do the test again
              if ( i < recvCount[p] ){
                done = false;
              }
            }

            //Create the "communicators" based on the content of curColStructure
            processCol(ksup);

            curColStructure.clear();

            if(done){
              break;
            }
          }
        }

        GetTime( timeEnd );
        statusOFS<<"Tree ranklist creation time: "<<timeEnd - timeSta<<std::endl; 

        //At this point, the roots know how many distinct trees they are involved in. 
        //Thus we can compute the total number of requireds tags based on that
        std::vector<int> tagCountPerSnode(numSuper,0);
        for(int ksup = 0; ksup<communicators.size();ksup++){
          if(communicators[ksup].size()>0){
            auto & blockList = communicators[ksup]; 
            tagCountPerSnode[ksup] += 2*blockList.size(); //reduce L + bcastL

            Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
            if(prootD == grid_->mpirank){
              tagCountPerSnode[ksup]++; //reduce D
            }
          }
        }
        MPI_Allreduce(MPI_IN_PLACE, tagCountPerSnode.data(),numSuper,MPI_INT,MPI_SUM,grid_->comm);

        //first, check if we do not have levels simply exceeding maxTag_ value by themselves
        //maxTag_ = *std::max_element(tagCountPerSnode.begin(), tagCountPerSnode.end());
        for(Int lidx = 0 ; lidx < wset.size(); lidx++){
          int tagcnt = 0;
          Int stepSuper = wset[lidx].size(); 
          for (Int esupidx=0; esupidx<stepSuper; esupidx++){
            Int ksup = wset[lidx][esupidx]; 
            if(tagcnt + tagCountPerSnode[ksup] <= maxTag_){
              tagcnt += tagCountPerSnode[ksup];
            }
            else{
              wset.insert(wset.begin()+lidx+1,std::vector<Int>());
              wset[lidx+1].insert(wset[lidx+1].begin(),&wset[lidx][esupidx],wset[lidx].data()+stepSuper);
              wset[lidx].resize(esupidx);
              break;
            }
          }
        }
 
        Int numSteps = wset.size();
        std::vector<int> tagCountPerLevel(numSteps,0);
        for(Int lidx = 0 ; lidx < numSteps; lidx++){
          Int stepSuper = wset[lidx].size(); 
          for (Int esupidx=0; esupidx<stepSuper; esupidx++){
            Int ksup = wset[lidx][esupidx]; 
            auto & blockList = communicators[ksup]; 
            if(blockList.size()>0){
              tagCountPerLevel[lidx] += 2*blockList.size(); //reduce L + bcastL 
            }

            Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
            if(prootD == grid_->mpirank){
              tagCountPerLevel[lidx]++; //reduce D
            }
          }
        }

        std::vector<int> tagCountPerLevelPerRoot(grid_->mpisize*numSteps,0);
        MPI_Allgather(tagCountPerLevel.data(),numSteps,MPI_INT,tagCountPerLevelPerRoot.data(),numSteps,MPI_INT,grid_->comm);


        //change storage format
        std::vector<int> tagOffsetPerLevelPerRoot(grid_->mpisize*numSteps+1,0);

        //do a partial_sum to see from which tag a particular root should start from
        syncPoints_.clear();

        int prevTag = 0;
        for(Int lidx = 0 ; lidx < numSteps; lidx++){
          bool needSync = false;
          int lprevTag = prevTag;
          tagOffsetPerLevelPerRoot[lidx*grid_->mpisize] = lprevTag;
          for(Int proot = 0; proot< grid_->mpisize; proot++){
            int tagcnt = tagCountPerLevelPerRoot[proot*numSteps+lidx];
            double maxTag = (double)lprevTag + (double)tagcnt - 1.0;

            if(maxTag <= (double)this->maxTag_){
              tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + proot] = lprevTag;
              lprevTag+=tagcnt;
            }
            else{
              needSync = true;
              break;
            }
          }

          if(needSync){
            //we need a barrier at the END of previous level
            syncPoints_.push_back(lidx);

            prevTag = 0;
            lprevTag = prevTag;
            tagOffsetPerLevelPerRoot[lidx*grid_->mpisize] = lprevTag;
            
            for(Int proot = 0; proot< grid_->mpisize; proot++){
              int tagcnt = tagCountPerLevelPerRoot[proot*numSteps+lidx];
              double maxTag = (double)lprevTag + (double)tagcnt - 1.0;
              if(maxTag <= (double)this->maxTag_){
                tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + proot] = lprevTag;
                lprevTag+=tagcnt;
              }
              else{
                //this should not happen anymore
                abort();
                //TODO this needs to be turned into an ErrorHandling function
                break;
              }
            } 
          }

          prevTag = lprevTag;
          tagOffsetPerLevelPerRoot[(lidx+1)*grid_->mpisize] = lprevTag;
        }


        //std::for_each(syncPoints_.begin(),syncPoints_.end(), [](int & t){statusOFS<<t<<" ";}); statusOFS<<std::endl;
        //statusOFS<<tagOffsetPerLevelPerRoot<<std::endl;
        //for(auto && syncPt: syncPoints_){
        //  assert(tagOffsetPerLevelPerRoot[syncPt*grid_->mpisize]==0);
        //}


        //Now, we can organize the Alltoallv to build the ranklists
        //std::vector< std::vector< std::vector<Int> > > communicators;
        GetTime( timeSta );
        {
          Real timeSta2, timeEnd2;
          //we need char as we are packing Ints and double (seed)
          std::vector<Int> sendBuffer;
          std::vector<int> sendCount(this->grid_->mpisize,0);
          std::vector<int> sendDispls(this->grid_->mpisize+1);
          std::vector<Int> recvBuffer;
          std::vector<int> recvCount(this->grid_->mpisize,0);
          std::vector<int> recvDispls(this->grid_->mpisize+1);

          GetTime( timeSta2 );
          for(Int ksup = 0; ksup< communicators.size(); ksup++){
            //Set tag of diagonal reduce?
            Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
            if(prootD == grid_->mpirank){
              auto & redDTree = redDTree2_[ ksup ];
              if(redDTree!=nullptr){
                auto & senders = communicatorsD[ksup]; 
                for(auto&& p:senders){
                  //trick to send diagonal reduce tag: ksup is set to -ksup-1 (to make it <0), next int is the tag
                  sendCount[p] += 2;
                }
              }
            }


            auto & blockList = communicators[ksup]; 
            if (blockList.size()>0){
              //std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );
              //Int startIdx = ( MYROW( grid_ ) == PROW( ksup, grid_ ) )?1:0;
              for(auto && ranklist : blockList){

                if(ranklist.size()>0){
                  //contains ksup + blockIdx + messageSize + blockSize + seed  + rank list size + ranks + tag 
                  int count = ranklist.size() +1 + sizeof(double)/sizeof(Int) +1; 
                  //Int ksup = ranklist[0]
                  //Int blockIdx = ranklist[1]
                  //Int messageSize = ranklist[2]
                  //Int blockSize = ranklist[3]
                  for(Int ip = 4;ip<ranklist.size();ip++){
                    sendCount[ranklist[ip]] += count;
                  }
                }
              }
            }
          } 

#if MPI_VERSION >= 3
          MPI_Request request_size = MPI_REQUEST_NULL;
          MPI_Ialltoall(sendCount.data(),1,MPI_INT,
              recvCount.data(),1,MPI_INT,this->grid_->comm,&request_size);
#else
          MPI_Alltoall(sendCount.data(),1,MPI_INT,
              recvCount.data(),1,MPI_INT,this->grid_->comm);
#endif

          sendDispls[0] = 0;
          std::partial_sum(sendCount.begin(),sendCount.end(),&sendDispls[1]);

          //Pack
          sendBuffer.resize(sendDispls.back());

          for(Int lidx = 0 ; lidx < numSteps; lidx++){
            Int stepSuper = wset[lidx].size(); 
            for (Int esupidx=0; esupidx<stepSuper; esupidx++){
              Int ksup = wset[lidx][esupidx]; 

              //Set tag of diagonal reduce?
              Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
              if(prootD == grid_->mpirank){
                auto & redDTree = redDTree2_[ ksup ];
                if(redDTree!=nullptr){
                  Int tagD = tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + prootD]++;
                  //Loop through all the ranks involved in the reduce
                  auto & senders = communicatorsD[ksup]; 
                  for(auto&& p:senders){
                    sendBuffer[sendDispls[p]++] = -ksup-1;
                    sendBuffer[sendDispls[p]++] = tagD;
                  }
                }
              }

              auto & blockList = communicators[ksup]; 
              if (blockList.size()>0){
                for(auto && ranklist : blockList){
                  if(ranklist.size()>0){
                    Int ksup2 = ranklist[0];
                    assert(ksup==ksup2);
                    Int blockIdx = ranklist[1];
                    Int messageSize = ranklist[2];
                    Int blockSize = ranklist[3];
                    Int tag = tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + grid_->mpirank];
                    tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + grid_->mpirank]+=2;
                    Int ranklistSize = ranklist.size()-4;
                    double seed = rand();

                    for(Int ip = 4;ip<ranklist.size();ip++){
                      Int p = ranklist[ip];
                      sendBuffer[sendDispls[p]++] = ksup;
                      sendBuffer[sendDispls[p]++] = blockIdx;
                      sendBuffer[sendDispls[p]++] = messageSize;
                      sendBuffer[sendDispls[p]++] = blockSize;
                      sendBuffer[sendDispls[p]++] = tag;
                      *((double*)&sendBuffer[sendDispls[p]]) = seed;
                      sendDispls[p]+=sizeof(double)/sizeof(Int);
                      sendBuffer[sendDispls[p]++] = ranklistSize;
                      std::copy(&ranklist[4],ranklist.data()+ranklist.size(),&sendBuffer[sendDispls[p]]);
                      sendDispls[p]+=ranklistSize;
                    }
                  }
                }
              }
            }
          }

          sendDispls[0] = 0;
          std::partial_sum(sendCount.begin(),sendCount.end(),&sendDispls[1]);

#if MPI_VERSION >= 3
          MPI_Status status;
          MPI_Wait(&request_size,&status);
#endif
          GetTime( timeEnd2 );
          statusOFS<<"Tree structure packing: "<<timeEnd2 - timeSta2<<std::endl; 

          recvDispls[0] = 0;
          std::partial_sum(recvCount.begin(),recvCount.end(),&recvDispls[1]);
          recvBuffer.resize(recvDispls.back());

          MPI_Datatype Int_type;
          MPI_Type_contiguous( sizeof(Int), MPI_BYTE, &Int_type );
          MPI_Type_commit(&Int_type);

          MPI_Alltoallv(sendBuffer.data(),sendCount.data(),sendDispls.data(),Int_type,
              recvBuffer.data(),recvCount.data(),recvDispls.data(),Int_type,
              this->grid_->comm);

          MPI_Type_free(&Int_type);

          Real timeSta3, timeEnd3;
          Real tresize = 0;
          Real tother = 0;
          GetTime( timeSta2 );
          GetTime( timeSta3 );
          snodeTreeOffset_.resize(numSuper+1,0);
          snodeBlkidxToTree_.resize(numSuper);
          snodeTreeToBlkidx_.resize(numSuper);
          GetTime( timeEnd3 );
          tresize+=timeEnd3 - timeSta3;

          //counting first
          GetTime( timeSta3 );
          size_t i = 0;
          while(i<recvBuffer.size()){
            Int ksup = recvBuffer[i++];
            if(ksup>=0){
              i+=4;
              i+=sizeof(double)/sizeof(Int);
              Int ranklistSize = recvBuffer[i++];
              i+=ranklistSize;
              //first do counts only
              snodeTreeOffset_[ksup+1]++;
            }
            else{
              //skip the tag of the diagonal reduce
              i++;
            }
          }
          GetTime( timeEnd3 );
          statusOFS<<"Time to count number of trees: "<<timeEnd3-timeSta3<<std::endl; 

          GetTime( timeSta3 );
          for(Int ksup = 0; ksup<numSuper; ksup++){
            Int treeCount = snodeTreeOffset_[ksup+1];
            snodeTreeToBlkidx_[ksup].resize(treeCount,-1);
          }
          GetTime( timeEnd3 );
          tresize+=timeEnd3 - timeSta3;

          GetTime( timeSta3 );
          std::vector<Int> heads(numSuper,0);
          std::partial_sum(snodeTreeOffset_.begin(), snodeTreeOffset_.end(), snodeTreeOffset_.begin());
          GetTime( timeEnd3 );
          tother += timeEnd3 - timeSta3;

          GetTime( timeSta3 );
          this->bcastLDataTree_.resize(snodeTreeOffset_.back(),nullptr);
          this->redLTree2_.resize(snodeTreeOffset_.back(),nullptr);
          GetTime( timeEnd3 );
          tresize+=timeEnd3 - timeSta3;

          Real tblkidx = 0;
          Real tcreat = 0;

          i = 0;
          while(i<recvBuffer.size()){
            GetTime( timeSta3 );
            Int ksup = recvBuffer[i++];
            if(ksup>=0){
              Int blockIdx = recvBuffer[i++];
              Int messageSize = recvBuffer[i++];
              Int blockSize = recvBuffer[i++];
              Int tag = recvBuffer[i++];
              double seed = *((double*)&recvBuffer[i]);
              i+=sizeof(double)/sizeof(Int);
              Int ranklistSize = recvBuffer[i++];
              Int * ranklist = &recvBuffer[i];
              i+=ranklistSize;

              Int offset = heads[ksup]++;
              snodeTreeToBlkidx_[ksup][offset] = blockIdx;
              GetTime( timeEnd3 );
              tother += timeEnd3 - timeSta3;

              GetTime( timeSta3 );
              snodeBlkidxToTree_[ksup][blockIdx] = snodeTreeOffset_[ksup] + offset;
              GetTime( timeEnd3 );
              tblkidx += timeEnd3 - timeSta3;

              GetTime( timeSta3 );
              auto & bcastLTree = bcastLDataTree_[ snodeTreeOffset_[ksup] + offset ];
              bcastLTree.reset(TreeBcast_v2<char>::Create(this->grid_->comm,ranklist,ranklistSize,messageSize,seed));
              bcastLTree->SetTag(tag);
#ifdef COMM_PROFILE_BCAST
              bcastLTree->SetGlobalComm(this->grid_->comm);
#endif

#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"L ["<<ksup<<"] blockIdx "<<blockIdx<<" messageSize "<<bcastLTree->GetMsgSize();
              statusOFS<<" tag "<<bcastLTree->GetTag()<<" { ";
              for(Int ir=0; ir < ranklistSize; ir++){statusOFS<<ranklist[ir]<<" ";}statusOFS<<"}"<<std::endl;
#endif

              auto & redLTree = redLTree2_[ snodeTreeOffset_[ksup] + offset ];
              redLTree.reset(TreeReduce_v2<T>::Create(this->grid_->comm,ranklist,ranklistSize,blockSize,seed));
              redLTree->SetTag(tag+1);
#ifdef COMM_PROFILE_BCAST
              redLTree->SetGlobalComm(this->grid_->comm);
#endif

#if ( _DEBUGlevel_ >= 1 )
              statusOFS<<"RL ["<<ksup<<"] blockIdx "<<blockIdx<<" messageSize "<<redLTree->GetMsgSize();
              statusOFS<<" tag "<<redLTree->GetTag()<<" { ";
              for(Int ir=0; ir < ranklistSize; ir++){statusOFS<<ranklist[ir]<<" ";}statusOFS<<"}"<<std::endl;
#endif
              GetTime( timeEnd3 );
              tcreat += timeEnd3 - timeSta3;
            }
            else{
              ksup = -(ksup+1);
              Int tag = recvBuffer[i++];
              //Set tag of diagonal reduce
              Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
              auto & redDTree = redDTree2_[ ksup ];
              assert(redDTree!=nullptr);
              redDTree->SetTag(tag);
            }
          }


          GetTime( timeEnd2 );

          statusOFS<<"Time to insert Blkidx to Treeidx: "<<tblkidx<<std::endl; 
          statusOFS<<"Time to create the trees: "<<tcreat<<std::endl; 
          statusOFS<<"Time to resize the structures: "<<tresize<<std::endl; 
          statusOFS<<"Time to other: "<<tother<<std::endl; 

          statusOFS<<"Tree structure allocation new: "<<timeEnd2 - timeSta2<<std::endl; 

        } 
        GetTime( timeEnd );
        statusOFS<<"Tree structure exchanges and creation time: "<<timeEnd - timeSta<<std::endl; 



        //for(Int lidx = 0 ; lidx < numSteps; lidx++){
        //  Int stepSuper = wset[lidx].size(); 
        //  for (Int esupidx=0; esupidx<stepSuper; esupidx++){
        //    Int ksup = wset[lidx][esupidx]; 

        //    Int treeCount = snodeTreeOffset_[ksup+1] - snodeTreeOffset_[ksup]; 

        //    for(Int offset = 0; offset<treeCount; offset++){
        //      Int treeIdx = snodeTreeOffset_[ksup] + offset;
        //      Int blkIdx = snodeTreeToBlkidx_[ksup][offset];
        //      {
        //        auto & tree = bcastLDataTree_[treeIdx]; 
        //        {
        //          statusOFS<<"L ["<<ksup<<"] blockIdx "<<blkIdx<<" messageSize "<<tree->GetMsgSize();
        //          statusOFS<<" tag "<<tree->GetTag()<<" { ";
        //          auto ranks = tree->GetDests();
        //          for(Int ir=0; ir < tree->GetDestCount(); ir++){statusOFS<<ranks[ir]<<" ";}statusOFS<<"}"<<std::endl;
        //        }
        //      }
        //    }
        //  }
        //}
        //

        //for(Int lidx = 0 ; lidx < numSteps; lidx++){
        //  Int stepSuper = wset[lidx].size(); 
        //  for (Int esupidx=0; esupidx<stepSuper; esupidx++){
        //    Int ksup = wset[lidx][esupidx]; 

        //    Int treeCount = snodeTreeOffset_[ksup+1] - snodeTreeOffset_[ksup]; 

        //    for(Int offset = 0; offset<treeCount; offset++){
        //      Int treeIdx = snodeTreeOffset_[ksup] + offset;
        //      Int blkIdx = snodeTreeToBlkidx_[ksup][offset];
        //      {
        //        auto & tree = redLTree2_[treeIdx]; 
        //        {
        //          statusOFS<<"RL ["<<ksup<<"] blockIdx "<<blkIdx<<" messageSize "<<tree->GetMsgSize();
        //          statusOFS<<" tag "<<tree->GetTag()<<" { ";
        //          auto ranks = tree->GetDests();
        //          for(Int ir=0; ir < tree->GetDestCount(); ir++){statusOFS<<ranks[ir]<<" ";}statusOFS<<"}"<<std::endl;
        //        }
        //      }
        //    }
        //  }
        //}
#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<tagOffsetPerLevelPerRoot<<std::endl;
        for(Int lidx = 0 ; lidx < numSteps; lidx++){
          Int stepSuper = wset[lidx].size(); 
          for (Int esupidx=0; esupidx<stepSuper; esupidx++){
            Int ksup = wset[lidx][esupidx]; 
            auto & tree = redDTree2_[ksup]; 
            if(tree){
              {
                if(tree->GetTag()==-1){
                  //Set tag of diagonal reduce?
                  Int prootD = PNUM(PROW(ksup,this->grid_),PCOL(ksup,this->grid_),this->grid_);
                  Int tagD = tagOffsetPerLevelPerRoot[lidx*grid_->mpisize + prootD]++;
                  tree->SetTag(tagD);
                }
                statusOFS<<"RD ["<<ksup<<"] messageSize "<<tree->GetMsgSize();
                statusOFS<<" tag "<<tree->GetTag()<<" { ";
                auto ranks = tree->GetDests();
                for(Int ir=0; ir < tree->GetDestCount(); ir++){statusOFS<<ranks[ir]<<" ";}statusOFS<<"}"<<std::endl;
              }
            }
          }
        }
#endif

        TIMER_STOP(ConstructCommunicationPattern);
        return ;
      }
    } 		// -----  end of method PMatrix::ConstructCommunicationPattern_P2p  ----- 

  template<typename T> 
    void PMatrix<T>::SelInv	(cublasHandle_t& handle, std::set<std::string> & quantSuperNode )
    {

      if(optionsFact_->Symmetric == 0){
        ErrorHandling( "The matrix is not symmetric, this routine can't handle it !" );
      }
      SelInv_P2p	(handle, quantSuperNode );


#ifdef GEMM_PROFILE
      statOFS<<"m"<<"\t"<<"n"<<"\t"<<"z"<<std::endl;
      for(std::deque<int >::iterator it = gemm_stat.begin(); it!=gemm_stat.end(); it+=3){
        statOFS<<*it<<"\t"<<*(it+1)<<"\t"<<*(it+2)<<std::endl;
      }
#endif

#if defined(COMM_PROFILE) || defined(COMM_PROFILE_BCAST)
      //std::cout<<"DUMPING COMM STATS "<<comm_stat.size()<<" "<<std::endl;
      commOFS<<HEADER_COMM<<std::endl;
      for(std::deque<int>::iterator it = comm_stat.begin(); it!=comm_stat.end(); it+=4){
        commOFS<<LINE_COMM(it)<<std::endl;
      }
#endif

      if (options_->symmetricStorage!=1){
        //reset the trees
        for(int i = 0 ; i< fwdToBelowTree_.size();++i){
          if(fwdToBelowTree_[i]!=NULL){
            fwdToBelowTree_[i]->Reset();
          }
        }
        for(int i = 0 ; i< fwdToRightTree_.size();++i){
          if(fwdToRightTree_[i]!=NULL){
            fwdToRightTree_[i]->Reset();
          }
        }
        for(int i = 0 ; i< redToLeftTree_.size();++i){
          if(redToLeftTree_[i]!=NULL){
            redToLeftTree_[i]->Reset();
          }
        }
        for(int i = 0 ; i< redToAboveTree_.size();++i){
          if(redToAboveTree_[i]!=NULL){
            redToAboveTree_[i]->Reset();
          }
        }
      }
      else{
        for(int i = 0 ; i< bcastLDataTree_.size();++i){
          if(bcastLDataTree_[i]!=NULL){
            bcastLDataTree_[i]->Reset();
          }
        }
        for(int i = 0 ; i< redLTree2_.size();++i){
          if(redLTree2_[i]!=NULL){
            redLTree2_[i]->Reset();
          }
        }
        for(int i = 0 ; i< redDTree2_.size();++i){
          if(redDTree2_[i]!=NULL){
            redDTree2_[i]->Reset();
          }
        }
      }




    } 		// -----  end of method PMatrix::SelInv  ----- 



  template<typename T> 
    void PMatrix<T>::SelInv_P2p	(cublasHandle_t& handle, std::set<std::string> & quantSuperNode  )
    {
      TIMER_START(SelInv_P2p);

#if ( _DEBUGlevel_ >= 1 )
      statusOFS<<"maxTag value: "<<maxTag_<<std::endl;
#endif
      Int numSuper = this->NumSuper(); //总的supernode数量

      // Main loop
      std::vector<std::vector<Int> > & superList = this->WorkingSet();//返回可以并行计算的supernode数组
      Int numSteps = superList.size();//进行numSteps次并行计算？

      Int lidx=0;//不懂
      Int rank = 0;//不懂，这个是会在函数中变化的，因为参数是引用的

      auto itNextSync = syncPoints_.begin();//同步点，应该是每次并行完都要同步一次
      for (lidx=0; lidx<numSteps ; lidx++){//开始numSteps次并行
        SelInvIntra_P2p(lidx,rank, handle, quantSuperNode);

#if ( _DEBUGlevel_ >= 1 )
        statusOFS<<"OUT "<<lidx<<"/"<<numSteps<<" "<<limIndex_<<std::endl;
#endif

#ifdef LIST_BARRIER
#ifndef ALL_BARRIER
        if (options_->maxPipelineDepth!=-1)
#endif
        {
          MPI_Barrier(grid_->comm);
        }
#endif

        //每次并行完同步一下？
        if (options_->symmetricStorage!=1){
          //find max snode.Index
          if(lidx>0 && (rank-1)%limIndex_==0){
            MPI_Barrier(grid_->comm);
          }
        }
        else{
          if(itNextSync!=syncPoints_.end()){
            if(*itNextSync == lidx+1){
              MPI_Barrier(grid_->comm);
              itNextSync++;
            }
          }
        }
      }

      MPI_Barrier(grid_->comm);

      TIMER_STOP(SelInv_P2p);
      return ;
    } 		// -----  end of method PMatrix::SelInv_P2p  ----- 



  template<typename T> 
    void PMatrix<T>::PreSelInv	(cublasHandle_t& handle, std::set<std::string> & quantSuperNode)
    {
      if (options_->symmetricStorage!=1){
#ifdef _PRINT_STATS_
      this->localFlops_ = 0.0;
#endif
      // if(grid_->mpirank == 0){
      //   std::cout<<"Begin PreSelInv"<<std::endl;
      // }
      // Real timeCost = 0;//复制数据需要的总时间
      // Real timeSta = 0;//一次复制开始的时间
      // Real timeEnd = 0;//一次复制结束的时间
      Int numSuper = this->NumSuper(); //得到总的supernode数量
      //if(
      // if(grid_->mpirank == 0){
      //   std::cout<<"Quant Indices:"<<std::endl;
      //   for(std::string str : quanSuperNode){
      //     std::cout<<str<<std::endl;
      //   }
      // }
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "L(i,k) <- L(i,k) * L(k,k)^{-1}" << std::endl << std::endl; 
#endif
      // int cnt_zero = 0;
      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        //如果processor和supernode在同一列
        //MYCOL返回当前processor所在的Column
        //PCOL返回对应supernod所在的processor所在的Column
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          // Broadcast the diagonal L block
          NumMat<T> nzvalLDiag;
          //得到supernode在本地的L矩阵的nonzero block数组，这里有多个L矩阵的原因主要是一个processor可能会在
          //一列占据多个L block
          //LBj返回supernode在本地的nonzero column block number
          std::vector<LBlock<T> >& Lcol = this->L( LBj( ksup, grid_ ) );
          if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){//如果processor和supernode也在同一行
            nzvalLDiag = Lcol[0].nzval;//第0个就是Lkk
            if( nzvalLDiag.m() != SuperSize(ksup, super_) ||
                nzvalLDiag.n() != SuperSize(ksup, super_) ){
              ErrorHandling( "The size of the diagonal block of L is wrong." );
            }
          } // Owns the diagonal block
          else
          {
            nzvalLDiag.Resize( SuperSize(ksup, super_), SuperSize(ksup, super_) );
          }
          //将Lkk传递给同一列的所有processor
          MPI_Bcast( (void*)nzvalLDiag.Data(), nzvalLDiag.ByteSize(),
              MPI_BYTE, PROW( ksup, grid_ ), grid_->colComm );
          
          std::stringstream ss;
          // Triangular solve
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            LBlock<T> & LB = Lcol[ib];
            if( LB.blockIdx > ksup ){//这里是确保它在下三角矩阵？
            #if ( _DEBUGlevel_ >= 2 )
              // Check the correctness of the triangular solve for the first local column
              if( LBj( ksup, grid_ ) == 0 ){
                statusOFS << "Diag   L(" << ksup << ", " << ksup << "): " << nzvalLDiag << std::endl;
                statusOFS << "Before solve L(" << LB.blockIdx << ", " << ksup << "): " << LB.nzval << std::endl;
              }
            #endif
              ss.str("");
              ss<<LB.blockIdx<<","<<ksup;
              //这里就是可以用来加速的地方了
              //这里是通过求解X * OP(A) = alpha * B来得到具体的值的
              //第一个参数R表示OP(A)在X右边，即X * OP(A)
              //第二个参数L表示A为下三角矩阵，因为它是L吗嘛
              //第三个参数N表示不需要对A进行操作，即OP(A) = A
              //第四个参数U表示A为单位三角矩阵，对角线为1
              //最后会把结果覆盖到LB.nzvl.Data()里面
              if(quantSuperNode.find(ss.str()) == quantSuperNode.end()){
                gpu_blas_dtrsm(handle, 'R', 'L', 'N', 'U', LB.numRow, LB.numCol, ONE<T>(),
                  nzvalLDiag.Data(), LB.numCol, LB.nzval.Data(), LB.numRow );
                // blas::Trsm( 'R', 'L', 'N', 'U', LB.numRow, LB.numCol, ONE<T>(),
                //   nzvalLDiag.Data(), LB.numCol, LB.nzval.Data(), LB.numRow );
              }else{
                //这是我魔改的部分
                // std::cout<<"Step 2 : Quant "<<ss.str()<<std::endl;
                // GetTime(timeSta);
                int m = nzvalLDiag.m_;
                int n = nzvalLDiag.n_;
                NumMat<float> nzvalLDiag_float(m, n);
                for(int j = 0;j<n;j++){
                  for(int i = 0;i<m;i++){
                    nzvalLDiag_float(i, j) = (float)nzvalLDiag(i, j);
                  }
                }
                // std::cout<<"Copy nzvalDiag done"<<std::endl;
                
                int LBm = LB.nzval.m_;
                int LBn = LB.nzval.n_;
                NumMat<float> LB_float(LBm, LBn);
                
                
                for(int j = 0;j<LBn;j++){
                  for(int i = 0;i<LBm;i++){
                    LB_float(i, j) = (float)LB.nzval(i, j);
                  }
                }
                
                // GetTime(timeEnd);
                // timeCost += timeEnd - timeSta;
                // std::cout<<"Copy LB done"<<std::endl;
                gpu_blas_strsm(handle, 'R', 'L', 'N', 'U', LB.numRow, LB.numCol, ONE<float>(), (const float*)nzvalLDiag_float.Data(), LB.numCol, LB_float.Data(), LB.numRow);
                // blas::Trsm('R', 'L', 'N', 'U', LB.numRow, LB.numCol, ONE<float>(), (const float*)nzvalLDiag_float.Data(), LB.numCol, LB_float.Data(), LB.numRow);
                // blas::Trsm( 'R', 'L', 'N', 'U', LB.numRow, LB.numCol, ONE<float>(),
                //   (const float*)nzvalLDiag.Data(), LB.numCol, (float)LB.nzval.Data(), LB.numRow );
                // std::cout<<"float TRSM done"<<std::endl;
                // GetTime(timeSta);
                for(int j = 0;j<LBn;j++){
                  for(int i = 0;i<LBm;i++){
                    LB.nzval(i, j) = (Real)LB_float(i, j);
                  }
                }
                // GetTime(timeEnd);
                // timeCost += timeEnd - timeSta;
                // std::cout<<"Copy back done"<<std::endl;
                LB_float.deallocate();
                nzvalLDiag_float.deallocate();
              }

              
#ifdef _PRINT_STATS_
              this->localFlops_+=flops::Trsm<T>('R',LB.numRow, LB.numCol);
#endif
#if ( _DEBUGlevel_ >= 2 )
              // Check the correctness of the triangular solve for the first local column
              if( LBj( ksup, grid_ ) == 0 ){
                statusOFS << "After solve  L(" << LB.blockIdx << ", " << ksup << "): " << LB.nzval << std::endl;
              }
#endif
            }
          }
        } // if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) )
      } // for (ksup)
//            if(grid_->mpirank == 0){
//        std::cout<<"End of Ksup"<<std::endl;
//      }

      // std::cout<<grid_->mpirank<<":"<<cnt_zero<<std::endl;        
              // double* recv_data = NULL;
              // int mpisize = grid_->mpisize;
              // int mpirank = grid_->mpirank;
              // // std::cout<<mpirank<<" Replicate time : " << timeCost << std::endl;
              // if(mpirank == 0){
              //   recv_data = (double*)malloc(sizeof(Real) * mpisize);
              // }
              // MPI_Gather(&timeCost, 1, MPI_DOUBLE, recv_data, 1, MPI_DOUBLE, 0, grid_->comm);
              // if(mpirank == 0){
              //   Real totalTime = 0;
              //   for(int i = 0;i<mpisize;i++){
              //     totalTime += recv_data[i];
              //   }
              //   std::cout<<"Replicate time : " << totalTime << std::endl;
              //   free(recv_data);
              // }

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "U(k,i) <- L(i,k)" << std::endl << std::endl; 
#endif
//if(grid_->mpirank == 0){
//        std::cout<<"Begin Send and Recv"<<std::endl;
//      }

      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        Int ksupProcRow = PROW( ksup, grid_ );//得到supernode对应的processor列
        Int ksupProcCol = PCOL( ksup, grid_ );//得到supernode对应的processor行

        Int sendCount = CountSendToCrossDiagonal(ksup);//需要发送到对称的另一边的L元素数量
        Int recvCount = CountRecvFromCrossDiagonal(ksup);//需要接受的对称的另一边的L元素数量，都是用于后面转化为U矩阵

        std::vector<MPI_Request > arrMpiReqsSend(sendCount, MPI_REQUEST_NULL );//发送L矩阵的句柄
        std::vector<MPI_Request > arrMpiReqsSizeSend(sendCount, MPI_REQUEST_NULL );//发送L矩阵大小的句柄
        std::vector<std::vector<char> > arrSstrLcolSend(sendCount);//临时记录发送数据
        std::vector<Int > arrSstrLcolSizeSend(sendCount);//发送数据大小

        std::vector<MPI_Request > arrMpiReqsRecv(recvCount, MPI_REQUEST_NULL );//同上
        std::vector<MPI_Request > arrMpiReqsSizeRecv(recvCount, MPI_REQUEST_NULL );
        std::vector<std::vector<char> > arrSstrLcolRecv(recvCount);
        std::vector<Int > arrSstrLcolSizeRecv(recvCount);



        // Sender
        if( isSendToCrossDiagonal_(grid_->numProcCol,ksup) ){
#if ( _DEBUGlevel_ >= 1 )
          statusOFS<<"["<<ksup<<"] P"<<MYPROC(grid_)<<" should send to "<<CountSendToCrossDiagonal(ksup)<<" processors"<<std::endl;
#endif

          Int sendIdx = 0;
          for(Int dstCol = 0; dstCol<grid_->numProcCol; dstCol++){
            if(isSendToCrossDiagonal_(dstCol,ksup) ){
              Int dst = PNUM(PROW(ksup,grid_),dstCol,grid_);//这里好像就是要去找Ukc的processor rank了
              if(MYPROC(grid_)!= dst){//如果不是Lkk
                // Pack L data
                std::stringstream sstm;
                std::vector<char> & sstrLcolSend = arrSstrLcolSend[sendIdx];
                Int & sstrSize = arrSstrLcolSizeSend[sendIdx];
                MPI_Request & mpiReqSend = arrMpiReqsSend[sendIdx];
                MPI_Request & mpiReqSizeSend = arrMpiReqsSizeSend[sendIdx];

                std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
                std::vector<LBlock<T> >&  Lcol = this->L( LBj(ksup, grid_) );//得到supernode在本地所有nonzero block数组
                // All blocks except for the diagonal block are to be sent right
                //TODO not true > this is a scatter operation ! Can we know the destination ?

                Int count = 0;
                if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
                  for( Int ib = 1; ib < Lcol.size(); ib++ ){
                    if( Lcol[ib].blockIdx > ksup &&  (Lcol[ib].blockIdx % grid_->numProcCol) == dstCol  ){
                      count++;
                    }
                  }
                }
                else{

                  for( Int ib = 0; ib < Lcol.size(); ib++ ){
                    if( Lcol[ib].blockIdx > ksup &&  (Lcol[ib].blockIdx % grid_->numProcCol) == dstCol  ){
                      count++;
                    }
                  }
                }

                serialize( (Int)count, sstm, NO_MASK );//将发送数量打包

                for( Int ib = 0; ib < Lcol.size(); ib++ ){
                  if( Lcol[ib].blockIdx > ksup &&  (Lcol[ib].blockIdx % grid_->numProcCol) == dstCol  ){ 
#if ( _DEBUGlevel_ >= 1 )
                    statusOFS<<"["<<ksup<<"] SEND contains "<<Lcol[ib].blockIdx<< " which corresponds to "<<GBj(ib,grid_)<<std::endl;
#endif
                    serialize( Lcol[ib], sstm, mask );//将发送数据打包
                  }
                }

                sstrLcolSend.resize( Size(sstm) );
                sstm.read( &sstrLcolSend[0], sstrLcolSend.size() );//将数据放到sstrLcolSend里面
                sstrSize = sstrLcolSend.size();



                // Send/Recv is possible here due to the one to one correspondence
                // in the case of square processor grid

#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<ksup<<"] P"<<MYPROC(grid_)<<" ("<<MYROW(grid_)<<","<<MYCOL(grid_)<<") ---> LBj("<<ksup<<")="<<LBj(ksup,grid_)<<" ---> P"<<dst<<" ("<<ksupProcRow<<","<<dstCol<<")"<<std::endl;
#endif
                MPI_Isend( &sstrSize, sizeof(sstrSize), MPI_BYTE, dst, SELINV_TAG_L_SIZE_CD, grid_->comm, &mpiReqSizeSend );//发送数据大小
                MPI_Isend( (void*)&sstrLcolSend[0], sstrSize, MPI_BYTE, dst, SELINV_TAG_L_CONTENT_CD, grid_->comm, &mpiReqSend );//发送数据


                PROFILE_COMM(MYPROC(this->grid_),dst,SELINV_TAG_L_SIZE_CD,sizeof(sstrSize));
                PROFILE_COMM(MYPROC(this->grid_),dst,SELINV_TAG_L_CONTENT_CD,sstrSize);

                //mpi::Send( sstm, dst,SELINV_TAG_L_SIZE_CD, SELINV_TAG_L_CONTENT_CD, grid_->comm );

                sendIdx++;
              } // if I am a sender
            }
          }
        }





        // Receiver
        if( isRecvFromCrossDiagonal_(grid_->numProcRow,ksup) ){


#if ( _DEBUGlevel_ >= 1 )
          statusOFS<<"["<<ksup<<"] P"<<MYPROC(grid_)<<" should receive from "<<CountRecvFromCrossDiagonal(ksup)<<" processors"<<std::endl;
#endif

          //得到这个supernode在本地的所有U矩阵的nonzero block row
          std::vector<UBlock<T> >& Urow = this->U( LBi( ksup, grid_ ) );
          std::vector<bool> isBlockFound(Urow.size(),false);


          Int recvIdx = 0;
          //receive size first
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,ksup) ){
              std::vector<LBlock<T> > LcolRecv;
              Int src = PNUM(srcRow,PCOL(ksup,grid_),grid_);//找到来源
              if(MYPROC(grid_)!= src){
                MPI_Request & mpiReqSizeRecv = arrMpiReqsSizeRecv[recvIdx];
                Int & sstrSize = arrSstrLcolSizeRecv[recvIdx];

                MPI_Irecv( &sstrSize, 1, MPI_INT, src, SELINV_TAG_L_SIZE_CD, grid_->comm, &mpiReqSizeRecv );//接受数据大小

                recvIdx++;
              }
            }
          }

          mpi::Waitall(arrMpiReqsSizeRecv);



          //receive content
          recvIdx = 0;
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,ksup) ){
              std::vector<LBlock<T> > LcolRecv;
              Int src = PNUM(srcRow,PCOL(ksup,grid_),grid_);
              if(MYPROC(grid_)!= src){
                MPI_Request & mpiReqRecv = arrMpiReqsRecv[recvIdx];
                Int & sstrSize = arrSstrLcolSizeRecv[recvIdx];
                std::vector<char> & sstrLcolRecv = arrSstrLcolRecv[recvIdx];
                sstrLcolRecv.resize(sstrSize);

                MPI_Irecv( &sstrLcolRecv[0], sstrSize, MPI_BYTE, src, SELINV_TAG_L_CONTENT_CD, grid_->comm, &mpiReqRecv );//接受数据

                recvIdx++;
              }
            }
          }

          mpi::Waitall(arrMpiReqsRecv);



          //Process the content
          //对接收到的数据进行unpack和转置，从而得到需要的矩阵
          recvIdx = 0;
          for(Int srcRow = 0; srcRow<grid_->numProcRow; srcRow++){
            if(isRecvFromCrossDiagonal_(srcRow,ksup) ){//如果我是接受者
              std::vector<LBlock<T> > LcolRecv;
              Int src = PNUM(srcRow,PCOL(ksup,grid_),grid_);
              if(MYPROC(grid_)!= src){

                Int & sstrSize = arrSstrLcolSizeRecv[recvIdx];
                std::vector<char> & sstrLcolRecv = arrSstrLcolRecv[recvIdx];
                std::stringstream sstm;

#if ( _DEBUGlevel_ >= 1 )
                statusOFS<<"["<<ksup<<"] P"<<MYPROC(grid_)<<" ("<<MYROW(grid_)<<","<<MYCOL(grid_)<<") <--- LBj("<<ksup<<") <--- P"<<src<<" ("<<srcRow<<","<<ksupProcCol<<")"<<std::endl;
#endif


                sstm.write( &sstrLcolRecv[0], sstrSize );

                // Unpack L data.  
                Int numLBlock;
                std::vector<Int> mask( LBlockMask::TOTAL_NUMBER, 1 );
                deserialize( numLBlock, sstm, NO_MASK );
                LcolRecv.resize(numLBlock);
                for( Int ib = 0; ib < numLBlock; ib++ ){
                  deserialize( LcolRecv[ib], sstm, mask );//解码输入的L矩阵
#if ( _DEBUGlevel_ >= 1 )
                  statusOFS<<"["<<ksup<<"] RECV contains "<<LcolRecv[ib].blockIdx<< " which corresponds to "<< ib * grid_->numProcRow + srcRow; // <<std::endl;
                  //                  statusOFS<<" L is on row "<< srcRow <<" whereas U is on col "<<((ib * grid_->numProcRow + srcRow)/grid_->numProcCol)%grid_->numProcCol <<std::endl;
                  statusOFS<<" L is on row "<< srcRow <<" whereas U is on col "<< LcolRecv[ib].blockIdx % grid_->numProcCol <<std::endl;
#endif
                }


                recvIdx++;

              } // sender is not the same as receiver
              else{
                // L is obtained locally, just make a copy. Do not include the diagonal block
                std::vector<LBlock<T> >& Lcol = this->L( LBj( ksup, grid_ ) );
                if( MYROW( grid_ ) != PROW( ksup, grid_ ) ){
                  LcolRecv.resize( Lcol.size() );
                  for( Int ib = 0; ib < Lcol.size(); ib++ ){
                    LcolRecv[ib] = Lcol[ib];
                  }
                }
                else{
                  LcolRecv.resize( Lcol.size() - 1 );
                  for( Int ib = 0; ib < Lcol.size() - 1; ib++ ){
                    LcolRecv[ib] = Lcol[ib+1];
                  }
                }
              } // sender is the same as receiver

              // Update U
              // Make sure that the size of L and the corresponding U blocks match.
              for( Int ib = 0; ib < LcolRecv.size(); ib++ ){
                LBlock<T> & LB = LcolRecv[ib];
                if( LB.blockIdx <= ksup ){
                  ErrorHandling( "LcolRecv contains the wrong blocks." );
                }
                for( Int jb = 0; jb < Urow.size(); jb++ ){
                  UBlock<T> &  UB = Urow[jb];
                  if( LB.blockIdx == UB.blockIdx ){
                    // Compare size
                    if( LB.numRow != UB.numCol || LB.numCol != UB.numRow ){
                      std::ostringstream msg;
                      msg << "LB(" << LB.blockIdx << ", " << ksup << ") and UB(" 
                        << ksup << ", " << UB.blockIdx << ")	do not share the same size." << std::endl
                        << "LB: " << LB.numRow << " x " << LB.numCol << std::endl
                        << "UB: " << UB.numRow << " x " << UB.numCol << std::endl;
                      ErrorHandling( msg.str().c_str() );
                    }

                    // Note that the order of the column indices of the U
                    // block may not follow the order of the row indices,
                    // overwrite the information in U.
                    UB.cols = LB.rows;
                    Transpose( LB.nzval, UB.nzval );//对矩阵L进行转置

#if ( _DEBUGlevel_ >= 1 )
                    statusOFS<<"["<<ksup<<"] USING "<<LB.blockIdx<< std::endl;
#endif
                    isBlockFound[jb] = true;
                    break;
                  } // if( LB.blockIdx == UB.blockIdx )
                } // for (jb)
              } // for (ib)
            }
          }

          for( Int jb = 0; jb < Urow.size(); jb++ ){
            UBlock<T> & UB = Urow[jb];
            if( !isBlockFound[jb] ){//检查是否所有的矩阵都转置完了
              ErrorHandling( "UBlock cannot find its update. Something is seriously wrong." );
            }
          }



        } // if I am a receiver


        //Wait until every receiver is done
        mpi::Waitall(arrMpiReqsSizeSend);
        mpi::Waitall(arrMpiReqsSend);


      } // for (ksup)
//if(grid_->mpirank == 0){
  //      std::cout<<"End of Send and Recv"<<std::endl;
    //  }


    //说实话我不知道下面在干嘛，论文里到这就已经停止了
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "L(i,i) <- [L(k,k) * U(k,k)]^{-1}" << std::endl << std::endl; 
#endif

      for( Int ksup3 = 0; ksup3 < numSuper; ksup3++ ){
        Int ksup2 = ksup3;
        if( MYROW( grid_ ) == PROW( ksup3, grid_ ) &&
            MYCOL( grid_ ) == PCOL( ksup3, grid_ )	){//如果本processor包含了Lkk
          IntNumVec ipiv( SuperSize( ksup3, super_ ) );
          // Note that the pivoting vector ipiv should follow the FORTRAN
          // notation by adding the +1
          for(Int i = 0; i < SuperSize( ksup3, super_ ); i++){
            ipiv[i] = i + 1;
          }
          LBlock<T> & LB = (this->L( LBj( ksup3, grid_ ) ))[0];//得到Lkk矩阵
#if ( _DEBUGlevel_ >= 2 )
          // Check the correctness of the matrix inversion for the first local column
          statusOFS << "Factorized A (" << ksup3 << ", " << ksup3 << "): " << LB.nzval << std::endl;
#endif
      //    if(grid_->mpirank == 0){
//	std::cout<<"Begin Getri"<<ksup3<<std::endl;
//}
          //也就是说现在Lkk为Lkk-1了，确实在第四步需要用到Ukk-1和Lkk-1
          lapack::Getri( SuperSize( ksup3, super_ ), LB.nzval.Data(), 
              SuperSize( ksup3, super_ ), ipiv.Data() );//求Lkk逆
  //        if(grid_->mpirank == 0){
  //      std::cout<<"End Getri"<<ksup3<<std::endl;
//}


#ifdef _PRINT_STATS_
          this->localFlops_+=flops::Getri<T>(SuperSize( ksup3, this->super_ ));
#endif
          // Symmetrize the diagonal block
          Symmetrize( LB.nzval );//将Lk据矩阵保持对称

#if ( _DEBUGlevel_ >= 2 )
          // Check the correctness of the matrix inversion for the first local column
          statusOFS << "Inversed   A (" << ksup3 << ", " << ksup3 << "): " << LB.nzval << std::endl;
#endif
        } // if I need to inverse the diagonal block
      } // for (ksup)
  //    if(grid_->mpirank == 0){
//	std::cout<<"end of Preselinv"<<std::endl;
//}
      }
      else{

#ifdef _PRINT_STATS_
        this->localFlops_ = 0.0;
#endif
        Int numSuper = this->NumSuper(); 

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "L(i,k) <- L(i,k) * L(k,k)^{-1}"
          << std::endl << std::endl; 
#endif

        //TODO These BCASTS can be done with a single Allgatherv within each row / column
        for( Int ksup = 0; ksup < numSuper; ksup++ ){
          if( MYCOL( this->grid_ ) == PCOL( ksup, this->grid_ ) ){
            // Broadcast the diagonal L block
            NumMat<T> nzvalLDiag;
            std::vector<LBlock<T> >& Lcol = this->L( LBj( ksup, this->grid_ ) );
            if( MYROW( this->grid_ ) == PROW( ksup, this->grid_ ) ){
              nzvalLDiag = Lcol[0].nzval;
              if( nzvalLDiag.m() != SuperSize(ksup, this->super_) ||
                  nzvalLDiag.n() != SuperSize(ksup, this->super_) ){
                ErrorHandling( 
                    "The size of the diagonal block of L is wrong." );
              }
            } // Owns the diagonal block
            else {
              nzvalLDiag.Resize(SuperSize(ksup, this->super_), SuperSize(ksup, this->super_));
            }
            MPI_Bcast( (void*)nzvalLDiag.Data(), nzvalLDiag.ByteSize(),
                MPI_BYTE, PROW( ksup, this->grid_ ), this->grid_->colComm );

            // Triangular solve
            for( Int ib = 0; ib < Lcol.size(); ib++ ){
              LBlock<T> & LB = Lcol[ib];
              if( LB.blockIdx > ksup  ){
                blas::Trsm( 'R', 'L', 'N', 'U', LB.numRow, LB.numCol,
                    ONE<T>(), nzvalLDiag.Data(), LB.numCol, 
                    LB.nzval.Data(), LB.numRow );
#ifdef _PRINT_STATS_
                this->localFlops_+=flops::Trsm<T>('R',LB.numRow, LB.numCol);
#endif
              }
            }
          } // if( MYCOL( this->grid_ ) == PCOL( ksup, this->grid_ ) )
        } // for (ksup)

        for( Int ksup = 0; ksup < numSuper; ksup++ ){
          if( MYPROC( this->grid_ ) == PNUM( PROW(ksup,this->grid_),PCOL(ksup,this->grid_), this->grid_ ) ){
            IntNumVec ipiv( SuperSize( ksup, this->super_ ) );
            // Note that the pivoting vector ipiv should follow the FORTRAN
            // notation by adding the +1
          //  std::iota(ipiv.Data(),ipiv.Data()+ipiv.m(),1);
          for(Int i = 0; i < SuperSize( ksup, super_ ); i++){
            ipiv[i] = i + 1;
          }

            LBlock<T> & LB = (this->L( LBj( ksup, this->grid_ ) ))[0];
            lapack::Getri( SuperSize( ksup, this->super_ ), LB.nzval.Data(), 
                SuperSize( ksup, this->super_ ), ipiv.Data() );
#ifdef _PRINT_STATS_
            this->localFlops_+=flops::Getri<T>(SuperSize( ksup, this->super_ ));
#endif
            // Symmetrize the diagonal block
            Symmetrize( LB.nzval );

          } // if I need to invert the diagonal block
        } // for (ksup)
      }
  //    if(grid_->mpirank == 0){
//	std::cout<<"Return PselInv"<<std::endl;
//}
      return ;
    } 		// -----  end of method PMatrix::PreSelInv  ----- 

  template<typename T>
    void PMatrix<T>::GetDiagonal	( NumVec<T>& diag )
    {
      Int numSuper = this->NumSuper(); 

      Int numCol = this->NumCol();
      const IntNumVec& perm    = super_->perm;
      const IntNumVec& permInv = super_->permInv;

      const IntNumVec * pPerm_r;
      const IntNumVec * pPermInv_r;

      pPerm_r = &super_->perm_r;
      pPermInv_r = &super_->permInv_r;

      const IntNumVec& perm_r    = *pPerm_r;
      const IntNumVec& permInv_r = *pPermInv_r;


      NumVec<T> diagLocal( numCol );
      SetValue( diagLocal, ZERO<T>() );

      diag.Resize( numCol );
      SetValue( diag, ZERO<T>() );

      for( Int orow = 0; orow < numCol; orow++){
        //row index in the permuted order
        Int row         = perm[ orow ];
        //col index in the permuted order
        Int col         = perm[ perm_r[ orow] ];

        Int blockColIdx = BlockIdx( col, super_ );
        Int blockRowIdx = BlockIdx( row, super_ );


        // I own the column	
        if( MYROW( grid_ ) == PROW( blockRowIdx, grid_ ) &&
            MYCOL( grid_ ) == PCOL( blockColIdx, grid_ ) ){
          // Search for the nzval
          bool isFound = false;

          if( blockColIdx <= blockRowIdx ){
            // Data on the L side

            std::vector<LBlock<T> >&  Lcol = this->L( LBj( blockColIdx, grid_ ) );

            for( Int ib = 0; ib < Lcol.size(); ib++ ){
#if ( _DEBUGlevel_ >= 1 )
              statusOFS << "blockRowIdx = " << blockRowIdx << ", Lcol[ib].blockIdx = " << Lcol[ib].blockIdx << ", blockColIdx = " << blockColIdx << std::endl;
#endif
              if( Lcol[ib].blockIdx == blockRowIdx ){
                IntNumVec& rows = Lcol[ib].rows;
                for( int iloc = 0; iloc < Lcol[ib].numRow; iloc++ ){
                  if( rows[iloc] == row ){
                    Int jloc = col - FirstBlockCol( blockColIdx, super_ );

                    diagLocal[ orow ] = Lcol[ib].nzval( iloc, jloc );


                    isFound = true;
                    break;
                  } // found the corresponding row
                }
              }
              if( isFound == true ) break;  
            } // for (ib)
          } 
          else{
            // Data on the U side

            std::vector<UBlock<T> >&  Urow = this->U( LBi( blockRowIdx, grid_ ) );

            for( Int jb = 0; jb < Urow.size(); jb++ ){
              if( Urow[jb].blockIdx == blockColIdx ){
                IntNumVec& cols = Urow[jb].cols;
                for( int jloc = 0; jloc < Urow[jb].numCol; jloc++ ){
                  if( cols[jloc] == col ){
                    Int iloc = row - FirstBlockRow( blockRowIdx, super_ );

                    diagLocal[ orow ] = Urow[jb].nzval( iloc, jloc );

                    isFound = true;
                    break;
                  } // found the corresponding col
                }
              }
              if( isFound == true ) break;  
            } // for (jb)
          } // if( blockColIdx <= blockRowIdx ) 

          // Did not find the corresponding row, set the value to zero.
          if( isFound == false ){
            statusOFS << "In the permutated order, (" << row << ", " << col <<
              ") is not found in PMatrix." << std::endl;
            diagLocal[orow] = ZERO<T>();
          }
        } 
      }

      //      //TODO This doesnt work with row perm
      //      for( Int ksup = 0; ksup < numSuper; ksup++ ){
      //        Int numRows = SuperSize(ksup, this->super_);
      //
      //        // I own the diagonal block	
      //        if( MYROW( grid_ ) == PROW( ksup, grid_ ) &&
      //            MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
      //          LBlock<T> & LB = this->L( LBj( ksup, grid_ ) )[0];
      //          for( Int i = 0; i < LB.numRow; i++ ){
      //            diagLocal( permInv[ LB.rows(i) ] ) = LB.nzval( i, perm[permInv_r[i]] );
      //          }
      //        }
      //      }

      // All processors own diag
      mpi::Allreduce( diagLocal.Data(), diag.Data(), numCol, MPI_SUM, grid_->comm );


      return ;
    } 		// -----  end of method PMatrix::GetDiagonal  ----- 



  template<typename T>
    void PMatrix<T>::PMatrixToDistSparseMatrix	( DistSparseMatrix<T>& A )
    {
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "Converting PMatrix to DistSparseMatrix." << std::endl;
#endif
      Int mpirank = grid_->mpirank;
      Int mpisize = grid_->mpisize;

      std::vector<Int>     rowSend( mpisize );
      std::vector<Int>     colSend( mpisize );
      std::vector<T>  valSend( mpisize );
      std::vector<Int>     sizeSend( mpisize, 0 );
      std::vector<Int>     displsSend( mpisize, 0 );

      std::vector<Int>     rowRecv( mpisize );
      std::vector<Int>     colRecv( mpisize );
      std::vector<T>  valRecv( mpisize );
      std::vector<Int>     sizeRecv( mpisize, 0 );
      std::vector<Int>     displsRecv( mpisize, 0 );

      Int numSuper = this->NumSuper();
      const IntNumVec& perm    = super_->perm;
      const IntNumVec& permInv = super_->permInv;

      const IntNumVec * pPerm_r;
      const IntNumVec * pPermInv_r;

      pPerm_r = &super_->perm_r;
      pPermInv_r = &super_->permInv_r;

      const IntNumVec& perm_r    = *pPerm_r;
      const IntNumVec& permInv_r = *pPermInv_r;

      // The number of local columns in DistSparseMatrix format for the
      // processor with rank 0.  This number is the same for processors
      // with rank ranging from 0 to mpisize - 2, and may or may not differ
      // from the number of local columns for processor with rank mpisize -
      // 1.
      Int numColFirst = this->NumCol() / mpisize;

      // Count the size first.
      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // L blocks
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj( ksup, grid_ ) );
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            for( Int j = 0; j < Lcol[ib].numCol; j++ ){
              Int jcol = permInv[ permInv_r[ j + FirstBlockCol( ksup, super_ ) ] ];
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              sizeSend[dest] += Lcol[ib].numRow;
            }
          }
        } // I own the column of ksup 

        // U blocks
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
          std::vector<UBlock<T> >&  Urow = this->U( LBi( ksup, grid_ ) );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            IntNumVec& cols = Urow[jb].cols;
            for( Int j = 0; j < cols.m(); j++ ){
              Int jcol = permInv[ permInv_r[ cols[j] ] ];
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              sizeSend[dest] += Urow[jb].numRow;
            }
          }
        } // I own the row of ksup
      } // for (ksup)

      // All-to-all exchange of size information
      MPI_Alltoall( 
          &sizeSend[0], 1, MPI_INT,
          &sizeRecv[0], 1, MPI_INT, grid_->comm );



      // Reserve the space
      for( Int ip = 0; ip < mpisize; ip++ ){
        if( ip == 0 ){
          displsSend[ip] = 0;
        }
        else{
          displsSend[ip] = displsSend[ip-1] + sizeSend[ip-1];
        }

        if( ip == 0 ){
          displsRecv[ip] = 0;
        }
        else{
          displsRecv[ip] = displsRecv[ip-1] + sizeRecv[ip-1];
        }
      }
      Int sizeSendTotal = displsSend[mpisize-1] + sizeSend[mpisize-1];
      Int sizeRecvTotal = displsRecv[mpisize-1] + sizeRecv[mpisize-1];

      rowSend.resize( sizeSendTotal );
      colSend.resize( sizeSendTotal );
      valSend.resize( sizeSendTotal );

      rowRecv.resize( sizeRecvTotal );
      colRecv.resize( sizeRecvTotal );
      valRecv.resize( sizeRecvTotal );

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "displsSend = " << displsSend << std::endl;
      statusOFS << "displsRecv = " << displsRecv << std::endl;
#endif

      // Put (row, col, val) to the sending buffer
      std::vector<Int>   cntSize( mpisize, 0 );

      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // L blocks
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj( ksup, grid_ ) );
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            IntNumVec&  rows = Lcol[ib].rows;
            NumMat<T>& nzval = Lcol[ib].nzval;
            for( Int j = 0; j < Lcol[ib].numCol; j++ ){
              Int jcol = permInv[ permInv_r[ j + FirstBlockCol( ksup, super_ ) ] ];
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              for( Int i = 0; i < rows.m(); i++ ){
                rowSend[displsSend[dest] + cntSize[dest]] = permInv[ rows[i] ];
                colSend[displsSend[dest] + cntSize[dest]] = jcol;
                valSend[displsSend[dest] + cntSize[dest]] = nzval( i, j );
                cntSize[dest]++;
              }
            }
          }
        } // I own the column of ksup 

        // U blocks
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
          std::vector<UBlock<T> >&  Urow = this->U( LBi( ksup, grid_ ) );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            IntNumVec& cols = Urow[jb].cols;
            NumMat<T>& nzval = Urow[jb].nzval;
            for( Int j = 0; j < cols.m(); j++ ){
              Int jcol = permInv[ permInv_r[ cols[j] ] ];
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              for( Int i = 0; i < Urow[jb].numRow; i++ ){
                rowSend[displsSend[dest] + cntSize[dest]] = permInv[ i + FirstBlockCol( ksup, super_ ) ];
                colSend[displsSend[dest] + cntSize[dest]] = jcol;
                valSend[displsSend[dest] + cntSize[dest]] = nzval( i, j );
                cntSize[dest]++;
              }
            }
          }
        } // I own the row of ksup
      }

      // Check sizes match
      for( Int ip = 0; ip < mpisize; ip++ ){
        if( cntSize[ip] != sizeSend[ip] )
        {
          ErrorHandling( "Sizes of the sending information do not match." );
        }
      }


      // Alltoallv to exchange information
      mpi::Alltoallv( 
          &rowSend[0], &sizeSend[0], &displsSend[0],
          &rowRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );
      mpi::Alltoallv( 
          &colSend[0], &sizeSend[0], &displsSend[0],
          &colRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );
      mpi::Alltoallv( 
          &valSend[0], &sizeSend[0], &displsSend[0],
          &valRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "Alltoallv communication finished." << std::endl;
#endif

      //#if ( _DEBUGlevel_ >= 1 )
      //	for( Int ip = 0; ip < mpisize; ip++ ){
      //		statusOFS << "rowSend[" << ip << "] = " << rowSend[ip] << std::endl;
      //		statusOFS << "rowRecv[" << ip << "] = " << rowRecv[ip] << std::endl;
      //		statusOFS << "colSend[" << ip << "] = " << colSend[ip] << std::endl;
      //		statusOFS << "colRecv[" << ip << "] = " << colRecv[ip] << std::endl;
      //		statusOFS << "valSend[" << ip << "] = " << valSend[ip] << std::endl;
      //		statusOFS << "valRecv[" << ip << "] = " << valRecv[ip] << std::endl;
      //	}
      //#endif

      // Organize the received message.
      Int firstCol = mpirank * numColFirst;
      Int numColLocal;
      if( mpirank == mpisize-1 )
        numColLocal = this->NumCol() - numColFirst * (mpisize-1);
      else
        numColLocal = numColFirst;

      std::vector<std::vector<Int> > rows( numColLocal );
      std::vector<std::vector<T> > vals( numColLocal );

      for( Int ip = 0; ip < mpisize; ip++ ){
        Int*     rowRecvCur = &rowRecv[displsRecv[ip]];
        Int*     colRecvCur = &colRecv[displsRecv[ip]];
        T*  valRecvCur = &valRecv[displsRecv[ip]];
        for( Int i = 0; i < sizeRecv[ip]; i++ ){
          rows[colRecvCur[i]-firstCol].push_back( rowRecvCur[i] );
          vals[colRecvCur[i]-firstCol].push_back( valRecvCur[i] );
        } // for (i)
      } // for (ip)

      // Sort the rows
      std::vector<std::vector<Int> > sortIndex( numColLocal );
      for( Int j = 0; j < numColLocal; j++ ){
        sortIndex[j].resize( rows[j].size() );
        for( Int i = 0; i < sortIndex[j].size(); i++ )
          sortIndex[j][i] = i;
        std::sort( sortIndex[j].begin(), sortIndex[j].end(),
            IndexComp<std::vector<Int>& > ( rows[j] ) );
      } // for (j)

      // Form DistSparseMatrix according to the received message	
      // NOTE: for indicies,  DistSparseMatrix follows the FORTRAN
      // convention (1 based) while PMatrix follows the C convention (0
      // based)
      A.size = this->NumCol();
      A.nnzLocal  = 0;
      A.colptrLocal.Resize( numColLocal + 1 );
      // Note that 1 is important since the index follows the FORTRAN convention
      A.colptrLocal(0) = 1;
      for( Int j = 0; j < numColLocal; j++ ){
        A.nnzLocal += rows[j].size();
        A.colptrLocal(j+1) = A.colptrLocal(j) + rows[j].size();
      }

      A.comm = grid_->comm;

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "nnzLocal = " << A.nnzLocal << std::endl;
      statusOFS << "nnz      = " << A.Nnz()      << std::endl;
#endif


      A.rowindLocal.Resize( A.nnzLocal );
      A.nzvalLocal.Resize(  A.nnzLocal );

      Int*     rowPtr = A.rowindLocal.Data();
      T*  nzvalPtr = A.nzvalLocal.Data();
      for( Int j = 0; j < numColLocal; j++ ){
        std::vector<Int>& rowsCur = rows[j];
        std::vector<Int>& sortIndexCur = sortIndex[j];
        std::vector<T>& valsCur = vals[j];
        for( Int i = 0; i < rows[j].size(); i++ ){
          // Note that 1 is important since the index follows the FORTRAN convention
          *(rowPtr++)   = rowsCur[sortIndexCur[i]] + 1;
          *(nzvalPtr++) = valsCur[sortIndexCur[i]]; 
        }
      }

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "A.colptrLocal[end]   = " << A.colptrLocal(numColLocal) << std::endl;
      statusOFS << "A.rowindLocal.size() = " << A.rowindLocal.m() << std::endl;
      statusOFS << "A.rowindLocal[end]   = " << A.rowindLocal(A.nnzLocal-1) << std::endl;
      statusOFS << "A.nzvalLocal[end]    = " << A.nzvalLocal(A.nnzLocal-1) << std::endl;
#endif



      return ;
    } 		// -----  end of method PMatrix::PMatrixToDistSparseMatrix  ----- 



  template<typename T>
    void PMatrix<T>::PMatrixToDistSparseMatrix_OLD	( const DistSparseMatrix<T>& A, DistSparseMatrix<T>& B	)
    {
#if ( _DEBUGlevel_ >= 1 )
      statusOFS << std::endl << "Converting PMatrix to DistSparseMatrix (2nd format)." << std::endl;
#endif
      Int mpirank = grid_->mpirank;
      Int mpisize = grid_->mpisize;

      std::vector<Int>     rowSend( mpisize );
      std::vector<Int>     colSend( mpisize );
      std::vector<T>  valSend( mpisize );
      std::vector<Int>     sizeSend( mpisize, 0 );
      std::vector<Int>     displsSend( mpisize, 0 );

      std::vector<Int>     rowRecv( mpisize );
      std::vector<Int>     colRecv( mpisize );
      std::vector<T>  valRecv( mpisize );
      std::vector<Int>     sizeRecv( mpisize, 0 );
      std::vector<Int>     displsRecv( mpisize, 0 );

      Int numSuper = this->NumSuper();
      const IntNumVec& permInv = super_->permInv;



      const IntNumVec * pPermInv_r;

      if(optionsFact_->RowPerm=="NOROWPERM"){
        pPermInv_r = &super_->permInv;
      }
      else{
        pPermInv_r = &super_->permInv_r;
      }

      const IntNumVec& permInv_r = *pPermInv_r;






      // The number of local columns in DistSparseMatrix format for the
      // processor with rank 0.  This number is the same for processors
      // with rank ranging from 0 to mpisize - 2, and may or may not differ
      // from the number of local columns for processor with rank mpisize -
      // 1.
      Int numColFirst = this->NumCol() / mpisize;

      // Count the size first.
      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // L blocks
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj( ksup, grid_ ) );
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            for( Int j = 0; j < Lcol[ib].numCol; j++ ){
              Int jcol = permInv( j + FirstBlockCol( ksup, super_ ) );
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              sizeSend[dest] += Lcol[ib].numRow;
            }
          }
        } // I own the column of ksup 

        // U blocks
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
          std::vector<UBlock<T> >&  Urow = this->U( LBi( ksup, grid_ ) );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            IntNumVec& cols = Urow[jb].cols;
            for( Int j = 0; j < cols.m(); j++ ){
              Int jcol = permInv( cols(j) );
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              sizeSend[dest] += Urow[jb].numRow;
            }
          }
        } // I own the row of ksup
      } // for (ksup)

      // All-to-all exchange of size information
      MPI_Alltoall( 
          &sizeSend[0], 1, MPI_INT,
          &sizeRecv[0], 1, MPI_INT, grid_->comm );



      // Reserve the space
      for( Int ip = 0; ip < mpisize; ip++ ){
        if( ip == 0 ){
          displsSend[ip] = 0;
        }
        else{
          displsSend[ip] = displsSend[ip-1] + sizeSend[ip-1];
        }

        if( ip == 0 ){
          displsRecv[ip] = 0;
        }
        else{
          displsRecv[ip] = displsRecv[ip-1] + sizeRecv[ip-1];
        }
      }
      Int sizeSendTotal = displsSend[mpisize-1] + sizeSend[mpisize-1];
      Int sizeRecvTotal = displsRecv[mpisize-1] + sizeRecv[mpisize-1];

      rowSend.resize( sizeSendTotal );
      colSend.resize( sizeSendTotal );
      valSend.resize( sizeSendTotal );

      rowRecv.resize( sizeRecvTotal );
      colRecv.resize( sizeRecvTotal );
      valRecv.resize( sizeRecvTotal );

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "displsSend = " << displsSend << std::endl;
      statusOFS << "displsRecv = " << displsRecv << std::endl;
#endif

      // Put (row, col, val) to the sending buffer
      std::vector<Int>   cntSize( mpisize, 0 );


      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // L blocks
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          std::vector<LBlock<T> >&  Lcol = this->L( LBj( ksup, grid_ ) );
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            IntNumVec&  rows = Lcol[ib].rows;
            NumMat<T>& nzval = Lcol[ib].nzval;
            for( Int j = 0; j < Lcol[ib].numCol; j++ ){
              Int jcol = permInv( j + FirstBlockCol( ksup, super_ ) );
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              for( Int i = 0; i < rows.m(); i++ ){
                rowSend[displsSend[dest] + cntSize[dest]] = permInv_r( rows(i) );
                colSend[displsSend[dest] + cntSize[dest]] = jcol;
                valSend[displsSend[dest] + cntSize[dest]] = nzval( i, j );
                cntSize[dest]++;
              }
            }
          }
        } // I own the column of ksup 


        // U blocks
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
          std::vector<UBlock<T> >&  Urow = this->U( LBi( ksup, grid_ ) );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            IntNumVec& cols = Urow[jb].cols;
            NumMat<T>& nzval = Urow[jb].nzval;
            for( Int j = 0; j < cols.m(); j++ ){
              Int jcol = permInv( cols(j) );
              Int dest = std::min( jcol / numColFirst, mpisize - 1 );
              for( Int i = 0; i < Urow[jb].numRow; i++ ){
                rowSend[displsSend[dest] + cntSize[dest]] = 
                  permInv_r( i + FirstBlockCol( ksup, super_ ) );
                colSend[displsSend[dest] + cntSize[dest]] = jcol;
                valSend[displsSend[dest] + cntSize[dest]] = nzval( i, j );
                cntSize[dest]++;
              }
            }
          }
        } // I own the row of ksup
      }



      // Check sizes match
      for( Int ip = 0; ip < mpisize; ip++ ){
        if( cntSize[ip] != sizeSend[ip] ){
          ErrorHandling( "Sizes of the sending information do not match." );
        }
      }

      // Alltoallv to exchange information
      mpi::Alltoallv( 
          &rowSend[0], &sizeSend[0], &displsSend[0],
          &rowRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );
      mpi::Alltoallv( 
          &colSend[0], &sizeSend[0], &displsSend[0],
          &colRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );
      mpi::Alltoallv( 
          &valSend[0], &sizeSend[0], &displsSend[0],
          &valRecv[0], &sizeRecv[0], &displsRecv[0],
          grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "Alltoallv communication finished." << std::endl;
#endif

      //#if ( _DEBUGlevel_ >= 1 )
      //	for( Int ip = 0; ip < mpisize; ip++ ){
      //		statusOFS << "rowSend[" << ip << "] = " << rowSend[ip] << std::endl;
      //		statusOFS << "rowRecv[" << ip << "] = " << rowRecv[ip] << std::endl;
      //		statusOFS << "colSend[" << ip << "] = " << colSend[ip] << std::endl;
      //		statusOFS << "colRecv[" << ip << "] = " << colRecv[ip] << std::endl;
      //		statusOFS << "valSend[" << ip << "] = " << valSend[ip] << std::endl;
      //		statusOFS << "valRecv[" << ip << "] = " << valRecv[ip] << std::endl;
      //	}
      //#endif

      // Organize the received message.
      Int firstCol = mpirank * numColFirst;
      Int numColLocal;
      if( mpirank == mpisize-1 )
        numColLocal = this->NumCol() - numColFirst * (mpisize-1);
      else
        numColLocal = numColFirst;

      std::vector<std::vector<Int> > rows( numColLocal );
      std::vector<std::vector<T> > vals( numColLocal );

      for( Int ip = 0; ip < mpisize; ip++ ){
        Int*     rowRecvCur = &rowRecv[displsRecv[ip]];
        Int*     colRecvCur = &colRecv[displsRecv[ip]];
        T*  valRecvCur = &valRecv[displsRecv[ip]];
        for( Int i = 0; i < sizeRecv[ip]; i++ ){
          rows[colRecvCur[i]-firstCol].push_back( rowRecvCur[i] );
          vals[colRecvCur[i]-firstCol].push_back( valRecvCur[i] );
        } // for (i)
      } // for (ip)

      // Sort the rows
      std::vector<std::vector<Int> > sortIndex( numColLocal );
      for( Int j = 0; j < numColLocal; j++ ){
        sortIndex[j].resize( rows[j].size() );
        for( Int i = 0; i < sortIndex[j].size(); i++ )
          sortIndex[j][i] = i;
        std::sort( sortIndex[j].begin(), sortIndex[j].end(),
            IndexComp<std::vector<Int>& > ( rows[j] ) );
      } // for (j)

      // Form DistSparseMatrix according to the received message	
      // NOTE: for indicies,  DistSparseMatrix follows the FORTRAN
      // convention (1 based) while PMatrix follows the C convention (0
      // based)
      if( A.size != this->NumCol() ){
        ErrorHandling( "The DistSparseMatrix providing the pattern has a different size from PMatrix." );
      }
      if( A.colptrLocal.m() != numColLocal + 1 ){
        ErrorHandling( "The DistSparseMatrix providing the pattern has a different number of local columns from PMatrix." );
      }

      B.size = A.size;
      B.nnz  = A.nnz;
      B.nnzLocal = A.nnzLocal;
      B.colptrLocal = A.colptrLocal;
      B.rowindLocal = A.rowindLocal;
      B.nzvalLocal.Resize( B.nnzLocal );
      SetValue( B.nzvalLocal, ZERO<T>() );
      // Make sure that the communicator of A and B are the same.
      // FIXME Find a better way to compare the communicators
      //			if( grid_->comm != A.comm ){
      //ErrorHandling( "The DistSparseMatrix providing the pattern has a different communicator from PMatrix." );
      //			}
      B.comm = grid_->comm;

      Int*     rowPtr = B.rowindLocal.Data();
      T*  nzvalPtr = B.nzvalLocal.Data();
      for( Int j = 0; j < numColLocal; j++ ){
        std::vector<Int>& rowsCur = rows[j];
        std::vector<Int>& sortIndexCur = sortIndex[j];
        std::vector<T>& valsCur = vals[j];
        std::vector<Int>  rowsCurSorted( rowsCur.size() );
        // Note that 1 is important since the index follows the FORTRAN convention
        for( Int i = 0; i < rowsCurSorted.size(); i++ ){
          rowsCurSorted[i] = rowsCur[sortIndexCur[i]] + 1;
        }

        // Search and match the indices
        std::vector<Int>::iterator it;
        for( Int i = B.colptrLocal(j) - 1; 
            i < B.colptrLocal(j+1) - 1; i++ ){
          it = std::lower_bound( rowsCurSorted.begin(), rowsCurSorted.end(),
              *(rowPtr++) );
          if( it == rowsCurSorted.end() ){
            // Did not find the row, set it to zero
            *(nzvalPtr++) = ZERO<T>();
          }
          else{
            // Found the row, set it according to the received value
            *(nzvalPtr++) = valsCur[ sortIndexCur[it-rowsCurSorted.begin()] ];
          }
        } // for (i)	
      } // for (j)

#if ( _DEBUGlevel_ >= 1 )
      statusOFS << "B.colptrLocal[end]   = " << B.colptrLocal(numColLocal) << std::endl;
      statusOFS << "B.rowindLocal.size() = " << B.rowindLocal.m() << std::endl;
      statusOFS << "B.rowindLocal[end]   = " << B.rowindLocal(B.nnzLocal-1) << std::endl;
      statusOFS << "B.nzvalLocal[end]    = " << B.nzvalLocal(B.nnzLocal-1) << std::endl;
#endif



      return ;
    } 		// -----  end of method PMatrix::PMatrixToDistSparseMatrix_OLD  ----- 


  // A (maybe) more memory efficient way for converting the PMatrix to a
  // DistSparseMatrix structure.
  //
  template<typename T>
  template<typename T1>
    void PMatrix<T>::PMatrixToDistSparseMatrix ( const DistSparseMatrix<T1>& A, DistSparseMatrix<T>& B )
    {
      this->PMatrixToDistSparseMatrix_ ( A.colptrLocal, A.rowindLocal, A.size, A.nnz, A.nnzLocal, B );
    }



  template<typename T>
    void PMatrix<T>::PMatrixToDistSparseMatrix_ ( const NumVec<Int> & AcolptrLocal, const NumVec<Int> & ArowindLocal, const Int Asize, const LongInt Annz, const Int AnnzLocal, DistSparseMatrix<T>& B )
    {
      if (options_->symmetricStorage!=1){
#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "Converting PMatrix to DistSparseMatrix (2nd format)." << std::endl;
#endif
        Int mpirank = grid_->mpirank;
        Int mpisize = grid_->mpisize;

        std::vector<Int>     rowSend( mpisize );
        std::vector<Int>     colSend( mpisize );
        std::vector<T>  valSend( mpisize );
        std::vector<Int>     sizeSend( mpisize, 0 );
        std::vector<Int>     displsSend( mpisize, 0 );

        std::vector<Int>     rowRecv( mpisize );
        std::vector<Int>     colRecv( mpisize );
        std::vector<T>  valRecv( mpisize );
        std::vector<Int>     sizeRecv( mpisize, 0 );
        std::vector<Int>     displsRecv( mpisize, 0 );

        Int numSuper = this->NumSuper();
        const IntNumVec& perm    = super_->perm;
        const IntNumVec& permInv = super_->permInv;

        const IntNumVec * pPerm_r;
        const IntNumVec * pPermInv_r;

        pPerm_r = &super_->perm_r;
        pPermInv_r = &super_->permInv_r;

        const IntNumVec& perm_r    = *pPerm_r;
        const IntNumVec& permInv_r = *pPermInv_r;


        // Count the sizes from the A matrix first
        Int numColFirst = this->NumCol() / mpisize;
        Int firstCol = mpirank * numColFirst;
        Int numColLocal;
        if( mpirank == mpisize-1 )
          numColLocal = this->NumCol() - numColFirst * (mpisize-1);
        else
          numColLocal = numColFirst;





        Int*     rowPtr = ArowindLocal.Data();
        Int*     colPtr = AcolptrLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){
          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          Int procCol     = PCOL( blockColIdx, grid_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procRow     = PROW( blockRowIdx, grid_ );
            Int dest = PNUM( procRow, procCol, grid_ );
#if ( _DEBUGlevel_ >= 1 )
            statusOFS << "("<< orow<<", "<<ocol<<") == "<< "("<< row<<", "<<col<<")"<< std::endl;
            statusOFS << "BlockIdx = " << blockRowIdx << ", " <<blockColIdx << std::endl;
            statusOFS << procRow << ", " << procCol << ", " 
              << dest << std::endl;
#endif
            sizeSend[dest]++;
          } // for (i)
        } // for (j)

        // All-to-all exchange of size information
        MPI_Alltoall( 
            &sizeSend[0], 1, MPI_INT,
            &sizeRecv[0], 1, MPI_INT, grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "sizeSend: " << sizeSend << std::endl;
        statusOFS << std::endl << "sizeRecv: " << sizeRecv << std::endl;
#endif



        // Reserve the space
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( ip == 0 ){
            displsSend[ip] = 0;
          }
          else{
            displsSend[ip] = displsSend[ip-1] + sizeSend[ip-1];
          }

          if( ip == 0 ){
            displsRecv[ip] = 0;
          }
          else{
            displsRecv[ip] = displsRecv[ip-1] + sizeRecv[ip-1];
          }
        }

        Int sizeSendTotal = displsSend[mpisize-1] + sizeSend[mpisize-1];
        Int sizeRecvTotal = displsRecv[mpisize-1] + sizeRecv[mpisize-1];

        rowSend.resize( sizeSendTotal );
        colSend.resize( sizeSendTotal );
        valSend.resize( sizeSendTotal );

        rowRecv.resize( sizeRecvTotal );
        colRecv.resize( sizeRecvTotal );
        valRecv.resize( sizeRecvTotal );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "displsSend = " << displsSend << std::endl;
        statusOFS << "displsRecv = " << displsRecv << std::endl;
#endif

        // Put (row, col) to the sending buffer
        std::vector<Int>   cntSize( mpisize, 0 );

        rowPtr = ArowindLocal.Data();
        colPtr = AcolptrLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){

          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          Int procCol     = PCOL( blockColIdx, grid_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procRow     = PROW( blockRowIdx, grid_ );
            Int dest = PNUM( procRow, procCol, grid_ );
            rowSend[displsSend[dest] + cntSize[dest]] = row;
            colSend[displsSend[dest] + cntSize[dest]] = col;
            cntSize[dest]++;
          } // for (i)
        } // for (j)


        // Check sizes match
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( cntSize[ip] != sizeSend[ip] ){
            ErrorHandling( "Sizes of the sending information do not match." );
          }
        }

        // Alltoallv to exchange information
        mpi::Alltoallv( 
            &rowSend[0], &sizeSend[0], &displsSend[0],
            &rowRecv[0], &sizeRecv[0], &displsRecv[0],
            grid_->comm );
        mpi::Alltoallv( 
            &colSend[0], &sizeSend[0], &displsSend[0],
            &colRecv[0], &sizeRecv[0], &displsRecv[0],
            grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "Alltoallv communication of nonzero indices finished." << std::endl;
#endif


#if ( _DEBUGlevel_ >= 1 )
        for( Int ip = 0; ip < mpisize; ip++ ){
          statusOFS << "rowSend[" << ip << "] = " << rowSend[ip] << std::endl;
          statusOFS << "rowRecv[" << ip << "] = " << rowRecv[ip] << std::endl;
          statusOFS << "colSend[" << ip << "] = " << colSend[ip] << std::endl;
          statusOFS << "colRecv[" << ip << "] = " << colRecv[ip] << std::endl;
        }


        //DumpLU();



#endif

        // For each (row, col), fill the nonzero values to valRecv locally.
        for( Int g = 0; g < sizeRecvTotal; g++ ){
          Int row = rowRecv[g];
          Int col = colRecv[g];

          Int blockRowIdx = BlockIdx( row, super_ );
          Int blockColIdx = BlockIdx( col, super_ );

          // Search for the nzval
          bool isFound = false;

          if( blockColIdx <= blockRowIdx ){
            // Data on the L side

            std::vector<LBlock<T> >&  Lcol = this->L( LBj( blockColIdx, grid_ ) );

            for( Int ib = 0; ib < Lcol.size(); ib++ ){
#if ( _DEBUGlevel_ >= 1 )
              statusOFS << "blockRowIdx = " << blockRowIdx << ", Lcol[ib].blockIdx = " << Lcol[ib].blockIdx << ", blockColIdx = " << blockColIdx << std::endl;
#endif
              if( Lcol[ib].blockIdx == blockRowIdx ){
                IntNumVec& rows = Lcol[ib].rows;
                for( int iloc = 0; iloc < Lcol[ib].numRow; iloc++ ){
                  if( rows[iloc] == row ){
                    Int jloc = col - FirstBlockCol( blockColIdx, super_ );
                    valRecv[g] = Lcol[ib].nzval( iloc, jloc );
                    isFound = true;
                    break;
                  } // found the corresponding row
                }
              }
              if( isFound == true ) break;  
            } // for (ib)
          } 
          else{
            // Data on the U side

            std::vector<UBlock<T> >&  Urow = this->U( LBi( blockRowIdx, grid_ ) );

            for( Int jb = 0; jb < Urow.size(); jb++ ){
              if( Urow[jb].blockIdx == blockColIdx ){
                IntNumVec& cols = Urow[jb].cols;
                for( int jloc = 0; jloc < Urow[jb].numCol; jloc++ ){
                  if( cols[jloc] == col ){
                    Int iloc = row - FirstBlockRow( blockRowIdx, super_ );
                    valRecv[g] = Urow[jb].nzval( iloc, jloc );
                    isFound = true;
                    break;
                  } // found the corresponding col
                }
              }
              if( isFound == true ) break;  
            } // for (jb)
          } // if( blockColIdx <= blockRowIdx ) 

          // Did not find the corresponding row, set the value to zero.
          if( isFound == false ){
            statusOFS << "In the permutated order, (" << row << ", " << col <<
              ") is not found in PMatrix." << std::endl;
            valRecv[g] = ZERO<T>();
          }

        } // for (g)


        // Feed back valRecv to valSend through Alltoallv. NOTE: for the
        // values, the roles of "send" and "recv" are swapped.
        mpi::Alltoallv( 
            &valRecv[0], &sizeRecv[0], &displsRecv[0],
            &valSend[0], &sizeSend[0], &displsSend[0],
            grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "Alltoallv communication of nonzero values finished." << std::endl;
#endif

        // Put the nonzero values from valSend to the matrix B.
        B.size = Asize;
        B.nnz  = Annz;
        B.nnzLocal = AnnzLocal;
        B.colptrLocal = AcolptrLocal;
        B.rowindLocal = ArowindLocal;
        B.nzvalLocal.Resize( B.nnzLocal );
        SetValue( B.nzvalLocal, ZERO<T>() );
        // Make sure that the communicator of A and B are the same.
        // FIXME Find a better way to compare the communicators
        //			if( grid_->comm != A.comm ){
        //ErrorHandling( "The DistSparseMatrix providing the pattern has a different communicator from PMatrix." );
        //			}
        B.comm = grid_->comm;

        for( Int i = 0; i < mpisize; i++ )
          cntSize[i] = 0;

        rowPtr = B.rowindLocal.Data();
        colPtr = B.colptrLocal.Data();
        T* valPtr = B.nzvalLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){
          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          Int procCol     = PCOL( blockColIdx, grid_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procRow     = PROW( blockRowIdx, grid_ );
            Int dest = PNUM( procRow, procCol, grid_ );
            valPtr[i] = valSend[displsSend[dest] + cntSize[dest]];
            cntSize[dest]++;
          } // for (i)
        } // for (j)

        // Check sizes match
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( cntSize[ip] != sizeSend[ip] ){
            ErrorHandling( "Sizes of the sending information do not match." );
          }
        }



        return ;
      }
      else{


#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "Converting PMatrix to DistSparseMatrix (2nd format)." << std::endl;
#endif
        Int mpirank = grid_->mpirank;
        Int mpisize = grid_->mpisize;

        std::vector<Int>     rowSend( mpisize );
        std::vector<Int>     colSend( mpisize );
        std::vector<T>  valSend( mpisize );
        std::vector<Int>     sizeSend( mpisize, 0 );
        std::vector<Int>     displsSend( mpisize, 0 );

        std::vector<Int>     rowRecv( mpisize );
        std::vector<Int>     colRecv( mpisize );
        std::vector<T>  valRecv( mpisize );
        std::vector<Int>     sizeRecv( mpisize, 0 );
        std::vector<Int>     displsRecv( mpisize, 0 );

        Int numSuper = this->NumSuper();
        const IntNumVec& perm    = super_->perm;
        const IntNumVec& permInv = super_->permInv;

        const IntNumVec * pPerm_r;
        const IntNumVec * pPermInv_r;

        pPerm_r = &super_->perm_r;
        pPermInv_r = &super_->permInv_r;

        const IntNumVec& perm_r    = *pPerm_r;
        const IntNumVec& permInv_r = *pPermInv_r;


        // Count the sizes from the A matrix first
        Int numColFirst = this->NumCol() / mpisize;
        Int firstCol = mpirank * numColFirst;
        Int numColLocal;
        if( mpirank == mpisize-1 )
          numColLocal = this->NumCol() - numColFirst * (mpisize-1);
        else
          numColLocal = numColFirst;





        Int*     rowPtr = ArowindLocal.Data();
        Int*     colPtr = AcolptrLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){
          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procCol     = PCOL( std::min(blockColIdx,blockRowIdx), grid_ );
            Int procRow     = PROW( std::max(blockColIdx,blockRowIdx), grid_ );
            Int dest = PNUM( procRow , procCol, grid_ );
#if ( _DEBUGlevel_ >= 1 )
            statusOFS << "("<< orow<<", "<<ocol<<") == "<< "("<< row<<", "<<col<<")"<< std::endl;
            statusOFS << "BlockIdx = " << blockRowIdx << ", " <<blockColIdx << std::endl;
            statusOFS << procRow << ", " << procCol << ", " 
              << dest << std::endl;
#endif
            sizeSend[dest]++;
          } // for (i)
        } // for (j)

        // All-to-all exchange of size information
        MPI_Alltoall( 
            &sizeSend[0], 1, MPI_INT,
            &sizeRecv[0], 1, MPI_INT, grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << std::endl << "sizeSend: " << sizeSend << std::endl;
        statusOFS << std::endl << "sizeRecv: " << sizeRecv << std::endl;
#endif



        // Reserve the space
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( ip == 0 ){
            displsSend[ip] = 0;
          }
          else{
            displsSend[ip] = displsSend[ip-1] + sizeSend[ip-1];
          }

          if( ip == 0 ){
            displsRecv[ip] = 0;
          }
          else{
            displsRecv[ip] = displsRecv[ip-1] + sizeRecv[ip-1];
          }
        }

        Int sizeSendTotal = displsSend[mpisize-1] + sizeSend[mpisize-1];
        Int sizeRecvTotal = displsRecv[mpisize-1] + sizeRecv[mpisize-1];

        rowSend.resize( sizeSendTotal );
        colSend.resize( sizeSendTotal );
        valSend.resize( sizeSendTotal );

        rowRecv.resize( sizeRecvTotal );
        colRecv.resize( sizeRecvTotal );
        valRecv.resize( sizeRecvTotal );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "displsSend = " << displsSend << std::endl;
        statusOFS << "displsRecv = " << displsRecv << std::endl;
#endif

        // Put (row, col) to the sending buffer
        std::vector<Int>   cntSize( mpisize, 0 );

        rowPtr = ArowindLocal.Data();
        colPtr = AcolptrLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){

          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procCol     = PCOL( std::min(blockColIdx,blockRowIdx), grid_ );
            Int procRow     = PROW( std::max(blockColIdx,blockRowIdx), grid_ );
            Int dest = PNUM( procRow , procCol, grid_ );
            rowSend[displsSend[dest] + cntSize[dest]] = row;
            colSend[displsSend[dest] + cntSize[dest]] = col;
            cntSize[dest]++;
          } // for (i)
        } // for (j)


        // Check sizes match
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( cntSize[ip] != sizeSend[ip] ){
            ErrorHandling( "Sizes of the sending information do not match." );
          }
        }

        // Alltoallv to exchange information
        mpi::Alltoallv( 
            &rowSend[0], &sizeSend[0], &displsSend[0],
            &rowRecv[0], &sizeRecv[0], &displsRecv[0],
            grid_->comm );
        mpi::Alltoallv( 
            &colSend[0], &sizeSend[0], &displsSend[0],
            &colRecv[0], &sizeRecv[0], &displsRecv[0],
            grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "Alltoallv communication of nonzero indices finished." << std::endl;
#endif


#if ( _DEBUGlevel_ >= 1 )
        for( Int ip = 0; ip < mpisize; ip++ ){
          statusOFS << "rowSend[" << ip << "] = " << rowSend[ip] << std::endl;
          statusOFS << "rowRecv[" << ip << "] = " << rowRecv[ip] << std::endl;
          statusOFS << "colSend[" << ip << "] = " << colSend[ip] << std::endl;
          statusOFS << "colRecv[" << ip << "] = " << colRecv[ip] << std::endl;
        }


        //DumpLU();



#endif

        // For each (row, col), fill the nonzero values to valRecv locally.
        for( Int g = 0; g < sizeRecvTotal; g++ ){
          Int row = rowRecv[g];
          Int col = colRecv[g];


          // Search for the nzval

          auto findBlock = [&] (Int g, Int row,Int col){
            bool transpose = false;

            bool isFound = false;

            Int lrow = std::max(row,col);
            Int lcol = std::min(row,col);

            Int blockRowIdx = BlockIdx( lrow, super_ );
            Int blockColIdx = BlockIdx( lcol, super_ );

            if( blockColIdx <= blockRowIdx ){
              // Data on the L side

              std::vector<LBlock<T> >&  Lcol = this->L( LBj( blockColIdx, grid_ ) );

              for( Int ib = 0; ib < Lcol.size(); ib++ ){
#if ( _DEBUGlevel_ >= 1 )
                statusOFS << "blockRowIdx = " << blockRowIdx << ", Lcol[ib].blockIdx = " << Lcol[ib].blockIdx << ", blockColIdx = " << blockColIdx << std::endl;
#endif
                if( Lcol[ib].blockIdx == blockRowIdx ){
                  IntNumVec& rows = Lcol[ib].rows;
                  for( int iloc = 0; iloc < Lcol[ib].numRow; iloc++ ){
                    if( rows[iloc] == lrow ){
                      Int jloc = lcol - FirstBlockCol( blockColIdx, super_ );
                      valRecv[g] = Lcol[ib].nzval( iloc, jloc );
                      isFound = true;
                      break;
                    } // found the corresponding row
                  }
                }
                if( isFound == true ) break;  
              } // for (ib)

            } 

            if( isFound == false ){
              statusOFS << "In the permutated order, (" << row << ", " << col <<
                ") is not found in PMatrix." << std::endl;
              valRecv[g] = ZERO<T>();
            }
          };

          findBlock(g,row,col);

          // Did not find the corresponding row, set the value to zero.

        } // for (g)


        // Feed back valRecv to valSend through Alltoallv. NOTE: for the
        // values, the roles of "send" and "recv" are swapped.
        mpi::Alltoallv( 
            &valRecv[0], &sizeRecv[0], &displsRecv[0],
            &valSend[0], &sizeSend[0], &displsSend[0],
            grid_->comm );

#if ( _DEBUGlevel_ >= 1 )
        statusOFS << "Alltoallv communication of nonzero values finished." << std::endl;
#endif

        // Put the nonzero values from valSend to the matrix B.
        B.size = Asize;
        B.nnz  = Annz;
        B.nnzLocal = AnnzLocal;
        B.colptrLocal = AcolptrLocal;
        B.rowindLocal = ArowindLocal;
        B.nzvalLocal.Resize( B.nnzLocal );
        SetValue( B.nzvalLocal, ZERO<T>() );
        // Make sure that the communicator of A and B are the same.
        // FIXME Find a better way to compare the communicators
        //			if( grid_->comm != A.comm ){
        //ErrorHandling( "The DistSparseMatrix providing the pattern has a different communicator from PMatrix." );
        //			}
        B.comm = grid_->comm;

        for( Int i = 0; i < mpisize; i++ )
          cntSize[i] = 0;

        rowPtr = B.rowindLocal.Data();
        colPtr = B.colptrLocal.Data();
        T* valPtr = B.nzvalLocal.Data();

        for( Int j = 0; j < numColLocal; j++ ){
          Int ocol = firstCol + j;
          Int col         = perm[ perm_r[ ocol] ];
          Int blockColIdx = BlockIdx( col, super_ );
          for( Int i = colPtr[j] - 1; i < colPtr[j+1] - 1; i++ ){
            Int orow = rowPtr[i]-1;
            Int row         = perm[ orow ];
            Int blockRowIdx = BlockIdx( row, super_ );
            Int procCol     = PCOL( std::min(blockColIdx,blockRowIdx), grid_ );
            Int procRow     = PROW( std::max(blockColIdx,blockRowIdx), grid_ );
            Int dest = PNUM( procRow , procCol, grid_ );

            valPtr[i] = valSend[displsSend[dest] + cntSize[dest]];
            cntSize[dest]++;
          } // for (i)
        } // for (j)

        // Check sizes match
        for( Int ip = 0; ip < mpisize; ip++ ){
          if( cntSize[ip] != sizeSend[ip] ){
            ErrorHandling( "Sizes of the sending information do not match." );
          }
        }



        return ;


      }
    }     // -----  end of method PMatrix::PMatrixToDistSparseMatrix  ----- 





  template<typename T>
    Int PMatrix<T>::NnzLocal	(  )
    {
      Int numSuper = this->NumSuper();
      Int nnzLocal = 0;
      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        if( MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          std::vector<LBlock<T> >& Lcol = this->L( LBj( ksup, grid_ ) );
          for( Int ib = 0; ib < Lcol.size(); ib++ ){
            nnzLocal += Lcol[ib].numRow * Lcol[ib].numCol;
          }
        } // if I own the column of ksup
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) ){
          std::vector<UBlock<T> >& Urow = this->U( LBi( ksup, grid_ ) );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            nnzLocal += Urow[jb].numRow * Urow[jb].numCol;
          }
        } // if I own the row of ksup
      }


      return nnzLocal;
    } 		// -----  end of method PMatrix::NnzLocal  ----- 


  template<typename T>
    LongInt PMatrix<T>::Nnz	(  )
    {
      LongInt nnzLocal = LongInt( this->NnzLocal() );
      LongInt nnz;

      MPI_Allreduce( &nnzLocal, &nnz, 1, MPI_LONG_LONG, MPI_SUM, grid_->comm );


      return nnz;
    } 		// -----  end of method PMatrix::Nnz  ----- 

  template< >
    inline void PMatrix<Real>::GetNegativeInertia	( Real& inertia )
    {
      Int numSuper = this->NumSuper(); 

      Real inertiaLocal = 0.0;
      inertia          = 0.0;

      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // I own the diagonal block	
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) &&
            MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          LBlock<Real> & LB = this->L( LBj( ksup, grid_ ) )[0];
          for( Int i = 0; i < LB.numRow; i++ ){
            if( LB.nzval(i, i) < 0 ){
//              Int fc = FirstBlockCol( ksup, this->super_ );
//              statusOFS<<"Neg L("<<fc+i<<", "<<fc+i<<") = "<< LB.nzval(i, i)<<std::endl;
              inertiaLocal++;
            }
          }
        }
      }

      // All processors own diag
      mpi::Allreduce( &inertiaLocal, &inertia, 1, MPI_SUM, grid_->comm );


      return ;
    } 		// -----  end of method PMatrix::GetNegativeInertia  ----- 


  template< >
    inline void PMatrix<Complex>::GetNegativeInertia	( Real& inertia )
    {
      Int numSuper = this->NumSuper(); 

      Real inertiaLocal = 0.0;
      inertia          = 0.0;

      for( Int ksup = 0; ksup < numSuper; ksup++ ){
        // I own the diagonal block	
        if( MYROW( grid_ ) == PROW( ksup, grid_ ) &&
            MYCOL( grid_ ) == PCOL( ksup, grid_ ) ){
          LBlock<Complex> & LB = this->L( LBj( ksup, grid_ ) )[0];
          for( Int i = 0; i < LB.numRow; i++ ){
            if( LB.nzval(i, i).real() < 0 ){
//              Int fc = FirstBlockCol( ksup, this->super_ );
//              statusOFS<<"Neg L("<<fc+i<<", "<<fc+i<<") = "<< LB.nzval(i, i)<<std::endl;
              inertiaLocal++;
            }
          }
        }
      }

      // All processors own diag
      mpi::Allreduce( &inertiaLocal, &inertia, 1, MPI_SUM, grid_->comm );


      return ;
    } 		// -----  end of method PMatrix::GetNegativeInertia  ----- 

  template<typename T>
    inline PMatrix<T> * PMatrix<T>::Create(const GridType * pGridType, const SuperNodeType * pSuper, const PSelInvOptions * pSelInvOpt , const FactorizationOptions * pFactOpt)
    { 
      PMatrix<T> * pMat = NULL;
      if(pFactOpt->Symmetric == 0){
        pMat = new PMatrixUnsym<T>( pGridType, pSuper, pSelInvOpt, pFactOpt  );
      }
      else{
        pMat = new PMatrix<T>( pGridType, pSuper, pSelInvOpt, pFactOpt  );
      }

      return pMat;
    } 		// -----  end of factory method PMatrix::Create  ----- 

  template<typename T>
    inline PMatrix<T> * PMatrix<T>::Create(const FactorizationOptions * pFactOpt)
    { 
      PMatrix<T> * pMat = NULL;
      if(pFactOpt->Symmetric == 0){
        pMat = new PMatrixUnsym<T>();
      }
      else{
        pMat = new PMatrix<T>();
      }

      return pMat;
    } 		// -----  end of factory method PMatrix::Create  ----- 



  template<typename T>
    inline void PMatrix<T>::DumpLU()
    {
      //#if ( _DEBUGlevel_ >= 1 )
      const IntNumVec& perm    = super_->perm;
      const IntNumVec& permInv = super_->permInv;

      const IntNumVec * pPerm_r;
      const IntNumVec * pPermInv_r;

      if(optionsFact_->RowPerm=="NOROWPERM"){
        pPerm_r = &super_->perm;
        pPermInv_r = &super_->permInv;
      }
      else{
        pPerm_r = &super_->perm_r;
        pPermInv_r = &super_->permInv_r;
      }

      const IntNumVec& perm_r    = *pPerm_r;
      const IntNumVec& permInv_r = *pPermInv_r;

      statusOFS<<"Col perm is "<<perm<<std::endl;
      statusOFS<<"Row perm is "<<perm_r<<std::endl;

      statusOFS<<"Inv Col perm is "<<permInv<<std::endl;
      statusOFS<<"Inv Row perm is "<<permInv_r<<std::endl;




      statusOFS<<"Content of L"<<std::endl;
      //dump L
      for(Int j = 0;j<this->L_.size();++j){
        std::vector<LBlock<T> >&  Lcol = this->L( j );
        if(Lcol.size()>0){
          Int blockColIdx = GBj( j, this->grid_ );
          Int fc = FirstBlockCol( blockColIdx, this->super_ );
          Int numCol = Lcol[0].numCol ;

          for(Int col = fc; col<fc+numCol;col++){
            for( Int ib = 0; ib < Lcol.size(); ib++ ){
              for(Int ir = 0; ir< Lcol[ib].rows.m(); ++ir){
               
                Int row = Lcol[ib].rows[ir];
                Int ocol = permInv_r[permInv[col]];

                if(row!=col){ continue;}

                Int orow = permInv[row];
                Int jloc = col - FirstBlockCol( blockColIdx, this->super_ );
                Int iloc = ir;
                T val = Lcol[ib].nzval( iloc, jloc );
                statusOFS << "("<< orow<<", "<<ocol<<") == "<< "("<< row<<", "<<col<<") = "<<val<< std::endl;
              }
            }
          }

//          for( Int ib = 0; ib < Lcol.size(); ib++ ){
//            for(Int ir = 0; ir< Lcol[ib].rows.m(); ++ir){
//              Int row = Lcol[ib].rows[ir];
//              for(Int col = fc; col<fc+Lcol[ib].numCol;col++){
//                Int ocol = permInv_r[permInv[col]];
//                Int orow = permInv[row];
//                Int jloc = col - FirstBlockCol( blockColIdx, this->super_ );
//                Int iloc = ir;
//                T val = Lcol[ib].nzval( iloc, jloc );
//                statusOFS << "("<< orow<<", "<<ocol<<") == "<< "("<< row<<", "<<col<<") = "<<val<< std::endl;
//              }
//            }
//          }
        }
      }

      statusOFS<<"Content of U"<<std::endl;

      //dump U
      for(Int i = 0;i<this->U_.size();++i){
        std::vector<UBlock<T> >&  Urow = this->U( i );
        if(Urow.size()>0){
          Int blockRowIdx = GBi( i, this->grid_ );
          Int fr = FirstBlockRow( blockRowIdx, this->super_ );
          for( Int jb = 0; jb < Urow.size(); jb++ ){
            for(Int row = fr; row<fr+Urow[jb].numRow;row++){
              for(Int ic = 0; ic< Urow[jb].cols.m(); ++ic){
                Int col = Urow[jb].cols[ic];
                Int ocol = permInv_r[permInv[col]];
                Int orow = permInv[row];
                Int iloc = row - FirstBlockRow( blockRowIdx, this->super_ );
                Int jloc = ic;
                T val = Urow[jb].nzval( iloc, jloc );
                statusOFS << "("<< orow<<", "<<ocol<<") == "<< "("<< row<<", "<<col<<") = "<<val<< std::endl;

              }
            }

          }
        }
      }
      //#endif

    }


  template<typename T>
    inline void PMatrix<T>::CopyLU( const PMatrix<T> & C){
      ColBlockIdx_ = C.ColBlockIdx_;
      RowBlockIdx_ = C.RowBlockIdx_;
      L_ = C.L_;
      U_ = C.U_;
    }

  template<typename T>
    inline double PMatrix<T>::GetTotalFlops(){
      double total = 0.0;
      MPI_Allreduce( &this->localFlops_,&total, 1,MPI_DOUBLE, MPI_SUM, this->grid_->comm );
      return total;
    }

} // namespace PEXSI



#endif //_PEXSI_PSELINV_IMPL_HPP_
