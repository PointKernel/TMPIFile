// @(#)root/io:$Id$
// Author: Amit Bashyal, August 2018

/*************************************************************************
 * Copyright (C) 1995-2009, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TMPIFile
#define ROOT_TMPIFile

#include "TClientInfo.h"
#include "TBits.h"
#include "TFileMerger.h"
#include "TMemFile.h"

#include "mpi.h"

#include <vector>

class TMPIFile : public TMemFile {

private:
  Int_t argc;
  Int_t fEndProcess = 0;
  Int_t fSplitLevel;
  Int_t fColor;

  MPI_Comm row_comm;
  MPI_Request fRequest = 0;

  char **argv;
  char fMPIFilename[1000];
  char *fSendBuf = 0; // Workers' message buffer

  struct ParallelFileMerger : public TObject {
  public:
    using ClientColl_t = std::vector<TClientInfo>;

    TString fFilename;
    TBits fClientsContact;
    UInt_t fNClientsContact;
    ClientColl_t fClients;
    TTimeStamp fLastMerge;
    TFileMerger fMerger;
    
    ParallelFileMerger(const char *filename, Int_t compression_settings, Bool_t writeCache = kFALSE);
    virtual ~ParallelFileMerger();
    
    ULong_t Hash() const;
    const char *GetName() const;
    
    Bool_t InitialMerge(TFile *input);
    Bool_t Merge();
    Bool_t NeedMerge(Float_t clientThreshold);
    Bool_t NeedFinalMerge();
    void RegisterClient(UInt_t clientID, TFile *file);
    
    TClientInfo tcl;
  };

  MPI_Comm SplitMPIComm(MPI_Comm source, Int_t comm_no); //<Divides workers per master
  
  void GetRootName();
  void UpdateEndProcess(); // update how many workers reached end of job

public:
  TMPIFile(const char *name, char *buffer, Long64_t size = 0, Option_t *option = "", Int_t split = 0, const char *ftitle = "", Int_t compress = 4);
  TMPIFile(const char *name, Option_t *option = "", Int_t split = 0, const char *ftitle = "", Int_t compress = 4); // no complete implementation
  virtual ~TMPIFile();

  // some functions on MPI information
  Int_t GetMPILocalSize();
  Int_t GetMPILocalRank();
  Int_t GetMPIColor();
  Int_t GetMPIGlobalRank();
  Int_t GetSplitLevel();
  Int_t GetMPIGlobalSize();

  // Master Functions
  void RunCollector(Bool_t cache = false);
  void R__MigrateKey(TDirectory *destination, TDirectory *source);
  void R__DeleteObject(TDirectory *dir, Bool_t withReset);
  Bool_t R__NeedInitialMerge(TDirectory *dir);
  void ReceiveAndMerge(Bool_t cache = false, MPI_Comm = 0, Int_t size = 0);
  Bool_t IsCollector();

  // Worker Functions
  void CreateBufferAndSend(MPI_Comm comm = 0);
  // Empty Buffer to signal the end of job...
  void CreateEmptyBufferAndSend(MPI_Comm comm = 0);
  void Sync();

  // Finalize work and save output in disk.
  void MPIClose();

  ClassDef(TMPIFile, 0)
};
#endif
